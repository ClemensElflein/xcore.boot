//
// Created by clemens on 7/8/24.
//

#include "service_discovery.h"

#include <nx_api.h>
#include <tx_api.h>

#include "ethernet/ethernet.hpp"
#include "ethernet/nx_ethernet.hpp"

NX_UDP_SOCKET service_advertisement_socket;
uint8_t sd_thread_stack[2048];
TX_THREAD sd_thread;

NX_PACKET *sd_packet;
NXD_ADDRESS sd_address;

void sd_thread_entry(ULONG arg);

bool InitServiceDiscovery() {
  sd_address.nxd_ip_version = NX_IP_VERSION_V4;
  sd_address.nxd_ip_address.v4 = IP_ADDRESS(233, 255, 255, 1);

  if (nx_udp_socket_create(&ip, &service_advertisement_socket, "sd socket",
                           NX_IP_NORMAL, NX_DONT_FRAGMENT, 0x80,
                           5) != NX_SUCCESS) {
    return false;
  }

  if (nx_udp_socket_bind(&service_advertisement_socket, 0, TX_WAIT_FOREVER) !=
      NX_SUCCESS) {
    return false;
  }

  if (tx_thread_create(&sd_thread, "ServiceDiscovery", sd_thread_entry, 0,
                       sd_thread_stack, sizeof(sd_thread_stack),
                       TX_MAX_PRIORITIES - 1, TX_MAX_PRIORITIES - 2,
                       TX_NO_TIME_SLICE, TX_AUTO_START) != TX_SUCCESS) {
    return false;
  }
  return true;
}

void sd_thread_entry(ULONG arg) {
  while (1) {
    // Create the service discovery packet
    if (nx_packet_allocate(&ethernet_pool, &sd_packet, NX_UDP_PACKET,
                           TX_WAIT_FOREVER) != NX_SUCCESS) {
      tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND);
      continue;
    }
    auto data = "YOOOOOOOO";
    nx_packet_data_append(sd_packet, (void *)data, sizeof(data), &ethernet_pool,
                          TX_WAIT_FOREVER);
    if (nxd_udp_socket_send(&service_advertisement_socket, sd_packet,
                            &sd_address, 4242) != NX_SUCCESS) {
      nx_packet_release(sd_packet);
    }
    tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND);
  }
}
