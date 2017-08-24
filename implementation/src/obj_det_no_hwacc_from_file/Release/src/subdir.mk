################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/eval_results.c \
../src/finalise_output.c \
../src/image_utils.c \
../src/load_model.c \
../src/run.c \
../src/run_network.c 

OBJS += \
./src/eval_results.o \
./src/finalise_output.o \
./src/image_utils.o \
./src/load_model.o \
./src/run.o \
./src/run_network.o 

C_DEPS += \
./src/eval_results.d \
./src/finalise_output.d \
./src/image_utils.d \
./src/load_model.d \
./src/run.d \
./src/run_network.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: SDSCC Compiler'
	sdscc -Wall -O3 -I"../src" -c -fmessage-length=0 -MT"$@" -fopenmp -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<" -sds-sys-config linux -sds-proc a53_0 -sds-pf /home/msc17f2/ma/object_detection_on_zynq_mpsoc/implementation/zcu102_linux_platform/zcu102_es2_ev
	@echo 'Finished building: $<'
	@echo ' '


