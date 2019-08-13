################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

# Add inputs and outputs from these tool invocations to the build variables 
CFG_SRCS += \
../mutex.cfg 

CPP_SRCS += \
../FIFO.cpp 

CMD_SRCS += \
../CC26X2R1_LAUNCHXL_TIRTOS.cmd 

C_SRCS += \
../CC26X2R1_LAUNCHXL.c \
../CC26X2R1_LAUNCHXL_fxns.c \
../ccfg.c \
../main.c \
../uart_printf.c \
../util.c 

GEN_CMDS += \
./configPkg/linker.cmd 

GEN_FILES += \
./configPkg/linker.cmd \
./configPkg/compiler.opt 

GEN_MISC_DIRS += \
./configPkg/ 

C_DEPS += \
./CC26X2R1_LAUNCHXL.d \
./CC26X2R1_LAUNCHXL_fxns.d \
./ccfg.d \
./main.d \
./uart_printf.d \
./util.d 

GEN_OPTS += \
./configPkg/compiler.opt 

OBJS += \
./CC26X2R1_LAUNCHXL.obj \
./CC26X2R1_LAUNCHXL_fxns.obj \
./FIFO.obj \
./ccfg.obj \
./main.obj \
./uart_printf.obj \
./util.obj 

CPP_DEPS += \
./FIFO.d 

GEN_MISC_DIRS__QUOTED += \
"configPkg\" 

OBJS__QUOTED += \
"CC26X2R1_LAUNCHXL.obj" \
"CC26X2R1_LAUNCHXL_fxns.obj" \
"FIFO.obj" \
"ccfg.obj" \
"main.obj" \
"uart_printf.obj" \
"util.obj" 

C_DEPS__QUOTED += \
"CC26X2R1_LAUNCHXL.d" \
"CC26X2R1_LAUNCHXL_fxns.d" \
"ccfg.d" \
"main.d" \
"uart_printf.d" \
"util.d" 

CPP_DEPS__QUOTED += \
"FIFO.d" 

GEN_FILES__QUOTED += \
"configPkg\linker.cmd" \
"configPkg\compiler.opt" 

C_SRCS__QUOTED += \
"../CC26X2R1_LAUNCHXL.c" \
"../CC26X2R1_LAUNCHXL_fxns.c" \
"../ccfg.c" \
"../main.c" \
"../uart_printf.c" \
"../util.c" 

CPP_SRCS__QUOTED += \
"../FIFO.cpp" 


