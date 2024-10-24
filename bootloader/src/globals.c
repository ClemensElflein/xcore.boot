//
// Created by clemens on 01.10.24.
//

#include "globals.h"

// This mutex will be locked by the reboot task as well as by the flash task
// This prevents the device from rebooting while flashing
mutex_t reboot_mutex;

EVENTSOURCE_DECL(netif_events);

void InitGlobals() { chMtxObjectInit(&reboot_mutex); }
