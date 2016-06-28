#!/bin/bash

# Note: Mininet must be run as root.  So invoke this shell script
# using sudo.

time=10
bwnet=100
delay=1

# Red settings (for DCTCP)
dctcp_red_limit=1000000
dctcp_red_avpkt=1000
dctcp_red_burst=100
dctcp_red_prob=1
iperf_port=5001
iperf=~/iperf-patched/src/iperf
#qsize=200
for qsize in 200; do
    dirf=dctcpgraphs-q$qsize
    for hosts in 3 5 7 9 10; do
	dir1=dctcpbb-h$hosts
	rm -rf $dir1
	mkdir $dir1
	for k in 5 10 15 20 40; do
	    dctcp_red_min=`expr $k \\* $dctcp_red_avpkt`
	    dctcp_red_max=`expr $dctcp_red_min + 1`
	    ./bin/python dctcp.py --delay $delay -b $bwnet -B $bwnet -k $k -d $dir1 --maxq $qsize -t $time \
	    --red_limit $dctcp_red_limit \
	    --red_min $dctcp_red_min \
	    --red_max $dctcp_red_max \
	    --red_avpkt $dctcp_red_avpkt \
	    --red_burst $dctcp_red_burst \
	    --red_prob $dctcp_red_prob \
	    --dctcp 1 \
	    --red 0\
	    --iperf $iperf -n $hosts
	done
    done
done
./bin/python plot_k_sweep.py -f dctcpbb-h3/k.txt dctcpbb-h5/k.txt dctcpbb-h7/k.txt \
dctcpbb-h9/k.txt dctcpbb-h10/k.txt -l 3hosts 5hosts 7hosts 9hosts 10hosts -o $dirf/n_and_k_sweep.png
rm -rf dctcpbb-h*
