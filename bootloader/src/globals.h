//
// Created by clemens on 01.10.24.
//

#ifndef GLOBALS_H
#define GLOBALS_H

#include "ch.h"

// This mutex will be locked by the reboot task as well as by the flash task
// This prevents the device from rebooting while flashing
extern mutex_t reboot_mutex;

void InitGlobals();

#endif  // GLOBALS_H
