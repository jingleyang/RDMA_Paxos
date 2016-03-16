# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/zookeeper/zoo.c

OBJS += \
./src/zookeeper/zoo.o


# Each subdirectory must supply rules for building sources it contributes
src/config-comp/%.o: ../src/config-comp/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc-4.8 -fPIC -rdynamic -std=gnu11 -DDEBUG=$(DEBUGOPT) -I"$(ROOT_DIR)/../.local/include" -I$(ZK_HOME)/src/c/include -I$(ZK_HOME)/src/c/generated -O0 -g3 -Wall -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


