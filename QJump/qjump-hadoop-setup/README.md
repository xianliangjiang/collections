# qjump-hadoop
QJump Hadoop configuration and setup. Used to reproduce QJump experiments that include Hadoop Map-Reduce interference. 

Note: Don't forget to update the namenode and jobtracker fields to the hostname you want to use.

To use this run "setup.sh" on any node that needs configuring. For the master node run "setup.sh master". 

Once the cluster is setup, run "make all" to build the Hadoop job and "make run" to execute it. 


