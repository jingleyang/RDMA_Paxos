STD_LIBS=-lsupc++ -lpthread -lstdc++ -lrt

all:: interpose.so

interpose.so:  spec_hooks.o consensus.o shm.o
	gcc -shared -Wl,-soname,interpose.so spec_hooks.o -o interpose.so -ldl $(STD_LIBS)

spec_hooks.o: spec_hooks.cpp consensus.h shm.h
	gcc -fPIC -rdynamic spec_hooks.cpp -c -o spec_hooks.o

consensus.o: consensus.c consensus.h consensus-msg.h db-interface.h
	gcc -fPIC -rdynamic consensus.c -c -o consensus.o

shm.o: shm.c shm.h
	gcc -fPIC -rdynamic shm.c -c -o shm.o