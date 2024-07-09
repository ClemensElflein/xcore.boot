//
// Created by clemens on 7/8/24.
//

#include "bootloader.h"

#include <nx_api.h>
#include <tx_api.h>
#include <tx_port.h>

#include <cstdint>

#include "board/board.h"
#include "board/debug_led.h"
#include "board/id_eeprom.h"
#include "ethernet/ethernet.hpp"
#include "ethernet/nx_ethernet.hpp"

// 256bits need to be written at a single time and the buffer needs to be
// aligned, so we first copy our bytes into this tmp buffer and then let the
// hardware write to flash.
uint8_t x[32] __attribute__((aligned(32))) = {};
using namespace Board;
NX_TCP_SOCKET update_socket;

uint8_t bootloader_thread_stack[10000];
TX_THREAD bootloader_thread;
FLASH_EraseInitTypeDef EraseInitStruct;
void BootloaderThreadEntry(ULONG arg);

bool InitBootloader() {
  if (nx_tcp_socket_create(&ip, &update_socket, "update socket", NX_IP_NORMAL,
                           NX_DONT_FRAGMENT, 0x80, NX_IP_TIME_TO_LIVE, NX_NULL,
                           NX_NULL)) {
    return false;
  }
  // Check, that the tmp address is 32 byte aligned, otherwise write will fail.
  if (reinterpret_cast<uint32_t>(&x) % 32 != 0) {
    return false;
  }

  tx_thread_create(&bootloader_thread, "Bootloader", BootloaderThreadEntry, 0,
                   bootloader_thread_stack, sizeof(bootloader_thread_stack),
                   TX_MAX_PRIORITIES - 1, TX_MAX_PRIORITIES - 2,
                   TX_NO_TIME_SLICE, TX_AUTO_START);

  return true;
}

void BootloaderThreadEntry(ULONG arg) {
  int error = 0;
  int success = 0;
  int i = 0;
  while (1) {
    DebugLed::SetColor(0, 255, 0);
    DebugLed::SetMode(DebugLed::PULSE_SLOW);
    tx_thread_sleep(1 * TX_TIMER_TICKS_PER_SECOND);
    NX_PACKET* packet = nullptr;

    if (nx_tcp_server_socket_listen(&ip, 4242, &update_socket, 5, NX_NULL) !=
        NX_SUCCESS) {
    }

    UINT status =
        nx_tcp_server_socket_accept(&update_socket, TX_TIMER_TICKS_PER_SECOND);

    if (status == NX_SUCCESS) {
      i = 0;
      success = 0;
      // Yellow
      DebugLed::SetColor(252.0f / 255.0f, 186.0f / 255.0f, 3.0f / 255.0f);
      DebugLed::SetMode(DebugLed::PULSE_SLOW);

      {
        // Before doing anything, we mark the image as invalid
        // TODO: think about if that is really necessary, since we will do
        // checksum comparison anyways.
        ID::bootloader_info info{};
        info.image_present = 0;
        if (!ID::SaveBootloaderInfo(&info)) {
          DebugLed::SetColor(255, 0, 0);
          DebugLed::SetMode(DebugLed::BLINK);
          while (1) {
            tx_thread_sleep(TX_WAIT_FOREVER);
          }
        }
      }

      HAL_FLASH_Unlock();
      /* Get the 1st sector to erase */
      // FirstSector = GetSector(FLASH_USER_START_ADDR);
      /* Get the number of sector to erase from 1st sector*/
      // NbOfSectors = GetSector(FLASH_USER_END_ADDR) - FirstSector + 1;

      // TODO: only erase the size we actually need
      EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
      EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
      EraseInitStruct.Banks = FLASH_BANK_1;
      EraseInitStruct.Sector = FLASH_SECTOR_1;
      EraseInitStruct.NbSectors = 6;
      uint32_t e = 0;
      HAL_FLASHEx_Erase(&EraseInitStruct, &e);

      uint32_t flash_address = 0x8020000;
      uint8_t byte_offset = 0;
      uint32_t success_count = 0;
      uint32_t error_count = 0;
      while (status == NX_SUCCESS) {
        // Got connect, read packets
        status =
            nx_tcp_socket_receive(&update_socket, &packet, NX_WAIT_FOREVER);
        if (status == NX_SUCCESS) {
          for (uint8_t* ptr = packet->nx_packet_prepend_ptr;
               ptr < packet->nx_packet_append_ptr; ptr++) {
            x[byte_offset] = *ptr;
            byte_offset++;
            if (byte_offset == 32) {
              if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, flash_address,
                                    reinterpret_cast<uint32_t>(&x)) == HAL_OK) {
                success_count++;
              } else {
                error_count++;
              }
              flash_address += 32;
              byte_offset = 0;
            }
          }

          nx_packet_release(packet);
          success++;
        } else {
          if (byte_offset != 0) {
            // write in progress, fill with 0 and finish the write
            while (byte_offset != 32) {
              x[byte_offset++] = 0;
            }
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, flash_address,
                              reinterpret_cast<uint32_t>(&x));
          }

          error++;
        }
      }

      HAL_FLASH_Lock();

      nx_tcp_socket_disconnect(&update_socket, 10);
      nx_tcp_server_socket_unaccept(&update_socket);
      nx_tcp_server_socket_relisten(&ip, 4242, &update_socket);

      {
        // Write done, store image info
        ID::bootloader_info info{};
        info.image_present = 1;
        if (!ID::SaveBootloaderInfo(&info)) {
          DebugLed::SetColor(255, 0, 0);
          DebugLed::SetMode(DebugLed::BLINK);
          while (1) {
            tx_thread_sleep(TX_WAIT_FOREVER);
          }
        }
      }

      // Reset to bootloader, bootloader will verify the image before actually
      // running it
      NVIC_SystemReset();
    } else {
      if (i++ == 5) {
        // Check, if we have a valid image. If so, run it.
        ID::bootloader_info info;
        if (ID::GetBootloaderInfo(&info)) {
          if (info.image_present) {
            MODIFY_REG(SYSCFG->UR2, SYSCFG_UR2_BOOT_ADD0,
                       (0x8020000 >> 16) << SYSCFG_UR2_BOOT_ADD0_Pos);
            NVIC_SystemReset();
          }
        }
      }
    }
  }
}
