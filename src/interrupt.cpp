//
// Created by clemens on 7/6/24.
//

#include "interrupt.h"

#include <stm32h7xx_hal.h>
extern TIM_HandleTypeDef htim17;
void TIM17_IRQHandler(void) {
  /* USER CODE BEGIN TIM17_IRQn 0 */

  /* USER CODE END TIM17_IRQn 0 */
  HAL_TIM_IRQHandler(&htim17);
  /* USER CODE BEGIN TIM17_IRQn 1 */

  /* USER CODE END TIM17_IRQn 1 */
}
