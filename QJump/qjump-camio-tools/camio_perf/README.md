CAMIO_PERF
==========
Generates packets at a high rate using standard TCP or UPD sockets. Packets can be spaced using the --pace option, which uses a high accuracy timer to delay packets by a certain number of micro (or nano) seconds. 



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
camio_perf:

|Mode     |Short|Long Option    | Description                                                                  |
|---------|-----|---------------|------------------------------------------------------------------------------|
|Optional | -s  |--size         |   - Size each packet should be                                               | 
|Required | -i  |--ip-port      |   - IP address and port in camio style eg udp:127.0.0.1:2000                 |
|Optional | -q  |--seq-start    |   - Starting number for sequences                                            |
|Optional | -p  |--pacing       |   - Time in us between pacing intervals                                      |
|Flag     | -h  |--help         |   - Print this help message                                                  |


