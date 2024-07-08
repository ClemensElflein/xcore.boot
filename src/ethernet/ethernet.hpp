//
// Created by clemens on 5/5/24.
//

#ifndef ETHERNET_HPP
#define ETHERNET_HPP

#include <nx_api.h>

#include "stm32h7xx_hal.h"
#ifdef __cplusplus
extern "C" {
#endif
#define RMII_MDC_Pin GPIO_PIN_1
#define RMII_MDC_GPIO_Port GPIOC
#define RMII_REF_CLK_Pin GPIO_PIN_1
#define RMII_REF_CLK_GPIO_Port GPIOA
#define RMII_MDIO_Pin GPIO_PIN_2
#define RMII_MDIO_GPIO_Port GPIOA
#define RMII_CRS_DV_Pin GPIO_PIN_7
#define RMII_CRS_DV_GPIO_Port GPIOA
#define RMII_RXD0_Pin GPIO_PIN_4
#define RMII_RXD0_GPIO_Port GPIOC
#define RMII_RXD1_Pin GPIO_PIN_5
#define RMII_RXD1_GPIO_Port GPIOC
#define RMII_TXD1_Pin GPIO_PIN_13
#define RMII_TXD1_GPIO_Port GPIOB
#define RMII_TX_EN_Pin GPIO_PIN_11
#define RMII_TX_EN_GPIO_Port GPIOG
#define RMII_TXD0_Pin GPIO_PIN_13
#define RMII_TXD0_GPIO_Port GPIOG

// Set when the received packet has some checksum error
#define RX_FLAG_CRC_ERROR 4
// Set when the received buffer is the first one of a packet which is spread
// over multiple descriptors
#define RX_FLAG_FIRST 0x20000000
// Set when the received buffer is the last one of a packet which is spread over
// multiple descriptors
#define RX_FLAG_LAST 0x10000000

// Set when the received buffer is the first one of a packet which is spread
// over multiple descriptors
#define TX_FLAG_FIRST 0x20000000
// Set when the received buffer is the last one of a packet which is spread over
// multiple descriptors
#define TX_FLAG_LAST 0x10000000

#define PHY_STATUS_LINK_DOWN ((int32_t)1)
#define PHY_STATUS_100MBITS_FULLDUPLEX ((int32_t)2)
#define PHY_STATUS_100MBITS_HALFDUPLEX ((int32_t)3)
#define PHY_STATUS_10MBITS_FULLDUPLEX ((int32_t)4)
#define PHY_STATUS_10MBITS_HALFDUPLEX ((int32_t)5)
#define PHY_STATUS_AUTONEGO_NOTDONE ((int32_t)6)

// Gets called to get a new Rx buffer. Return only buffers which can be accessed
// by eth DMA
typedef void (*AllocRxBuffer)(void** buffer, void** user_ptr);
// Gets called once a buffer was successfully transmitted to the network.
typedef void (*FreeTxBuffer)(void* buffer, void* user_ptr);
// Callback which is called on received packet.
typedef void (*ReceivedCallback)(void* buffer, void* user_ptr, size_t size,
                                 uint32_t rx_flags);

/**
 * Call this to initialize the Ethernet for the current board.
 */
void InitEthernet(AllocRxBuffer alloc_func, FreeTxBuffer free_func,
                  ReceivedCallback received_callback, size_t buffer_size);

bool TxPacket(void* buffer, void* user_ptr, size_t size, uint32_t tx_flags);

void ProcessTx();
void ProcessRx();

/**
 * Fetches the state of the phy, updates mac accordingly and returns the state.
 * @return the state of the phy
 */
uint32_t UpdatePhyState();

extern NX_PACKET_POOL ethernet_pool;

#ifdef __cplusplus
}
#endif

#endif  // ETHERNET_HPP
