static int (*fp_recv)(int socket, void *buffer, size_t length, int flags);
ssize_t recv(int socket, void *buffer, size_t length, int flags) {
}