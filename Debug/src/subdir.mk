################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/Loader.c \
../src/MiniLinker.c \
../src/Parser.c 

OBJS += \
./src/Loader.o \
./src/MiniLinker.o \
./src/Parser.o 

C_DEPS += \
./src/Loader.d \
./src/MiniLinker.d \
./src/Parser.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cygwin C Compiler'
	gcc -I"C:\android-ndk-r16b\sysroot\usr\include" -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


