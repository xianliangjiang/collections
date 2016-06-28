import re
from argparse import ArgumentParser

# Parse arguments
parser = ArgumentParser(description="Transforming output to more concise version")
parser.add_argument('-f', '--file',
                    help="Input file for data, e.g.: -f output_raw.txt",
                    dest="infile",
                    required=True,
                    default=None)
parser.add_argument('-o', '--out',
                    help="Output file for formatted data, e.g.: -o output_formatted.txt",
                    dest="out",
                    required=True,
                    default=None)
args = parser.parse_args()

try:
    f = open(args.out, 'w')
    with open(args.infile) as outputFile:
	# simple flag to identify 1st vs 2nd entry for a host
	isSecond = 0
        for line in outputFile:
            bandwidth = re.search("Bandwidth:\s(\d+.\d+)", line)
            if bandwidth:
                host_bw = bandwidth.group(1)
                f.write(host_bw + "\t")
                continue
            elapsed_time = re.search("real\t\dm(\d+.\d+)s", line)
            if elapsed_time:
		time_s = elapsed_time.group(1)
		time_ms = str(float(time_s) * 1000)
                f.write(time_ms)
                if isSecond:
                    f.write("\n")
                    isSecond = 0
                else:
                    f.write("\t")
                    isSecond = 1
    f.close()
except EnvironmentError:
    print 'Error reading file'
