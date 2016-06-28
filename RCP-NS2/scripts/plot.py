import math
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import scipy
import sys

# Parse command-line arguments.
RCP_DATA_FILE = sys.argv[1]
TCP_DATA_FILE = sys.argv[2]

PLOT_FILE = sys.argv[3]

# Bottleneck link speed in b/s.
C = float(sys.argv[4]) * 1000000000
# RTT in seconds.
RTT = float(sys.argv[5])
RHO = float(sys.argv[6])
PKT_SIZE = 8 * 1000
SHAPE = float(sys.argv[7])
MEAN = float(sys.argv[8])

# Flow sizes in terms of packets. Used for generating slow-start and PS lines.
FLOW_SIZES = range(0, 100000, 10)

# ---------------------------------------------------------------------------- #

def gen_slowstart():
    """Generates the TCP slow-start line."""
    ss_vals = []
    for L in FLOW_SIZES:
        ss_vals.append(((math.log(L + 1, 2) + 0.5) * RTT) + L / C)
    return ss_vals

def gen_ps():
    """Generates the processor sharing line."""
    ps_vals = []
    for L in FLOW_SIZES:
        ps_vals.append(1.5 * RTT + (L * PKT_SIZE / (C * (1 - RHO))))
    return ps_vals

def parse_data_file(data_file):
    """Parses a data file. Should contain lines of the form "size duration"."""
    flow_sizes = []
    flow_durations = []
    with open(data_file, 'r') as logfile:
        for line in logfile:
            values = line.split()
            flow_size, flow_duration = values[0], values[1]
            flow_sizes.append(flow_size)
            flow_durations.append(flow_duration)

    return (flow_sizes, flow_durations)

def parse_rcp():
    return parse_data_file(RCP_DATA_FILE)

def parse_tcp():
    return parse_data_file(TCP_DATA_FILE)

def plot_semilog(ps_data, ss_data, rcp_x, rcp_y, tcp_x, tcp_y):
    """Generates the semilog plot."""
    plt.xlabel('Flow Size [pkts] (normal scale)')
    plt.ylabel('Average Flow Completion Time [sec]')
    plt.yscale('log')
    plt.axis([0, 2000, 0.1, 100])

    plt.plot(tcp_x, tcp_y, 'g.-', label = 'TCP (avg.)')
    plt.plot(rcp_x, rcp_y, 'b+-', label='RCP (avg.)')
    plt.plot(FLOW_SIZES, ss_data, 'r-', label='Slow-Start')
    plt.plot(FLOW_SIZES, ps_data, 'r--', label='PS')

    plt.legend(loc='upper right')
    plt.savefig("semilog-" + PLOT_FILE)
    plt.close()

def plot_loglog(ps_data, ss_data, rcp_x, rcp_y, tcp_x, tcp_y):
    """Generates the loglog plot."""
    plt.xlabel('Flow Size [pkts] (log)')
    plt.xscale('log')
    plt.ylabel('Average Flow Completion Time [sec]')
    plt.yscale('log')
    plt.axis([1000, 100000, 0.1, 100])

    plt.plot(tcp_x, tcp_y, 'g.-', label = 'TCP (avg.)')
    plt.plot(rcp_x, rcp_y, 'b+-', label='RCP (avg.)')
    plt.plot(FLOW_SIZES, ss_data, 'r-', label='Slow-Start')
    plt.plot(FLOW_SIZES, ps_data, 'r--', label='PS')

    plt.legend(loc='upper right')
    plt.savefig("loglog-" + PLOT_FILE)
    plt.close()


# Parse data.
ps_line = gen_ps()
ss_line = gen_slowstart()
(rcp_x, rcp_y) = parse_data_file(RCP_DATA_FILE)
(tcp_x, tcp_y) = parse_data_file(TCP_DATA_FILE)

# Generate plots.
plot_semilog(ps_line, ss_line, rcp_x, rcp_y, tcp_x, tcp_y)
plot_loglog(ps_line, ss_line, rcp_x, rcp_y, tcp_x, tcp_y)
