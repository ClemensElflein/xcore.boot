#include "id_eeprom.h"

#include <string.h>

#include "board_ex.h"
#include "ch.h"
#include "hal.h"
static I2CConfig i2cConfig = {0};

void ID_EEPROM_Init() {
#if BOARD_HAS_EEPROM
  // TODO: init eeprom
  i2cAcquireBus(&I2CD4);

  // Calculated depending on clock source, check reference manual
  i2cConfig.timingr = 0xE14;

  if (i2cStart(&I2CD4, &i2cConfig) != HAL_RET_SUCCESS) {
    while (1)
      ;
  }
  i2cReleaseBus(&I2CD4);
#endif
}

bool ID_EEPROM_GetMacAddress(uint8_t *buf, size_t buflen) {
#if BOARD_HAS_EEPROM
  i2cAcquireBus(&I2CD4);

  uint8_t reg = 0xFA;
  bool success = i2cMasterTransmit(&I2CD4, EEPROM_DEVICE_ADDRESS, &reg, 1, buf,
                                   buflen) == MSG_OK;
  i2cReleaseBus(&I2CD4);
  return success;
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
  i2cAcquireBus(&I2CD4);

  uint8_t reg = 0x00;
  bool success = i2cMasterTransmit(&I2CD4, EEPROM_DEVICE_ADDRESS, &reg, 1,
                                   (uint8_t *)buffer,
                                   sizeof(struct bootloader_info)) == MSG_OK;
  i2cReleaseBus(&I2CD4);
  return success;
#else
  // no eeprom - we don't store anything (the board will just try to boot
  // without checking).
  return false;
#endif
  return true;
}

bool ID_EEPROM_SaveBootloaderInfo(struct bootloader_info *buffer) {
#if BOARD_HAS_EEPROM
  i2cAcquireBus(&I2CD4);

  bool success = true;
  // Write single bytes
  for (uint8_t i = 0; success && i < sizeof(struct bootloader_info); i++) {
    uint8_t data[2] = {i, ((uint8_t *)buffer)[i]};
    success &= i2cMasterTransmit(&I2CD4, EEPROM_DEVICE_ADDRESS, data, 2, NULL,
                                 0) == MSG_OK;
    // Wait for write to finish
    uint8_t dummy = 0;
    while (i2cMasterTransmit(&I2CD4, EEPROM_DEVICE_ADDRESS, &dummy, 1, NULL,
                             0) == MSG_RESET) {
      if (i2cGetErrors(&I2CD4) != I2C_ACK_FAILURE) {
        success = false;
        break;
      }
      chThdSleep(1);
    }
  }

  i2cReleaseBus(&I2CD4);
  return success;
#else
  // no eeprom - we don't store anything (the board will just try to boot
  // without checking).
  return false;
#endif
  return true;
}
