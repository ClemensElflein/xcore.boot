//
// Created by clemens on 01.10.24.
//

#ifndef GLOBALS_H
#define GLOBALS_H

#include <id_eeprom.h>

#include "ch.h"

#define NETIF_EVENT_ADDRESS_VALID 1

// This mutex will be locked by the reboot task as well as by the flash task
// This prevents the device from rebooting while flashing
extern mutex_t reboot_mutex;

extern event_source_t netif_events;

extern struct board_info board_info;
extern struct carrier_board_info carrier_board_info;

void InitGlobals(void);

#endif  // GLOBALS_H
