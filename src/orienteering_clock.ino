#include "hw.h"
#include "display_control.h"

#define DEBOUNCE 10  // button debouncer, how many ms to debounce, 5+ ms is usually plenty
#define BUZZER_SHORT_BEEP  100 // in ms
#define BUZZER_LONG_BEEP   350 // in ms


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
    DisplayControl.setBrightness(5);


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

    //DisplayControl.setValue(8888, 88, 0);
    //DisplayControl.updateDisplay();
    /*
    for (int i = 0; i < 6; ++i) {
      DisplayControl.setDP(i, 0);
    }
    */

    // start with "-600 seconds"
    //time_offset = (-600L)*1000 + millis();
    time_offset = -60000L + millis();
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
    
    // when we start, we clear out the "just" indicators
    for (index = 0; index < NUMBUTTONS; ++index) { 
        justpressed[index] = 0;
        justreleased[index] = 0;
    }

    if ((lasttime + DEBOUNCE) > millis()) {
        // not enough time has passed to debounce
        return;
    }
    // ok we have waited DEBOUNCE milliseconds, lets reset the timer
    lasttime = millis();

    for (index = 0; index < NUMBUTTONS; ++index) { // when we start, we clear out the "just" indicators
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

int buzzerState = 0;

void loop() {

   /*
    digitalWrite(10, LOW);
    shiftOut(11, 13, MSBFIRST, 0xF0);
    shiftOut(11, 13, MSBFIRST, 0x0F);
    //digitalWrite(10, LOW);
    digitalWrite(10, HIGH);
    Serial.println("SENT using digitalWrite");
    digitalWrite(9, LOW);
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

    // BUTTONS: handle button presses
    check_switches();
    if (pressed[BUTTON_SET_IDX]) {
        if (justpressed[BUTTON_UP_IDX]) {
            Serial.print("UP: before=");
            Serial.print(time_offset);
            Serial.print("; after=");
            time_offset += 60000L; // add one minute
            Serial.println(time_offset);
        } else if (justpressed[BUTTON_DOWN_IDX]) {
            time_offset -= 60000L; // substract one minute
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
    
    


    // update time
    long int curr_secs = millis() + time_offset;
    long int curr_ms = curr_secs % 1000;
    curr_secs = curr_secs / 1000;
    unsigned int curr_mins = abs(curr_secs / 60);
    unsigned int only_secs = abs(curr_secs % 60);
    DisplayControl.setValue(curr_mins, only_secs, (curr_secs < 0));
    //DisplayControl.setValue(abs(curr_secs), 0, (curr_secs < 0));
    //DisplayControl.setBrightness(only_secs % 10 + 1);

    // handle buzzer
    if (curr_secs < 0) {
      only_secs = 59 - only_secs; //only_secs = 59..0
      curr_ms = 999 + curr_ms; //curr_ms = -999... 0
    }
    
    if (only_secs > 55 || only_secs == 0) {
      //Serial.println(curr_ms);
      int buzzerOff = (only_secs == 0) ? BUZZER_LONG_BEEP : BUZZER_SHORT_BEEP;
      if (curr_ms < buzzerOff && buzzerState == 0) {
        Serial.print("BUZZ ON: ");
        Serial.println(curr_ms);
        buzzerState = 1;
        digitalWrite(buzzerPin, 1);
      } else if (curr_ms >= buzzerOff && buzzerState == 1) {      
        Serial.print("BUZZ OFF: ");
        Serial.println(curr_ms);
        buzzerState = 0;
        digitalWrite(buzzerPin, 0);
      }
    }
    /*
       if (Serial.available() > 0) {
                // read the incoming byte:
                byte byte1 = Serial.read();
                val = val * 10 + (byte1 - '0');
                count += 1;
                if (count == 2) {
                  Serial.print("I received: ");
                  Serial.println(val, DEC);
                  DisplayControl.setValue(1234, val, 0);
                  count = 0; val = 0;
                }
        }
        */
}

