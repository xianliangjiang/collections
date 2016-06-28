'''
Plot queue occupancy over time
'''
from helper import *
import plot_defaults

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
        return {'color': 'blue'}
    elif i == 1:
	return {'color': 'red', 'ls': '-.'}
    elif i == 2:
        return {'color': 'green'}
    elif i == 3:
        return {'color': 'black'}
    else:
        return {'color': 'orange'}

m.rc('figure', figsize=(16, 6))
fig = figure()
ax = fig.add_subplot(111)
for i, f in enumerate(args.files):
    data = read_list(f)
    xaxis = map(float, col(0, data))
    xaxis = map(lambda x: x , xaxis)
    qlens = map(float, col(1, data))

    xaxis = xaxis[::args.every]
    qlens = qlens[::args.every]
    ax.plot(xaxis, qlens, marker='o', lw=2, **get_style(i))
    ax.xaxis.set_major_locator(MaxNLocator(4))

plt.legend(args.legend, 'lower right')
plt.ylim([0,101])
plt.ylabel("Throughput (Mbps)")
plt.grid(True)
plt.xlabel("Marking threshold (K)")
plt.title("Sweep of K")

if args.out:
    print 'saving to', args.out
    plt.savefig(args.out)
else:
    plt.show()
