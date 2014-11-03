#include "display_control.h"
#include "hw.h"

#include <Arduino.h>

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
    setupSPI();

    // compute initial values
    for (int i = 0; i < LED_DISPLAYS_CNT; ++i) {
        DPstate[i] = 0;
    }

    setBrightness(DEFAULT_BRIGHTNESS);
    setValue(DEFAULT_BIG_VALUE, DEFAULT_SMALL_VALUE, 1);

    // setup timer (2) interrupt

    cli();  // Temporarily stop interrupts

    // See registers in ATmega328 datasheet
    // the interrupt will be each (Clock_freq/64/3)
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1  = 0;                             // Initialize counter value to 0
    OCR1A = 3;                              // Set compare Match Register to 3
    TCCR1B |= (1 << WGM12);		            // Turn on CTC mode
    TCCR1B |= (1 << CS11) | (1 << CS10);    // Set prescaler to 64
    TIMSK1 |= (1 << OCIE1A);                // Enable timer compare interrupt

    sei();  // Continue allowing interrupts

    // TODO: update delays to get reasonable values to _timerCounterOn/OffEnd.
    //updDelay();
    //_timerCounter=0;
}

static void DisplayControlClass::setupSPI() {
    // use direct port mappings to omit SPI library
    byte clr;
    SPCR |= ((1<<SPE) | (1<<MSTR));   // enable SPI as master
    SPCR &= ~((1<<SPR1) | (1<<SPR0)); // clear prescaler bits
    clr = SPSR; // clear SPI status reg
    clr = SPDR; // clear SPI data reg
    SPSR |= (1<<SPI2X); // set prescaler bits
    delay(10);
}

static void DisplayControlClass::updateTimings() {

    // On-time for each display is total time spent per digit times the duty cycle. The
    // off-time is the rest of the cycle for the given display.

    long int digitOnUS = (((long) ONE_DIGIT_US) * currBrightness) / MAX_BRIGHTNESS;
    long int digitOffUS = ONE_DIGIT_US - digitOnUS;
    _digitOnDelay=temp;
    _digitOffDelay=_digitDelay-_digitOnDelay;

    // Artefacts in duty cycle control appeared when these values changed while interrupts happening (A kind of stepping in brightness appeared)
    cli();
    _timerCounterOnEnd=(_digitOnDelay/16)-1;
    _timerCounterOffEnd=(_digitOffDelay/16)-1;
    if(_digitOnDelay==0) _timerCounterOnEnd=0;
    if(_digitOffDelay==0) _timerCounterOffEnd=0;
    _timerCounter=0;
    sei();
}


// timed interrupt
static void DisplayControlClass::updateDisplay() {
    // Increment the library's counter
    _timerCounter++;

    // Finished with on-part. Turn off digit, and switch to the off-phase (_timerPhase=0)
    if((_timerCounter >= _timerCounterOnEnd) && (_timerPhase == 1)) {
        _timerCounter=0;
        _timerPhase=0;

        //TODO: set G pin high!
    }

    // Finished with the off-part. Switch to next digit and turn it on.
    if((_timerCounter >= _timerCounterOffEnd)&&(_timerPhase == 0)) {
        _timerCounter = 0;
        _timerPhase=1;

        _timerDigit++;

        if (_timerDigit>=_numOfDigits) _timerDigit=0;

        //TODO: display current digit
    }

  // power off all digits
  for(byte i = 0; i < DIGITS; ++i) {
    //digitalWrite(digitPins[i], LOW);

    byte val_to_send = (val >> (i * BITS_PER_DIGIT)) & (~(1<<BITS_PER_DIGIT));
    //   Serial.print("val_to_send = ");
    //   Serial.println(~val_to_send);

    digitalWrite(latchPin, LOW);
    shiftOut(dataPin, clockPin, MSBFIRST, ((~val_to_send)));
    digitalWrite(latchPin, HIGH)

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

// connect updateDisplay to timer(2) interrupt
ISR(TIMER1_COMPA_vect) {
    DisplayControl.updateDisplay();
}
