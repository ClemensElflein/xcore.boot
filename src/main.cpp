//
// Created by clemens on 7/6/24.
//

#include "main.h"

#include <tx_api.h>

#include "board/board.h"
#include "board/debug_led.h"
#include "board/id_eeprom.h"
#include "ethernet/nx_ethernet.hpp"

using namespace Board;

uint8_t blink_thread_stack[4096];
TX_THREAD blink_thread;
TX_MUTEX color_mutex;

void blink_thread_entry(uint32_t arg) {
  DebugLed::SetColor(0, 0, 255);
  DebugLed::SetMode(DebugLed::PULSE);

  uint8_t addr[6] = {};

  bool test = Board::ID::GetMacAddress(addr, sizeof(addr));

  if (!test) {
    DebugLed::SetColor(255, 0, 0);
    DebugLed::SetMode(DebugLed::BLINK);
    while (1) {
      tx_thread_sleep(TX_WAIT_FOREVER);
    }
  }
  DebugLed::SetColor(0, 255, 0);
  while (1) {
    tx_thread_sleep(TX_WAIT_FOREVER);
  }
}

void tx_application_define(void *) {
  // Init board (DebugLED, Ethernet, ...)
  InitBoard();

  SCB_EnableDCache();
  SCB_DisableDCache();
  nx_ethernet_init();

  tx_thread_create(&blink_thread, "Blink", blink_thread_entry, 0,
                   blink_thread_stack, sizeof(blink_thread_stack),
                   TX_MAX_PRIORITIES - 1, TX_MAX_PRIORITIES - 2,
                   TX_NO_TIME_SLICE, TX_AUTO_START);
}

int main() {
  // Init basics (HAL, sysclock)
  InitBoardBase();
  // Start ThreadX, so that advanced initializers can use ThreadX services
  tx_kernel_enter();
}
