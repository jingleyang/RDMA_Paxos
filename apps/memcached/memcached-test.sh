#!/bin/bash

#run memcached in localhost
./memcached-install/bin/memcached -m 64 -l 127.0.0.1 -p 11211 -u root -P /var/run/memcachedrep.pid -d

#test memcached using mcperf
#mcperf -h to see help document
./mcperf-install/bin/mcperf --linger=0 --call-rate=0 --num-calls=100000 --conn-rate=0 --num-conns=1 --sizes=d10

