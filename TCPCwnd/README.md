cs244_pa3
=========

Programming Assignment 3 for CS 244
- Create a new EC2 instance (c3.large) with the following community AMI: “ami-e2b6ded2”
 - in order to view the results of the experiment, remember to add a custom security rule allowing inbound TCP traffic to port 8000
- Log into the instance and clone our github repo
 - git clone https://github.com/stephenq/cs244_pa3.git
 - cd cs244_pa3
- Run the experiment. The experiment will run for 25-30 minutes.
 - sudo ./run.sh
- The results of the experiment are plotted in a graph saved as “results.png”
 - To view the results, run ‘python -m SimpleHTTPServer’ and point your web browser at \<hostname\>:8000/results.png

