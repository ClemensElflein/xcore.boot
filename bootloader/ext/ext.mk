# List of all the board related files.
STM32_HAL_SRC = ./ext/stm32h7xx_hal_driver/Src/stm32h7xx_hal_flash.c \
                ./ext/stm32h7xx_hal_driver/Src/stm32h7xx_hal_flash_ex.c

# Required include directories
STM32_HAL_INC = ./ext/stm32h7xx_hal_driver/Inc

SHA_SRC = ./ext/sha/sha256.c
SHA_INC = ./ext/sha


# Shared variables
ALLCSRC += $(STM32_HAL_SRC) $(SHA_SRC)
ALLINC  += $(STM32_HAL_INC) $(SHA_INC)
