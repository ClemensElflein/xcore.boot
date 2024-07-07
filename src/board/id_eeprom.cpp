//
// Created by clemens on 7/7/24.
//

#include "id_eeprom.h"

#include <stm32h7xx_hal.h>

#include "board.h"

I2C_HandleTypeDef hi2c4;

void Board::ID::Init() {
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /* USER CODE BEGIN I2C4_MspInit 0 */

  /* USER CODE END I2C4_MspInit 0 */

  /** Initializes the peripherals clock
   */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_I2C4;
  PeriphClkInitStruct.I2c4ClockSelection = RCC_I2C4CLKSOURCE_D3PCLK1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
    Error_Handler();
  }

  __HAL_RCC_GPIOD_CLK_ENABLE();
  /**I2C4 GPIO Configuration
  PD12     ------> I2C4_SCL
  PD13     ------> I2C4_SDA
  */
  GPIO_InitStruct.Pin = GPIO_PIN_12 | GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF4_I2C4;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /* I2C4 clock enable */
  __HAL_RCC_I2C4_CLK_ENABLE();
  /* USER CODE BEGIN I2C4_MspInit 1 */

  hi2c4.Instance = I2C4;
  hi2c4.Init.Timing = 0x60404E72;
  hi2c4.Init.OwnAddress1 = 0;
  hi2c4.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c4.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c4.Init.OwnAddress2 = 0;
  hi2c4.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c4.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c4.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c4) != HAL_OK) {
    Error_Handler();
  }

  /** Configure Analogue filter
   */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c4, I2C_ANALOGFILTER_ENABLE) != HAL_OK) {
    Error_Handler();
  }

  /** Configure Digital filter
   */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c4, 0) != HAL_OK) {
    Error_Handler();
  }
}
bool Board::ID::GetMacAddress(void* buf, size_t buflen) {
  HAL_StatusTypeDef status = HAL_I2C_Mem_Read(
      &hi2c4, eeprom_device_address, 0xFA, I2C_MEMADD_SIZE_8BIT,
      static_cast<uint8_t*>(buf), 6, 1000);
  return status == HAL_OK;
}
