//
// Created by clemens on 7/6/24.
//

#ifndef INTERRUPT_H
#define INTERRUPT_H

#ifdef __cplusplus
extern "C" {
#endif

void TIM17_IRQHandler(void);
void HardFault_Handler();
#ifdef __cplusplus
}
#endif
#endif  // INTERRUPT_H
