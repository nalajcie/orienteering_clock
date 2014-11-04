#include "display_control.h"
#include "hw.h"

#include <Arduino.h>

/* TODO: license
 *
 * Some code based on SevenSeg library by Sigvald Marholm (http://playground.arduino.cc/Main/SevenSeg)
 */

// declaration of static class and it's variables
DisplayControlClass DisplayControl;

byte DisplayControlClass::values[LED_DISPLAYS_CNT];  // values to be send
byte DisplayControlClass::DPstate[LED_DISPLAYS_CNT]; // dot-point state

unsigned int DisplayControlClass::currBigValue;
unsigned int DisplayControlClass::currSmallValue;

byte DisplayControlClass::currShowMinus;
byte DisplayControlClass::currBrightness;

// display-connected variables
DisplayState DisplayControlClass::displayState;
byte DisplayControlClass::displayDigit;
long int DisplayControlClass::timerCounter;
long int DisplayControlClass::timerCounterOnEnd;
long int DisplayControlClass::timerCounterOffEnd;

static const byte digit_values[] = {
    0xFC, // 0 -> 1111 1100
    0x60, // 1 -> 0110 0000
    0xDA, // 2 -> 1101 1010
    0xF2, // 3 -> 1111 0010
    0x66, // 4 -> 0110 0110
    0xB6, // 5 -> 1011 0110
    0xBE, // 6 -> 1011 1110
    0xE0, // 7 -> 1110 0000
    0xFE, // 8 -> 1111 1110
    0xF6, // 9 -> 1111 0110
};

#define VALUE_MINUS (2) // - -> 0000 0010

// for quicker pin switching
#define LATCH_PIN_PORTB (DISPLAY_LATCH_PIN - 8)
#define G_PIN_PORTB     (DISPLAY_G_PIN - 8)

void DisplayControlClass::setup() {

    // setup pins
    pinMode(DISPLAY_G_PIN, OUTPUT);
    pinMode(DISPLAY_LATCH_PIN, OUTPUT);
    setupSPI();

    // init values
    for (int i = 0; i < LED_DISPLAYS_CNT; ++i) {
        DPstate[i] = 0;
    }

    setBrightness(DEFAULT_BRIGHTNESS);
    setValue(DEFAULT_BIG_VALUE, DEFAULT_SMALL_VALUE, 1);

    // setup timer (2) interrupt

    cli();  // Temporarily stop interrupts

    // See registers in ATmega328 datasheet
    // the interrupt will be each (Clock_freq/(64*3))
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1  = 0;                             // Initialize counter value to 0
    OCR1A = 3;                              // Set compare Match Register to 3
    TCCR1B |= (1 << WGM12);		            // Turn on CTC mode
    TCCR1B |= (1 << CS11) | (1 << CS10);    // Set prescaler to 64
    TIMSK1 |= (1 << OCIE1A);                // Enable timer compare interrupt

    sei();  // Continue allowing interrupts

    // update timings to get reasonable values to timerCounterOn/OffEnd.
    updateTimings();
}

void DisplayControlClass::setupSPI() {
    // use direct port mappings to omit SPI library
    byte clr;
    SPCR |= ((1<<SPE) | (1<<MSTR));   // enable SPI as master
    SPCR &= ~((1<<SPR1) | (1<<SPR0)); // clear prescaler bits
    clr = SPSR; // clear SPI status reg
    clr = SPDR; // clear SPI data reg
    SPSR |= (1<<SPI2X); // set prescaler bits
    delay(10);
}

void DisplayControlClass::updateTimings() {
    long int digitOnMs = (((long) ONE_DIGIT_MS) * currBrightness) / MAX_BRIGHTNESS;
    long int digitOffMs = ONE_DIGIT_MS - digitOnMs;

    // Artefacts in duty cycle control appeared when these values changed while interrupts happening (A kind of stepping in brightness appeared)
    cli();
    timerCounterOnEnd = (digitOnMs == 0) ? 0 : ((digitOnMs / TIMER_DIVIDER_MS) - 1);
    timerCounterOffEnd = (digitOffMs == 0) ? 0 : ((digitOffMs / TIMER_DIVIDER_MS) - 1);
    timerCounter=0;
    sei();
}

// timed interrupt
void DisplayControlClass::updateDisplay() {
    // Increment the library's counter
    timerCounter++;

    if ((displayState == DISPLAY_ON) && (timerCounter >= timerCounterOnEnd)) {
        // Finished with on-part. Turn off digit, and switch to the off-phase (_timerPhase=0)
        timerCounter = 0;
        displayState = DISPLAY_OFF;

        // setting G pin high will quickly disable the outputs
        bitSet(PORTB, G_PIN_PORTB);
    } else if ((displayState == DISPLAY_OFF) && (timerCounter >= timerCounterOffEnd)) {
        // Finished with the off-part. Switch to next digit and turn it on.
        timerCounter = 0;
        displayState = DISPLAY_ON;
        displayDigit = (displayDigit + 1) % LED_DISPLAYS_CNT;

        // display next digit
        bitSet(PORTB, LATCH_PIN_PORTB);
        spiTransfer(values[displayDigit]);  // segments to display current digits (low-side driver)
        spiTransfer(1 << displayDigit);     // digit numer (high-side driver)
        bitClear(PORTB, LATCH_PIN_PORTB);

        // enable high-side driver outputs
        bitClear(PORTB, G_PIN_PORTB);
    }
}

// TODO: display leading '0's in hour mode
void DisplayControlClass::computeBigValues() {
    int index = LED_DISPLAYS_BIG_CNT - 1;
    int value = currBigValue;
    int currDigit;
    int valueEndIdx = -1;

    // set ALL value registers to clear previous digits and set DPstate
    // use do-while to display at least one '0'
    do {
        currDigit = value % 10;
        value = value / 10;
        values[index] = ~(digit_values[currDigit] | DPstate[index]); // negation because of Common-Anode
        index -= 1;

        // check if value has ended and mark first free slot
        if (valueEndIdx < 0 && value == 0) {
            valueEndIdx = index;
        } else {
            // value has already ended, do not display zeros
            values[index] = ~(DPstate[index]); // negation because of Common-Anode
        }
    } while (index >= 0);

    if (currShowMinus && (valueEndIdx >= 0)) {
        values[valueEndIdx] = VALUE_MINUS | DPstate[index];
    }
}

void DisplayControlClass::computeSmallValues() {
    int index = LED_DISPLAYS_CNT - 1;
    int value = currSmallValue;
    int currDigit;

    // always display leading '0's
    do {
        currDigit = value % 10;
        value = value / 10;
        values[index] = digit_values[currDigit] | DPstate[index];
        index -= 1;
    } while (index >= LED_DISPLAYS_BIG_CNT);
}

// connect updateDisplay to timer(2) interrupt
ISR(TIMER1_COMPA_vect) {
    DisplayControl.updateDisplay();
}
