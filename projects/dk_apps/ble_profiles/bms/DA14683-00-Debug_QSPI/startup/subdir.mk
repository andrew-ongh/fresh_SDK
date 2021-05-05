################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
C:/new_SDK/sdk/bsp/startup/config.c \
C:/new_SDK/sdk/bsp/startup/system_ARMCM0.c 

S_UPPER_SRCS += \
C:/new_SDK/sdk/bsp/startup/startup_ARMCM0.S \
C:/new_SDK/sdk/bsp/startup/vector_table.S 

OBJS += \
./startup/config.o \
./startup/startup_ARMCM0.o \
./startup/system_ARMCM0.o \
./startup/vector_table.o 

S_UPPER_DEPS += \
./startup/startup_ARMCM0.d \
./startup/vector_table.d 

C_DEPS += \
./startup/config.d \
./startup/system_ARMCM0.d 


# Each subdirectory must supply rules for building sources it contributes
startup/config.o: C:/new_SDK/sdk/bsp/startup/config.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross ARM C Compiler'
	arm-none-eabi-gcc -mcpu=cortex-m0 -mthumb -Og -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -Werror -Wall  -g3 -Ddg_configBLACK_ORCA_IC_REV=BLACK_ORCA_IC_REV_B -Ddg_configBLACK_ORCA_IC_STEP=BLACK_ORCA_IC_STEP_B -I"../../../../../sdk/interfaces/ble_stack/" -I"C:\new_SDK\projects\dk_apps\ble_profiles\bms\config" -I"C:\new_SDK\sdk\bsp\adapters\include" -I"C:\new_SDK\sdk\bsp\memory\include" -I"C:\new_SDK\sdk\interfaces\ble\config" -I"C:\new_SDK\sdk\interfaces\ble\include" -I"C:\new_SDK\sdk\interfaces\ble\include\adapter" -I"C:\new_SDK\sdk\interfaces\ble\include\manager" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\config" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\hl\src\host\att" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\hl\src\host\att\attc" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\hl\src\host\att\attm" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\hl\src\host\att\atts" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\hl\src\host\gap" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\hl\src\host\gap\gapc" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\hl\src\host\gap\gapm" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\hl\src\host\gatt" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\hl\src\host\gatt\gattc" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\hl\src\host\gatt\gattm" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\hl\src\host\l2c\l2cc" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\hl\src\host\l2c\l2cm" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\hl\src\host\smp" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\hl\src\host\smp\smpc" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\hl\src\host\smp\smpm" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\hl\src\rwble_hl" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\ll\src\controller\em" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\ll\src\controller\llc" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\ll\src\controller\lld" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\ll\src\controller\llm" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\ll\src\rwble" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\profiles" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ea\api" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\em\api" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\hci\api" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\hci\src" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\modules\common\api" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\modules\dbg\api" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\modules\gtl\api" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\modules\gtl\src" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\modules\h4tl\api" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\modules\ke\api" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\modules\ke\src" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\modules\nvds\api" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\modules\rwip\api" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\plf\black_orca\src\arch" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\plf\black_orca\src\arch\ll\armgcc_4_8" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\plf\black_orca\src\arch\boot\armgcc_4_8" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\plf\black_orca\src\arch\compiler\armgcc_4_8" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\plf\black_orca\src\build\ble-full\reg\fw" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\plf\black_orca\src\driver\flash" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\plf\black_orca\src\driver\reg" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\plf\black_orca\src\driver\rf" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\plf\black_orca\src\driver\rf\api" -I"C:\new_SDK\sdk\interfaces\ble_services\include" -I"C:\new_SDK\sdk\bsp\include" -I"C:\new_SDK\sdk\bsp\config" -I"C:\new_SDK\sdk\bsp\system\sys_man\include" -I"C:\new_SDK\sdk\bsp\free_rtos\include" -I"C:\new_SDK\sdk\bsp\osal" -I"C:\new_SDK\sdk\bsp\peripherals\include" -include"C:\new_SDK\projects\dk_apps\ble_profiles\bms\config\custom_config_qspi.h" -std=gnu11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

startup/startup_ARMCM0.o: C:/new_SDK/sdk/bsp/startup/startup_ARMCM0.S
	@echo 'Building file: $<'
	@echo 'Invoking: Cross ARM GNU Assembler'
	arm-none-eabi-gcc -mcpu=cortex-m0 -mthumb -Og -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -Werror -Wall  -g3 -x assembler-with-cpp -Ddg_configBLACK_ORCA_IC_REV=BLACK_ORCA_IC_REV_B -Ddg_configBLACK_ORCA_IC_STEP=BLACK_ORCA_IC_STEP_B -I"../../../../../sdk/interfaces/ble_stack/" -I"C:\new_SDK\sdk\bsp\config" -include"C:\new_SDK\projects\dk_apps\ble_profiles\bms\config\custom_config_qspi.h" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

startup/system_ARMCM0.o: C:/new_SDK/sdk/bsp/startup/system_ARMCM0.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross ARM C Compiler'
	arm-none-eabi-gcc -mcpu=cortex-m0 -mthumb -Og -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -Werror -Wall  -g3 -Ddg_configBLACK_ORCA_IC_REV=BLACK_ORCA_IC_REV_B -Ddg_configBLACK_ORCA_IC_STEP=BLACK_ORCA_IC_STEP_B -I"../../../../../sdk/interfaces/ble_stack/" -I"C:\new_SDK\projects\dk_apps\ble_profiles\bms\config" -I"C:\new_SDK\sdk\bsp\adapters\include" -I"C:\new_SDK\sdk\bsp\memory\include" -I"C:\new_SDK\sdk\interfaces\ble\config" -I"C:\new_SDK\sdk\interfaces\ble\include" -I"C:\new_SDK\sdk\interfaces\ble\include\adapter" -I"C:\new_SDK\sdk\interfaces\ble\include\manager" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\config" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\hl\src\host\att" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\hl\src\host\att\attc" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\hl\src\host\att\attm" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\hl\src\host\att\atts" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\hl\src\host\gap" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\hl\src\host\gap\gapc" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\hl\src\host\gap\gapm" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\hl\src\host\gatt" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\hl\src\host\gatt\gattc" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\hl\src\host\gatt\gattm" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\hl\src\host\l2c\l2cc" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\hl\src\host\l2c\l2cm" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\hl\src\host\smp" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\hl\src\host\smp\smpc" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\hl\src\host\smp\smpm" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\hl\src\rwble_hl" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\ll\src\controller\em" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\ll\src\controller\llc" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\ll\src\controller\lld" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\ll\src\controller\llm" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\ll\src\rwble" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ble\profiles" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\ea\api" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\em\api" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\hci\api" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\ip\hci\src" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\modules\common\api" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\modules\dbg\api" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\modules\gtl\api" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\modules\gtl\src" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\modules\h4tl\api" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\modules\ke\api" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\modules\ke\src" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\modules\nvds\api" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\modules\rwip\api" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\plf\black_orca\src\arch" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\plf\black_orca\src\arch\ll\armgcc_4_8" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\plf\black_orca\src\arch\boot\armgcc_4_8" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\plf\black_orca\src\arch\compiler\armgcc_4_8" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\plf\black_orca\src\build\ble-full\reg\fw" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\plf\black_orca\src\driver\flash" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\plf\black_orca\src\driver\reg" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\plf\black_orca\src\driver\rf" -I"C:\new_SDK\sdk\interfaces\ble\src\stack\plf\black_orca\src\driver\rf\api" -I"C:\new_SDK\sdk\interfaces\ble_services\include" -I"C:\new_SDK\sdk\bsp\include" -I"C:\new_SDK\sdk\bsp\config" -I"C:\new_SDK\sdk\bsp\system\sys_man\include" -I"C:\new_SDK\sdk\bsp\free_rtos\include" -I"C:\new_SDK\sdk\bsp\osal" -I"C:\new_SDK\sdk\bsp\peripherals\include" -include"C:\new_SDK\projects\dk_apps\ble_profiles\bms\config\custom_config_qspi.h" -std=gnu11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

startup/vector_table.o: C:/new_SDK/sdk/bsp/startup/vector_table.S
	@echo 'Building file: $<'
	@echo 'Invoking: Cross ARM GNU Assembler'
	arm-none-eabi-gcc -mcpu=cortex-m0 -mthumb -Og -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -Werror -Wall  -g3 -x assembler-with-cpp -Ddg_configBLACK_ORCA_IC_REV=BLACK_ORCA_IC_REV_B -Ddg_configBLACK_ORCA_IC_STEP=BLACK_ORCA_IC_STEP_B -I"../../../../../sdk/interfaces/ble_stack/" -I"C:\new_SDK\sdk\bsp\config" -include"C:\new_SDK\projects\dk_apps\ble_profiles\bms\config\custom_config_qspi.h" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


