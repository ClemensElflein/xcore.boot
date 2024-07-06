//
// Created by clemens on 7/7/24.
//

#ifndef BOARD_H
#define BOARD_H

#include "stm32h7xx_hal.h"

namespace Board {
// Initialize board hardware (clocks, etc)
void InitBoardBase();
void InitBoard();

void Error_Handler();
}  // namespace Board

#endif  // BOARD_H
