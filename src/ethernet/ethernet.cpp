//
// Created by clemens on 5/5/24.
//

#include "ethernet.hpp"

#include <cstdint>

/**
 * Make the descriptor 32 byte long, so that we can cache invalidate exactly one
 * descriptor without interfering with the neighboring descriptors.
 */
struct EthDescriptor {
  volatile uint32_t TDES0;
  volatile uint32_t TDES1;
  volatile uint32_t TDES2;
  volatile uint32_t TDES3;
  volatile uint32_t user_ptr;
  volatile uint32_t original_buffer;
  volatile uint32_t reserved[2];
};

static constexpr uint8_t TX_DESC_COUNT = 5;
static constexpr uint8_t RX_DESC_COUNT = 5;

volatile EthDescriptor TX_DESC_TABLE[TX_DESC_COUNT]
    __attribute__((section(".TxDecripSection"))){};
volatile EthDescriptor RX_DESC_TABLE[RX_DESC_COUNT]
    __attribute__((section(".RxDecripSection"))){};

// Keep track of the current TX descriptor (this is the next one to use)
volatile uint8_t current_tx_descriptor = 0;
// Keep track of the next TX descriptor to clean (TX finished, we free the
// buffer)
volatile uint8_t clean_tx_descriptor = 0;

// Index of current RX descriptor (i.e. the one which contains the data on RX
// interrupt)
volatile uint8_t current_rx_descriptor = 0;

AllocRxBuffer alloc_rx_buffer_func = nullptr;
FreeTxBuffer free_tx_buffer_func = nullptr;
ReceivedCallback received_callback_func = nullptr;
size_t rx_buffer_size = 0;

/**
 * Functions for PHY driver
 */
int32_t mdio_read(uint32_t, uint32_t, uint32_t *);

int32_t mdio_write(uint32_t, uint32_t, uint32_t);

int32_t get_tick() { return HAL_GetTick(); }

// Keep track of last PhyState in order to just change it, if it really did
// change.
uint32_t phy_state_ = 0;

/**
 * Init the GPIO Pins for RMII
 */
static void InitGPIO();

/**
 * Init the PHY
 */
static void InitPHY();

/**
 * Init the MAC (Setup DMA etc, so we only need to do DMA things later)
 */
static void InitMAC();

/**
 * Init DMA according to RM0468 section 63.9.1
 */
static void InitDMA();

void InitEthernet(AllocRxBuffer alloc_func, FreeTxBuffer free_func,
                  ReceivedCallback received_callback, size_t buffer_size) {
  /**
   * TODOS:
   * - generate a mac address from UID
   * - allow d-cache (use MPU / invalidate cache)
   * - arp offloading (low prio)
   * - multicast
   */

  phy_state_ = PHY_STATUS_LINK_DOWN;
  alloc_rx_buffer_func = alloc_func;
  free_tx_buffer_func = free_func;
  received_callback_func = received_callback;
  rx_buffer_size = buffer_size;

  if (alloc_rx_buffer_func == nullptr || free_tx_buffer_func == nullptr ||
      received_callback_func == nullptr || buffer_size == 0) {
    return;
  }

  // Enable interrupt
  HAL_NVIC_SetPriority(ETH_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(ETH_IRQn);

  // Init the Ethernet according to reference manual RM0468 section 63.9
  InitGPIO();
  InitPHY();
  InitDMA();
  InitMAC();
}

void BuildRxDescriptors() {
  SCB_InvalidateDCache_by_Addr((void *)(&RX_DESC_TABLE), sizeof(RX_DESC_TABLE));

  uint32_t build_rx_descriptor = 0;
  // Fill all RX descriptors with fresh buffers, so that the DMA can use it.
  while (!READ_BIT(RX_DESC_TABLE[build_rx_descriptor].TDES3, 0x80000000) &&
         RX_DESC_TABLE[build_rx_descriptor].TDES0 == 0) {
    void *buffer = nullptr;
    void *user_ptr = nullptr;
    alloc_rx_buffer_func(&buffer, &user_ptr);

    if (!buffer) {
      // we did not get a buffer, don't build a descriptor.
      return;
    }
    auto &entry = RX_DESC_TABLE[build_rx_descriptor];
    entry.TDES0 = reinterpret_cast<uint32_t>(buffer);
    entry.user_ptr = reinterpret_cast<uint32_t>(user_ptr);
    // Keep the original buffer address around, after RX the TDES0 will be
    // overwritten.
    entry.original_buffer = reinterpret_cast<uint32_t>(buffer);
    entry.TDES1 = 0;
    entry.TDES2 = 0;
    // OWN, IOC, BUF1V
    entry.TDES3 = 0xC1000000;
    // Flush cache before telling DMA about it
    __DSB();
    SCB_CleanDCache_by_Addr((uint32_t *)(&entry), sizeof(EthDescriptor));
    WRITE_REG(ETH->DMACRDTPR,
              reinterpret_cast<uint32_t>(RX_DESC_TABLE + build_rx_descriptor));
    build_rx_descriptor = (build_rx_descriptor + 1) % RX_DESC_COUNT;
  }
}

bool TxPacket(void *buffer, void *user_ptr, size_t size, uint32_t tx_flags) {
  // In this implementation, we only support single packets.
  if (tx_flags != (TX_FLAG_FIRST | TX_FLAG_LAST)) {
    return false;
  }
  // Check that the size int doesn't overflow the actual descriptor size.
  if (size > 0x3FF) {
    return false;
  }
  SCB_InvalidateDCache_by_Addr((void *)(TX_DESC_TABLE + current_tx_descriptor),
                               sizeof(EthDescriptor));
  SCB_CleanDCache_by_Addr((uint32_t *)(buffer), size);
  // Check, if the next descriptor is available.
  // No need to clean the d-cache, we set TDES3 and TDES0 in ISR which will be
  // in the cache At this point in time, DMA has already removed the OWN bit
  // (thereby triggering the ISR), so it's all in sync
  if (TX_DESC_TABLE[current_tx_descriptor].TDES0 != 0 ||
      TX_DESC_TABLE[current_tx_descriptor].TDES3 & 0x80000000 != 0) {
    // Descriptor buffer still in use, don't overwrite or we'll leak memory.
    // Need to wait for ISR to clean this up.
    return false;
  }

  TX_DESC_TABLE[current_tx_descriptor].TDES0 =
      reinterpret_cast<uint32_t>(buffer);
  TX_DESC_TABLE[current_tx_descriptor].original_buffer =
      reinterpret_cast<uint32_t>(buffer);
  TX_DESC_TABLE[current_tx_descriptor].user_ptr =
      reinterpret_cast<uint32_t>(user_ptr);
  TX_DESC_TABLE[current_tx_descriptor].TDES1 = 0;
  TX_DESC_TABLE[current_tx_descriptor].TDES2 = 0x80000000 | size;
  // FD, LD, OWN
  TX_DESC_TABLE[current_tx_descriptor].TDES3 =
      0x80000000 | (3 << 16) | tx_flags;

  // SET_BIT(TX_DESC_TABLE[0].TDES3, ETH_DMATXNDESCRF_FD);
  // SET_BIT(TX_DESC_TABLE[0].TDES3, ETH_DMATXNDESCRF_LD);
  // SET_BIT(TX_DESC_TABLE[0].TDES3, ETH_DMATXNDESCRF_OWN);
  // Make sure write is done (in cache) and flush cache
  __DSB();
  SCB_CleanDCache_by_Addr((uint32_t *)(TX_DESC_TABLE + current_tx_descriptor),
                          sizeof(EthDescriptor));
  // Skip to next descriptor.
  current_tx_descriptor = (current_tx_descriptor + 1) % TX_DESC_COUNT;
  // Set the tail
  WRITE_REG(ETH->DMACTDTPR,
            reinterpret_cast<uint32_t>(TX_DESC_TABLE + current_tx_descriptor));
  return true;
}

void InitPHY() {
  // Reset the chip
  // mdio_write(31, 0x1F, 0xFA00);
  // mdio_write(31, 0x01, 0xFFFF);

  // Wait for switch to be ready
  while (1) {
    // Set address each time to be safe
    mdio_write(31, 0x1f, 0xFA00);
    uint32_t value = 0;
    mdio_read(31, 0x00, &value);
    tx_thread_sleep(10);
    // Check, if lowest bit is set. Otherwise wait for startup.
    if (value & 0x1) {
      break;
    }
  }

  tx_thread_sleep(10);

  // Set RMII; 100Mbit; full duplex.
  // Check datasheet for magic numbers (3.6.3 RMII Configuration REFCLK Master)
  mdio_write(31, 0x1F, 0xF100);
  mdio_write(31, 0x00, 0x40B3);
  mdio_write(31, 0x1F, 0xF410);
  mdio_write(31, 0x00, 0x2A05);
  // mdio_write(31, 0x1F, 0);
}

void InitDMA() {
  // Set reset bit.
  SET_BIT(ETH->DMAMR, ETH_DMAMR_SWR);
  // Wait for reset to complete (bit is cleared by HW)
  while (READ_BIT(ETH->DMAMR, ETH_DMAMR_SWR)) {
    /*wait*/
    HAL_Delay(10);
  }

  WRITE_REG(ETH->DMASBMR, ETH_DMASBMR_FB | ETH_DMASBMR_AAL);

  for (auto &desc : TX_DESC_TABLE) {
    desc.TDES0 = 0;
    desc.TDES1 = 0;
    desc.TDES2 = 0;
    desc.TDES3 = 0;
    desc.original_buffer = 0;
    desc.user_ptr = 0;
    desc.reserved[0] = desc.reserved[1] = 0;
  }
  for (auto &desc : RX_DESC_TABLE) {
    desc.TDES0 = 0;
    desc.TDES1 = 0;
    desc.TDES2 = 0;
    desc.TDES3 = 0;
    desc.original_buffer = 0;
    desc.user_ptr = 0;
    desc.reserved[0] = desc.reserved[1] = 0;
  }

  current_tx_descriptor = 0;
  current_rx_descriptor = 0;

  SCB_CleanDCache_by_Addr((uint32_t *)(TX_DESC_TABLE), sizeof(TX_DESC_TABLE));
  SCB_CleanDCache_by_Addr((uint32_t *)(RX_DESC_TABLE), sizeof(RX_DESC_TABLE));

  // Set descriptor skip length
  WRITE_REG(ETH->DMACCR, ETH_DMACCR_DSL_128BIT);

  // Set ring length
  WRITE_REG(ETH->DMACTDRLR, TX_DESC_COUNT - 1);
  WRITE_REG(ETH->DMACRDRLR, RX_DESC_COUNT - 1);

  // Set list beginning
  WRITE_REG(ETH->DMACTDLAR, reinterpret_cast<uint32_t>(&TX_DESC_TABLE));
  WRITE_REG(ETH->DMACRDLAR, reinterpret_cast<uint32_t>(&RX_DESC_TABLE));

  // Set tail pointer to first descriptor, since we don't have queued anything
  WRITE_REG(ETH->DMACTDTPR, reinterpret_cast<uint32_t>(&TX_DESC_TABLE));
  WRITE_REG(ETH->DMACRDTPR, reinterpret_cast<uint32_t>(&RX_DESC_TABLE));

  WRITE_REG(ETH->DMACTCR, ETH_DMACTCR_TPBL_32PBL);
  WRITE_REG(ETH->DMACRCR, ETH_DMACRCR_RPBL_32PBL | rx_buffer_size);

  __DSB();

  // Build initial RX descriptors
  BuildRxDescriptors();

  // Enable normal and abnormal interrupts
  WRITE_REG(ETH->DMACIER, ETH_DMACIER_RSE | ETH_DMACIER_RBUE |
                              ETH_DMACIER_FBEE | ETH_DMACIER_TIE |
                              ETH_DMACIER_RIE | ETH_DMACIER_NIE);

  // Enable DMA Tx and Rx
  SET_BIT(ETH->DMACTCR, ETH_DMACTCR_ST);
  SET_BIT(ETH->DMACRCR, ETH_DMACRCR_SR);

  // MTL initialization
  WRITE_REG(ETH->MTLTQOMR, ETH_MTLTQOMR_TSF);
  WRITE_REG(ETH->MTLRQOMR, ETH_MTLRQOMR_RSF);
}

int32_t mdio_read(uint32_t phy_addr, uint32_t reg, uint32_t *val) {
  // Wait for not busy
  while (READ_BIT(ETH->MACMDIOAR, ETH_MACMDIOAR_MB)) {
  }
  // Read operation
  MODIFY_REG(ETH->MACMDIOAR, ETH_MACMDIOAR_MOC_Msk, ETH_MACMDIOAR_MOC_RD);
  // Set address
  MODIFY_REG(ETH->MACMDIOAR, ETH_MACMDIOAR_PA_Msk,
             (phy_addr << ETH_MACMDIOAR_PA_Pos) & ETH_MACMDIOAR_PA_Msk);
  MODIFY_REG(ETH->MACMDIOAR, ETH_MACMDIOAR_RDA_Msk,
             (reg << ETH_MACMDIOAR_RDA_Pos) & ETH_MACMDIOAR_RDA_Msk);
  SET_BIT(ETH->MACMDIOAR, ETH_MACMDIOAR_MB);
  __DSB();
  // Wait for not busy
  while (READ_BIT(ETH->MACMDIOAR, ETH_MACMDIOAR_MB)) {
  }
  *val = ETH->MACMDIODR;
  return 0;
}

int32_t mdio_write(uint32_t phy_addr, uint32_t reg, uint32_t val) {
  // Wait for not busy
  while (READ_BIT(ETH->MACMDIOAR, ETH_MACMDIOAR_MB)) {
  }
  ETH->MACMDIODR = val;
  // Read operation
  MODIFY_REG(ETH->MACMDIOAR, ETH_MACMDIOAR_MOC_Msk, ETH_MACMDIOAR_MOC_WR);
  // Set address
  MODIFY_REG(ETH->MACMDIOAR, ETH_MACMDIOAR_PA_Msk,
             (phy_addr << ETH_MACMDIOAR_PA_Pos) & ETH_MACMDIOAR_PA_Msk);
  MODIFY_REG(ETH->MACMDIOAR, ETH_MACMDIOAR_RDA_Msk,
             (reg << ETH_MACMDIOAR_RDA_Pos) & ETH_MACMDIOAR_RDA_Msk);
  SET_BIT(ETH->MACMDIOAR, ETH_MACMDIOAR_MB);
  __DSB();
  // Wait for not busy
  while (READ_BIT(ETH->MACMDIOAR, ETH_MACMDIOAR_MB)) {
  }
  return 0;
}

void InitGPIO() {
  __HAL_RCC_ETH1MAC_CLK_ENABLE();
  __HAL_RCC_ETH1TX_CLK_ENABLE();
  __HAL_RCC_ETH1RX_CLK_ENABLE();

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  /**ETH GPIO Configuration
  PC1     ------> ETH_MDC
  PA1     ------> ETH_REF_CLK
  PA2     ------> ETH_MDIO
  PA7     ------> ETH_CRS_DV
  PC4     ------> ETH_RXD0
  PC5     ------> ETH_RXD1
  PB13     ------> ETH_TXD1
  PG11     ------> ETH_TX_EN
  PG13     ------> ETH_TXD0
  */
  GPIO_InitTypeDef GPIO_InitStruct = {};

  GPIO_InitStruct.Pin = RMII_MDC_Pin | RMII_RXD0_Pin | RMII_RXD1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = RMII_REF_CLK_Pin | RMII_MDIO_Pin | RMII_CRS_DV_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
  HAL_GPIO_Init(RMII_TXD1_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = RMII_TX_EN_Pin | GPIO_PIN_14;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  // RMII Mode
  MODIFY_REG(SYSCFG->PMCR, SYSCFG_PMCR_EPIS_SEL_Msk, SYSCFG_PMCR_EPIS_SEL_2);
}

void InitMAC() {
  // Setup SMA for MDIO
  WRITE_REG(ETH->MACMDIOAR, ETH_MACMDIOAR_CR_DIV124);

  // Set the MAC address
  WRITE_REG(ETH->MACA0HR, 0x0203);
  WRITE_REG(ETH->MACA0LR, 0x01E18000);

  // Perfect filtering
  WRITE_REG(ETH->MACPFR, 0);

  // Enable source address replace of MAC address 0
  // Enable checksum offload
  // Don't enable TX and RX, we will do this, once the PHY is reporting the
  // speed and duplex
  WRITE_REG(ETH->MACCR, ETH_MACCR_IPC | ETH_MACCR_SARC_REPADDR0);
}

void ProcessRx() {
  SCB_InvalidateDCache_by_Addr((void *)(RX_DESC_TABLE), sizeof(RX_DESC_TABLE));

  while (!READ_BIT(RX_DESC_TABLE[current_rx_descriptor].TDES3, 0x80000000) &&
         RX_DESC_TABLE[current_rx_descriptor].original_buffer != 0) {
    auto &entry = RX_DESC_TABLE[current_rx_descriptor];
    void *buffer = reinterpret_cast<void *>(entry.original_buffer);
    void *user_ptr = reinterpret_cast<void *>(entry.user_ptr);
    size_t size = entry.TDES3 & 0x3FFF;

    // First and Last flags.
    uint32_t rx_flags = entry.TDES3 & 0x30000000;
    // Check for CRC errors
    // entry.TDES1 & 0x7: COE has processed the packet
    // (entry.TDES1&0x30) && entry.TDES1&0x08 &&: COE has processed ip header
    // AND it was an error entry.TDES3 & 0x1300000: CRC, overflow or receive
    // error
    if ((entry.TDES1 & 0x7 && entry.TDES1 & 0x30 && entry.TDES1 & 0x8) ||
        (entry.TDES3 & 0x1300000)) {
      rx_flags |= RX_FLAG_CRC_ERROR;
    }

    SCB_InvalidateDCache_by_Addr(buffer, size);
    received_callback_func(buffer, user_ptr, size, rx_flags);

    alloc_rx_buffer_func(&buffer, &user_ptr);

    entry.TDES0 = reinterpret_cast<uint32_t>(buffer);
    entry.user_ptr = reinterpret_cast<uint32_t>(user_ptr);
    // Keep the original buffer address around, after RX the TDES0 will be
    // overwritten.
    entry.original_buffer = reinterpret_cast<uint32_t>(buffer);
    entry.TDES1 = 0;
    entry.TDES2 = 0;

    if (entry.TDES0 == 0) {
      // not successful, don't allow DMA
      entry.TDES3 = 0;
    } else {
      // success, give to DMA
      // OWN, IOC, BUF1V
      entry.TDES3 = 0xC1000000;
    }

    // Write to cache before flushing cache
    __DSB();
    SCB_CleanDCache_by_Addr((uint32_t *)(&entry), sizeof(EthDescriptor));
    current_rx_descriptor = (current_rx_descriptor + 1) % RX_DESC_COUNT;
  }
  WRITE_REG(ETH->DMACRDTPR,
            reinterpret_cast<uint32_t>(RX_DESC_TABLE + current_rx_descriptor));
}

void ProcessTx() {
  SCB_InvalidateDCache_by_Addr((void *)(TX_DESC_TABLE), sizeof(TX_DESC_TABLE));
  while (!READ_BIT(TX_DESC_TABLE[clean_tx_descriptor].TDES3, 0x80000000) &&
         TX_DESC_TABLE[clean_tx_descriptor].original_buffer != 0) {
    // We have transmit interrupt, clear the buffer and increment the pointer
    free_tx_buffer_func(
        reinterpret_cast<void *>(TX_DESC_TABLE[clean_tx_descriptor].TDES0),
        reinterpret_cast<void *>(TX_DESC_TABLE[clean_tx_descriptor].user_ptr));
    // Clean the buffer
    TX_DESC_TABLE[clean_tx_descriptor].TDES0 = 0;
    TX_DESC_TABLE[clean_tx_descriptor].original_buffer = 0;
    TX_DESC_TABLE[clean_tx_descriptor].user_ptr = 0;
    // Make sure that buffer belongs to us, need to do this, since we haven't
    // invalidated the d-cache
    TX_DESC_TABLE[clean_tx_descriptor].TDES3 = 0;
    // Make sure write is done before writing cache.
    __DSB();
    SCB_CleanDCache_by_Addr((uint32_t *)(TX_DESC_TABLE + clean_tx_descriptor),
                            sizeof(EthDescriptor));

    clean_tx_descriptor = (clean_tx_descriptor + 1) % TX_DESC_COUNT;
  }
}

uint32_t UpdatePhyState() {
  uint32_t state = PHY_STATUS_100MBITS_FULLDUPLEX;

  if (state == phy_state_) return state;
  phy_state_ = state;
  switch (state) {
    case PHY_STATUS_100MBITS_FULLDUPLEX:
      SET_BIT(ETH->MACCR,
              ETH_MACCR_FES | ETH_MACCR_DM | ETH_MACCR_TE | ETH_MACCR_RE);
      break;
    case PHY_STATUS_100MBITS_HALFDUPLEX:
      CLEAR_BIT(ETH->MACCR, ETH_MACCR_DM);
      SET_BIT(ETH->MACCR, ETH_MACCR_FES | ETH_MACCR_TE | ETH_MACCR_RE);
      break;
    case PHY_STATUS_10MBITS_FULLDUPLEX:
      CLEAR_BIT(ETH->MACCR, ETH_MACCR_FES);
      SET_BIT(ETH->MACCR, ETH_MACCR_DM | ETH_MACCR_TE | ETH_MACCR_RE);
      break;
    case PHY_STATUS_10MBITS_HALFDUPLEX:
      CLEAR_BIT(ETH->MACCR, ETH_MACCR_DM | ETH_MACCR_FES);
      SET_BIT(ETH->MACCR, ETH_MACCR_TE | ETH_MACCR_RE);
      break;
    default:
      CLEAR_BIT(ETH->MACCR, ETH_MACCR_TE | ETH_MACCR_RE);
      break;
  }

  return state;
}
