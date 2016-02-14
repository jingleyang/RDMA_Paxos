#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <tern/runtime/runtime.h>

using namespace tern;

#define __USE_TERN_RUNTIME
//#define PRINT_DEBUG

/*
#ifdef __USE_TERN_RUNTIME
#define __INSTALL_RUNTIME
#endif
#ifdef __INSTALL_RUNTIME
struct preload_static_block
{
  preload_static_block()
  {
    InstallRuntime();
  }
};
// notice, it doesn't work for functions called in static constructors of program 
static preload_static_block preload_initializer;
#endif
*/

#define __SPEC_HOOK___libc_start_main

#include "spec_hooks.cpp"