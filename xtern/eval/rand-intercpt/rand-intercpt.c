static int (*fp_recv)(int socket, void *buffer, size_t length, int flags);
ssize_t recv(int socket, void *buffer, size_t length, int flags) {
  OPERATION_START;
  RESOLVE(recv);
  int ret = fp_recv(socket, buffer, length, flags);
  int cur_errno = errno;
  OPERATION_END;
  errno = cur_errno;
  return ret;
}