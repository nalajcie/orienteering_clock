#include "hw.h"
#include "display_control.h"

#define DEBOUNCE 10  // button debouncer, how many ms to debounce, 5+ ms is usually plenty

// here is where we define the buttons that we'll use. button "1" is the first, button "5" is the 5th, etc
byte buttons[] = {BUTTON_UP, BUTTON_DOWN, BUTTON_SET, BUTTON_MODE};
// This handy macro lets us determine how big the array up above is, by checking the size
#define NUMBUTTONS sizeof(buttons)
// we will track if a button is just pressed, just released, or 'currently pressed'
byte pressed[NUMBUTTONS], justpressed[NUMBUTTONS], justreleased[NUMBUTTONS];

const int buzzerPin = 3;


// difference between "real time" and "millis()" in milliseconds
long time_offset;

void setup() {
    // DEBUG: initialize serial
    Serial.begin(9600);
    Serial.println("reset");

    // DISPLAY: setup display control
    DisplayControl.setup();

    //set pins to output because they are addressed in the main loop
    pinMode(buzzerPin, OUTPUT);
    pinMode(9, OUTPUT);
    pinMode(10, OUTPUT);
    pinMode(11, OUTPUT);
    pinMode(13, OUTPUT);

    // BUTTONS: Make input & enable pull-up resistors on switch pins

    for (int i = 0; i < NUMBUTTONS; ++i) {
        pinMode(buttons[i], INPUT);
        digitalWrite(buttons[i], HIGH);
    }

    //DisplayControl.setValue(1234, 88, 0);
    //DisplayControl.updateDisplay();
    //DisplayControl.setDP(3, 1);
   
    // start with "-600 seconds"
    //time_offset = (-600L)*1000 + millis();
    time_offset = -60000L + millis();
    Serial.println(time_offset);
}

// BUTTONS: check the buttons state (with debouncing)
void check_switches() {
    static byte previousstate[NUMBUTTONS];
    static byte currentstate[NUMBUTTONS];
    static long lasttime;
    byte index;

    // TODO: will not happen?
    if (millis() < lasttime) { // we wrapped around, lets just try again
        lasttime = millis();
    }

    if ((lasttime + DEBOUNCE) > millis()) {
        // not enough time has passed to debounce
        return;
    }
    // ok we have waited DEBOUNCE milliseconds, lets reset the timer
    lasttime = millis();

    for (index = 0; index < NUMBUTTONS; ++index) { // when we start, we clear out the "just" indicators
        justpressed[index] = 0;
        justreleased[index] = 0;

        currentstate[index] = digitalRead(buttons[index]);   // read the button

        /*
        Serial.print(index, DEC);
        Serial.print(": cstate=");
        Serial.print(currentstate[index], DEC);
        Serial.print(", pstate=");
        Serial.print(previousstate[index], DEC);
        Serial.print(", press=");
        */

        if (currentstate[index] == previousstate[index]) {
            //TODO: think of a way to autorepeat keys
            if ((pressed[index] == LOW) && (currentstate[index] == LOW)) {
                // just pressed
                justpressed[index] = 1;
            }
            else if ((pressed[index] == HIGH) && (currentstate[index] == HIGH)) {
                // just released
                justreleased[index] = 1;
            }
            pressed[index] = !currentstate[index];  // remember, digital HIGH means NOT pressed
        }
        //Serial.println(pressed[index], DEC);
        previousstate[index] = currentstate[index];   // keep a running tally of the buttons
    }
}



void loop() {

   /*
    digitalWrite(10, LOW);
    shiftOut(11, 13, MSBFIRST, 0x0F);
    shiftOut(11, 13, MSBFIRST, 0x02);
    //digitalWrite(10, LOW);
    digitalWrite(10, HIGH);
    Serial.println("SENT using digitalWrite");
    //digitalWrite(9, LOW);
    delay(1000);
    digitalWrite(10, LOW);
    shiftOut(11, 13, MSBFIRST, 0xF0);
    shiftOut(11, 13, MSBFIRST, 0x02);
    //digitalWrite(10, LOW);
    digitalWrite(10, HIGH);
    Serial.println("SENT using digitalWrite");
    //digitalWrite(9, LOW);
    delay(1000);

*/
/*
if (debouncer.update()) {
    buttonState = debouncer.read();
    if (buttonState == HIGH) {
    pwm_time = pwm_time * 10;
    Serial.println(pwm_time);
    } else {
    Serial.println(pwm_time);
    }
    }
    */
/*
    // BUTTONS: handle button presses
    check_switches();
    if (pressed[BUTTON_SET_IDX]) {
        if (justpressed[BUTTON_UP_IDX]) {
            time_offset += 60*1000; // add one minute
        } else if (justpressed[BUTTON_DOWN_IDX]) {
            time_offset -= 60*1000; // substract one minute
        } else {
            // TODO?
        }
    } else {
        if (justpressed[BUTTON_UP_IDX]) {
            DisplayControl.incBrightness();
        } else if (justpressed[BUTTON_DOWN_IDX]) {
            DisplayControl.decBrightness();
        }
    }

    if (justpressed[BUTTON_MODE_IDX]) {
        //TODO
    }
*/
    // update time
    long int curr_secs = millis() + time_offset;
    curr_secs = curr_secs / 1000;
    unsigned int curr_mins = abs(curr_secs / 60);
    unsigned int only_secs = abs(curr_secs % 60);
    //DisplayControl.setValue(curr_mins, only_secs, (curr_secs < 0));
    DisplayControl.setValue(abs(curr_secs), 0, (curr_secs < 0));
    DisplayControl.setBrightness(only_secs % 10 + 1);


    // TODO: handle buzzer
}

