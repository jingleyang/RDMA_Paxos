# Set the root directory of this project's object files

# Set the directory root of this project's source files

INC_DIRS=-I$(SRC_ROOT)/include/ -I$(OBJ_ROOT)/include -I.
STD_LIBS=-lsupc++ -lpthread -lstdc++ -lrt
TERN_LIBS=-L$(OBJ_ROOT)/lib/ -lruntime -lcommon
CFLAGS += $(INC_DIRS)
HOOK_DIR=$(SRC_ROOT)/dync_hook

all:: interpose.so

interpose.so: spec_hooks.cpp hook.cpp
	gcc -fPIC -rdynamic $(HOOK_DIR)/hook.cpp $(CFLAGS) -c -o interpose.o
	gcc $(CFLAGS) -shared -Wl,-soname,interpose.so interpose.o -o interpose.so -ldl $(TERN_LIBS) $(STD_LIBS)
