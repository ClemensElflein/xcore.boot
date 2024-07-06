//
// Created by clemens on 7/6/24.
//

#ifndef DEBUG_LED_H
#define DEBUG_LED_H

namespace Board::DebugLed {
void InitHw();

void SetColor(float r, float g, float b);
}  // namespace Board::DebugLed

#endif  // DEBUG_LED_H
