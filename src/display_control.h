#ifndef _DISPLAY_CONTROL_H
#define _DISPLAY_CONTROL_H
/* Display control driver for Orieentering Clock.
 *
 * HARDWARE:
 * - 6 common anode 7-segment led displays
 * - 2 chained shift registers connected to SPI port:
 *    - first: high side switch (controlling MOSFETs throuth NPN transistors) (first 6 drains)
 *    - second: low-side switch for CA segments A-G (D0-D6) and dot point (D7)
 *
 * The update is timer-interrupt driven.
 */

#include <Arduino.h>

// configuration
#define LED_DISPLAYS_BIG_CNT  4
#define LED_DISPLAYS_SMALL_CNT  2
#define LED_DISPLAYS_CNT  (LED_DISPLAYS_BIG_CNT + LED_DISPLAYS_SMALL_CNT)

#define MIN_BRIGHTNESS 1
#define MAX_BRIGHTNESS 10

#define DEFAULT_BIG_VALUE   (10)
#define DEFAULT_SMALL_VALUE 0
#define DEFAULT_BRIGHTNESS 7

class DisplayControlClass {
public:
    // setup SPI and timer interrupts - call at start
    static void setup();
    // set value to be displayed. Supports only integer values. Flag to show '-' in big display.
    static inline void setValue(unsigned int bigValue, unsigned int smallValue, byte showMinus);

    static inline void setBrightness(byte brightness); // set brightness level. valid values: MIN-MAX_BRIGHTNESS
    static inline void incBrightness(); // decrease brightness level till MIN_BRIGHTNESS
    static inline void decBrightness(); // increase brightness level till MAX_BRIGHTNESS

    // enable/disable dot point at given LED segment
    static inline void setDP(byte ledSegment, byte value);

private:
    static void computeBigValues();   // populate values to be send
    static void computeSmallValues(); // populate values to be send


    // variables
    static byte values[LED_DISPLAYS_CNT];  // values to be send
    static byte DPstate[LED_DISPLAYS_CNT]; // dot-point state

    static unsigned int currBigValue;
    static unsigned int currSmallValue;

    static byte currShowMinus;
    static byte currBrightness;
};

extern DisplayControlClass DisplayControl;

static inline void DisplayControlClass::setValue(unsigned int bigValue, unsigned int smallValue, byte showMinus) {
    // for debug: check constraints
    if (bigValue > 9999 || (showMinus && bigValue > 999) || smallValue > 99)
        return;

    if (bigValue != currBigValue || showMinus != currShowMinus) {
        currBigValue = bigValue;
        currShowMinus = (showMinus > 0);
        computeBigValues();
    }

    if (smallValue != currSmallValue) {
        currSmallValue = smallValue;
        computeSmallValues();
    }
}

static inline void DisplayControlClass::setBrightness(byte brightness) {
    if (brightness <= MAX_BRIGHTNESS && brightness >= MIN_BRIGHTNESS) {
        currBrightness = brightness;
    }
}
static inline void DisplayControlClass::incBrightness() {
    if (currBrightness < MAX_BRIGHTNESS) {
        currBrightness += 1;
    }
}
static inline void DisplayControlClass::decBrightness() {
    if (currBrightness > MIN_BRIGHTNESS) {
        currBrightness -= 1;
    }
}

static inline void DisplayControlClass::setDP(byte ledSegment, byte value) {
    DPstate[ledSegment] = (value > 0);
    if (ledSegment < LED_DISPLAYS_BIG_CNT) {
        computeBigValues();
    } else {
        computeSmallValues();
    }
}

#endif //_DISPLAY_CONTROL_H
