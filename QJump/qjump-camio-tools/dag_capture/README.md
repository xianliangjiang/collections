DAG_CAPTURE
============
Captures network trace in to memory using Endace DAG Capture Card. 

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

dag_capture_ng:

|Mode     |Short|Long Option    | Description                                                                  |
|---------|-----|---------------|------------------------------------------------------------------------------|
|Optional | -i  |--dag-in       |   - The DAG input card to listen on [dag:/dev/dag0] |
|Optional | -s  |--samples      |   - Number of samples to capture [1000000] |
|Optional | -t  |--timeout      |   - Time in seconds to listen for samples, 0 = unlimited [10] |
|Flag     | -h  |--help         |   - Print this help message |


