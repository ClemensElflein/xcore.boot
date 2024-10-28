//
// Created by clemens on 29.09.24.
//

#include "heartbeat.h"

#include "ch.h"
#include "hal.h"

// Gets updated by the idle task to the current tick value.
volatile uint32_t last_idle_tick = 0;

static volatile uint8_t blink_state = 0;

static virtual_timer_t heartbeat_timer;

static void heartbeat_timer_cb(void *arg) {
  (void)arg;
  bool elapsed =
      TIME_I2MS(chVTTimeElapsedSinceX(last_idle_tick)) > HEARTBEAT_RECENT_MS;
  switch (blink_state) {
    case 0:
      if (elapsed) {
#ifndef BOARD_HEARTBEAT_LED_INVERTED
        palSetLine(LINE_HEARTBEAT_LED_RED);
        palSetLine(LINE_HEARTBEAT_LED_GREEN);
#else
        palClearLine(LINE_HEARTBEAT_LED_RED);
        palClearLine(LINE_HEARTBEAT_LED_GREEN);
#endif
      } else {
#ifndef BOARD_HEARTBEAT_LED_INVERTED
        palClearLine(LINE_HEARTBEAT_LED_GREEN);
        palClearLine(LINE_HEARTBEAT_LED_RED);
#else
        palSetLine(LINE_HEARTBEAT_LED_GREEN);
        palSetLine(LINE_HEARTBEAT_LED_RED);
#endif
      }
      chSysLockFromISR();
      chVTSetI(&heartbeat_timer, TIME_MS2I(100), heartbeat_timer_cb, NULL);
      chSysUnlockFromISR();
      blink_state++;
      break;
    case 1:
#ifndef BOARD_HEARTBEAT_LED_INVERTED
      palSetLine(LINE_HEARTBEAT_LED_GREEN);
      palClearLine(LINE_HEARTBEAT_LED_RED);
#else
      palClearLine(LINE_HEARTBEAT_LED_GREEN);
      palSetLine(LINE_HEARTBEAT_LED_RED);
#endif
      chSysLockFromISR();
      chVTSetI(&heartbeat_timer, TIME_MS2I(100), heartbeat_timer_cb, NULL);
      chSysUnlockFromISR();
      blink_state++;
      break;
    case 2:
      if (elapsed) {
#ifndef BOARD_HEARTBEAT_LED_INVERTED
        palSetLine(LINE_HEARTBEAT_LED_RED);
        palSetLine(LINE_HEARTBEAT_LED_GREEN);
#else
        palClearLine(LINE_HEARTBEAT_LED_RED);
        palClearLine(LINE_HEARTBEAT_LED_GREEN);
#endif
      } else {
#ifndef BOARD_HEARTBEAT_LED_INVERTED
        palClearLine(LINE_HEARTBEAT_LED_GREEN);
        palClearLine(LINE_HEARTBEAT_LED_RED);
#else
        palSetLine(LINE_HEARTBEAT_LED_GREEN);
        palSetLine(LINE_HEARTBEAT_LED_RED);
#endif
      }
      chSysLockFromISR();
      chVTSetI(&heartbeat_timer, TIME_MS2I(100), heartbeat_timer_cb, NULL);
      chSysUnlockFromISR();
      blink_state++;
      break;
    default:
#ifndef BOARD_HEARTBEAT_LED_INVERTED
      palSetLine(LINE_HEARTBEAT_LED_GREEN);
      palClearLine(LINE_HEARTBEAT_LED_RED);
#else
      palClearLine(LINE_HEARTBEAT_LED_GREEN);
      palSetLine(LINE_HEARTBEAT_LED_RED);
#endif

      chSysLockFromISR();
      chVTSetI(&heartbeat_timer, TIME_MS2I(500), heartbeat_timer_cb, NULL);
      chSysUnlockFromISR();
      blink_state = 0;
      break;
  }
}

void InitHeartbeat() {
  chVTObjectInit(&heartbeat_timer);
  heartbeat_timer_cb(NULL);
}
