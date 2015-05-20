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
#define REFRESH_RATE (1000000L / (ONE_DIGIT_US * LED_DISPLAYS_CNT)) // about 70 Hz, so 70/6 per digit


// check if storage class for timerCounter is OK - will not get higher than ONE_DIGIT_TICKS + 1
#if ONE_DIGIT_TICKS > 254
#warning "ONE_DIGIT_TICKS too large, increase storage class for timerCounter"
#endif

//BIG TODO: change to PURE C, to enable static linkage (!!!)

class DisplayControlClass {
public:
    // setup SPI and timer interrupts - call at start
    static void setup();
    // set value to be displayed. Supports only integer values. Flag to show '-' in big display.
    static inline void setValue(unsigned int bigValue, unsigned int smallValue, uint8_t showMinus);

    static inline void setBrightness(uint8_t brightness); // set brightness level. valid values: MIN-MAX_BRIGHTNESS
    static inline void incBrightness(); // decrease brightness level till MIN_BRIGHTNESS
    static inline void decBrightness(); // increase brightness level till MAX_BRIGHTNESS

    // enable/disable dot point at given LED segment
    static inline void setDP(uint8_t ledSegment, uint8_t value);

    // updating display in interrupt
    static void updateDisplay();

    // SPI setup routine
    static void setupSPI();
    static inline uint8_t spiTransfer(uint8_t data);


private:
    // populate values to be send
    static void computeBigValues();
    static void computeSmallValues();

    // updating display timings
    static void updateTimings();

    // variables
    static uint8_t values[LED_DISPLAYS_CNT];  // values to be send
    static uint8_t DPstate[LED_DISPLAYS_CNT]; // dot-point state

    static unsigned int currBigValue;
    static unsigned int currSmallValue;

    static uint8_t currShowMinus;
    static uint8_t currBrightness;

    // display-connected variables
    static uint8_t displayDigit;
    static uint8_t timerCounter;
    static uint8_t timerCounterOnEnd;
};

extern DisplayControlClass DisplayControl;

inline void DisplayControlClass::setValue(unsigned int bigValue, unsigned int smallValue, uint8_t showMinus) {
    // for debug: check constraints
    if (bigValue > 9999 || (showMinus && bigValue > 999) || smallValue > 99) {
        Serial.print("Value too big: ");
        Serial.println(bigValue);
        return;
    }

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

inline void DisplayControlClass::setBrightness(uint8_t brightness) {
    if (brightness <= MAX_BRIGHTNESS && brightness >= MIN_BRIGHTNESS) {
        currBrightness = brightness;
    }
    updateTimings();
}
inline void DisplayControlClass::incBrightness() {
    if (currBrightness < MAX_BRIGHTNESS) {
        currBrightness += 1;
    }
    updateTimings();
}
inline void DisplayControlClass::decBrightness() {
    if (currBrightness > MIN_BRIGHTNESS) {
        currBrightness -= 1;
    }
    updateTimings();
}

inline void DisplayControlClass::setDP(uint8_t ledSegment, uint8_t value) {
    DPstate[ledSegment] = (value > 0) ? 0x80 : 0;
    if (ledSegment < LED_DISPLAYS_BIG_CNT) {
        computeBigValues();
    } else {
        computeSmallValues();
    }
}

// hand-made SPI transfer routine
inline uint8_t DisplayControlClass::spiTransfer(uint8_t data) {
    SPDR = data;                    // Start the transmission
    loop_until_bit_is_set(SPSR, SPIF);
    return SPDR;                    // return the received uint8_t, we don't need that
}

#endif //_DISPLAY_CONTROL_H
