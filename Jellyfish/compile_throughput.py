'''
Compile the throughput data
'''

from argparse import ArgumentParser

import glob
import string

import numpy

BITS_TO_MBITS = 1.0/1000000

parser = ArgumentParser(description="Jellyfish Plots")
parser.add_argument('-dir',
                    help="Directory containing inputs for the throughput.",
                    dest="dir",
                    default=None)

parser.add_argument('-o',
                    help="Output txt file for the plot.",
                    dest="out",
                    default=None)

parser.add_argument('-f',
                    type=int,
                    help="Number of flows",
                    dest='flows',
                    default=1)

args = parser.parse_args()

def calculateThroughput(filename):
    f = open(filename)
    lines = f.readlines()

    if len(lines) == 0:
        return -1
    # get last line summary
    summary = lines[-1]
    tp = string.split(summary, ',')[-1]
    tp = float(string.replace(tp, "\n", ""))
    return tp

def compileData(topo, routing):
    throughputs = []
    for f in glob.glob("%s_%s_%s_*/iperf_client*.txt" % (args.dir, topo, routing)):
        tp = calculateThroughput(f)
        if tp != -1:
            throughputs.append(calculateThroughput(f))

    return numpy.array(throughputs).mean() * args.flows

jf_ksp = compileData('jf', 'ksp') * BITS_TO_MBITS
jf_ecmp = compileData('jf', 'ecmp') * BITS_TO_MBITS
#ft_ksp = compileData('ft', 'ksp') * BITS_TO_MBITS
ft_ecmp = compileData('ft', 'ecmp') * BITS_TO_MBITS

if args.out:
    print "Saving output to %s" % args.out
    # save to output file
    f = open(args.out, 'w')
    f.write("Jellyfish K-Shortest Paths: %sMb/s\n" % jf_ksp)
    f.write("Jellyfish ECMP: %sMb/s\n" % jf_ecmp)
    f.write("FatTree ECMP: %sMb/s\n" % ft_ecmp)
    f.close()
else:
    print "Jellyfish K-Shortest Paths: %sMb/s\n" % jf_ksp
    print "Jellyfish ECMP: %sMb/s\n" % jf_ecmp
#    print "FatTree K_Shortest Paths: %sMb/s\n" % ft_ksp
    print "FatTree ECMP: %sMb/s\n" % ft_ecmp
    
