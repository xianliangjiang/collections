DAG_ANALYSE
===========
Converts dag capture files into ASCII. Reports basic statistics about each file.

Requirements
============

OpenSSL
-------
This software requires the openssl headers. On Ubuntu these can be installed with 
sudo apt-get install openssl-dev

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

Dag analyse has the following options  

dag_analyse:

|Mode     |Short|Long Option    | Description                                                                  |
|---------|-----|---------------|------------------------------------------------------------------------------|
|Required | -i  | --dag0-in     |   - Dag ERF input file name. eg /tmp/dag_cap_A  |
|Flag     | -s  | --std-out     |    - Write the output to std-out  |
|Optional | -o  | --dag0-out    |    - Dag ERF output file name. eg /tmp/dag_cap_A.txt [erf.out]  |
|Optional | -f  | --dag0-off    |    - The offset (in records) to jump to in dag0.  |
|Optional | -l  | --dag0-len    |    - The length (in records) to display from dag0. (-1 == all)  |
|Optional | -p  | --packet-type |    - Filter by packets with the given type. 1=tcp, 2=udp, 3=icmp, -1 == all  |
|Flag     | -H  | --write-header|    - Write a header describing each file's columns  |
|Flag     | -c  | --use-csv     |    - Write the output in CSV format (default = False) | 
|Flag     | -b  | --use-bin     |    - Write the output in bin format (default = False)  |
|Flag     | -h  | --help        |    - Print this help message  |
  


