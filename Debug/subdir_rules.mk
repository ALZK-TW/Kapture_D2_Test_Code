################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

# Each subdirectory must supply rules for building sources it contributes
%.obj: ../%.c $(GEN_OPTS) | $(GEN_FILES)
	@echo 'Building file: "$<"'
	@echo 'Invoking: ARM Compiler'
	"C:/TI/ccsv8/tools/compiler/ti-cgt-arm_18.12.1.LTS/bin/armcl" -mv7M4 --code_state=16 --float_support=FPv4SPD16 -me --include_path="C:/TI/simplelink_cc13x2_26x2_sdk_3_10_00_53/source/ti/ble5stack/inc" --include_path="C:/Users/hukid/workspace_v8/Kapture_D2_Test_Code" --include_path="C:/TI/simplelink_cc13x2_26x2_sdk_3_10_00_53/source/ti/posix/ccs" --include_path="C:/TI/ccsv8/tools/compiler/ti-cgt-arm_18.12.1.LTS/include" --define=CLOSE_TOUCH_PANEL --define=OLD_ME --define=DEBUG --define=DeviceFamily_CC26X2 -g --diag_warning=225 --diag_warning=255 --diag_wrap=off --display_error_number --gen_func_subsections=on --preproc_with_compile --preproc_dependency="$(basename $(<F)).d_raw" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '

%.obj: ../%.cpp $(GEN_OPTS) | $(GEN_FILES)
	@echo 'Building file: "$<"'
	@echo 'Invoking: ARM Compiler'
	"C:/TI/ccsv8/tools/compiler/ti-cgt-arm_18.12.1.LTS/bin/armcl" -mv7M4 --code_state=16 --float_support=FPv4SPD16 -me --include_path="C:/TI/simplelink_cc13x2_26x2_sdk_3_10_00_53/source/ti/ble5stack/inc" --include_path="C:/Users/hukid/workspace_v8/Kapture_D2_Test_Code" --include_path="C:/TI/simplelink_cc13x2_26x2_sdk_3_10_00_53/source/ti/posix/ccs" --include_path="C:/TI/ccsv8/tools/compiler/ti-cgt-arm_18.12.1.LTS/include" --define=CLOSE_TOUCH_PANEL --define=OLD_ME --define=DEBUG --define=DeviceFamily_CC26X2 -g --diag_warning=225 --diag_warning=255 --diag_wrap=off --display_error_number --gen_func_subsections=on --preproc_with_compile --preproc_dependency="$(basename $(<F)).d_raw" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '

build-526128641:
	@$(MAKE) --no-print-directory -Onone -f subdir_rules.mk build-526128641-inproc

build-526128641-inproc: ../mutex.cfg
	@echo 'Building file: "$<"'
	@echo 'Invoking: XDCtools'
	"C:/TI/xdctools_3_51_02_21_core/xs" --xdcpath="C:/TI/simplelink_cc13x2_26x2_sdk_3_10_00_53/source;C:/TI/simplelink_cc13x2_26x2_sdk_3_10_00_53/kernel/tirtos/packages;" xdc.tools.configuro -o configPkg -t ti.targets.arm.elf.M4F -p ti.platforms.simplelink:CC2642R1F -r release -c "C:/TI/ccsv8/tools/compiler/ti-cgt-arm_18.12.1.LTS" --compileOptions "-mv7M4 --code_state=16 --float_support=FPv4SPD16 -me --include_path=\"C:/TI/simplelink_cc13x2_26x2_sdk_3_10_00_53/source/ti/ble5stack/inc\" --include_path=\"C:/Users/hukid/workspace_v8/Kapture_D2_Test_Code\" --include_path=\"C:/TI/simplelink_cc13x2_26x2_sdk_3_10_00_53/source/ti/posix/ccs\" --include_path=\"C:/TI/ccsv8/tools/compiler/ti-cgt-arm_18.12.1.LTS/include\" --define=CLOSE_TOUCH_PANEL --define=OLD_ME --define=DEBUG --define=DeviceFamily_CC26X2 -g --diag_warning=225 --diag_warning=255 --diag_wrap=off --display_error_number --gen_func_subsections=on  " "$<"
	@echo 'Finished building: "$<"'
	@echo ' '

configPkg/linker.cmd: build-526128641 ../mutex.cfg
configPkg/compiler.opt: build-526128641
configPkg/: build-526128641


