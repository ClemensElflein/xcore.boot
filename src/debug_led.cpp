//
// Created by clemens on 7/6/24.
//

#include "debug_led.h"

#include "hw.h"

TIM_HandleTypeDef tim_led1_pwm;

void DebugLed::InitHw() {
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
}

void DebugLed::SetColor(float r, float g, float b) {
  r = pow(fmaxf(0.0, fminf(1.0, r)), 2.2);
  g = pow(fmaxf(0.0, fminf(1.0, g)), 2.2);
  b = pow(fmaxf(0.0, fminf(1.0, b)), 2.2);

  tim_led1_pwm.Instance->CCR1 = static_cast<uint16_t>(r * 65535.0f);
  tim_led1_pwm.Instance->CCR2 = static_cast<uint16_t>(g * 65535.0f);
  tim_led1_pwm.Instance->CCR3 = static_cast<uint16_t>(b * 65535.0f);
}
