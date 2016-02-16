################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/log/log.c

OBJS += \
./src/log/log.o

C_DEPS += \
./src/log/log.d


# Each subdirectory must supply rules for building sources it contributes
src/log/%.o: ../src/log/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc-4.8 -fPIC -rdynamic -std=gnu11 -DDEBUG=$(DEBUGOPT) -I"$(ROOT_DIR)/../.local/include" -O0 -g3 -Wall -pg -c -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


