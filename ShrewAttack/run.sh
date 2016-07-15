#!/bin/bash

# Exit on any failure
set -e

# Check for uninitialized variables
set -o nounset

ctrlc() {
	killall -9 python
	mn -c
	exit
}

trap ctrlc SIGINT

start=`date`
exptid=`date +%b%d-%H:%M`

rootdir=shrewattack-$exptid
plotpath=util
iperf=~/iperf-patched/src/iperf

for run in 1; do
for period in 0.5 0.6 0.7 0.8 0.9 0.95 1.0 1.05 1.1 1.15 1.2 1.5 2.0 2.5 3.0 3.5 4.0 4.5 5.0; do
	dir=$rootdir

	python shrewattack.py --bw-host 15 \
		--cong reno \
		--bw-net 1.5 \
		--delay 2 \
		--dir $dir \
		--iperf $iperf \
		--period $period \
		--length 0.15 \
		--maxq 24

    python convert_data.py -f $dir/reno-0.15-$period-bwm.txt -p $period -o $dir/reno-0.15-raw_data.txt
done
done

python base_plotter.py -f $dir -o $dir

for run in 1; do
for cong in reno cubic vegas; do
for length in 0.03 0.05 0.07 0.09; do
for period in 0.5 0.6 0.7 0.8 0.9 0.95 1.0 1.05 1.1 1.15 1.2 1.5 2.0 2.5 3.0 3.5 4.0 4.5 5.0; do
	dir=$rootdir

	python shrewattack.py --bw-host 15 \
		--cong $cong \
		--bw-net 1.5 \
		--delay 2 \
		--dir $dir \
		--iperf $iperf \
		--period $period \
		--length $length \
		--maxq 15

    python convert_data.py -f $dir/$cong-$length-$period-bwm.txt -p $period -o $dir/$cong-$length-raw_data.txt
done
done
done
done

python plotter.py -f $dir -o $dir
echo "Started at" $start
echo "Ended at" `date`
