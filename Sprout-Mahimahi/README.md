# cs244pa3

Running Sprout on mahimahi
==========================

1. Install things (see below)

2. Run the experiment

  `$ ./inception.sh`

  This will take approximately 5 hrs and 20 minutes and will produce two output files: `results/out.txt` (the raw log) and `results/out.tsv` (the nicely formatted output). Note! If you are running on a local machine, this script may prompt you to enter your password approximately 1hr and 5 minutes into the experiment to configure tcp. This was not required when running on EC2. 

  If you want to produce graphs with matplotlib after the experiment has run:

  `$ python make_graphs.py out.tsv <PREFIX>`

  This will produce one graph for each trace that was run (5 in total) in the format `PREFIX_TRACE.png`


Installation
============

1. Clone this repo

  `$ git clone https://github.com/dmetaxak/cs244pa3.git`

2. Install mahimahi (http://mahimahi.mit.edu/#getting)

  Install the dependencies:
  
  `$ sudo apt-get install build-essential git debhelper autotools-dev dh-autoreconf iptables protobuf-compiler libprotobuf-dev pkg-config libssl-dev dnsmasq-base ssl-cert libxcb-present-dev libcairo2-dev libpango1.0-dev iproute2 apache2-dev apache2-bin iptables dnsmasq-base gnuplot iproute2 apache2-api-20120211 libwww-perl`
  
  Install mahimahi itself:
  
      $ git clone https://github.com/ravinet/mahimahi (the mahimahi tools provide our emulated cellular network and measurement tools)
      $ cd mahimahi
      $ ./autogen.sh && ./configure && make
      $ sudo make install

  To check if mahimahi is running, try:

  `$ mm-delay 20 mm-link --meter-all /usr/share/mahimahi/traces/Verizon-LTE-short.up /usr/share/mahimahi/traces/Verizon-LTE-short.down` (this should spawn a delay shell with 4 graphs monitoring uplink/downlink)

  `$ exit`

2. Build Sprout (http://alfalfa.mit.edu/)

  Install dependencies:
  
  `$  sudo apt-get install libboost-math-dev libboost-math1.54.0 libprotobuf8 libprotobuf-dev `
  
  (note, this list may still be incomplete, the different machines we tested on required the further installation of different sets of dependencies)

      $ cd alfalfa
      $ ./autogen.sh
      $ ./configure --enable-examples
      $ make

  This may require that you install additional packages along the way. If you get any errors of someting not being found when you try to `make`, install those packages using `apt-get`.

3. Install iperf

`$ sudo apt-get install iperf`

4. (For graphing the results) install matplotlib if desired

`$ sudo apt-get install python-matplotlib`
