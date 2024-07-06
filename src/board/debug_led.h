//
// Created by clemens on 7/6/24.
//

#ifndef DEBUG_LED_H
#define DEBUG_LED_H

namespace Board {
namespace DebugLed {

enum MODE { BLINK, PULSE };

void InitHw();

void SetColor(float r, float g, float b);
void SetMode(MODE m);

}  // namespace DebugLed
}  // namespace Board

#endif  // DEBUG_LED_H
