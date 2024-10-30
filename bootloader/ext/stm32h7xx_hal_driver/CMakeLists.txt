add_library(stm32h7xx_hal OBJECT
        Src/stm32h7xx_hal_flash.c
        Src/stm32h7xx_hal_flash_ex.c
)
target_include_directories(stm32h7xx_hal PUBLIC Inc)
target_link_libraries(stm32h7xx_hal PUBLIC ChibiOS)