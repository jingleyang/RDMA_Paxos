#!/bin/sh

../bin/redis-cli -h 127.0.0.1 -p 6379 hello 1
../bin/redis-cli -h 127.0.0.1 -p 6379 hello 2 

echo "Slave01 will be forced to set a diversity hash"
../bin/redis-cli -h 127.0.0.1 -p 16379 diversity 0xaabbccdd

../bin/redis-cli -h 127.0.0.1 -p 6379 hello 3 

echo "After checkpoint, The hash value will be corrected as 0xb4e8848c53810b01"
../bin/redis-cli -h 127.0.0.1 -p 16379 diversity 0xb4e8848c53810b01 

../bin/redis-cli -h 127.0.0.1 -p 6379 hello 4
../bin/redis-cli -h 127.0.0.1 -p 6379 hello 5
