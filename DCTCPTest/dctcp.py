

"CS244 Winter 2013 Assignment 3: DCTCP"

from mininet.topo import Topo
from mininet.node import CPULimitedHost
from mininet.link import TCLink
from mininet.net import Mininet
from mininet.log import lg, info
from mininet.util import dumpNodeConnections
from mininet.cli import CLI
from subprocess import Popen, PIPE
from time import sleep, time
from multiprocessing import Process
from argparse import ArgumentParser
from monitor import monitor_qlen
from startopo import StarTopo
import termcolor as T
import sys
import os
import math

# Number of samples to skip for reference util calibration.
CALIBRATION_SKIP = 20

# Number of samples to grab for reference util calibration.
CALIBRATION_SAMPLES = 10

# Number of samples to take in get_rates() before returning.
NSAMPLES = 8

# Time to wait between samples, in seconds, as a float.
SAMPLE_PERIOD_SEC = 1.0

# Time to wait for first sample, in seconds, as a float.
SAMPLE_WAIT_SEC = 3.0


def cprint(s, color, cr=True):
    """Print in color
       s: string to print
       color: color to use"""
    if cr:
        print T.colored(s, color)
    else:
        print T.colored(s, color),


# Get the number of bytes on that particular interface
def get_txbytes(iface):
    f = open('/proc/net/dev', 'r')
    lines = f.readlines()
    for line in lines:
        if iface in line:
            break
    f.close()
    if not line:
        raise Exception("could not find iface %s in /proc/net/dev:%s" %
                        (iface, lines))
    # Extract TX bytes from:
    # Inter-|   Receive                                                |  Transmit
    # face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed
    # lo: 6175728   53444    0    0    0     0          0         0  6175728   53444    0    0    0     0       0          0
    return float(line.split()[9])


def get_rates(iface, nsamples=NSAMPLES, period=SAMPLE_PERIOD_SEC,
              wait=SAMPLE_WAIT_SEC):
    """Returns the interface @iface's current utilization in Mb/s.  It
    utilization measured over @period time.  Before measuring it waits
    for @wait seconds to 'warm up'."""
    # Returning nsamples requires one extra to start the timer.
    nsamples += 1
    last_time = 0
    last_txbytes = 0
    ret = []
    sleep(wait)
    while nsamples:
        nsamples -= 1
        txbytes = get_txbytes(iface)
        now = time()
        elapsed = now - last_time
        # if last_time:
        #    print "elapsed: %0.4f" % (now - last_time)
        last_time = now
        # Get rate in Mbps; correct for elapsed time.
        rate = (txbytes - last_txbytes) * 8.0 / 1e6 / elapsed
        if last_txbytes != 0:
            # Wait for 1 second sample
            ret.append(rate)
        last_txbytes = txbytes
        print '.',
        sys.stdout.flush()
        sleep(period)
    return ret


# Parsing arguments for the code
parser = ArgumentParser(description="Bufferbloat tests")
parser.add_argument('--bw-host', '-B',
                    type=float,
                    help="Bandwidth of host links (Mb/s)",
                    default=100)

parser.add_argument('--bw-net', '-b',
                    type=float,
                    help="Bandwidth of bottleneck (network) link (Mb/s)",
                    required=True)

parser.add_argument('--delay',
                    type=float,
                    help="Link propagation delay (ms)",
                    required=True)

parser.add_argument('--dir', '-d',
                    help="Directory to store outputs",
                    required=True)

parser.add_argument('--hosts', '-n',
                    help="Duration (sec) to run the experiment",
                    type=int,
                    default=3)

parser.add_argument('--time', '-t',
                    help="Duration (sec) to run the experiment",
                    type=int,
                    default=10)

parser.add_argument('--maxq',
                    type=int,
                    help="Max buffer size of network interface in packets",
                    default=100)

# RED Parameters 
parser.add_argument('--mark_threshold', '-k',
                    help="Marking threshold",
                    type=int,
                    default="20")

parser.add_argument('--red_limit',
                    help="RED limit",
                    default="1000000")

parser.add_argument('--red_min',
                    help="RED min marking threshold",
                    default="20000")

parser.add_argument('--red_max',
                    help="RED max marking threshold",
                    default="25000")

parser.add_argument('--red_avpkt',
                    help="RED average packet size",
                    default="1000")

parser.add_argument('--red_burst',
                    help="RED burst size",
                    default="20")

parser.add_argument('--red_prob',
                    help="RED marking probability",
                    default="1")

parser.add_argument('--dctcp',
                    help="Enable DCTCP",
                    type=int,
                    default="0")

parser.add_argument('--red',
                    help="Enable RED",
                    type=int,
                    default="0")

parser.add_argument('--iperf',
                    dest="iperf",
                    help="Path to custom iperf",
                    required=True)

parser.add_argument('--vtcp',
                    type=int,
                    default=None)

############################
# Linux uses CUBIC-TCP by default that doesn't have the usual sawtooth
# behaviour.  For those who are curious, invoke this script with
# --cong cubic and see what happens...
# sysctl -a | grep cong should list some interesting parameters.
parser.add_argument('--cong',
                    help="Congestion control algorithm to use",
                    default="reno")

# Expt parameters
args = parser.parse_args()

CUSTOM_IPERF_PATH = '/usr/bin/iperf'
assert (os.path.exists(CUSTOM_IPERF_PATH))

if not os.path.exists(args.dir):
    os.makedirs(args.dir)


# Simple wrappers around monitoring utilities.  You are welcome to
# contribute neatly written (using classes) monitoring scripts for
# Mininet!
def start_tcpprobe(outfile="cwnd.txt"):
    os.system("rmmod tcp_probe; modprobe tcp_probe full=1;")
    Popen("cat /proc/net/tcpprobe > %s/%s" % (args.dir, outfile),
          shell=True)


def stop_tcpprobe():
    Popen("killall -9 cat", shell=True).wait()


# Enable DCTCP and ECN in the Linux Kernel
def SetDCTCPState():
    Popen("sysctl -w net.ipv4.tcp_congestion_control=reno", shell=True).wait()
    Popen("sysctl -w net.ipv4.tcp_ecn=1", shell=True).wait()


# Disable DCTCP and ECN in the Linux Kernel
def ResetDCTCPState():
    Popen("sysctl -w net.ipv4.tcp_congestion_control=reno", shell=True).wait()
    Popen("sysctl -w net.ipv4.tcp_ecn=0", shell=True).wait()


# Monitor the queue occupancy
def start_qmon(iface, interval_sec=0.001, outfile="q.txt"):
    monitor = Process(target=monitor_qlen,
                      args=(iface, interval_sec, outfile))
    monitor.start()
    return monitor


# Start the receiver of the flows, its fixed to be h0 here
def start_receiver(net, ecn):
    h0 = net.getNodeByName('h0')
    h0.popen("sysctl -w net.ipv4.tcp_ecn=%u" % ecn)
    print "Starting iperf server..."
    server = h0.popen("%s -s -w 16m" % CUSTOM_IPERF_PATH)


# Start senders sending traffic to receiver h0
def start_senders(net, ecn, vtcp):
    h0 = net.getNodeByName('h0')
    for i in range(args.hosts - 1):
        print "Starting iperf client..."
        hn = net.getNodeByName('h%d' % (i + 1))
        hn.popen("sysctl -w net.ipv4.tcp_ecn=%u" % ecn)
        hn.popen("sysctl -w net.ipv4.tcp_vtcp=%u" % vtcp)
        client = hn.popen("%s -c " % CUSTOM_IPERF_PATH + h0.IP() + " -t 1000")


# Function to compute the median
def median(l):
    "Compute median from an unsorted list of values"
    s = sorted(l)
    if len(s) % 2 == 1:
        return s[(len(l) + 1) / 2 - 1]
    else:
        lower = s[len(l) / 2 - 1]
        upper = s[len(l) / 2]
        return float(lower + upper) / 2


# Set the speed of an interface
def set_speed(iface, spd):
    "Change htb maximum rate for interface"
    cmd = ("tc class change dev %s parent 5:0 classid 5:1 "
           "htb rate %s burst 15k" % (iface, spd))
    os.system(cmd)


# Set the red parameters correctly
def set_red(iface, red_params):
    "Change RED params for interface"
    cmd = ("tc qdisc change dev %s parent 5:1 handle 6: "
           "red limit %s min %s max %s avpkt %s "
           "ecn burst %s probability %s" % (iface, red_params['limit'],
                                        red_params['min'], red_params['max'], red_params['avpkt'],
                                        red_params['burst'], red_params['prob']))
    os.system(cmd)


def dctcp():
    if not os.path.exists(args.dir):
        os.makedirs(args.dir)
    os.system("sudo sysctl -w net.ipv4.tcp_congestion_control=%s" % args.cong)
    if (args.dctcp):
        SetDCTCPState()
        edctcp = 1
    else:
        ResetDCTCPState()
        edctcp = 1

    # Set the red parameters passed to this code, otherwise use the default
    # settings that are set in Mininet code.
    red_settings = {}
    red_settings['limit'] = args.red_limit
    red_settings['min'] = args.red_min
    red_settings['max'] = args.red_max
    red_settings['avpkt'] = args.red_avpkt
    red_settings['burst'] = args.red_burst
    red_settings['prob'] = args.red_prob
    print red_settings
    # Instantiate the topology using the require parameters
    topo = StarTopo(n=args.hosts, bw_host=args.bw_host,
                    delay='%sms' % args.delay,
                    bw_net=args.bw_net,
                    maxq=args.maxq,
                    enable_dctcp=edctcp,
                    enable_red=args.red,
                    red_params=red_settings,
                    show_mininet_commands=0)
    net = Mininet(topo=topo, host=CPULimitedHost, link=TCLink,
                  autoPinCpus=True)
    net.start()
    # This dumps the topology and how nodes are interconnected through
    # links.
    dumpNodeConnections(net.hosts)
    # This performs a basic all pairs ping test.
    net.pingAll()

    # Allow for connections to be set up initially and then revert back the
    # speed of the bottleneck link to the original passed value
    iface = "s0-eth1"
    #set_red(iface, red_settings)
    eecn = 0
    if (args.dctcp):
	eecn = 1
    vtcp=0
    if (args.vtcp):
        vtcp=1
    print (topo.port('s0', 'h0'))
    print (net.getNodeByName('s0').intf('lo'))
    print ("I just printed the first switch")
    #set_speed(iface, "2Gbit")
    #set_speed(iface, "10Kbit")
   # if (eecn == 1):
   #     os.system("sudo wireshark &") 
   # 	sleep(20)
    print ("part 1")
    start_receiver(net, eecn)
    print ("part 2")
    start_senders(net, eecn, vtcp)
    print ("part 3")
   # sleep(5)
    #set_speed(iface, "%.2fMbit" % args.bw_net)
    # Let the experiment stabilize initially
    sleep(20)

    # Start monitoring the queue sizes.
    qmon = start_qmon(iface='s0-eth1',
                      outfile='%s/q.txt' % (args.dir))

    # Start all the monitoring processes
    start_tcpprobe("cwnd.txt")
    # Run the experiment for the specified time
    start_time = time()
    while True:
        now = time()
        delta = now - start_time
        if delta > args.time:
            break
            # print "%.1fs left..." % (args.time - delta)

    # If the experiment involves marking bandwidth for different threshold
    # then get the rate of the bottlenect link
    if (args.mark_threshold):
        rates = get_rates(iface='s0-eth1', nsamples=CALIBRATION_SAMPLES + CALIBRATION_SKIP)
        rates = rates[CALIBRATION_SKIP:]
        reference_rate = median(rates)
        if (reference_rate > 20):
	    print "I AM OPENING KTXT"
            with open(args.dir + "/k.txt", "a") as myfile:
                myfile.write(str(args.mark_threshold) + ",")
                myfile.write(str(reference_rate))
                myfile.write("\n")
                myfile.close()

    stop_tcpprobe()
    qmon.terminate()
    net.stop()
    # Ensure that all processes you create within Mininet are killed.
    # Sometimes they require manual killing.
    Popen("pgrep -f webserver.py | xargs kill -9", shell=True).wait()


if __name__ == "__main__":
    dctcp()
