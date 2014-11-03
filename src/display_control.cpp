#include "display_control.h"
#include "hw.h"

#include <Arduino.h>
#include <SPI.h>

DisplayControlClass DisplayControl;

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

// for quicker latching
#define LATCH_PIN_PORTB (DISPLAY_LATCH_PIN - 8)
  

static void DisplayControlClass::setup() {
    // setup SPI
    pinMode(DISPLAY_LATCH_PIN, OUTPUT);
    SPI.begin(); 

    // compute initial values
    for (int i = 0; i < LED_DISPLAYS_CNT; ++i) {
        DPstate[i] = 0;
    }

    setBrightness(DEFAULT_BRIGHTNESS);
    setValue(DEFAULT_BIG_VALUE, DEFAULT_SMALL_VALUE, 1);
    
    // setup timer interrupt

    cli();  // Temporarily stop interrupts

    // See registers in ATmega328 datasheet
    TCCR0A = 0;
    TCCR0B = 0;
    TCNT0 = 0;					// Initialize counter value to 0
    OCR0A = 3;					// Set Compare Match Register to 3
    TCCR0A |= (1<<WGM01);			// Turn on CTC mode
    TCCR0B |= (1<<CS01) | (1<<CS00);		// Set prescaler to 64
    TIMSK0 |= (1<<OCIE0A);			// Enable timer compare interrupt

    sei();  // Continue allowing interrupts

    // TODO: update delays to get reasonable values to _timerCounterOn/OffEnd.
    //updDelay();
    //_timerCounter=0;
}


// timed interrupt
static void DisplayControlClass::updateDisplay() {
  // power off all digits 
  for(byte i = 0; i < DIGITS; ++i) {  
    //digitalWrite(digitPins[i], LOW);

    byte val_to_send = (val >> (i * BITS_PER_DIGIT)) & (~(1<<BITS_PER_DIGIT));
    //   Serial.print("val_to_send = ");
    //   Serial.println(~val_to_send);

    digitalWrite(latchPin, LOW);
    shiftOut(dataPin, clockPin, MSBFIRST, ((~val_to_send)));
    digitalWrite(latchPin, HIGH);

    digitalWrite(digitPins[i], HIGH);
    delayMicroseconds(duty_cycle * pwm_time / 10);
    digitalWrite(digitPins[i], LOW);
    delayMicroseconds((10-duty_cycle) * pwm_time / 10);

  }
}

// TODO: display leading '0's in hour mode
static void DisplayControlClass::computeBigValues() {
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
        index -= 1;

        // check if value has ended and mark first free slot
        if (valueEndIdx < 0 && value == 0) {
            valueEndIdx = index;
        } else {
            // value has already ended, do not display zeros
            values[index] = DPstate[index];
        }
    } while (index >= 0);

    if (currShowMinus && valueEndIdx >= 0) {
        values[valueEndIdx] = VALUE_MINUS | DPstate[index];
    }
}

static void DisplayControlClass::computeSmallValues() {
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
