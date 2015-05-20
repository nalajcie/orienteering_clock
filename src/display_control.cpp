#include "display_control.h"
#include "hw.h"

#include <Arduino.h>

/* TODO: license
 *
 * Some code based on SevenSeg library by Sigvald Marholm (http://playground.arduino.cc/Main/SevenSeg)
 */

#define DEBUG_DISPLAY
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

static const byte digit_values[] = {
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

#define VALUE_MINUS (0x40) // - -> 0100 0000

// for quicker pin switching
#define LATCH_PIN_PORTB (DISPLAY_LATCH_PIN - 8)
#define G_PIN_PORTB     (DISPLAY_G_PIN - 8)

void DisplayControlClass::setup() {
#ifdef DEBUG_DISPLAY
    Serial.println("DC::setup");
#endif

    // setup pins
    pinMode(DISPLAY_G_PIN, OUTPUT);
    digitalWrite(DISPLAY_G_PIN, LOW);
    pinMode(DISPLAY_LATCH_PIN, OUTPUT);
    setupSPI();

    // init values
    for (int i = 0; i < LED_DISPLAYS_CNT; ++i) {
        DPstate[i] = 0;
    }
    // force change at first
    currSmallValue = 0xFF;
    currBigValue = 0xFFFF;
    setBrightness(1);//DEFAULT_BRIGHTNESS);
    setValue(DEFAULT_BIG_VALUE, DEFAULT_SMALL_VALUE, 1);
    displayState = DISPLAY_OFF;
    displayDigit = LED_DISPLAYS_CNT - 1;

    // setup timer (2) interrupt
    cli();  // Temporarily stop interrupts

    // See registers in ATmega328 datasheet
    // the interrupt will be each (Clock_freq/(64*3))
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1  = 0;                             // Initialize counter value to 0
    OCR1A = 3;                              // Set compare Match Register to 3
    TCCR1B |= (1 << WGM12);                 // Turn on CTC mode
    TCCR1B |= (1 << CS11) | (1 << CS10);    // Set prescaler to 64
    TIMSK1 |= (1 << OCIE1A);                // Enable timer compare interrupt

    sei();  // Continue allowing interrupts

    // update timings to get reasonable values to timerCounterOnEnd.
    updateTimings();
}

void DisplayControlClass::setupSPI() {
    pinMode(DISPLAY_SPI_MOSI_PIN, OUTPUT);
    pinMode(UNUSED_SPI_MISO_PIN, INPUT);
    pinMode(DISPLAY_SPI_SCK_PIN,  OUTPUT);

    // use direct port mappings to omit SPI library
    byte clr;
    SPCR |= ((1<<SPE) | (1<<MSTR));   // enable SPI as master
    SPCR &= ~((1<<SPR1) | (1<<SPR0)); // clear prescaler bits
    clr = SPSR;         // clear SPI status reg
    clr = SPDR;         // clear SPI data reg
    SPSR |= (1<<SPI2X); // set prescaler bits
    (void)clr;          // avoid "unused" warning
    delay(10);
}

void DisplayControlClass::updateTimings() {
    // Artefacts in duty cycle control appeared when these values changed while interrupts happening (A kind of stepping in brightness appeared)
    //cli();
    timerCounterOnEnd = ((long) ONE_DIGIT_TICKS) * currBrightness / MAX_BRIGHTNESS;
    //sei();

#ifdef DEBUG_DISPLAY
    Serial.print("TIMINGS: timerCounterOnEnd = ");
    Serial.print(timerCounterOnEnd);
    Serial.print(", timerCounterOffEnd = ");
    Serial.println(ONE_DIGIT_TICKS);
#endif
}

// timed interrupt
void DisplayControlClass::updateDisplay() {

    if ((displayState == DISPLAY_ON) && (timerCounter >= timerCounterOnEnd)) {
        // Finished with on-part. Turn off digit, and switch to the off-phase
        // setting G pin high will quickly disable the outputs
        bitSet(PORTB, G_PIN_PORTB);
        displayState = DISPLAY_OFF;
    } else if ((displayState == DISPLAY_OFF) && (timerCounter >= ONE_DIGIT_TICKS)) {
        // Finished with the off-part. Switch to next digit and turn it on.
        timerCounter = 0;
        displayState = DISPLAY_ON;
        displayDigit = (displayDigit + 1) % LED_DISPLAYS_CNT;

        // display next digit
        spiTransfer(values[displayDigit]);  // segments to display current digits (low-side driver)
        spiTransfer(1 << displayDigit);     // digit numer (high-side driver)

        bitClear(PORTB, LATCH_PIN_PORTB);
        bitSet(PORTB, LATCH_PIN_PORTB);

        // enable high-side driver outputs
        bitClear(PORTB, G_PIN_PORTB);
    }

    // Increment the library's counter
    ++timerCounter;
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

void DisplayControlClass::computeSmallValues() {
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

// connect updateDisplay to timer(2) interrupt
ISR(TIMER1_COMPA_vect) {
    DisplayControl.updateDisplay();
}
