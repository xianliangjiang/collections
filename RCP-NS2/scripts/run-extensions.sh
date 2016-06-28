#!/bin/bash

# Exit on any failure.
set -e

# Check for uninitialized variables.
set -o nounset

start=`date`
exptid=`date +%b%d-%H:%M`

capacity=2.4                   # Link speed in Gb/s
rtt=0.1                        # RTT in seconds
rttl=(0.05 0.1 0.2)
load=0.9                       # Load
shape=1.2                      # Pareto shape parameter
shapel=(1.1 1.2 1.4 1.8 2.2)
mean=25                        # Pareto mean parameter

RCPin=data/rcp-flow.tr
RCPout=data/rcp-flow-vs-delay.out
RCPlog=data/rcp.log

TCPin=data/tcp-flow.tr
TCPout=data/tcp-flow-vs-delay.out
TCPlog=data/tcp.log

echo "Started at" $start

# Run experiment and scripts.
echo "Running simulations for different RTTs"
for rtt in ${rttl[@]}
do
  echo "RTT $rtt"
  plot=plot-$rtt-$shape.png
  perl -w run.pl $RCPin $RCPlog $TCPin $TCPlog $capacity $rtt $load $shape $mean
  perl -w average.pl $RCPin $RCPout
  perl -w average.pl $TCPin $TCPout

  echo "Plotting data"
  python plot.py $RCPout $TCPout $plot $capacity $rtt $load $shape $mean
done

echo "Running simulations for different Pareto shapes"
rtt=0.1
for shape in ${shapel[@]}
do
  echo "Shape $shape"
  plot=plot-$rtt-$shape.png
  perl -w run.pl $RCPin $RCPlog $TCPin $TCPlog $capacity $rtt $load $shape $mean
  perl -w average.pl $RCPin $RCPout
  perl -w average.pl $TCPin $TCPout

  echo "Plotting data"
  python plot.py $RCPout $TCPout $plot $capacity $rtt $load $shape $mean
done

echo "Ended at" `date`
