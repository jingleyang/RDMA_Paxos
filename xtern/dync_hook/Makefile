STD_LIBS=-lsupc++ -lpthread -lstdc++ -lrt

all:: interpose.so

interpose.so: helper.o spec_hooks.o
	gcc -shared -Wl,-soname,interpose.so helper.o spec_hooks.o -o interpose.so -ldl $(STD_LIBS)

helper.o: helper.c helper.h
	gcc -fPIC -rdynamic helper.c -c -o helper.o

spec_hooks.o: spec_hooks.cpp helper.h
	gcc -fPIC -rdynamic spec_hooks.cpp -c -o spec_hooks.o
