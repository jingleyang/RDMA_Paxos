#include "tern/runtime/runtime.h"

using namespace tern;

extern "C" {

pthread_t replicate_th;

static void *__tern_thread_func(void *arg) {
}

void __tern_prog_begin(void) {
	Runtime::__pthread_create(&replicate_th, NULL, __tern_thread_func, NULL);
}

}