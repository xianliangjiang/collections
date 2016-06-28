from random import randint
import sys

if(len(sys.argv) != 5):
        print("Usage: ./generateTrace.py OUTPUT_FILE N_INTERVALS MIN_THROUGHPUT MAX_THROUGHPUT")
OUTPUT_FILE = sys.argv[1]
N_INTERVALS = int(sys.argv[2])
MIN_T = int(sys.argv[3])
MAX_T = int(sys.argv[4])
with open(OUTPUT_FILE, "wb") as f:
	curTimeInMS = 0
	for nIntervals in xrange(0, N_INTERVALS):
		throughput = randint(MIN_T, MAX_T)
		mbps = throughput * 1000000
		mbpms = mbps / 1000
		packetsp10ms = float(mbpms) / (1500 * 8) * 10
		packetsp10ms = int(packetsp10ms)
		#print "Adding data for throughput " + str(throughput)
		#print "Packets per 10 ms " + str(packetsp10ms)
		for i in xrange(0, 5000, 10):
			for j in xrange(0, packetsp10ms):
				f.write(str(curTimeInMS))
				f.write("\n")
			curTimeInMS += 10
	f.close()
