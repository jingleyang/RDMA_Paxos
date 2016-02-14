using namespace tern;

Runtime::Runtime() {
}

int Runtime::__pthread_create(pthread_t *th, const pthread_attr_t *a, void *(*func)(void*), void *arg) {
  int ret;
  ret = pthread_create(th, a, func, arg);
  return ret;
}

void Runtime::__pthread_exit(void *value_ptr) {
  pthread_exit(value_ptr);
}