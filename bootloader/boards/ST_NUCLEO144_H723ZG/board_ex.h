#ifndef BOARD_EX_H
#define BOARD_EX_H

#define BOARD_HAS_RGB_STATUS 0
#define BOARD_HAS_RGB_HEARTBEAT 0

#define BOARD_HAS_EEPROM 0

// Default mac address (same as the one provided in lwip)
#define BOARD_ETHADDR_0 0xC2
#define BOARD_ETHADDR_1 0xAF
#define BOARD_ETHADDR_2 0x51
#define BOARD_ETHADDR_3 0x03
#define BOARD_ETHADDR_4 0xCF
#define BOARD_ETHADDR_5 0x46

#define BOARD_NAME "Nucleo144-H723ZG"
#define BOARD_VERSION_MAJOR 1
#define BOARD_VERSION_MINOR 0

// Define the fallback IP settings for this board (if DHCP fails)
// 10.0.0.254
#define FALLBACK_IP_ADDRESS 0x0A0000FE
// 10.0.0.1
#define FALLBACK_GATEWAY 0x0A000001
// 255.255.255.0
#define FALLBACK_NETMASK 0xFFFFFF00


// Flash information for the bootloader
#define BOOT_ADDRESS 0x8020000
// Available flash pages for user program
#define FLASH_PAGE_COUNT 7
// Size of each flash page in bytes
#define FLASH_PAGE_SIZE_BYTES 0x20000
// Size of flash memory for the user program
#define PROGRAM_FLASH_SIZE_BYTES (FLASH_PAGE_COUNT * FLASH_PAGE_SIZE_BYTES)


#endif