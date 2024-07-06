
file(GLOB_RECURSE HAL_SOURCES "stm32h7xx_hal_driver/Src/*.c")

add_library(stm32h7xx_hal_driver EXCLUDE_FROM_ALL
        cmsis_device_h7/Source/Templates/gcc/startup_stm32h723xx.s
        cmsis_device_h7/Source/Templates/system_stm32h7xx.c
        ${HAL_SOURCES}
)

# Set the target
target_compile_definitions(stm32h7xx_hal_driver PUBLIC
        USE_HAL_DRIVER
        STM32H723xx
        $<$<CONFIG:Debug>:DEBUG>
)
set(THREADX_ARCH cortex_m7)
SET(THREADX_TOOLCHAIN gnu)

target_include_directories(stm32h7xx_hal_driver PUBLIC
        conf
        cmsis_core/Include
        cmsis_device_h7/Include
        "stm32h7xx_hal_driver/Inc"
)

add_subdirectory(threadx)
