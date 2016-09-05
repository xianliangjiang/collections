# Copyright (c) 2015, Ionel Gogr
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice, this
#   list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
#
# * Neither the name of the project, the copyright holder nor the names of its
#   contributors may be used to endorse or promote products derived from
#   this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#!/bin/bash

# Fetch and decompress the data directory
wget https://www.dropbox.com/s/2iqya269d3j592f/data.tar.bz2?dl=1 -O data.tar.bz2
tar -xvf data.tar.bz2

# Create the tmpfs
sudo mount -t tmpfs tmpfs /mnt/tmpfs/ -o size=36000M,mode=1777

# Install Hadoop
sudo echo "deb [arch=amd64] http://archive.cloudera.com/cdh4/ubuntu/precise/amd64/cdh precise-cdh4 contrib" >> /etc/apt/sources.list
sudo echo "deb-src http://archive.cloudera.com/cdh4/ubuntu/precise/amd64/cdh precise-cdh4 contrib" >> /etc/apt/sources.list
sudo apt-get update
sudo apt-get install hadoop hadoop-0.20-conf-pseudo hadoop-0.20-mapreduce hadoop-0.20-mapreduce-jobtracker hadoop-0.20-mapreduce-tasktracker hadoop-client hadoop-hdfs hadoop-hdfs-datanode hadoop-hdfs-namenode hadoop-hdfs-secondarynamenode hadoop-mapreduce hadoop-yarn

# Copy the config files
# NOTE: Update the namenode and jobtracker fields to the hostname you want to use.
sudo cp hadoop_config/* /etc/hadoop/conf/

# Create the local HDFS directories
mkdir -p /mnt/tmpfs/hadoop-hdfs/cache/hdfs/dfs/data
mkdir -p /mnt/tmpfs/hadoop-hdfs/cache/hdfs/dfs/name
mkdir -p /mnt/tmpfs/hadoop-hdfs/cache/hdfs/dfs/namesecondary
mkdir -p /mnt/tmpfs/hadoop-hdfs/cache/mapred/mapred
sudo chmod 777 -R /mnt/tmpfs/hadoop-hdfs/cache/

if [[ $1 == "master" ]]; then
    # Format the namenode
    sudo su
    su hdfs
    hadoop namenode -format
    exit
    exit
    # Restart Hadoop
    sudo /etc/init.d/hadoop-hdfs-namenode restart
    sudo /etc/init.d/hadoop-0.20-mapreduce-jobtracker restart
fi

sudo /etc/init.d/hadoop-hdfs-datanode restart
sudo /etc/init.d/hadoop-0.20-mapreduce-tasktracker restart

if [[ $1 == "master" ]]; then
    # Create the HDFS directories
    hadoop fs -mkdir /qjump/

    # Load data on to HDFS
    hadoop fs -put data/* /qjump/
fi
