#!/usr/bin/python

from mininet.topo import Topo
from mininet.node import CPULimitedHost
from mininet.link import TCLink
from mininet.net import Mininet
from mininet.log import lg
from mininet.util import dumpNodeConnections
from mininet.cli import CLI

import subprocess
from subprocess import Popen, PIPE
from time import sleep, time
from multiprocessing import Process
import termcolor as T
from argparse import ArgumentParser

import sys
import socket
import os
from util.monitor import monitor_qlen
from util.monitor import monitor_devs_ng
from util.monitor import monitor_devs
from util.helper import stdev


def cprint(s, color, cr=True):
    """Print in color
       s: string to print
       color: color to use"""
    if cr:
        print T.colored(s, color)
    else:
        print T.colored(s, color),


# Parse arguments

parser = ArgumentParser(description="Shrew attack tests")
parser.add_argument('--bw-host', '-B',
                    dest="bw_host",
                    type=float,
                    action="store",
                    help="Bandwidth of host links",
                    required=True)

parser.add_argument('--bw-net', '-b',
                    dest="bw_net",
                    type=float,
                    action="store",
                    help="Bandwidth of network link",
                    required=True)

parser.add_argument('--delay',
                    dest="delay",
                    type=float,
                    help="Delay in milliseconds of host links",
                    default=87)

parser.add_argument('--dir', '-d',
                    dest="dir",
                    action="store",
                    help="Directory to store outputs",
                    default="results",
                    required=True)


parser.add_argument('--maxq',
                    dest="maxq",
                    action="store",
                    help="Max buffer size of network interface in packets",
                    default=15)

parser.add_argument('--cong',
                    dest="cong",
                    help="Congestion control algorithm to use",
                    default="reno")

parser.add_argument('--iperf',
                    dest="iperf",
                    help="Path to custom iperf",
                    required=True)

parser.add_argument('--period',
		    dest="period",
		    type=float,
                    action="store",
		    help="Period between attacks in sec",
		    required=True)

parser.add_argument('--length',
		    dest="length",
		    type=float,
		    action="store",
		    help="Length of attack in sec",
		    required=True)

# Expt parameters
args = parser.parse_args()

CUSTOM_IPERF_PATH = args.iperf
assert(os.path.exists(CUSTOM_IPERF_PATH))

if not os.path.exists(args.dir):
    os.makedirs(args.dir)

lg.setLogLevel('info')

# Topology to be instantiated in Mininet
class DOSTopo(Topo):
    "DOS topology for Shrew Attack experiment"

    def build(self, cpu=None, bw_host=None, bw_net=None,
	      delay=None, maxq=None):
        s0 = self.addSwitch('s0')
        s1 = self.addSwitch('s1') 
        goodHost = self.addHost('goodHost')
        badHost = self.addHost('badHost')
        goodReceiver = self.addHost('gServer')
        badReceiver = self.addHost('bServer') 
        self.addLink(s0, s1, bw=bw_net, cpu=cpu, max_queue_size=maxq, delay=delay)
        self.addLink(goodHost, s0, bw=bw_host, cpu=cpu, delay=delay)
        self.addLink(badHost, s0, bw=bw_host, cpu=cpu, delay=delay)
        self.addLink(goodReceiver, s1, bw=bw_host, cpu=cpu, delay=delay)
        self.addLink(badReceiver, s1, bw=bw_host, cpu=cpu, delay=delay)
        return	

def start_tcpprobe():
    "Install tcp_probe module and dump to file"
    os.system("rmmod tcp_probe 2>/dev/null; modprobe tcp_probe;")
    Popen("cat /proc/net/tcpprobe > %s/tcp_probe.txt" %
          args.dir, shell=True)

def stop_tcpprobe():
    os.system("killall -9 cat; rmmod tcp_probe &>/dev/null;")

def start_bwmon(interval_sec=0.1, outfile="bwm.txt"):
    try:
        monitor = Process(target=monitor_devs_ng,
                        args=(outfile, interval_sec))
        monitor.daemon=True
        monitor.start()
    except:
        raise
    finally:
        return monitor


# Hint: iperf command to start the receiver:
#       '%s -s -p %s > %s/iperf_server.txt' %
#        (CUSTOM_IPERF_PATH, 5001, args.dir)
# Note: The output file should be <args.dir>/iperf_server.txt
#       It will be used later in count_connections()

def start_receivers(net):
    seconds = 3600
    gServer = net.get('gServer')
    bServer = net.get('bServer')

    gServer.popen('%s -s -p %s > %s/iperf_server.txt' % (CUSTOM_IPERF_PATH, 5001, args.dir), shell=True)
    bServer.popen('%s -s -u -p %s > %s/iperf_server2.txt' % (CUSTOM_IPERF_PATH, 5001, args.dir), shell=True)
    pass

def start_sender(net):
    # Seconds to run iperf; keep this very high
    seconds = 3600
    receiver = net.get('gServer')
    goodHost = net.get('goodHost')
    
    goodHost.cmd('touch {0}/output_file'.format(args.dir))
    goodHost.popen('%s -c %s -p %s -t %d -i 1 -yc -Z %s > %s/%s' % (
            CUSTOM_IPERF_PATH, receiver.IP(), 5001, seconds, args.cong, args.dir, "{0}-bw.txt".format(args.period)),shell=True)
    return

#TODO: Start attack flow in a daemon thread to periodically 
# send 
def start_attacker(net):
    receiver = net.get('bServer')
    attacker = net.get('badHost')
    attacker.popen('python start_attacker.py -p {0} -l {1} -d {2}'.format(args.period, args.length, receiver.IP()), shell=True)
    return

def avg(s):
    if ',' in s:
        lst = [float(f) for f in s.split(',')]
    elif type(s) == str:
        lst = [float(s)]
    elif type(s) == list:
        lst = s
    return sum(lst)/len(lst)

def main():
    "Create network and run Buffer Sizing experiment"

    start = time()
    # Reset to known state
    topo = DOSTopo(bw_host=args.bw_host,
                    delay='%sms' % (args.delay),
                    bw_net=args.bw_net, maxq=int(args.maxq))
    net = Mininet(topo=topo, host=CPULimitedHost, link=TCLink)
    net.start()
    dumpNodeConnections(net.hosts)
    net.pingAll()

    start_receivers(net)

    start_tcpprobe()

    cprint("Starting experiment", "green")

    start_sender(net)
    host = net.get('goodHost')
    host.popen("ip route change 10.0.0.0/8 dev %s rto_min %s scope link src %s proto kernel" % ('goodHost-eth0', 0.9, host.IP()), shell=True).communicate()
    #wait for iperf tcp to start
    sleep(10)
    start_attacker(net)
    sleep(10)
    bwmon = start_bwmon(outfile="{0}/{1}-{2}-{3}-bwm.txt".format(args.dir, args.cong, args.length, args.period))
    sleep(10)
    # Shut down iperf processes
    os.system('killall -9 ' + CUSTOM_IPERF_PATH)

    net.stop()
    Popen("killall -9 top bwm-ng tcpdump cat mnexec", shell=True).wait()
    stop_tcpprobe()
    bwmon.terminate()
    end = time()

if __name__ == '__main__':
    try:
        main()
    except:
        print "-"*80
        print "Caught exception.  Cleaning up..."
        print "-"*80
        import traceback
        traceback.print_exc()
        os.system("killall -9 top bwm-ng tcpdump cat mnexec iperf; mn -c")

