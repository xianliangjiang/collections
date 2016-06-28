import math
import csv
import numpy
import matplotlib as m
import os
if os.uname()[0] == "Darwin":
    m.use("MacOSX")
else:
    m.use("Agg")
import matplotlib.pyplot as plt
from argparse import ArgumentParser
from matplotlib.ticker import FormatStrFormatter

# Parse arguments
parser = ArgumentParser(description="Takes nicely formatted data and makes a bar chart")
parser.add_argument('-f', '--file',
                    help="Input file for data, e.g.: -f improvment.txt",
                    dest="infile",
                    required=True,
                    default=None)
parser.add_argument('-o', '--out',
                    help="Save plot to output file, e.g.: --out plot.png",
                    dest="out",
                    required=True,
                    default=None)
args = parser.parse_args()

try:
    base = [0]*8
    exp = [0]*8

    with open(args.infile) as csvfile:
        resp_ind = 0
        bw_ind = 0
        for line in csvfile:
            if line[0] != '-':
                fields = line.split('\t')
                base[bw_ind] = base[bw_ind] + float(fields[0])/10.
                exp[bw_ind] = exp[bw_ind] + float(fields[1])/10.
                bw_ind = bw_ind + 1
            else:
                bw_ind = 0

    abs_improvement = []
    perc_improvement = []

    for i in range(0,8):
	abs_improvement.append(base[i]-exp[i])
	perc_improvement.append(abs_improvement[i]*100.0/base[i])	

    N = 8
    ind = numpy.arange(N)  # the x locations for the groups
    width = 0.35       # the width of the bars

    fig, ax = plt.subplots()
    ax2 = ax.twinx()
    rects1 = ax.bar(ind, abs_improvement, width, color='#FF6262', edgecolor='r', log=1)
    rects2 = ax2.bar(ind+width, perc_improvement, width, color='b', edgecolor='b')

    # add labels
    ax.set_ylabel('Improvement (ms)')
    ax2.set_ylabel('Improvement (%)')
    ax.set_xlabel('Bandwidth (Kbps)')
    ax.set_xticks(ind+width)
    ax.set_xticklabels( ('56', '256', '512', '1000', '2000', '3000', '5000', '5000+') )
    ax.set_yscale('log')
    ax.yaxis.set_major_formatter(FormatStrFormatter('%1.0f'))
    ax.set_yticks([1,10,100,1000,10000])
    ax.grid(True)
    ax.set_axisbelow(True)
    ax2.set_yticks(numpy.linspace(0, 50, 6))
    ax.legend( (rects1[0], rects2[0]), ('Absolute Improvement', 'Percentage Improvement') )
    plt.savefig(args.out)
except EnvironmentError:
    print 'Error reading file'
