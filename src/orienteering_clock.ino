#include "hw.h"
#include "test.h"
#include "display_control.h"
#include "buttons.h"


#define BUZZER_SHORT_BEEP  100 // in ms
#define BUZZER_LONG_BEEP   350 // in ms

#define VOLTAGE_0          670
#define VOLTAGE_100        870

int buzzerState = 0;
int buzzerActive = 1;

int battPercent = 0;

static void readBattVoltage() {
    int voltage = analogRead(0);
    battPercent = (voltage - VOLTAGE_0) / 2;
    Serial.print("ANALOG 0: ");
    Serial.println(voltage);
    Serial.print("percent: ");
    Serial.println(battPercent);
}

// difference between "real time" and "millis()" in milliseconds
long time_offset;

void setup() {
    // DEBUG: initialize serial
    Serial.begin(9600);
    Serial.println("reset");

#ifdef TEST_SPI
    test_spi();
#else
    // DISPLAY: setup display control
    display_setup();
#endif

    // setup buzzer pin
    pinMode(BUZZ_CTL, OUTPUT);

    buttons_setup();
    pinMode(14, INPUT);

    // start with "-601 seconds" to display -10:00 at startup
    time_offset = -600999L - millis();

}

void loop() {

    // BUTTONS: handle button presses
    buttons_update();

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
            display_incBrightness();
        } else if (justpressed[BUTTON_DOWN_IDX]) {
            display_decBrightness();
        }
    }

    if (justpressed[BUTTON_MODE_IDX]) {
        // TODO: change mode
        buzzerActive = !buzzerActive;
        if (!buzzerActive && buzzerState) { // buzzer is now "playing" - silence it
            buzzerState = 0;
            digitalWrite(BUZZ_CTL, 0);
        }
        display_showBuzzState(buzzerActive);
    }

#ifdef HAS_BUTTON_BATT
    if (justpressed[BUTTON_BATT_IDX]) {
        readBattVoltage();
        display_showBattState(battPercent);
    }
#endif // HAS_BUTTON_BATT

    /// update time
    long int curr_secs = millis() + time_offset;
    long int curr_ms = curr_secs % 1000;
    curr_secs = curr_secs / 1000;
    unsigned int curr_mins = abs(curr_secs / 60);
    unsigned int only_secs = abs(curr_secs % 60);
    display_setValue(curr_mins, only_secs, (curr_secs < 0));


    /// handle buzzer
    if (buzzerActive) {
        if (curr_secs < 0) { // change negative values to proper ones
          only_secs = 59 - only_secs; //only_secs = 59..0
          curr_ms = 999 + curr_ms; //curr_ms = -999... 0
        }
        if (only_secs > 55 || only_secs == 0) {
            const int buzzerOff = (only_secs == 0) ? BUZZER_LONG_BEEP : BUZZER_SHORT_BEEP;
            if (curr_ms < buzzerOff && buzzerState == 0) {
                buzzerState = 1;
                digitalWrite(BUZZ_CTL, 1);
                readBattVoltage();
            } else if (curr_ms >= buzzerOff && buzzerState == 1) {
                buzzerState = 0;
                digitalWrite(BUZZ_CTL, 0);
            }
        }
    }
}
