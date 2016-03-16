# A demo about how to freeze a process by CRIU and restore it at another LXC container
## Environment Setup

Host Linux: Ubuntu 14.04 LTS, Linux 3.16.0-62-generic #83~14.04.1-Ubuntu SMP x86_64 GNU/Linux

LXC version:

```
$ lxc-create --version
2.0.0.rc10
```
CRIU version:

```
$ criu --version
Version: 2.0
GitID: v2.0
```

## Install CRIU and LXC

### CRIU
Please visit [criu.org](https://criu.org/Installation) to learn more installation instructions.

I have written a shell script to install libraries which is install_deps.sh. And in order to install criu to a diy path, I made some modification which is at the criu.diff.

The path checkpoint/demo/criu_lxc/prepare/criu/out contains binary I have compiled and can be used directly.

The file out/lib/libnl-3.so.200 and out/lib/libprotobuf-c.so.0 can be copied into /lib/x86_64-linux-gnu/ and out/sbin/criu can be copied into /sbin/ if you want to use criu directly.

### LXC
The default lxc for ubuntu is too old to use. We need a daily lxc. The script checkpoint/demo/criu_lxc/prepare/lxc/install_lxc.sh will install a daily lxc.


## Demo Description
