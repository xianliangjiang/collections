PACKET_GEN
==========

Generates packets using netmap at up to linerate for all packet sizes.

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


packet_gen:

|Mode     |Short|Long Option      | Description                                                                  |
|---------|-----|-----------------|------------------------------------------------------------------------------|
|Required | -i  |--interface      |    Interface name to generate packets on eg eth0|
|Required | -s  |--src            |    Source trailing IP number eg 106 for 10.10.0.106 |
|Required | -m  |--mac            |    Destination MAC address number as a hex string eg 0x90E2BA27FBE0|
|Required | -d  |--dst            |    Destination trailing IP number eg 112 for 10.10.0.102|
|Optional | -n  |--num-pkts       |    Number of packets to send before stopping. -1=inf [-1]|
|Optional | -I  |--init-seq       |    Initial sequence number to use [0]|
|Optional | -L  |--listener       |    Description of a command lister eg udp:192.168.0.1:2000 [NULL]|
|Optional | -u  |--use-seq        |    Use sequence numbers in packets [true]|
|Optional | -o  |--offset         |    How long in microseconds to sleep before beginning to send|
|Optional | -t  |--timeout        |    time to run for [60s]|
|Optional | -w  |--wait           |    How long burst window is in nanos minimum of 5000ns, maximum of 70secs [5]|
|Optional | -b  |--burst          |    How many packets to send in each burst [1]|
|Optional | -l  |--length         |    Length of the entire packet in bytes [64]|
|Optional | -V  |--vlan-id        |    VLAN ID, if you wish to generate 802.1Q VLAN tagged packets, -1 if not desired. Must be in the rage of 1-4094 [-1]|
|Optional | -P  |--vlan-pri       |    VLAN priority. Priority if you are generating VLAN tagged packets. Must be in the range 0 - 7 [0]|
|Optional | -N  |--pri-pkts       |    Every N packets a high priority packet will be issued [1], all others are low|
|Flag     | -C  |--distagg-client |   Run in distribute aggregate in client mode|
|Flag     | -D  |--distagg-server |   Run in distribute aggregate in server mode|
|Flag     | -h  |--help           |    Print this help message|



