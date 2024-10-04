//
// Created by clemens on 7/7/24.
//

#ifndef ID_EEPROM_H
#define ID_EEPROM_H

#include "stdbool.h"
#include "stddef.h"
#include "stdint.h"

#define ID_EEPROM_DEVICE_ID 0b10100110

#pragma pack(push, 1)
struct bootloader_info {
  uint8_t image_present;
  uint32_t image_size;
  uint32_t image_sha256[8];
  uint32_t crc;
} __attribute__((packed));
#pragma pack(pop)

void ID_EEPROM_Init();
bool ID_EEPROM_GetMacAddress(uint8_t* buf, size_t buflen);
bool ID_EEPROM_GetBootloaderInfo(struct bootloader_info* buffer);
bool ID_EEPROM_SaveBootloaderInfo(struct bootloader_info* buffer);

#endif  // ID_EEPROM_H
