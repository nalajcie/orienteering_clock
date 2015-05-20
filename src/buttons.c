#include "buttons.h"
#include "hw.h"

#include <Arduino.h>

/* TODO: license
 *
 */

//#define DEBUG_BUTTONS

// here is where we define the buttons that we'll use. button "1" is the first, button "5" is the 5th, etc
#ifdef HAS_BUTTON_BATT
byte buttons[] = {BUTTON_UP, BUTTON_DOWN, BUTTON_SET, BUTTON_MODE, BUTTON_BATT};
#else
byte buttons[] = {BUTTON_UP, BUTTON_DOWN, BUTTON_SET, BUTTON_MODE};
#endif

// This handy macro lets us determine how big the array up above is, by checking the size
#define NUMBUTTONS sizeof(buttons)
// we will track if a button is just pressed, just released, or 'currently pressed'
byte pressed[NUMBUTTONS], justpressed[NUMBUTTONS], justreleased[NUMBUTTONS];


void buttons_setup() {
    // Make input & enable pull-up resistors on switch pins
    int i;
    for (i = 0; i < NUMBUTTONS; ++i) {
        pinMode(buttons[i], INPUT);
        digitalWrite(buttons[i], HIGH);
    }
}

// check the buttons state (with debouncing)
void buttons_update() {
    static byte previousstate[NUMBUTTONS];
    static byte currentstate[NUMBUTTONS];
    static long next_press[NUMBUTTONS];     // when the key should be auto-repeated
    static long lasttime;
    byte index;

    long int curr_ms = millis();
    // will not happen - we do not plan running for 49 days straight
    if (curr_ms < lasttime) { // we wrapped around, lets just try again
        lasttime = curr_ms;
    }

    // when we start, we clear out the "just" indicators
    for (index = 0; index < NUMBUTTONS; ++index) {
        justpressed[index] = 0;
        justreleased[index] = 0;
    }

    if ((lasttime + BUTTON_DEBOUNCE) > curr_ms) {
        // not enough time has passed to debounce
        return;
    }
    // ok we have waited BUTTON_DEBOUNCE milliseconds, lets reset the timer
    lasttime = curr_ms;

    for (index = 0; index < NUMBUTTONS; ++index) { // when we start, we clear out the "just" indicators
        currentstate[index] = digitalRead(buttons[index]);   // read the button

#ifdef DEBUG_BUTTONS
        Serial.print(index, DEC);
        Serial.print(": cstate=");
        Serial.print(currentstate[index], DEC);
        Serial.print(", pstate=");
        Serial.print(previousstate[index], DEC);
        Serial.print(", press=");
#endif

        // check for button state change
        if (currentstate[index] == previousstate[index]) {
            if ((pressed[index] == 0) && (currentstate[index] == LOW)) {
                // just pressed
                justpressed[index] = 1;
                next_press[index] = curr_ms + BUTTON_FIRST_REPEAT;
            }
            else if ((pressed[index] == 1) && (currentstate[index] == HIGH)) {
                // just released
                justreleased[index] = 1;
                next_press[index] = 0;
            }
            pressed[index] = !currentstate[index];  // remember, digital HIGH means NOT pressed

            // auto-repeat keys
            if (pressed[index] && next_press[index] < curr_ms) {
                justpressed[index] = 1;
                next_press[index] = curr_ms + BUTTON_NEXT_REPEAT;
            }
        }
#ifdef DEBUG_BUTTONS
        Serial.println(pressed[index], DEC);
#endif
        previousstate[index] = currentstate[index];   // keep a running tally of the buttons
    }
}

