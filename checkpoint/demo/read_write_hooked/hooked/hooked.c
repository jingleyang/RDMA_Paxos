#include <unistd.h>
#include <dlfcn.h>
#include <stdio.h> 
ssize_t read(int fd, void *buf, size_t count){
ssize_t (* orig_read)(int fd, void *buf, size_t count);
orig_read = dlsym(RTLD_NEXT,"read");
ssize_t nread = orig_read(fd,buf,count);
printf("[read_hooked] read %ld bytes\n",nread);
return nread;
}

ssize_t write(int fd, const void *buf, size_t count){
ssize_t (* orig_write)(int fd, const void *buf, size_t count);
orig_write = dlsym(RTLD_NEXT,"read");
ssize_t nwrite = orig_write(fd,buf,count);
printf("[write_hooked] write %ld bytes\n",nwrite);
return nwrite;
}
