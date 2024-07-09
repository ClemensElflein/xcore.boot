//
// Created by clemens on 7/7/24.
//

#ifndef ID_EEPROM_H
#define ID_EEPROM_H

#include <cstddef>
#include <cstdint>

namespace Board::ID {

constexpr uint16_t eeprom_device_address = 0b10100110;
#pragma pack(push, 1)
struct bootloader_info {
  uint8_t image_present;
  uint32_t image_size;
  uint32_t image_crc32;
  uint32_t crc;
} __attribute__((packed));
#pragma pack(pop)

void Init();
bool GetMacAddress(void* buf, size_t buflen);
bool GetBootloaderInfo(struct bootloader_info* buffer);
bool SaveBootloaderInfo(struct bootloader_info* buffer);
uint32_t GetBoardVersion();
}  // namespace Board::ID

#endif  // ID_EEPROM_H
