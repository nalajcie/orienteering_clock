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

// enable for debugging
//#define DEBUG_DISPLAY

// configuration
#define LED_DISPLAYS_BIG_CNT  4
#define LED_DISPLAYS_SMALL_CNT  2
#define LED_DISPLAYS_CNT  (LED_DISPLAYS_BIG_CNT + LED_DISPLAYS_SMALL_CNT)

#define MIN_BRIGHTNESS 2
#define MAX_BRIGHTNESS 10

#define DEFAULT_BIG_VALUE   10
#define DEFAULT_SMALL_VALUE 0
#define DEFAULT_BRIGHTNESS  7

//TODO: tidy it here
#define DESIRED_REFRESH_RATE (625/9) // about 70 Hz, so 70/6 per digit, ONE_DIGIT_TICKS == 200
//1000000/69.444444444444444/6/12 == 200 TICKs/digit

// do not change these:
// TODO: showud we increase TIMER_DIVISOR to be call the update at a lower frequency?
#define CLOCK_HZ 16000000
#define TIMER_DIVISOR (64*3)
#define ONE_TICK_US (1000000L / (CLOCK_HZ / TIMER_DIVISOR)) // exactly 12us

#define ONE_CYCLE_US (1000000L / DESIRED_REFRESH_RATE)
#define ONE_DIGIT_US (ONE_CYCLE_US / LED_DISPLAYS_CNT)  // for 50Hz: 3333.3333

#define ONE_DIGIT_TICKS  (ONE_DIGIT_US / ONE_TICK_US)


enum DisplayState {
    DISPLAY_ON,
    DISPLAY_OFF,
};

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

    // updating display in interrupt
    static void updateDisplay();

    // SPI setup routine
    static void setupSPI();
    static inline byte spiTransfer(byte data);


private:
    // populate values to be send
    static void computeBigValues();
    static void computeSmallValues();

    // updating display timings
    static void updateTimings();

    // variables
    static byte values[LED_DISPLAYS_CNT];  // values to be send
    static byte DPstate[LED_DISPLAYS_CNT]; // dot-point state

    static unsigned int currBigValue;
    static unsigned int currSmallValue;

    static byte currShowMinus;
    static byte currBrightness;

    // display-connected variables
    static DisplayState displayState;
    static byte displayDigit;
    static long int timerCounter;
    static long int timerCounterOnEnd;
};

extern DisplayControlClass DisplayControl;

inline void DisplayControlClass::setValue(unsigned int bigValue, unsigned int smallValue, byte showMinus) {
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

inline void DisplayControlClass::setBrightness(byte brightness) {
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

inline void DisplayControlClass::setDP(byte ledSegment, byte value) {
    DPstate[ledSegment] = (value > 0) ? 0x80 : 0;
    if (ledSegment < LED_DISPLAYS_BIG_CNT) {
        computeBigValues();
    } else {
        computeSmallValues();
    }
}

// hand-made SPI transfer routine
inline byte DisplayControlClass::spiTransfer(byte data) {
    SPDR = data;                    // Start the transmission
    loop_until_bit_is_set(SPSR, SPIF);
    return SPDR;                    // return the received byte, we don't need that
}

#endif //_DISPLAY_CONTROL_H
