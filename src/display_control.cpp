#include "display_control.h"
#include "test.h"
#include "hw.h"

#include <Arduino.h>

/* TODO: license
 *
 * Some code based on SevenSeg library by Sigvald Marholm (http://playground.arduino.cc/Main/SevenSeg)
 */

// static variables
static uint8_t values[LED_DISPLAYS_CNT];  // values to be send
static uint8_t DPstate[LED_DISPLAYS_CNT]; // dot-point state

static unsigned int currBigValue;
static unsigned int currSmallValue;

static uint8_t currShowMinus;
static uint8_t currBrightness;
static uint8_t currMaxBrightness;
static uint8_t currMode;

// display-related variables
static uint8_t displayDigit;
static uint8_t timerCounter;
static uint8_t timerCounterOnEnd;

// support overriding default output with other text
static const uint8_t* override_big;
static uint8_t override_small[2];

static uint16_t override_times;


// values for each of the digit
static const uint8_t digit_values[] = {
    0x3F, // 0 -> 1111 1100 MSB = 0011 1111 LSB
    0x06, // 1 -> 0110 0000 MSB = 0000 0110 LSB
    0x5B, // 2 -> 1101 1010 MSB = 0101 1011 LSB
    0x4F, // 3 -> 1111 0010 MSB = 0100 1111 LSB
    0x66, // 4 -> 0110 0110 MSB = 0110 0110 LSB
    0x6D, // 5 -> 1011 0110 MSB = 0110 1101 LSB
    0x7D, // 6 -> 1011 1110 MSB = 0111 1101 LSB
    0x07, // 7 -> 1110 0000 MSB = 0000 0111 LSB
    0x7F, // 8 -> 1111 1110 MSB = 0111 1111 LSB
    0x6F, // 9 -> 1111 0110 MSB = 0110 1111 LSB
};

static const uint8_t word_BUZZ[] = {0x7F, 0x3E, 0x5B, 0x5B};
static const uint8_t word_BATT[] = {0x7F, 0x77, 0x31, 0x31};

#define VALUE_MINUS (0x40) // - -> 0100 0000

// for quicker pin switching
#define LATCH_PIN_PORTB (DISPLAY_LATCH_PIN - 8)
#define G_PIN_PORTB     (DISPLAY_G_PIN - 8)

///-------------------///
/// INTERNAL ROUTINES ///
///-------------------///

///////// SPI-related functions ////////////

#ifndef TEST_SPI
static
#endif
void setupSPI() {
    pinMode(DISPLAY_SPI_MOSI_PIN, OUTPUT);
    pinMode(UNUSED_SPI_MISO_PIN, INPUT);
    pinMode(DISPLAY_SPI_SCK_PIN,  OUTPUT);

    // use direct port mappings to omit SPI library
    uint8_t clr;
    SPCR |= ((1<<SPE) | (1<<MSTR));   // enable SPI as master
    SPCR &= ~((1<<SPR1) | (1<<SPR0)); // clear prescaler bits
    clr = SPSR;         // clear SPI status reg
    clr = SPDR;         // clear SPI data reg
    SPSR |= (1<<SPI2X); // set prescaler bits
    (void)clr;          // avoid "unused" warning
    delay(10);
}

// hand-made SPI transfer routine
#ifndef TEST_SPI
static
#endif
uint8_t spiTransfer(uint8_t data) {
    SPDR = data;                    // Start the transmission
    loop_until_bit_is_set(SPSR, SPIF);
    return SPDR;                    // return the received uint8_t, we don't need that
}

///////// TIMER-related functions ////////////

static void setupTimer() {
    // setup timer (2) interrupt
    cli();  // Temporarily stop interrupts

    // See registers in ATmega328 datasheet
    // the interrupt will be each (Clock_freq/(64*TIME_COMPARER))
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1  = 0;                             // Initialize counter value to 0
    OCR1A = TIMER_COMPARER;                 // Set compare Match Register to 3
    TCCR1B |= (1 << WGM12);                 // Turn on CTC mode
    TCCR1B |= (1 << CS11) | (1 << CS10);    // Set prescaler to 64
    TIMSK1 |= (1 << OCIE1A);                // Enable timer compare interrupt

    sei();  // Continue allowing interrupts
}

static void updateTimings() {
    // we need to have all outputs closed for at least one cycle to close MOSFETs
    // otherwise there will be ghosting on max brightness
    timerCounterOnEnd = BRIGHTNESS_STEP_TICKS * currBrightness - 1;

#ifdef DEBUG_DISPLAY
    Serial.print("TIMINGS: timerCounterOnEnd = ");
    Serial.print(timerCounterOnEnd);
    Serial.print(", timerCounterOffEnd = ");
    Serial.println(ONE_DIGIT_TICKS);
#endif
}

// timed interrupt
static void updateDisplay() {
    if (timerCounter == 0) { // switch to next digit and turn it on.
        displayDigit = (displayDigit + 1) % LED_DISPLAYS_CNT;

        if (override_times == 0) {
            // display next digit
            spiTransfer(values[displayDigit]);  // segments to display current digits (low-side driver)
            spiTransfer(1 << displayDigit);     // digit numer (high-side driver)
        } else {
            // segments to display current digits (low-side driver)
            if (displayDigit < LED_DISPLAYS_BIG_CNT) {
                spiTransfer(override_big[displayDigit]);
            } else {
                spiTransfer(override_small[displayDigit-LED_DISPLAYS_BIG_CNT]);
            }
            spiTransfer(1 << displayDigit);     // digit numer (high-side driver)
            override_times -= 1;
        }

        // move data from shift-registers to output-registers
        bitClear(PORTB, LATCH_PIN_PORTB);
        bitSet(PORTB, LATCH_PIN_PORTB);

        // enable high-side driver outputs
        bitClear(PORTB, G_PIN_PORTB);
    }

    if (timerCounter == timerCounterOnEnd) {
        // Finished with on-part. Turn off digit, and switch to the off-phase
        // setting G pin high will quickly disable the outputs
        bitSet(PORTB, G_PIN_PORTB);
    }

    // increment the library's counter
    timerCounter = (timerCounter + 1) % ONE_DIGIT_TICKS;
}

// connect updateDisplay to timer(2) interrupt
ISR(TIMER1_COMPA_vect) {
    updateDisplay();
}

///////// TIMER-related functions ////////////

static void computeBigValues() {
    int value = currBigValue;
    int8_t index = LED_DISPLAYS_BIG_CNT - 1;
    uint8_t currDigit;
    int8_t valueEndIdx = -1;

    if (currMode == MODE_HOURS) {
        // change MINUTES to HOURS.MINUTES
        value = (value / 60) * 100 + value % 60;
    }

    // set ALL value registers to clear previous digits and set DPstate
    // use do-while to display at least one '0'
    do {
        currDigit = value % 10;
        value = value / 10;
        values[index] = digit_values[currDigit] | DPstate[index];

        // check if value has ended and mark first free slot
        if (valueEndIdx < 0 && value == 0) {
            valueEndIdx = index - 1;
        } else if (valueEndIdx >= 0) {
            // value has already ended, do not display zeros
            values[index] = DPstate[index];
        }

        index -= 1;
    } while (index >= 0);

    if (currMode == MODE_HOURS) {
        // add leading zeros in HOURS mode
        while (valueEndIdx > 0) {
            values[valueEndIdx] = digit_values[0] | DPstate[valueEndIdx];
            valueEndIdx -= 1;
        }
    }

    if (currShowMinus && (valueEndIdx >= 0)) {
        values[valueEndIdx] = VALUE_MINUS | DPstate[valueEndIdx];
    }

#ifdef DEBUG_DISPLAY
    for (int i = 0; i < LED_DISPLAYS_CNT; ++i) {
        Serial.print("values[");
        Serial.print(i);
        Serial.print("] = ");
        Serial.println(values[i]);
    }
#endif
}

static void computeSmallValues() {
    int value = currSmallValue;
    int currDigit;
#ifndef BUG_INVERTED_SMALL_DISPLAYS
    int index = LED_DISPLAYS_BIG_CNT + LED_DISPLAYS_SMALL_CNT - 1;
#else
    int index = LED_DISPLAYS_BIG_CNT;
#endif
    // always display leading '0's
    do {
        currDigit = value % 10;
        value = value / 10;
        values[index] = digit_values[currDigit] | DPstate[index];
#ifndef BUG_INVERTED_SMALL_DISPLAYS
        index -= 1;
    } while (index >= LED_DISPLAYS_BIG_CNT);
#else
        index += 1;
    } while (index < LED_DISPLAYS_BIG_CNT + LED_DISPLAYS_SMALL_CNT);

#endif
}

///--------------------///
/// EXTERNAL INTERFACE ///
///--------------------///
void display_setup() {
#ifdef DEBUG_DISPLAY
    Serial.println("DC::setup");
    Serial.print("REFRESH_RATE=");
    Serial.println(REFRESH_RATE);
#endif

    // setup pins
    pinMode(DISPLAY_G_PIN, OUTPUT);
    pinMode(DISPLAY_LATCH_PIN, OUTPUT);
    bitSet(PORTB, G_PIN_PORTB);
    setupSPI();

    // init values
    memset(DPstate, 0, sizeof(DPstate));
    memset(values, 0, sizeof(values));
    currMode = MODE_MINUTES;
    currMaxBrightness = MAX_BRIGHTNESS;
    currBrightness = DEFAULT_BRIGHTNESS;
    override_times = 0;
    timerCounter = 0;
    displayDigit = LED_DISPLAYS_CNT - 1;

    // force change at first
    currSmallValue = 0xFF;
    currBigValue = 0xFFFF;

    // setup timings
    setupTimer();
    updateTimings();
}

void display_setValue(unsigned int bigValue, unsigned int smallValue, uint8_t showMinus) {
#ifdef DEBUG_DISPLAY
    // for debug: check constraints
    if (bigValue > 9999 || (showMinus && bigValue > 999) || smallValue > 99) {
        Serial.print("Value too big: ");
        Serial.println(bigValue);
        return;
    }
#endif

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

void display_setBrightness(uint8_t brightness) {
    if (brightness <= currMaxBrightness && brightness >= MIN_BRIGHTNESS) {
        currBrightness = brightness;
    }
    updateTimings();
}
void display_incBrightness() {
    if (currBrightness < currMaxBrightness) {
        currBrightness += 1;
    }
    updateTimings();
}
void display_decBrightness() {
    if (currBrightness > MIN_BRIGHTNESS) {
        currBrightness -= 1;
    }
    updateTimings();
}
void display_setMaxBrightness(uint8_t max_brightness) {
    uint8_t old_brightness = currMaxBrightness;
    currMaxBrightness = max_brightness;

    // (0) check if anything changes
    if (old_brightness == currMaxBrightness) {
        return;
    }

    // (1) lowering down max brightness - check if we need to lower down current brightness
    if (max_brightness < old_brightness) {
        if (currBrightness > currMaxBrightness) {
            currBrightness = currMaxBrightness;
            updateTimings();
        }
    } else {
        // (2) going up with max brightness - try to increase current brightness
        while (currBrightness < DEFAULT_BRIGHTNESS) {
            display_incBrightness();
        }
    }
}

void display_setDP(uint8_t ledSegment, uint8_t value) {
    DPstate[ledSegment] = (value > 0) ? 0x80 : 0;
    if (ledSegment < LED_DISPLAYS_BIG_CNT) {
        computeBigValues();
    } else {
        computeSmallValues();
    }
}

void display_setMode(uint8_t mode) {
    // add 'comma' point
    DPstate[1] = (mode == MODE_HOURS) ? 0x80 : 0;
    currMode = mode;
    computeBigValues();
}

void display_toggleMode() {
    display_setMode(!currMode);
}

void display_showBuzzState(int buzzState) {
    override_big = word_BUZZ;
#ifndef BUG_INVERTED_SMALL_DISPLAYS
    override_small[0] = 0;
    override_small[1] = digit_values[buzzState];
#else
    override_small[0] = digit_values[buzzState];
    override_small[1] = 0;
#endif

    override_times = REFRESH_RATE * LED_DISPLAYS_CNT; // about 1s
}

void display_showBattState(int percent) {
    override_big = word_BATT;
    if (percent > 99) {
        override_small[0] = VALUE_MINUS;
        override_small[1] = VALUE_MINUS;
    } else {
#ifndef BUG_INVERTED_SMALL_DISPLAYS
        override_small[0] = digit_values[percent / 10];
        override_small[1] = digit_values[percent % 10];
#else
        override_small[0] = digit_values[percent % 10];
        override_small[1] = digit_values[percent / 10];
#endif
    }

    override_times = REFRESH_RATE * LED_DISPLAYS_CNT; // about 1s
}

