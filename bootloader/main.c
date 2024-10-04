#include <board_ex.h>
#include <bootloader.h>
#include <globals.h>
#include <heartbeat.h>
#include <id_eeprom.h>
#include <service_discovery.h>

#include "ch.h"
#include "hal.h"
#include "lwipthread.h"

/*
 * Application entry point.
 */
int main(void) {
  // Need to disable D-Cache for the Ethernet MAC to work properly
  SCB_DisableDCache();
  /*
   * System initializations.
   * - HAL initialization, this also initializes the configured device drivers
   *   and performs the board-specific initializations.
   * - Kernel initialization, the main() function becomes a thread and the
   *   RTOS is active.
   */
  halInit();
  chSysInit();

  /*
   * InitGlobals() sets up global variables shared by threads. (e.g. mutex)
   * InitHeartbeat() starts the heartbeat timer to blink an LED as long as the
   * RTOS is running.
   */
  InitGlobals();
  InitHeartbeat();

  /*
   * Setup LWIP stack using the MAC address provided by the EEPROM.
   */
  uint8_t mac_address[6] = {0};
  ID_EEPROM_GetMacAddress(mac_address, sizeof(mac_address));
  lwipthread_opts_t lwipconf_opts = {0};
  lwipconf_opts.addrMode = NET_ADDRESS_DHCP;
  lwipconf_opts.address = FALLBACK_IP_ADDRESS;
  lwipconf_opts.gateway = FALLBACK_GATEWAY;
  lwipconf_opts.netmask = FALLBACK_NETMASK;
  lwipconf_opts.macaddress = mac_address;
  lwipInit(&lwipconf_opts);

  // Start the ServiceDiscovery Task.
  InitServiceDiscovery();

  // Start the actual Bootloader Task
  InitBootloaderThread();

  chThdSleep(TIME_INFINITE);

  uint32_t sleep_count = 1000 / 100;
  while (sleep_count-- > 0) {
    chThdSleep(TIME_MS2I(100));
    if (palReadLine(LINE_BUTTON)) {
      // sleep forever, don't boot into user software
      chThdSleep(TIME_INFINITE);
    }
  }

  chMtxLock(&reboot_mutex);

  MODIFY_REG(SYSCFG->UR2, SYSCFG_UR2_BOOT_ADD0,
             (BOOT_ADDRESS >> 16) << SYSCFG_UR2_BOOT_ADD0_Pos);
  NVIC_SystemReset();

  while (1) {
    chThdSleep(100);
  }
}
