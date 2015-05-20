#include "hw.h"
#include "display_control.h"
#include "buttons.h"

#define TEST_SPI 0

#define BUZZER_SHORT_BEEP  100 // in ms
#define BUZZER_LONG_BEEP   350 // in ms


const int buzzerLed = 5;

int buzzerState = 0;
int buzzerActive = 1;


// difference between "real time" and "millis()" in milliseconds
long time_offset;

// TODO: move to debug
void test_spi()
{
    pinMode(9, OUTPUT);
    pinMode(10, OUTPUT);
    pinMode(11, OUTPUT);
    pinMode(12, INPUT);
    pinMode(13, OUTPUT);

    DisplayControl.setupSPI();
    digitalWrite(9, LOW);
    //digitalWrite(10, LOW);
    //shiftOut(11, 13, MSBFIRST, 0x0F);
    //shiftOut(11, 13, MSBFIRST, 0xF0);

    digitalWrite(10, HIGH);
    byte incoming0 = DisplayControl.spiTransfer(0x0F);
    byte incoming1 = DisplayControl.spiTransfer(0xF0);
    byte incoming2 = DisplayControl.spiTransfer(0x0F);
    byte incoming3 = DisplayControl.spiTransfer(0xF0);
    digitalWrite(10, LOW);
    digitalWrite(10, HIGH);

    Serial.print("SENT: 0x0F RECEIVED: ");
    Serial.println(incoming0);
    Serial.print("SENT: 0xF0 RECEIVED: ");
    Serial.println(incoming1);
    Serial.print("SENT: 0x0F RECEIVED: ");
    Serial.println(incoming2);
    Serial.print("SENT: 0xF0 RECEIVED: ");
    Serial.println(incoming3);
}


void setup() {
    // DEBUG: initialize serial
    Serial.begin(9600);
    Serial.println("reset");

#if TEST_SPI
    test_spi();
#else
    // DISPLAY: setup display control
    DisplayControl.setup();
#endif

    // setup buzzer pin
    pinMode(BUZZ_CTL, OUTPUT);

    setup_buttons();
    pinMode(14, INPUT);



    // enable buzzer LED if needed
    //DisplayControl.setDP(buzzerLed, (buzzerActive != 0));

    // start with "-601 seconds" to display -10:00 at startup
    time_offset = -600999L - millis();

}

void loop() {

    // BUTTONS: handle button presses
    update_buttons();

    /// buttons state machine
    if (justpressed[BUTTON_SET_IDX]) {
        //TODO: do something here?
    }

    if (pressed[BUTTON_SET_IDX]) {
        if ((pressed[BUTTON_UP_IDX] && justpressed[BUTTON_DOWN_IDX])
            || (pressed[BUTTON_DOWN_IDX] && justpressed[BUTTON_UP_IDX])) {
            // reset seconds
            time_offset = (time_offset / (60000L)) * (60000L);
        } else if (justpressed[BUTTON_UP_IDX]) {
            time_offset += 60000L; // add one minute
        } else if (justpressed[BUTTON_DOWN_IDX]) {
            time_offset -= 60000L; // substract one minute
        }
    } else {
        if (justpressed[BUTTON_UP_IDX]) {
            DisplayControl.incBrightness();
        } else if (justpressed[BUTTON_DOWN_IDX]) {
            DisplayControl.decBrightness();
        }
    }

    if (justpressed[BUTTON_MODE_IDX]) {
        // TODO: change mode
        // TODO: toggle buzzer using different button?
        buzzerActive = !buzzerActive;
        if (!buzzerActive && buzzerState) { // buzzer is now "playing" - silence it
            buzzerState = 0;
            digitalWrite(BUZZ_CTL, 0);
        }
        //DisplayControl.setDP(buzzerLed, (buzzerActive != 0));
    }

    /// update time
    long int curr_secs = millis() + time_offset;
    long int curr_ms = curr_secs % 1000;
    curr_secs = curr_secs / 1000;
    unsigned int curr_mins = abs(curr_secs / 60);
    unsigned int only_secs = abs(curr_secs % 60);
    DisplayControl.setValue(curr_mins, only_secs, (curr_secs < 0));


    /// handle buzzer
    if (buzzerActive) {
        if (curr_secs < 0) { // change negative values to proper ones
          only_secs = 59 - only_secs; //only_secs = 59..0
          curr_ms = 999 + curr_ms; //curr_ms = -999... 0
        }
        if (only_secs > 55 || only_secs == 0) {
            const int buzzerOff = (only_secs == 0) ? BUZZER_LONG_BEEP : BUZZER_SHORT_BEEP;
            if (curr_ms < buzzerOff && buzzerState == 0) {
                Serial.print("BUZZ ON: ");
                Serial.println(curr_ms);
                int voltage = analogRead(0);
                Serial.print("ANALOG 0: ");
                Serial.println(voltage);
                buzzerState = 1;
                digitalWrite(BUZZ_CTL, 1);
            } else if (curr_ms >= buzzerOff && buzzerState == 1) {
                Serial.print("BUZZ OFF: ");
                Serial.println(curr_ms);
                buzzerState = 0;
                digitalWrite(BUZZ_CTL, 0);
            }
        }
    }

}

