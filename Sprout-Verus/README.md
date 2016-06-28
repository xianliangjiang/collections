# CS244 PA3: Verus vs. Sprout
###### Andrew Lim (alim16@stanford.edu) and Ryan Hermstein (rherms@stanford.edu)

### Code structure:

* The Sprout code is located in ***alfalfa/src/examples***. We use the ***sendonly*** branch of alfalfa.
* The Verus code is located in ***verus/src***.
* The scripts that run our experiments as well as our variable trace generator script are in ***scripts***.
* Our modified plotting scripts for Verus and the combined Verus-Sprout throughput graph are in ***plot_scripts***.
* Our implementation of RED for Mahimahi is located in ***mahimahi/src/packet/red_packet_queue.hh***.

### Setting up an EC2 Instance

If you are unfamiliar with EC2, consider looking at [CS244's guide to getting started](http://web.stanford.edu/class/cs244/ec2setup.html) and [Amazon's beginner guide](http://docs.aws.amazon.com/AWSEC2/latest/UserGuide/AccessingInstancesLinux.html).

To run the experiments, you will need to set up an EC2 instance from our public AMI.
Make sure that your region is set to US West (N. California) region, as this is where
the AMI is registered. You can change your region in the top right corner of the upper 
navigation bar. In the EC2 Management Console, click on AMIs under Images in the left sidebar. Make sure to search for "Public Images" instead of "Owned by Me". Our AMI id is: **ami-38afd458** and the AMI name is: **cs244-pa3-verus-sprout-alim16-rherms**. Right click the AMI and click "Launch" to launch an instance.

The experiments should be run on a **single m4.xlarge** instance with **30 GiB of General Purpose SSD** 
storage (this will be the default when you configure instance settings). When configuring the
security group, you will want to add an **All UDP** rule with **port range 0 - 65535** and source set to **My IP**.
Once you have configured these settings, the instance is ready to be launched.

### Running the Experiments
After launching an EC2 instance from our public AMI, SSH into your instance. All the executables
should be already compiled. Thus, you can run the experiments immediately using the following commands:

```sh
cd PA3
python run_experiment.py
```
The experiments will take ~30 minutes to run. The resulting graphs will be placed in the **results** subdirectory 
within the **PA3** directory. SCP the graph files to your local machine, and open them in a web browser to view.
The plot html file names will include the following information: verus or sprout (which congestion control algorithm),
var or alma (indicate whether the trace used was the synthetic variable trace or the Alma Street trace), and throughput
or delay (what kind of plot). The plot that compares the throughputs is named "var_throughput_comp.html".

Don't forget to stop your instance after obtaining the result graphs! Also, if you want to close the SSH connection to your instance while the experiments are running, look into "screen".

Note: If you would like to re-run the experiments, you should first run the cleanup script to delete any logs/outputs
from the previous run. From within **PA3**:

```sh
./cleanup_results.sh
```

Also, if you would like to run the experiments using the modified Verus parameters we considered, you will have to
recompile the Verus server and client. From within **PA3**:

```sh
cd verus/src
cp verus_modified.hpp verus.hpp
make clean && make
```

You can then navigate back up to the topmost level of the **PA3** directory and run the **run_experiment.py** script.
Likewise if you would like to return to the original Verus params, from within **PA3**:

```sh
cd verus/src
cp verus_original.hpp verus.hpp
make clean && make
```

If you would like to run the experiments including some of the additional traces (ATT and Verizon), use the 
**run_experiment_full.py** script.


