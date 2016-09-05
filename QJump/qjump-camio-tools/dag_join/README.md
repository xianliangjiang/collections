DAG_JOIN
========
Looks for all packets in dag0 that also that appear in dag1.
Reports packets that are in dag0 that are not in dag1.
Calculates the latency between cap-in1 and cap-in0.
Outputs as a binary by appending into the data section of the ERF, in ssv and csv formats. 

Use -f and -l to control the offset and length to use from cap-in0. This makes the application trivial to parallelize for long traces.


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


Building
========
To build a debug version run:
./build.sh

To build a release version run:
./build.sh --variant=release

Running
=======

dag_join:

|Mode     |Short|Long Option    | Description                                                                  |
|---------|-----|---------------|------------------------------------------------------------------------------|
|Required | -i  | --dag0        |    - The first input filename. eg /tmp/dag1.blob                             |
|Required | -I  | --dag1        |    - The second input filename. eg /tmp/dag0.blob                            |
|Optional | -o  | --output      |    - The dag1 output file in log format. eg log:/tmp/dag1_out.log            |
|Optional | -f  | --offset      |    - The offset (in records) to jump to for comparison.                      |
|Optional | -l  | --length      |    - The length in records to process                                        |
|Optional | -w  | --window      |    - Number of microseconds to keep looking for a match (2500us)             |
|Flag     | -v  | --verbose     |    - Output feedback                                                         | 
|Flag     | -V  | --vverbose    |    - Output more feedback                                                    |
|Flag     | -h  | --help        |    - Print this help message                                                 |


