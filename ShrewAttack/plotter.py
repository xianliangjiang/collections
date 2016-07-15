import csv
from util.helper import *
import util.plot_defaults

from matplotlib.ticker import MaxNLocator
from pylab import figure

parser = argparse.ArgumentParser()

parser.add_argument('--file', '-f',
                    help="data file directory",
                    required=True,
                    action="store",
                    dest="file")

parser.add_argument('-o',
                    help="Output directory",
                    required=True,
                    action="store",
                    dest="dir")

args = parser.parse_args()
to_plot = []


cong = ['reno', 'cubic', 'vegas']
bursts = ['0.03', '0.05', '0.07', '0.09']
graphfiles = []
for burst in bursts:
	for tcptype in cong:
		data = read_list(args.file + '/' + tcptype + '-' + burst +'-raw_data.txt')
		xs = col(0, data)
		ys = col(1, data)
		plt.plot(xs, ys, label=tcptype)
	plt.title('Shrew-attack TCP throughput. Burst = ' + burst)
	plt.legend(loc='upper left')
	plt.xlabel('seconds')
	plt.ylabel("% thoroughput")
	plt.grid(True)
	plt.savefig("{0}/{1}-result.png".format(args.dir, burst))
        plt.close()
