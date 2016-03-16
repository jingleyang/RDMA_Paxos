#!/bin/bash

sudo apt-get update
sudo apt-get install -y libevent-dev g++ make wget

#memcached install
if [ ! -f memcached-1.4.25.tar.gz ]
then
    wget http://www.memcached.org/files/memcached-1.4.25.tar.gz
else
    echo "memcached-1.4.25.tar.gz exists."
fi
tar zxvf memcached-1.4.25.tar.gz
cd memcached-1.4.25
./configure --prefix=`pwd`/../memcached-install
make 
sudo make install
cd ..

#mcperf install to test memcached
if [ ! -f mcperf-0.1.1.tar.gz ]
then
    wget https://storage.googleapis.com/google-code-archive-downloads/v2/code.google.com/twemperf/mcperf-0.1.1.tar.gz
else
    echo "mcperf-0.1.1.tar.gz exists."
fi
tar zxvf mcperf-0.1.1.tar.gz
cd mcperf-0.1.1
./configure --prefix=`pwd`/../mcperf-install
make 
sudo make install
cd ..

