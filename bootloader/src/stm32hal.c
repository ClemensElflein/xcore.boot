#include "ch.h"
#include "stm32h7xx_hal.h"

// Implements the GetTick method using Chibi's system time.
// This is needed for STM32HAL to calculate timeouts.
uint32_t HAL_GetTick(void) { return TIME_I2MS(chVTGetSystemTime()); }
