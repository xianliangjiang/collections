#!/usr/bin/perl -w

# Parse command-line arguments.
$RCPin = $ARGV[0];
$RCPlog = $ARGV[1];
$TCPin = $ARGV[2];
$TCPlog = $ARGV[3];

# General parameters.
$capacity = $ARGV[4];
$rtt = $ARGV[5];
$load = $ARGV[6];
$num_bottleneck_links = 1;
$pareto_shape = $ARGV[7];
$mean_flow_size = $ARGV[8];

# ------------------------------------ RCP ----------------------------------- #

$init_num_flows = 5000;
$sim_end = 300;
$alpha = 0.1;
$beta = 1;

`nice ns rcp.tcl $sim_end $capacity $rtt $load $num_bottleneck_links $alpha $beta $init_num_flows $mean_flow_size $pareto_shape $RCPin > $RCPlog`;

# ------------------------------------ TCP ----------------------------------- #

$init_num_flows = 10000;
$num_flows = 100000;

`nice -n +20 ns tcp.tcl $num_flows $capacity $rtt $load $num_bottleneck_links $init_num_flows $mean_flow_size $pareto_shape $TCPin > $TCPlog`;
