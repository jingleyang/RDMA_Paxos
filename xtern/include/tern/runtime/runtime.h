#include <pthread.h>

namespace tern {

struct Runtime {
  Runtime();

  // thread management
  static int __pthread_create(pthread_t *th, const pthread_attr_t *a, void *(*func)(void*), void *arg);
};

}