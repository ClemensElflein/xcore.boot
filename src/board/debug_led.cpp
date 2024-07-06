//
// Created by clemens on 7/6/24.
//

#include "debug_led.h"

#include <tx_api.h>

#include "board.h"

TIM_HandleTypeDef tim_led1_pwm;

namespace Board::DebugLed {

uint8_t blink_thread_stack[512];
TX_THREAD blink_thread;
TX_MUTEX color_mutex;

float current_r, current_g, current_b;

MODE mode = BLINK;

void blink_thread_entry(uint32_t arg) {
  float r, g, b;
  MODE m;
  while (true) {
    tx_mutex_get(&color_mutex, TX_WAIT_FOREVER);
    r = current_r;
    g = current_g;
    b = current_b;
    m = mode;
    tx_mutex_put(&color_mutex);

    switch (m) {
      case PULSE:
        for (float scale = 0.0f; scale < 1.0f; scale += 0.02) {
          tim_led1_pwm.Instance->CCR1 =
              static_cast<uint16_t>(powf(r, 2.2) * 65535.0f * scale);
          tim_led1_pwm.Instance->CCR2 =
              static_cast<uint16_t>(powf(g, 2.2) * 65535.0f * scale);
          tim_led1_pwm.Instance->CCR3 =
              static_cast<uint16_t>(powf(b, 2.2) * 65535.0f * scale);
          tx_thread_sleep(1);
        }
        for (float scale = 1.0f; scale > 0.0f; scale -= 0.02) {
          tim_led1_pwm.Instance->CCR1 =
              static_cast<uint16_t>(powf(r, 2.2) * 65535.0f * scale);
          tim_led1_pwm.Instance->CCR2 =
              static_cast<uint16_t>(powf(g, 2.2) * 65535.0f * scale);
          tim_led1_pwm.Instance->CCR3 =
              static_cast<uint16_t>(powf(b, 2.2) * 65535.0f * scale);
          tx_thread_sleep(1);
        }

        break;
      default:;
      case BLINK:
        tim_led1_pwm.Instance->CCR1 =
            static_cast<uint16_t>(powf(r, 2.2) * 65535.0f);
        tim_led1_pwm.Instance->CCR2 =
            static_cast<uint16_t>(powf(g, 2.2) * 65535.0f);
        tim_led1_pwm.Instance->CCR3 =
            static_cast<uint16_t>(powf(b, 2.2) * 65535.0f);
        tx_thread_sleep(10);
        tim_led1_pwm.Instance->CCR1 = 0;
        tim_led1_pwm.Instance->CCR2 = 0;
        tim_led1_pwm.Instance->CCR3 = 0;
        tx_thread_sleep(10);
        break;
    }
  }
}
void InitHw() {
  __HAL_RCC_GPIOF_CLK_ENABLE();

  __HAL_RCC_TIM23_CLK_ENABLE();
  TIM_ClockConfigTypeDef sClockSourceConfig = {};
  TIM_MasterConfigTypeDef sMasterConfig = {};
  TIM_OC_InitTypeDef sConfigOC = {};

  tim_led1_pwm.Instance = TIM23;
  tim_led1_pwm.Init.Prescaler = 0;
  tim_led1_pwm.Init.CounterMode = TIM_COUNTERMODE_UP;
  tim_led1_pwm.Init.Period = 0xFFFF;
  tim_led1_pwm.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  tim_led1_pwm.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&tim_led1_pwm) != HAL_OK) {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&tim_led1_pwm, &sClockSourceConfig) != HAL_OK) {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&tim_led1_pwm) != HAL_OK) {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&tim_led1_pwm, &sMasterConfig) !=
      HAL_OK) {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_LOW;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&tim_led1_pwm, &sConfigOC, TIM_CHANNEL_1) !=
      HAL_OK) {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&tim_led1_pwm, &sConfigOC, TIM_CHANNEL_2) !=
      HAL_OK) {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&tim_led1_pwm, &sConfigOC, TIM_CHANNEL_3) !=
      HAL_OK) {
    Error_Handler();
  }
  {
    GPIO_InitTypeDef GPIO_InitStruct = {};
    __HAL_RCC_GPIOF_CLK_ENABLE();
    /**TIM23 GPIO Configuration
    PF0     ------> TIM23_CH1
    PF1     ------> TIM23_CH2
    PF2     ------> TIM23_CH3
    */
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF13_TIM23;

    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);
  }

  HAL_TIM_PWM_Start(&tim_led1_pwm, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&tim_led1_pwm, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&tim_led1_pwm, TIM_CHANNEL_3);

  current_r = current_g = current_b = 0.0f;

  tx_mutex_create(&color_mutex, "DebugLed Mutex", 0);

  tx_thread_create(&blink_thread, "Blink", blink_thread_entry, 0,
                   blink_thread_stack, sizeof(blink_thread_stack), 8, 8,
                   TX_NO_TIME_SLICE, TX_AUTO_START);
}

void SetColor(float r, float g, float b) {
  tx_mutex_get(&color_mutex, TX_WAIT_FOREVER);
  current_r = fmaxf(0.0, fminf(1.0, r));
  current_g = fmaxf(0.0, fminf(1.0, g));
  current_b = fmaxf(0.0, fminf(1.0, b));
  tx_mutex_put(&color_mutex);
}

void SetMode(MODE m) {
  tx_mutex_get(&color_mutex, TX_WAIT_FOREVER);
  mode = m;
  tx_mutex_put(&color_mutex);
}
}  // namespace Board::DebugLed
