## Rate Control Protocol (RCP) for Congestion Control

Angela Gong and Alice Yeh

### Introduction

We've reproduced several graphs from the paper "Why Flow-Completion Time is the
Right Metric for Congestion Control", by Nandita Dukkipati and Nick McKeown.

### System Requirements

We have provided a [VM](http://cs.stanford.edu/~agong/cs244-vm.tar.gz) which you
can run locally, or an Amazon EC2 AMI called "CS244-2015-RCP" in the US West
Oregon cluster that you can run. Both of these contain all the code and setup
needed for the experiment.

However, if you would like to reproduce *everything* on your own, you can
follow the "Installation and Reproduction Instructions" below.

### Installation and Reproduction Instructions

1. Locate an [Ubuntu 14.04 installation](http://releases.ubuntu.com/14.04/). We
   used an Ubuntu 14.04 server on an Amazon EC2 c3.large instance.
2. Make sure the following are installed on Ubuntu: git, gcc, make, g++, tcl,
   libx11-dev, libxt-dev, nam, perl, python, python-matplotlib, python-scipy.
3. `cd ~`
4. Download the all-in-one version of
   [ns-2](http://www.isi.edu/nsnam/ns/ns-build.html) by running
   `wget http://sourceforge.net/projects/nsnam/files/allinone/ns-allinone-2.35/ns-allinone-2.35.tar.gz/download`.
5. Untar the package by running `tar -xvf download`.
6. Clone this repository by running
   `git clone https://github.com/anjoola/cs244-rcp.git`.
7. Patch the ns-2 code by copying the files in `~/cs244-rcp/patch/` and replacing
   the ones in `~/ns-allinone-2.35/ns-2.35/`. Do this by running
   `cp -r ~/cs244-rcp/patch/* ~/ns-allinone-2.35/ns-2.35/`.
8. `sudo ./install in ~/ns-allinone-2.35/`. This will take a while.
9. Add the following to `~/.bashrc`, then restart bash:

         # LD_LIBRARY_PATH
         OTCL_LIB=~/ns-allinone-2.35/otcl-1.14
         NS2_LIB=~/ns-allinone-2.35/lib
         USR_LOCAL_LIB=/usr/local/lib
         export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$OTCL_LIB:$NS2_LIB:$USR_LOCAL_LIB
      
         # TCL_LIBRARY
         TCL_LIB=~/ns-allinone-2.35/tcl8.5.10/library
         USR_LIB=/usr/lib
         export TCL_LIBRARY=$TCL_LIB:$USR_LIB
      
         # PATH
         NS=~/ns-allinone-2.35/ns-2.35/
         NAM=~/ns-allinone-2.34/nam-1.15/
         PATH=$PATH:$NS:$NAM

### Generating Results

This will take about 1-2 hours on an Ubuntu 14.04 VM with 4 GB of RAM on a host
OS with a 1.8GHz Intel i7 processor, or 30-60 minutes on a 2.6GHz Intel i5. On
an Amazon EC2 c3.large instance, it will take about 20-40 minutes.

    cd ~/cs244-rcp/scripts
    chmod +x run.sh
    ./run.sh

All the data is produced in the `~/cs244-rcp/scripts/data/` folder, and the
plots are in `~/cs244-rcp/scripts/{semilog, loglog}-plot-[rtt]-[shape].png`.
This only generates the main graphs (it would take too long to run the
extensions).

If you wanted to produce ALL the graphs (**this is not recommended**), do the
following, which will take several hours:

    cd ~/cs244-rcp/scripts
    chmod +x run-extensions.sh
    ./run-extensions.sh

Hint: Use `CTRL`+`ALT`+`T` to launch a terminal if you're using the Ubuntu VM.
