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
                    help="Input file for data, e.g.: -f output_formatted.txt",
                    dest="infile",
                    required=True,
                    default=None)
parser.add_argument('-o', '--out',
                    help="Save plot to output file, e.g.: --out plot.png",
                    dest="out",
                    required=True,
                    default=None)
args = parser.parse_args()

# bandwidth buckets
buckets = {}
buckets["56"] = []
buckets["256"] = []
buckets["512"] = []
buckets["1000"] = []
buckets["2000"] = []
buckets["3000"] = []
buckets["5000"] = []
buckets["5000+"] = []

def align_yaxis(ax1, v1, ax2, v2):
    """adjust ax2 ylimit so that v2 in ax2 is aligned to v1 in ax1"""
    _, y1 = ax1.transData.transform((0, v1))
    _, y2 = ax2.transData.transform((0, v2))
    inv = ax2.transData.inverted()
    _, dy = inv.transform((0, 0)) - inv.transform((0, y1-y2))
    miny, maxy = ax2.get_ylim()
    ax2.set_ylim(miny+dy, maxy+dy)

# args:
#       bw - bandwidth (Kbps)
#       latency_small - response latency for smaller initcwnd
#       latency_large - response latency for larger initcwnd

def addToBucket(bw, latency_small, latency_large):
        improvement_abs = latency_small - latency_large
        improvement_perc = (improvement_abs / latency_small) * 100.0

        if bw <= 56:
                buckets["56"].append((improvement_abs, improvement_perc))
        elif bw <= 256:
                buckets["256"].append((improvement_abs, improvement_perc))
        elif bw <= 512:
                buckets["512"].append((improvement_abs, improvement_perc))
        elif bw <= 1000:
                buckets["1000"].append((improvement_abs, improvement_perc))
        elif bw <= 2000:
                buckets["2000"].append((improvement_abs, improvement_perc))
        elif bw <= 3000:
                buckets["3000"].append((improvement_abs, improvement_perc))
        elif bw <= 5000:
                buckets["5000"].append((improvement_abs, improvement_perc))
        else:
                buckets["5000+"].append((improvement_abs, improvement_perc))
try:
        with open(args.infile) as csvfile:
                reader = csv.reader(csvfile, delimiter='\t')
                for row in reader:
                        addToBucket(float(row[0]), float(row[1]), float(row[2]))
        improvement = []
        if buckets["56"]:
		improvement.append(numpy.mean(buckets["56"], 0))
	else:
		improvement.append((0,0))
        if buckets["256"]:
		improvement.append(numpy.mean(buckets["256"], 0))
	else:
		improvement.append((0,0))
        if buckets["512"]:
		improvement.append(numpy.mean(buckets["512"], 0))
	else:
		improvement.append((0,0))
        if buckets["1000"]:
		improvement.append(numpy.mean(buckets["1000"], 0))
	else:
		improvement.append((0,0))
        if buckets["2000"]:
		improvement.append(numpy.mean(buckets["2000"], 0))
	else:
		improvement.append((0,0))
        if buckets["3000"]:
		improvement.append(numpy.mean(buckets["3000"], 0))
	else:
		improvement.append((0,0))
        if buckets["5000"]:
		improvement.append(numpy.mean(buckets["5000"], 0))
	else:
		improvement.append((0,0))
        if buckets["5000+"]:
		improvement.append(numpy.mean(buckets["5000+"], 0))
	else:
		improvement.append((0,0))

	avg_improvement = []
        perc_improvement = []

	for a in improvement:
		avg_improvement.append(a[0])
		perc_improvement.append(a[1])

        N = 8
        ind = numpy.arange(N)  # the x locations for the groups
        width = 0.35       # the width of the bars

        fig, ax = plt.subplots()
	ax2 = ax.twinx()
        rects1 = ax.bar(ind, avg_improvement, width, color='#FF6262', edgecolor='r', log=1)
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
