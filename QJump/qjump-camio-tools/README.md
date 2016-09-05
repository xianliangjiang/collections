qjump-camio-tools
===================

QJump tools based on the CamIO 1.0 library found here: https://github.com/mgrosvenor/camio1.0

Tools
=====

dag_capture
-----------
A fast in memory capture program for use with Endace DAG cards. 


dag_anaylse
-----------
Reads the ERF encoded output of the dag_capture progam and coverts to ASCII

dag_join
--------
Takes two ERF encoded files and looks for matching packets to join. Calculates the latency. 


packet_gen
----------
A full featured packet generator based on Netmap for use with Intel X520 NICs or similar

camio_perf
----------
A simple packet generator of the style of iperf. Uses sockets to generate high rate packets and has a precise pacing option. 


Requirements
============

Cake
-----
To build it, you must have the "cake" build system installed. 
You can obtain cake from https://github.com/Zomojo/Cake
To insall cake, follow the instructions in "INSTALL".

CamIO 1.0
---------
You can obtain the camio 1.0 library from 
https://github.com/mgrosvenor/camio1.0
To build camio, run "build.sh" in the root directory.




