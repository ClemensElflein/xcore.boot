//
// Created by clemens on 7/6/24.
//

#include "main.h"

#include <tx_api.h>

#include "board/board.h"
#include "board/debug_led.h"
#include "board/id_eeprom.h"
#include "ethernet/ethernet.hpp"
#include "ethernet/nx_ethernet.hpp"

using namespace Board;

uint8_t blink_thread_stack[10000];
TX_THREAD blink_thread;
TX_MUTEX color_mutex;

NX_UDP_SOCKET service_advertisement_socket;
NX_TCP_SOCKET update_socket;
NXD_ADDRESS sd_address;

void blink_thread_entry(uint32_t arg) {
  DebugLed::SetColor(0, 0, 255);
  DebugLed::SetMode(DebugLed::PULSE);

  sd_address.nxd_ip_version = NX_IP_VERSION_V4;
  sd_address.nxd_ip_address.v4 = IP_ADDRESS(233, 255, 255, 1);

  // wait for address
  uint32_t flags = 0;
  tx_event_flags_get(&dhcp_flags, DHCP_FLAGS_HAS_IP, TX_AND, &flags,
                     TX_WAIT_FOREVER);
  if (nx_tcp_socket_create(&ip, &update_socket, "update socket", NX_IP_NORMAL,
                           NX_DONT_FRAGMENT, 0x80, NX_IP_TIME_TO_LIVE, NX_NULL,
                           NX_NULL)) {
    DebugLed::SetColor(255, 0, 0);
    DebugLed::SetMode(DebugLed::BLINK);
    while (1) {
      tx_thread_sleep(100);
    }
  }

  if (nx_udp_socket_create(&ip, &service_advertisement_socket, "sd socket",
                           NX_IP_NORMAL, NX_DONT_FRAGMENT, 0x80,
                           5) != NX_SUCCESS) {
    DebugLed::SetColor(255, 0, 0);
    DebugLed::SetMode(DebugLed::BLINK);
    while (1) {
      tx_thread_sleep(100);
    }
  }
  if (nx_udp_socket_bind(&service_advertisement_socket, 0, TX_WAIT_FOREVER) !=
      NX_SUCCESS) {
    DebugLed::SetColor(255, 0, 0);
    DebugLed::SetMode(DebugLed::BLINK);
    while (1) {
      tx_thread_sleep(100);
    }
  }
  uint8_t addr[6] = {};

  bool test = Board::ID::GetMacAddress(addr, sizeof(addr));

  if (!test) {
    DebugLed::SetColor(255, 0, 0);
    DebugLed::SetMode(DebugLed::BLINK);
    while (1) {
      tx_thread_sleep(100);
    }
  }

  int error = 0;
  int success = 0;
  while (1) {
    DebugLed::SetColor(0, 255, 0);
    DebugLed::SetMode(DebugLed::PULSE_SLOW);
    tx_thread_sleep(1 * TX_TIMER_TICKS_PER_SECOND);
    NX_PACKET *packet = nullptr;
    if (nx_packet_allocate(&ethernet_pool, &packet, NX_UDP_PACKET,
                           TX_WAIT_FOREVER) == NX_SUCCESS) {
      auto data = "YOOOOOOOO";
      nx_packet_data_append(packet, (void *)data, sizeof(data), &ethernet_pool,
                            TX_WAIT_FOREVER);

      if (nxd_udp_socket_send(&service_advertisement_socket, packet,
                              &sd_address, 4242) != NX_SUCCESS) {
        nx_packet_release(packet);
      }
    }

    packet = nullptr;

    if (nx_tcp_server_socket_listen(&ip, 4242, &update_socket, 5, NX_NULL) !=
        NX_SUCCESS) {
    }

    UINT status =
        nx_tcp_server_socket_accept(&update_socket, TX_TIMER_TICKS_PER_SECOND);

    if (status == NX_SUCCESS) {
      success = 0;
      // Yellow
      DebugLed::SetColor(252.0f / 255.0f, 186.0f / 255.0f, 3.0f / 255.0f);
      DebugLed::SetMode(DebugLed::PULSE_SLOW);
      while (status == NX_SUCCESS) {
        // Got connect, read packets
        status =
            nx_tcp_socket_receive(&update_socket, &packet, NX_WAIT_FOREVER);
        if (status == NX_SUCCESS) {
          nx_packet_release(packet);
          success++;
          if (success > 100) {
            auto data = "YOOOOOOOO";
            nx_packet_data_append(packet, (void *)data, sizeof(data),
                                  &ethernet_pool, TX_WAIT_FOREVER);
            nx_tcp_socket_send(&update_socket, packet,
                               TX_TIMER_TICKS_PER_SECOND);
            break;
          }
        } else {
          error++;
        }
      }

      nx_tcp_socket_disconnect(&update_socket, 10);
      nx_tcp_server_socket_unaccept(&update_socket);
      nx_tcp_server_socket_relisten(&ip, 4242, &update_socket);
    }
  }
}

void tx_application_define(void *) {
  // Init board (DebugLED, Ethernet, ...)
  InitBoard();

  // Need to enable->disable because ethernet driver calls DCache functions
  // which hard fault, if we don't do this on boot.
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
