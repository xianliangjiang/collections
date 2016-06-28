'''
Plot queue occupancy over time
'''
from helper import *
import plot_defaults
from numpy import *
from matplotlib.pyplot import *
from matplotlib.ticker import MaxNLocator
from pylab import figure

parser = argparse.ArgumentParser()
parser.add_argument('--files', '-f',
                    help="Queue timeseries output to one plot",
                    required=True,
                    action="store",
                    nargs='+',
                    dest="files")

parser.add_argument('--legend', '-l',
                    help="Legend to use if there are multiple plots.  File names used as default.",
                    action="store",
                    nargs="+",
                    default=None,
                    dest="legend")

parser.add_argument('--out', '-o',
                    help="Output png file for the plot.",
                    default=None, # Will show the plot
                    dest="out")

parser.add_argument('--labels',
                    help="Labels for x-axis if summarising; defaults to file names",
                    required=False,
                    default=[],
                    nargs="+",
                    dest="labels")

parser.add_argument('--every',
                    help="If the plot has a lot of data points, plot one of every EVERY (x,y) point (default 1).",
                    default=1,
                    type=int)

args = parser.parse_args()

if args.legend is None:
    args.legend = []
    for file in args.files:
        args.legend.append(file)

to_plot=[]
def get_style(i):
    if i == 0:
        return {'color': 'red'}
    else:
        return {'color': 'blue', 'ls': '-.'}

m.rc('figure', figsize=(16, 6))
fig = figure()
ax = fig.add_subplot(111)
for i, f in enumerate(args.files):
    data = read_list(f)
    xaxis = map(float, col(0, data))
    start_time = xaxis[0]
    xaxis = map(lambda x: x - start_time, xaxis)
    qlens = map(float, col(1, data))

    xaxis = xaxis[::args.every]
    qlens = qlens[::args.every]
    qlens = sorted(qlens)
    total_sum = sum(qlens)
    cdf = [0] * len(qlens)
    cdf[0] = qlens[0]
    i = 1
    while i < len(qlens):
	cdf[i] = cdf[i-1] + qlens[i]
	cdf[i-1] = cdf[i-1]/total_sum
	i += 1
    cdf[i-1] = cdf[i-1] / total_sum
    #coeff = polyfit(qlens, cdf, 2)
    #polynomial = poly1d(coeff)
    #res = polynomial(qlens)
    #ax.plot(qlens, res, label=args.legend[0], lw=2)
    ax.plot(qlens, cdf, lw=2)
    #ax.xaxis.set_major_locator(MaxNLocator(4))

plt.legend(args.legend, 'upper center')
plt.ylabel("CDF")
plt.ylim((0,1))
plt.grid(True)
plt.xlabel("Queue occupancy (packets)")
if args.out:
    print 'saving to', args.out
    plt.savefig(args.out)
else:
    plt.show()
