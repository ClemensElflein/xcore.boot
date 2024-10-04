#include "id_eeprom.h"

#include "board_ex.h"
#include "ch.h"
#include "hal.h"
static I2CConfig i2cConfig;

void ID_EEPROM_Init() {
#if BOARD_HAS_EEPROM
  // TODO: init eeprom
  while (1)
    ;
#endif
}

bool ID_EEPROM_GetMacAddress(uint8_t *buf, size_t buflen) {
#if BOARD_HAS_EEPROM
  // TODO: Implement
  return false;
#else
#if !defined(BOARD_ETHADDR_0) || !defined(BOARD_ETHADDR_1) || \
    !defined(BOARD_ETHADDR_2) || !defined(BOARD_ETHADDR_3) || \
    !defined(BOARD_ETHADDR_4) || !defined(BOARD_ETHADDR_5)
#error \
    "If the board does not have an EEPROM, the BOARD_ETHADDR_X macros need to be defined."
#endif
  if (buflen < 6) {
    return false;
  }
  buf[0] = BOARD_ETHADDR_0;
  buf[1] = BOARD_ETHADDR_1;
  buf[2] = BOARD_ETHADDR_2;
  buf[3] = BOARD_ETHADDR_3;
  buf[4] = BOARD_ETHADDR_4;
  buf[5] = BOARD_ETHADDR_5;
#endif
  return true;
}

bool ID_EEPROM_GetBootloaderInfo(struct bootloader_info *buffer) {
#if BOARD_HAS_EEPROM
  // TODO: implement
  return false;
#else
  // no eeprom - we don't store anything (the board will just try to boot
  // without checking).
  return false;
#endif
  return true;
}

bool ID_EEPROM_SaveBootloaderInfo(struct bootloader_info *buffer) {
#if BOARD_HAS_EEPROM
  // TODO: implement
  return false;
#else
  // no eeprom - we don't store anything (the board will just try to boot
  // without checking).
  return false;
#endif
  return true;
}

/*//
// Created by clemens on 7/7/24.
//

#include <FastCRC.h>
#include <stm32h7xx_hal.h>
#include <tx_api.h>

#include "board.h"
#include "id_eeprom.h"

I2C_HandleTypeDef hi2c4;
TX_MUTEX eeprom_mutex;
FastCRC32 CRC32;

void Board::ID::Init() {
  tx_mutex_create(&eeprom_mutex, "EEPROM", 0);
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /* USER CODE BEGIN I2C4_MspInit 0 #1#

  /* USER CODE END I2C4_MspInit 0 #1#

  /** Initializes the peripherals clock
   #1#
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_I2C4;
  PeriphClkInitStruct.I2c4ClockSelection = RCC_I2C4CLKSOURCE_D3PCLK1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
    Error_Handler();
  }

  __HAL_RCC_GPIOD_CLK_ENABLE();
  /**I2C4 GPIO Configuration
  PD12     ------> I2C4_SCL
  PD13     ------> I2C4_SDA
  #1#
  GPIO_InitStruct.Pin = GPIO_PIN_12 | GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF4_I2C4;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /* I2C4 clock enable #1#
  __HAL_RCC_I2C4_CLK_ENABLE();
  /* USER CODE BEGIN I2C4_MspInit 1 #1#

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
   #1#
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c4, I2C_ANALOGFILTER_ENABLE) != HAL_OK) {
    Error_Handler();
  }

  /** Configure Digital filter
   #1#
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c4, 0) != HAL_OK) {
    Error_Handler();
  }
}
bool Board::ID::GetMacAddress(void* buf, size_t buflen) {
  if (buflen < 6) return false;
  tx_mutex_get(&eeprom_mutex, TX_WAIT_FOREVER);
  HAL_StatusTypeDef status = HAL_I2C_Mem_Read(
      &hi2c4, eeprom_device_address, 0xFA, I2C_MEMADD_SIZE_8BIT,
      static_cast<uint8_t*>(buf), 6, 1000);
  tx_mutex_put(&eeprom_mutex);
  return status == HAL_OK;
}
bool Board::ID::SaveBootloaderInfo(struct bootloader_info* buffer) {
  tx_mutex_get(&eeprom_mutex, TX_WAIT_FOREVER);
  buffer->crc = CRC32.crc32((uint8_t*)buffer,
                            sizeof(struct bootloader_info) - sizeof(uint32_t));

  HAL_StatusTypeDef status = HAL_I2C_Mem_Write(
      &hi2c4, eeprom_device_address, 0x00, I2C_MEMADD_SIZE_8BIT,
      (uint8_t*)(buffer), sizeof(struct bootloader_info), 1000);
  tx_mutex_put(&eeprom_mutex);
  return status == HAL_OK;
}
bool Board::ID::GetBootloaderInfo(struct bootloader_info* buffer) {
  tx_mutex_get(&eeprom_mutex, TX_WAIT_FOREVER);
  HAL_StatusTypeDef status = HAL_I2C_Mem_Read(
      &hi2c4, eeprom_device_address, 0x00, I2C_MEMADD_SIZE_8BIT,
      (uint8_t*)(buffer), sizeof(struct bootloader_info), 1000);
  uint32_t expected_crc = CRC32.crc32(
      (uint8_t*)buffer, sizeof(struct bootloader_info) - sizeof(uint32_t));
  tx_mutex_put(&eeprom_mutex);
  return status == HAL_OK && expected_crc == buffer->crc;
}*/
