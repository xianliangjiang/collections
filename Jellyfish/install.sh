#!/bin/bash

sudo apt-get install -y python-setuptools
# build RipL
cd ripl
sudo python setup.py develop

# build RipLPOX
cd ../riplpox
sudo python setup.py develop
