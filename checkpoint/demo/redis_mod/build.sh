#!/bin/sh
mkdir -p out
make V=1 OPTIMIZATION="-O0" MALLOC="libc" 
make install PREFIX=$PWD/out 
