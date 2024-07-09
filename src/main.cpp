//
// Created by clemens on 7/6/24.
//

#include "main.h"

#include <tx_api.h>

#include "board/board.h"
#include "board/debug_led.h"
#include "board/id_eeprom.h"
#include "bootloader.h"
#include "ethernet/ethernet.hpp"
#include "ethernet/nx_ethernet.hpp"
#include "service_discovery.h"

using namespace Board;

TX_MUTEX color_mutex;

void blink_thread_entry(uint32_t arg) {
  // wait for address
  uint32_t flags = 0;
  // tx_event_flags_get(&dhcp_flags, DHCP_FLAGS_HAS_IP, TX_AND, &flags,
  //                    TX_WAIT_FOREVER);

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
  // tx_thread_create(&blink_thread, "Blink", blink_thread_entry, 0,
  //                  blink_thread_stack, sizeof(blink_thread_stack),
  //                  TX_MAX_PRIORITIES - 1, TX_MAX_PRIORITIES - 2,
  //                  TX_NO_TIME_SLICE, TX_AUTO_START);
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
