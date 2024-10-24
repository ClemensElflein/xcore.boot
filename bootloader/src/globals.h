//
// Created by clemens on 01.10.24.
//

#ifndef GLOBALS_H
#define GLOBALS_H

#include "ch.h"

#define NETIF_EVENT_ADDRESS_VALID 1

// This mutex will be locked by the reboot task as well as by the flash task
// This prevents the device from rebooting while flashing
extern mutex_t reboot_mutex;

extern event_source_t netif_events;

void InitGlobals();

#endif  // GLOBALS_H
