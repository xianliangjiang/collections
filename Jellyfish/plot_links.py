'''
Plot
'''

import matplotlib as m
m.use("Agg")
import matplotlib.pyplot as plt

from argparse import ArgumentParser

import glob
import string

parser = ArgumentParser(description="Jellyfish Plots")
parser.add_argument('-dir',
                    help="Directory containing inputs for the plot.",
                    dest="dir",
                    default=None)

parser.add_argument('-p',
                    type=float,
                    help="Probability",
                    default=1.0)

parser.add_argument('-o',
                    help="Output png file for the plot.",
                    dest="out",
                    default=None) # Will show the plot

args = parser.parse_args()

def plotData(filename, label, color):
    f = open(filename)
    lines = f.readlines()

    keys = []
    values = []
    cdf = 0
    for line in lines:
        datum = string.split(line.strip(), " ")
        values.append(int(datum[0]))
        cdf += int(datum[1])
        keys.append(cdf)

    plt.plot(keys, values, lw=1, label=label, color=color, drawstyle="steps-pre")

'''
plt.plot(keys, values, lw=2, label="8 shortest paths", color="blue", drawstyle="steps-pre")
plt.plot(keys, values, lw=1, label="64 way ecmp", color="green", drawstyle="steps-pre")
plt.plot(keys, values, lw=1, label="8 way ecmp", color="red", drawstyle="steps-pre")
'''

for f in glob.glob("%s/*.txt" % args.dir):
    if str(args.p) not in f:
        continue
    
    label = f[len(args.dir) + 1:-len('.txt')]
    color = 'red' if 'ecmp' in label else 'blue'
    plotData(f, label, color)
    
#plt.xlim((start,end))
#plt.ylim((start,end))
plt.xlabel("Rank of Link")
plt.ylabel("# Paths Link is on")
plt.legend(loc=2)

if args.out:
    print "Saving output to %s" % args.out
    plt.savefig(args.out)
else:
    plt.show()
