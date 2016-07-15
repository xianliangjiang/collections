# CS244-shrew-attack

How to reproduce results:

  Launch a new EC2 instance on AWS

  Search the community AMIs for 'CS244 shrew attack' (ami-7dc7f84d)

  Choose a c3.large instance that is not EBS only

  Set security group to allow ssh and open port 8000 if you want to view the results through python SimpleHTTPServer

  More details on setting up the instance here : (http://web.stanford.edu/class/cs244/ec2setup.html)

  ssh into the instance
```
$ cd CS244-shrew-attack
$ sudo ./run.sh
```
NOTE: Running the tests will take about 2hrs 30mins or more depending on AWS load.
Running in the background may cause issues, so to be safe run the script in the foreground.

Plots will be output in a new directory named shrewattack-*timestamp*

