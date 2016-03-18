# How to add a testing task

## Introduction

## Testing Environment

CPU:
```
processor   : 11
vendor_id   : GenuineIntel
cpu family  : 6
model       : 62
model name  : Intel(R) Xeon(R) CPU E5-2420 v2 @ 2.20GHz
stepping    : 4
microcode   : 0x428
cpu MHz     : 2500.007
cache size  : 15360 KB
physical id : 0
siblings    : 12
core id     : 5
cpu cores   : 6
apicid      : 11
initial apicid  : 11

```

Memory:
```
admin@hemingserver1:~/RDMA_Paxos/apps$ cat /proc/meminfo 
MemTotal:       32896520 kB
MemFree:        21244800 kB
MemAvailable:   30374680 kB

```

Linux:
```
admin@hemingserver1:~/RDMA_Paxos/apps$ uname -a
Linux hemingserver1 3.19.0-25-generic #26~14.04.1-Ubuntu SMP Fri Jul 24 21:16:20 UTC 2015 x86_64 x86_64 x86_64 GNU/Linux

```

Disk:
```
=== START OF INFORMATION SECTION ===
Device Model:     INTEL SSDSC2BB160G4T
Serial Number:    PHWL445600MP160MGN
LU WWN Device Id: 5 5cd2e4 04c43c8f2
Add. Product Id:  DELL(tm)
Firmware Version: D201DL13
User Capacity:    160,041,885,696 bytes [160 GB]
Sector Sizes:     512 bytes logical, 4096 bytes physical
Rotation Rate:    Solid State Device
```

Disk Speed:

```
admin@hemingserver1:~/RDMA_Paxos/apps$ dd if=/dev/zero of=/tmp/output bs=8k count=20k
20480+0 records in
20480+0 records out
167772160 bytes (168 MB) copied, 0.890525 s, 188 MB/s
admin@hemingserver1:~/RDMA_Paxos/apps$ dd if=/dev/zero of=/tmp/output bs=8k count=20k
20480+0 records in
20480+0 records out
167772160 bytes (168 MB) copied, 0.893626 s, 188 MB/s
admin@hemingserver1:~/RDMA_Paxos/apps$ 

```

