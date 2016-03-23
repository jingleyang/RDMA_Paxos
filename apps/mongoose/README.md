Install Apache Bench:  
apt-get install apache2-utils


Use ./mk to download and install mongoose.  

Run the server:  
`env LD_PRELOAD=/home/cheng/RDMA_Paxos/shm/target/interpose.so node_id=0 ./mongoose -I /usr/bin/php-cgi -p 7000 &`
