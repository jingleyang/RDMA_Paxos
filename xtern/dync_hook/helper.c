#include "helper.h"
#include <pthread.h>
#include <stdio.h>

pthread_t replicate_th;

static void *__tern_thread_func(void *arg) {
    printf("Hello World\n");
}

void __tern_prog_begin(void) {
    pthread_create(&replicate_th, NULL, __tern_thread_func, NULL);
}
