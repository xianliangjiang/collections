#!/bin/bash

# Note: Mininet must be run as root.  So invoke this shell script
# using sudo.

time=80
bwnet=100
delay=0.5

# Red settings (for DCTCP)
dctcp_red_limit=1000000
dctcp_red_min=20000
dctcp_red_max=20001
dctcp_red_avpkt=1000
dctcp_red_burst=20
dctcp_red_prob=1
iperf_port=5001
iperf=~/iperf-patched/src/iperf
for qsize in 200; do
    dirf=dctcpgraphs-q$qsize
    for hosts in 3 21; do
	dir1=dctcpbb-h$hosts
	python dctcp.py --delay $delay -b $bwnet -B $bwnet -d $dir1 --maxq $qsize -t $time \
	--red_limit $dctcp_red_limit \
	--red_min $dctcp_red_min \
	--red_max $dctcp_red_max \
	--red_avpkt $dctcp_red_avpkt \
	--red_burst $dctcp_red_burst \
	--red_prob $dctcp_red_prob \
	--dctcp 1 \
	--red 0 \
	--iperf $iperf -k 0 -n $hosts
	dir2=tcpbb-h$hosts
	python dctcp.py --delay $delay -b 100 -d $dir2 --maxq $qsize -t $time \
	--dctcp 0 --red 0 --iperf $iperf -k 0 -n $hosts
    done
    python plot_cdf.py -f dctcpbb-h3/q.txt dctcpbb-h21/q.txt tcpbb-h3/q.txt \
    tcpbb-h21/q.txt -l dctcp2flows dctcp20flows tcp2flows tcp20flows -o $dirf/cdf_flows.png
    rm -rf dctcpbb-h3 dctcpbb-h21 tcpbb-h21 tcpbb-h3
done
