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
parser = ArgumentParser(description="Takes nicely formatted data and calculate improvment")
parser.add_argument('-f', '--file',
                    help="Input file for data, e.g.: -f output_formatted.txt",
                    dest="infile",
                    required=True,
                    default=None)
parser.add_argument('-o', '--out',
                    help="Save plot to output file, e.g.: --out improvment.txt",
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
# args:
#       bw - bandwidth (Kbps)
#       latency_small - response latency for smaller initcwnd
#       latency_large - response latency for larger initcwnd

def addToBucket(bw, latency_small, latency_large):
        # improvement_abs = latency_small - latency_large
        # improvement_perc = (improvement_abs / latency_small) * 100.0
        if bw <= 56:
                buckets["56"].append((latency_small, latency_large))
        elif bw <= 256:
                buckets["256"].append((latency_small, latency_large))
        elif bw <= 512:
                buckets["512"].append((latency_small, latency_large))
        elif bw <= 1000:
                buckets["1000"].append((latency_small, latency_large))
        elif bw <= 2000:
                buckets["2000"].append((latency_small, latency_large))
        elif bw <= 3000:
                buckets["3000"].append((latency_small, latency_large))
        elif bw <= 5000:
                buckets["5000"].append((latency_small, latency_large))
        else:
                buckets["5000+"].append((latency_small, latency_large))

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

    improvement_small = []
    improvement_large = []

    for a in improvement:
        improvement_small.append(a[0])
        improvement_large.append(a[1])

    outfile = open(args.out, 'a')
    for ind in xrange(len(improvement)):
        outfile.write("%s\t%s\n" % (improvement_small[ind], improvement_large[ind]))
    outfile.write("--\n")
    outfile.close()

except EnvironmentError:
    print 'Error reading file'
