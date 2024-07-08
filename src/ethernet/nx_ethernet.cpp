//
// Created by clemens on 5/7/24.
//

#include "nx_ethernet.hpp"

#include <nx_api.h>
#include <nxd_dhcp_client.h>

#include "ethernet.hpp"

/**
 * The pool storage for incoming ethernet packets and the NX_PACKET_POOL to
 * manage them.
 */
uint8_t RXBUF[pool_size] __attribute__((section(".EthPoolSection"))){};
NX_PACKET_POOL ethernet_pool;

/**
 * IP instance and the stack pointer for the IP thread
 */
uint8_t ip_thread_stack[1024];
NX_IP ip{};

// ARP cache
uint8_t arp_cache[1024];

/**
 * ethernet_thread is used to start the DHCP process
 */
TX_THREAD ethernet_thread;
uint8_t ethernet_thread_stack[512];

NX_DHCP dhcp __attribute__((section(".EthPoolSection"))){};

/**
 * Tracks which events were seen in the ISR. The driver thread is then called to
 * process the events This is to make the ISR as short as possible
 */
constexpr uint32_t DEFERRED_ISR_TX = 1;
constexpr uint32_t DEFERRED_ISR_RX = 2;
volatile uint32_t deferred_isr = 0;

// Forward definition of driver entry point
VOID nx_stm32_eth_driver(NX_IP_DRIVER *driver_req_ptr);

bool TxPacket(NX_PACKET *packet);

void ProcessIsr();

static void get_packet(void **buffer, void **user_ptr) {
  NX_PACKET *packet;

  if (nx_packet_allocate(&ethernet_pool, &packet, NX_RECEIVE_PACKET,
                         NX_NO_WAIT) == NX_SUCCESS) {
    // Move the prepend_ptr to ensure correct alignment after reception
    // (NetX expects the packet to start at 32 bit aligned, but eth header is 14
    // bytes, so we add 2 bytes)
    packet->nx_packet_prepend_ptr += 2;
    *buffer = packet->nx_packet_prepend_ptr;
    *user_ptr = packet;
    SCB_CleanDCache_by_Addr((uint32_t *)(packet), sizeof(NX_PACKET));
  } else {
    *buffer = nullptr;
    *user_ptr = nullptr;
  }
}

static void transmit_free_packet(void *buff, void *user_ptr) {
  auto pkt = static_cast<NX_PACKET *>(user_ptr);
  nx_packet_transmit_release(pkt);
}

static void received_callback(void *buff, void *user_ptr, size_t size,
                              uint32_t rx_flags) {
  if (!user_ptr) return;

  auto packet = static_cast<NX_PACKET *>(user_ptr);

  // We don't support packet chains.
  if (rx_flags != (RX_FLAG_FIRST | RX_FLAG_LAST)) {
    nx_packet_release(packet);
    return;
  }

  // For the first packet, we need to have at least 14 bytes for the header
  // also we don't accept packets with CRC error.
  if (((rx_flags & RX_FLAG_FIRST) && size < 18) ||
      (rx_flags & RX_FLAG_CRC_ERROR)) {
    nx_packet_release(packet);
    return;
  }

  // Skip ethernet header
  packet->nx_packet_prepend_ptr += 14;
  // Remove Ethernet header and CRC
  packet->nx_packet_length = size - 18;
  // Set the append pointer
  packet->nx_packet_append_ptr =
      packet->nx_packet_prepend_ptr + packet->nx_packet_length;

  // Last, send it out.
  // Get the ethertype
  uint16_t type =
      *reinterpret_cast<uint16_t *>(packet->nx_packet_prepend_ptr - 2);

  switch (type) {
    case 0x0608:
      // arp only has deferred API
      _nx_arp_packet_deferred_receive(&ip, packet);
      break;
    case 0x0008:
      // don't use deferred, since we're already in ip thread here.
      _nx_ip_packet_receive(&ip, packet);
      break;
    default:
      nx_packet_release(packet);
      break;
  }
}

bool TxPacket(NX_PACKET *packet) {
#ifndef NX_DISABLE_PACKET_CHAIN
  // We only support single packets, no chains
  if (packet->nx_packet_next) {
    nx_packet_release(packet);
    return false;
  }
#endif

  // Need to flush, since we will invalidate on read
  SCB_CleanDCache_by_Addr((uint32_t *)(packet), sizeof(NX_PACKET));
  /* Process packet send requests.  */
  if (!TxPacket(packet->nx_packet_prepend_ptr, packet,
                packet->nx_packet_append_ptr - packet->nx_packet_prepend_ptr,
                RX_FLAG_FIRST | RX_FLAG_LAST)) {
    nx_packet_release(packet);
    return false;
  }
  return true;
}

void ethernet_thread_entry(ULONG input) {
  nx_dhcp_create(&dhcp, &ip, "nx");

  nx_dhcp_start(&dhcp);

  while (1) {
    UpdatePhyState();
    tx_thread_sleep(10 * TX_TIMER_TICKS_PER_SECOND);
  }
}

void nx_ethernet_init() {
  nx_system_initialize();
  // +14 for the ethernet header
  nx_packet_pool_create(&ethernet_pool, "NetXDuo App Pool",
                        max_payload_size + 14, RXBUF, sizeof(RXBUF));

  nx_ip_create(&ip, "NetX Ip instance", 0, 0, &ethernet_pool,
               nx_stm32_eth_driver, ip_thread_stack, sizeof(ip_thread_stack),
               9);
  nx_arp_enable(&ip, arp_cache, sizeof(arp_cache));
  nx_icmp_enable(&ip);
  nx_udp_enable(&ip);

  tx_thread_create(&ethernet_thread, "NetXDuo thread", ethernet_thread_entry, 0,
                   ethernet_thread_stack, sizeof(ethernet_thread_stack), 10, 10,
                   TX_NO_TIME_SLICE, TX_AUTO_START);
}

VOID nx_stm32_eth_driver(NX_IP_DRIVER *driver_req_ptr) {
  // TODO: check, if the packet is split accross packets

  driver_req_ptr->nx_ip_driver_status = NX_SUCCESS;
  switch (driver_req_ptr->nx_ip_driver_command) {
    case NX_LINK_INTERFACE_ATTACH:
      return;
    case NX_LINK_INITIALIZE: {
      InitEthernet(get_packet, transmit_free_packet, received_callback,
                   max_payload_size);
      // A place for the ISR to store the events we need to process.
      deferred_isr = 0;
      nx_ip_interface_address_mapping_configure(
          driver_req_ptr->nx_ip_driver_ptr,
          driver_req_ptr->nx_ip_driver_interface->nx_interface_index, NX_TRUE);
      nx_ip_interface_physical_address_set(
          driver_req_ptr->nx_ip_driver_ptr,
          driver_req_ptr->nx_ip_driver_interface->nx_interface_index,
          htons(ETH->MACA0LR),
          htons(ETH->MACA0LR >> 16) << 16 | htons(ETH->MACA0HR), NX_FALSE);
      nx_ip_interface_capability_set(
          driver_req_ptr->nx_ip_driver_ptr,
          driver_req_ptr->nx_ip_driver_interface->nx_interface_index,
          NX_INTERFACE_CAPABILITY_IPV4_TX_CHECKSUM |
              NX_INTERFACE_CAPABILITY_IPV4_RX_CHECKSUM |
              NX_INTERFACE_CAPABILITY_TCP_TX_CHECKSUM |
              NX_INTERFACE_CAPABILITY_TCP_RX_CHECKSUM |
              NX_INTERFACE_CAPABILITY_UDP_TX_CHECKSUM |
              NX_INTERFACE_CAPABILITY_UDP_RX_CHECKSUM |
              NX_INTERFACE_CAPABILITY_ICMPV4_TX_CHECKSUM |
              NX_INTERFACE_CAPABILITY_ICMPV4_RX_CHECKSUM);
      driver_req_ptr->nx_ip_driver_interface->nx_interface_ip_mtu_size = 1500;
      return;
    }
    case NX_LINK_ENABLE:
      driver_req_ptr->nx_ip_driver_ptr->nx_ip_driver_link_up = NX_TRUE;
      return;
    case NX_LINK_DISABLE:
      driver_req_ptr->nx_ip_driver_ptr->nx_ip_driver_link_up = NX_FALSE;
      return;
    case NX_LINK_ARP_RESPONSE_SEND:
    case NX_LINK_ARP_SEND: {
      // Add ethernet header
      NX_PACKET *pkt = driver_req_ptr->nx_ip_driver_packet;
      // Prepend the Ethernet frame: DST MAC, SRC MAC: Ethertype
      // Don't actually need to set the SRC, hw will do it for us, but we need
      // the space
      pkt->nx_packet_prepend_ptr -= 14;
      pkt->nx_packet_length += 14;

      *reinterpret_cast<uint16_t *>(pkt->nx_packet_prepend_ptr) =
          htons(driver_req_ptr->nx_ip_driver_physical_address_msw);
      *reinterpret_cast<uint32_t *>(pkt->nx_packet_prepend_ptr + 2) =
          htonl(driver_req_ptr->nx_ip_driver_physical_address_lsw);
      *reinterpret_cast<uint16_t *>(pkt->nx_packet_prepend_ptr + 12) = 0x0608;

      if (!TxPacket(pkt)) {
        // Nonzero is error
        driver_req_ptr->nx_ip_driver_status = 1;
      }
      return;
    }

    case NX_LINK_PACKET_BROADCAST:
    case NX_LINK_RARP_SEND:
    case NX_LINK_PACKET_SEND: {
      // Add ethernet header
      NX_PACKET *pkt = driver_req_ptr->nx_ip_driver_packet;
      // Prepend the Ethernet frame: DST MAC, SRC MAC: Ethertype
      // Don't actually need to set the SRC, hw will do it for us, but we need
      // the space
      pkt->nx_packet_prepend_ptr -= 14;
      pkt->nx_packet_length += 14;

      *reinterpret_cast<uint16_t *>(pkt->nx_packet_prepend_ptr) =
          htons(driver_req_ptr->nx_ip_driver_physical_address_msw);
      *reinterpret_cast<uint32_t *>(pkt->nx_packet_prepend_ptr + 2) =
          htonl(driver_req_ptr->nx_ip_driver_physical_address_lsw);
      *reinterpret_cast<uint16_t *>(pkt->nx_packet_prepend_ptr + 12) = 0x0008;

      if (!TxPacket(pkt)) {
        // Nonzero is error
        driver_req_ptr->nx_ip_driver_status = 1;
      }

      return;
    }
    case NX_LINK_DEFERRED_PROCESSING:
      ProcessIsr();
      return;
  }

  /* Return the unhandled command status.  */
  driver_req_ptr->nx_ip_driver_status = NX_UNHANDLED_COMMAND;
}

void ProcessIsr() {
  // Buffer and reset, so that if ISR gets called in the meantime
  uint32_t buffered_deferred_isr = deferred_isr;
  deferred_isr = 0;
  // Make sure it's actually written so that the ISR reads it correctly
  __DSB();
  if (buffered_deferred_isr & DEFERRED_ISR_TX) {
    ProcessTx();
  }
  if (buffered_deferred_isr & DEFERRED_ISR_RX) {
    ProcessRx();
  }
}

void ETH_IRQHandler() {
  tx_trace_isr_enter_insert(ETH_IRQn);
  uint32_t reason = ETH->DMACSR;
  if (READ_BIT(reason, ETH_DMACSR_TI)) {
    deferred_isr |= DEFERRED_ISR_TX;
    __DSB();
    _nx_ip_driver_deferred_processing(&ip);
  }
  if (READ_BIT(reason, ETH_DMACSR_RI | ETH_DMACSR_RBU)) {
    deferred_isr |= DEFERRED_ISR_RX;
    __DSB();
    _nx_ip_driver_deferred_processing(&ip);
  }
  // Clear Flags
  ETH->DMACSR = reason;
  tx_trace_isr_exit_insert(ETH_IRQn);
}
