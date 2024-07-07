//
// Created by clemens on 7/7/24.
//

#ifndef ID_EEPROM_H
#define ID_EEPROM_H

#include <cstddef>
#include <cstdint>

namespace Board::ID {

constexpr uint16_t eeprom_device_address = 0b10100110;

void Init();
bool GetMacAddress(void* buf, size_t buflen);
uint32_t GetBoardVersion();
}  // namespace Board::ID

#endif  // ID_EEPROM_H
