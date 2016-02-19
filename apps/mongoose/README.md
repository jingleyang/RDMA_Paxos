Install Apache Bench:  
apt-get install apache2-utils


Use ./mk download and install mongoose  

run the server:  
LD_PRELOAD=/home/wangcheng/Downloads/RDMA_Paxos-master/shm/target/interpose.so ./mongoose -I /usr/bin/php-cgi -p 7000 -t 8 &

