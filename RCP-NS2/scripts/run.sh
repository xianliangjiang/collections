#!/bin/bash

# Exit on any failure.
set -e

# Check for uninitialized variables.
set -o nounset

start=`date`
exptid=`date +%b%d-%H:%M`

capacity=2.4      # Link speed in Gb/s
rtt=0.1           # RTT in seconds
load=0.9          # Load
shape=1.2         # Pareto shape parameter
mean=25           # Pareto mean parameter

RCPin=data/rcp-flow.tr
RCPout=data/rcp-flow-vs-delay.out
RCPlog=data/rcp.log

TCPin=data/tcp-flow.tr
TCPout=data/tcp-flow-vs-delay.out
TCPlog=data/tcp.log

plot=plot-$rtt-$shape.png

echo "Started at" $start

# Run experiment and scripts.
echo "Running simulations..."
perl -w run.pl $RCPin $RCPlog $TCPin $TCPlog $capacity $rtt $load $shape $mean
perl -w average.pl $RCPin $RCPout
perl -w average.pl $TCPin $TCPout

echo "Plotting data"
python plot.py $RCPout $TCPout $plot $capacity $rtt $load $shape $mean

echo "Ended at" `date`
