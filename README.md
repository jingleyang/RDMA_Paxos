# RDMA_Paxos

This project combines RDMA (Remote Direct Memory Access) and Paxos.  

OS: Ubuntu 14.04.02 64bit.  

Install depdendent libraries/tools:  
`sudo apt-get install libxml2-dev`  
  
  
1. Installing kernel source packages  
sudo apt-get install libtool autoconf automake linux-tools-common
sudo apt-get install fakeroot build-essential crash kexec-tools makedumpfile kernel-wedge  
sudo apt-get build-dep linux  
sudo apt-get install git-core libncurses5 libncurses5-dev libelf-dev  
sudo apt-get install linux-headers-$(uname -r)  
  
2. Commands to checkout a brand-new project:  
`git clone https://github.com/wangchenghku/RDMA_Paxos` 
