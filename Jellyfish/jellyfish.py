#!/usr/bin/python 

"CS244 Spring 2013 Assignment 3: Jellyfish"

from mininet.topo import Topo
from mininet.node import CPULimitedHost
from mininet.link import TCLink
from mininet.node import RemoteController
from mininet.net import Mininet
from mininet.log import lg, info
from mininet.util import dumpNodeConnections
from mininet.cli import CLI

from ripl.ripl.dctopo import FatTreeTopo, JellyfishTopo

from ripl.ripl.routing import KSPRouting, ECMPRouting, HashedStructuredRouting, RandomStructuredRouting

import shlex

from subprocess import Popen, PIPE
from time import sleep, time
from multiprocessing import Process
from argparse import ArgumentParser
from random import randrange, seed, random, shuffle

from monitor import monitor_devs_ng
import termcolor as T

import sys
import os
import math

CUSTOM_IPERF_PATH = "iperf-patched/src/iperf"

parser = ArgumentParser(description="Jellyfish Tests")

parser.add_argument('-t',
                    dest="topo",
                    action="store",
                    help="Topology",
                    default="jf")

parser.add_argument('-r',
                    dest="routing",
                    action="store",
                    help="Routing algorithm",
                    default="ksp")

parser.add_argument('-e',
                    dest="exp",
                    action="store",
                    help="Experiment",
                    default="l")

parser.add_argument('-nse',
                    dest="nServers",
                    type=int,
                    action="store",
                    help="Number of servers",
                    default=16)

parser.add_argument('-nsw',
                    dest="nSwitches",
                    type=int,
                    action="store",
                    help="Number of switches",
                    default=20)

parser.add_argument('-np',
                    dest="nPorts",
                    type=int,
                    action="store",
                    help="Number of ports per switch",
                    default=4)

parser.add_argument('-dir',
                    dest="dir",
                    action="store",
                    help="Directory to store outputs",
                    required=True)

parser.add_argument('--cong',
                    dest="cong",
                    help="Congestion control algorithm to use",
                    default="bic")

parser.add_argument('-p',
                    dest='p',
                    type=float,
                    action='store',
                    help='Probability that a host pair is included in the route computation',
                    default=1.0)

parser.add_argument('-f',
                    dest='flows',
                    type=int,
                    action='store',
                    help='Number of flows for TCP throughput test')

args = parser.parse_args()

def increment_link_count(link, link_counts):
    if link in link_counts:
        link_counts[link] += 1
    else:
        link_counts[link] = 1

# route is a list of nodes
# link_counts is a dictionary of links to their path counts
def parse_route(route, link_counts):
    if len(route) == 0:
        return
    curr_start = route[0]
    for i in range(1, len(route)):
        node = route[i]
        link = (curr_start, node)
        increment_link_count(link, link_counts)
        curr_start = node

# routes is a list of lists of nodes
# link_counts is a dictionary of links to their path counts
def parse_routes(routes, link_counts):
    for route in routes:
        parse_route(route, link_counts)

def containsHost(link, hosts):
    return (link[0] in hosts) or (link[1] in hosts)

def sort_counts(link_counts, hosts):
    counts = {}
    for link in link_counts:
        if containsHost(link, hosts):
            continue
        if link_counts[link] in counts:
            counts[link_counts[link]] += 1
        else:
            counts[link_counts[link]] = 1
    return counts

def write_counts(counts, filename):
    f = open("%s/%s" % (args.dir, filename), 'w')
    for num_paths in sorted(counts.iterkeys()):
        f.write("%s %s\n" % (str(num_paths), str(counts[num_paths])))
    f.close()

ROUTING = { 
    'ksp' : KSPRouting,
    'ecmp' : ECMPRouting,
    'hashed' : HashedStructuredRouting,
    'random' : RandomStructuredRouting
}

def links_experiment(topo, tp, routing):
    seed(0)
    link_counts = {}
    routing_obj = ROUTING[routing](topo)
    hosts = topo.hosts()
    for src in hosts:
        for dst in hosts:
            if src == dst:
                # skip if same node
                continue
            r = random()
            if r < args.p:
                #routes = routing_obj.get_routes(src, dst)
                # routes don't include the src and dst hosts, so pass those to parse
                #parse_routes(routes, link_counts)
                route = routing_obj.get_route(src, dst, 0)
                parse_route(route, link_counts)

    counts = sort_counts(link_counts, hosts)
    #print link_counts
    write_counts(counts, "%s-%s-%s.txt" % (tp, routing, str(args.p)))

def start_receiver(net, receiver):
    r = net.getNodeByName(receiver)
    print "Starting server at %s" % receiver
    r.cmd("%s -s -p %s > %s/iperf_server%s.txt &" %
             (CUSTOM_IPERF_PATH, 5001, args.dir, receiver), shell=True)

def start_sender(net, sender, receiver, flow):
    seconds = 30

    s = net.getNodeByName(sender)
    r = net.getNodeByName(receiver)
    
    print "Starting connection between %s and %s" % (sender, receiver)
    s.sendCmd("%s -c %s -p %s -t %d -i 1 -yc -Z %s > %s/iperf_client%s-%s-%s.txt &" % (CUSTOM_IPERF_PATH, r.IP(), 5001, seconds, args.cong, args.dir, sender, receiver, str(flow)))
    s.waitOutput()

def stop_iperf(net, host):
    h = net.getNodeByName(host)

def count_connections():
    "Count current connections in iperf output file"
    out = args.dir + "/iperf_server*"
    lines = Popen("grep -R connected %s | wc -l" % out,
                  shell=True, stdout=PIPE).communicate()[0]
    return int(lines)

def throughput_experiment(net, topo, flows):

    monitor = Process(target=monitor_devs_ng, args=('%s/bwm.txt' % args.dir, 1.0))
    monitor.start()

    hosts = topo.hosts()
    for host in hosts:
        start_receiver(net, host)

    net.getNodeByName(hosts[0]).popen("ping " + net.getNodeByName(hosts[1]).IP() + " -c 5 -i 0.1 > ping.txt", shell=True)

    start = time()

    receivers = hosts[:]

    # find sender - receiver pairs
    match = False
    while not match:
        shuffle(receivers)

        match = True
        for i in range(len(hosts)):
            if hosts[i] == receivers[i]:
                match = False
                break

    for i in range(len(hosts)):
        for j in range(flows):
            start_sender(net, hosts[i], receivers[i], j)
    
    succeeded = 0
    wait_time = 120
    while wait_time > 0 and succeeded != len(hosts) * flows:
        wait_time -= 1
        succeeded = count_connections()
        print 'Connections %d/%d succeeded\r' % (succeeded, len(hosts) * flows),
        sys.stdout.flush()
        sleep(1)

    #for host in hosts:
    #    stop_iperf(net, host)

    sleep(40)

    '''
    for host in hosts:
        h = net.getNodeByName(host)
        h.waitOutput()
    '''

    os.system('killall -9 ' + CUSTOM_IPERF_PATH)

    end = time()
    print "\nExperiment took %.3f seconds to complete" % (end - start)

    monitor.terminate()
    os.system("killall -9 bwm-ng")
    
#    os.system('killall -9 iperf' )

    print "Finished throughput experiment"

def experiment(tp="jf", routing="ksp", exp="l"):
    # create the result directory
    if not os.path.exists(args.dir):
        os.makedirs(args.dir)

    if tp == "jf":
        topo = JellyfishTopo(nServers=args.nServers,nSwitches=args.nSwitches,nPorts=args.nPorts)
    elif tp == "ft":
        topo = FatTreeTopo(k=args.nPorts)
    
    print "Starting Mininet"
    net = Mininet(topo=topo, host=CPULimitedHost, link=TCLink, controller=RemoteController,autoSetMacs=True)
    net.start()
    
    #print "Dumping node connections"
    #dumpNodeConnections(net.hosts)
    
    if tp == "jf":
        pox_args = shlex.split("pox/pox.py riplpox.riplpox --topo=%s,%s,%s,%s --routing=%s --mode=reactive" % (tp, args.nServers, args.nSwitches, args.nPorts, routing))
    elif tp == "ft":
        pox_args = shlex.split("pox/pox.py riplpox.riplpox --topo=%s,%s --routing=%s --mode=reactive" % (tp, args.nPorts, routing))
      
    print "Starting RiplPox"
    with open(os.devnull, "w") as fnull:
        p = Popen(pox_args, stdout=fnull, stderr=fnull)
    sleep(15)

    print "Starting experiments for topo %s and routing %s" % (tp, routing)
    #net.pingAll()

    if exp == 'l':
        links_experiment(topo, tp, routing)
    elif exp == 't':
        throughput_experiment(net, topo, args.flows)

    print "Stopping Mininet"
    net.stop()
    print "Stopping RiplPox"
    p.terminate()
    sleep(10)
    
    # Ensure that all processes you create within Mininet are killed.       
    # Sometimes they require manual killing.                                
    Popen("pgrep -f webserver.py | xargs kill -9", shell=True).wait()

if __name__ == "__main__":
    experiment(tp=args.topo, routing=args.routing, exp=args.exp)
