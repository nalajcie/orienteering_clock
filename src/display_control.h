#ifndef _DISPLAY_CONTROL_H
#define _DISPLAY_CONTROL_H
/* Display control driver for Orieentering Clock.
 *
 * HARDWARE:
 * - 6 common anode 7-segment led displays
 * - 2 chained shift registers connected to SPI port:
 *    - first: high side switch (controlling MOSFETs throuth NPN transistors) (first 6 drains)
 *    - second: low-side switch for CA segments A-G (D0-D6)
 *
 * The update is timer-interrupt driven.
 */

#include <Arduino.h>
#include "test.h"

// enable for debugging
#define DEBUG_DISPLAY

// configuration
#define LED_DISPLAYS_BIG_CNT    4
#define LED_DISPLAYS_SMALL_CNT  2
#define LED_DISPLAYS_CNT        (LED_DISPLAYS_BIG_CNT + LED_DISPLAYS_SMALL_CNT)

#define MIN_BRIGHTNESS          1
#define MAX_BRIGHTNESS          10

#define DEFAULT_BRIGHTNESS      7

#define ONE_DIGIT_US    2400

// do not change these:
#define CLOCK_HZ        16000000
#define TIMER_PRESCALER (64)
#define TIMER_COMPARER  (15)
#define TIMER_DIVISOR   (TIMER_PRESCALER * TIMER_COMPARER)
#define ONE_TICK_US     (1000000L / (CLOCK_HZ / TIMER_DIVISOR)) // exactly 60us


#define ONE_DIGIT_TICKS (ONE_DIGIT_US / ONE_TICK_US)                // 40 ticks
#define BRIGHTNESS_STEP_TICKS (ONE_DIGIT_TICKS / MAX_BRIGHTNESS)    // 4 ticks

// compute refresh rate
#define REFRESH_RATE (1000000L / (ONE_DIGIT_US * LED_DISPLAYS_CNT)) // about 70 Hz (every digit)


// check if storage class for timerCounter is OK - will not get higher than ONE_DIGIT_TICKS + 1
#if ONE_DIGIT_TICKS > 254
#warning "ONE_DIGIT_TICKS too large, increase storage class for timerCounter"
#endif

// setup SPI and timer interrupts - call at start
void display_setup();
// set value to be displayed. Supports only integer values. Flag to show '-' in big display.
void display_setValue(unsigned int bigValue, unsigned int smallValue, uint8_t showMinus);

#define MODE_MINUTES    0
#define MODE_HOURS      1
void display_setMode(uint8_t mode);
void display_toggleMode();

void display_showBuzzState(int buzzState);
void display_showBattState(int percent);

void display_setBrightness(uint8_t brightness); // set brightness level. valid values: MIN-MAX_BRIGHTNESS
void display_incBrightness(); // decrease brightness level till MIN_BRIGHTNESS
void display_decBrightness(); // increase brightness level till MAX_BRIGHTNESS

// enable/disable dot point at given LED segment
void display_setDP(uint8_t ledSegment, uint8_t value);

#ifdef TEST_SPI
// SPI routines are externally visible only when 'TEST_SPI' is enabled
void setupSPI();
uint8_t spiTransfer(uint8_t data);
#endif

#endif //_DISPLAY_CONTROL_H
