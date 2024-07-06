//
// Created by clemens on 7/6/24.
//

#include "main.h"

#include "board/board.h"
#include "board/debug_led.h"

using namespace Board;

int main() {
  HAL_Init();
  InitBoard();

  float r = 0;

  while (1) {
    while (r < 1.0) {
      HAL_Delay(10);
      r += 0.02;
      DebugLed::SetColor(r, 0, 0);
    }
    while (r > 0) {
      HAL_Delay(10);
      r -= 0.02;
      DebugLed::SetColor(r, 0, 0);
    }
    while (r < 1.0) {
      HAL_Delay(10);
      r += 0.02;
      DebugLed::SetColor(0, r, 0);
    }
    while (r > 0) {
      HAL_Delay(10);
      r -= 0.02;
      DebugLed::SetColor(0, r, 0);
    }
    while (r < 1.0) {
      HAL_Delay(10);
      r += 0.02;
      DebugLed::SetColor(0, 0, r);
    }
    while (r > 0) {
      HAL_Delay(10);
      r -= 0.02;
      DebugLed::SetColor(0, 0, r);
    }
  }
}
