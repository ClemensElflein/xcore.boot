//
// Created by clemens on 7/6/24.
//

#include "main.h"

#include <fx_api.h>
#include <nxd_http_server.h>
#include <tx_api.h>

#include "board/board.h"
#include "board/debug_led.h"
#include "board/id_eeprom.h"
#include "bootloader.h"
#include "ethernet/ethernet.hpp"
#include "ethernet/nx_ethernet.hpp"
#include "filex/ram_driver.h"
#include "service_discovery.h"

using namespace Board;

TX_MUTEX color_mutex;
TX_THREAD blink_thread;
uint8_t blink_thread_stack[2048];
FX_MEDIA ram_disk;
unsigned char media_memory[512];
NX_HTTP_SERVER http_server;
uint8_t http_thread_stack[2048 * 16];

void blink_thread_entry(uint32_t arg) {
  fx_media_open(&ram_disk, "RAM DISK", fx_ram_driver,
                _home_clemens_Dev_xcore_xcore_boot_image_image_dat,
                media_memory, sizeof(media_memory));
  nx_http_server_create(&http_server, "test", &ip, &ram_disk, http_thread_stack,
                        sizeof(http_thread_stack), &ethernet_pool, NX_NULL,
                        NX_NULL);
  nx_http_server_start(&http_server);

  uint8_t addr[6] = {};

  bool test = Board::ID::GetMacAddress(addr, sizeof(addr));

  if (!test) {
    DebugLed::SetColor(255, 0, 0);
    DebugLed::SetMode(DebugLed::BLINK);
    while (1) {
      tx_thread_sleep(100);
    }
  }
}

void tx_application_define(void *) {
  // Init board (DebugLED, Ethernet, ...)
  InitBoard();
  DebugLed::SetColor(0, 0, 255);
  DebugLed::SetMode(DebugLed::PULSE);

  nx_ethernet_init();

  fx_system_initialize();

  if (!InitServiceDiscovery()) {
    DebugLed::SetColor(255, 0, 0);
    DebugLed::SetMode(DebugLed::BLINK);
    return;
  }

  if (!InitBootloader()) {
    DebugLed::SetColor(255, 0, 0);
    DebugLed::SetMode(DebugLed::BLINK);
    return;
  }

  //
  //
  //
  tx_thread_create(&blink_thread, "Blink", blink_thread_entry, 0,
                   blink_thread_stack, sizeof(blink_thread_stack),
                   TX_MAX_PRIORITIES - 1, TX_MAX_PRIORITIES - 2,
                   TX_NO_TIME_SLICE, TX_AUTO_START);
}

int main() {
  // Init basics (HAL, sysclock)
  InitBoardBase();
  // Need to enable->disable because ethernet driver calls DCache functions
  // which hard fault, if we don't do this on boot.
  SCB_EnableDCache();
  SCB_DisableDCache();

  // Start ThreadX, so that advanced initializers can use ThreadX services
  tx_kernel_enter();
  // we should never get here.
  while (1)
    ;
}
