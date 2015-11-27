#include "hw.h"
#include "test.h"
#include "display_control.h"
#include "buttons.h"

#define DEBUG_SERIAL

#define BUZZER_SHORT_BEEP  100 // in ms
#define BUZZER_LONG_BEEP   350 // in ms

#define SET_HOLD_MS        3000 // 3 seconds

// raw values read by analog input
#define VOLTAGE_0          670
#define VOLTAGE_100        870

#define BATT_PERCENT_OK          100
#define BATT_PERCENT_WARN        50
#define BATT_PERCENT_LOW         30
#define BATT_PERCENT_CRITICAL    10
#define BATT_PERCENT_CRITICAL    10

#define VOLTAGE_SENSE_EVERY_MS        500 // 0.5 second

int8_t buzzerState = 0;
int8_t buzzerActive = 1;
int16_t battPercent = 0;

// difference between "real time" and "millis()" in milliseconds
long time_offset;

static int8_t get_batt_level(int8_t percent) {
    if (percent > BATT_PERCENT_WARN) {
        return BATT_PERCENT_OK;
    } else if (percent > BATT_PERCENT_LOW) {
        return BATT_PERCENT_WARN;
    } else if (percent > BATT_PERCENT_CRITICAL) {
        return BATT_PERCENT_LOW;
    } else {
        return BATT_PERCENT_CRITICAL;
    }
}

static void voltage_update(long int curr_time) {
#ifdef HAS_VIN_SENSE
    static long int last_volt_sense = 0;
    static uint16_t v_avg = (VOLTAGE_0 + VOLTAGE_100) / 2;
    static uint16_t v_sum_avg = v_avg * 4;
    static uint8_t batt_level = BATT_PERCENT_OK;

    if ((curr_time - last_volt_sense) < VOLTAGE_SENSE_EVERY_MS) {
        return;
    }

    last_volt_sense = curr_time;

    // simple averaging on last 4 readouts
    uint16_t v = analogRead(VIN_SENSE_PIN - 14);
    v_sum_avg += v - v_avg;
    v_avg = v_sum_avg / 4;
    if (v_avg > VOLTAGE_0) {
        battPercent = (v_avg - VOLTAGE_0) / 2;
    } else {
        battPercent = 0;
    }

    int8_t new_batt_level = get_batt_level(battPercent);
    if (new_batt_level != batt_level) {
        // state transition - TODO: add more visual aids!
        batt_level = new_batt_level;
        switch (batt_level) {
            case BATT_PERCENT_OK:
                display_setMaxBrightness(MAX_BRIGHTNESS);
                break;
            case BATT_PERCENT_WARN:
                display_setMaxBrightness(BRIGHTNESS_WARN);
                break;
            case BATT_PERCENT_LOW:
                display_setMaxBrightness(BRIGHTNESS_LOW);
                break;
            case BATT_PERCENT_CRITICAL:
                display_setMaxBrightness(BRIGHTNESS_CRITICAL);
                break;
        }
    }


#ifdef DEBUG_SERIAL
    Serial.print("BATTERY STATE: ");
    Serial.print("\tANALOG 0: ");       Serial.println(v);
    Serial.print("\tSUM: ");            Serial.println(v_sum_avg);
    Serial.print("\tAVG: ");            Serial.println(v_avg);
    Serial.print("\tpercent: ");        Serial.println(battPercent);
    Serial.print("\tlevel: ");          Serial.println(battPercent);
#endif
#endif
}


static void check_button_state(long int curr_time) {
    // buttons state machine
    // FUNCTIONS:
    // + UP, DOWN       -> INC/DEC brightness
    // + SET + UP/DOWN  -> change MINUTES
    // + MODE           -> change buzzer state
    // + BATT           -> display battery state
    // + SET + MODE     -> change display mode
    // + SET (hold 3s)  -> reset seconds

    static long int set_pressed_since = 0;

    // SET hold sensing
    if (justpressed[BUTTON_SET_IDX]) {
        if (set_pressed_since == 0) {
            set_pressed_since = curr_time;
        }
    } else if (justreleased[BUTTON_SET_IDX]) {
        set_pressed_since = 0; // will start sensing for SET hold next time
    }

    if (pressed[BUTTON_SET_IDX]) {
        if (justpressed[BUTTON_UP_IDX]) {
            set_pressed_since = -1;// do not sense the SET hold
            time_offset += 60000L; // add one minute
        } else if (justpressed[BUTTON_DOWN_IDX]) {
            set_pressed_since = -1;// do not sense the SET hold
            time_offset -= 60000L; // substract one minute
        } else if (justpressed[BUTTON_MODE_IDX]) {
            set_pressed_since = -1;// do not sense the SET hold
            display_toggleMode();
        } else if (set_pressed_since > 0 && (curr_time - set_pressed_since) > SET_HOLD_MS) {
            // SET hold
            time_offset -= (time_offset + curr_time) % 60000L;   // reset seconds
            set_pressed_since = -1;                              // do not sense the SET hold
        }
    } else {
        if (justpressed[BUTTON_UP_IDX]) {
            display_incBrightness();
        } else if (justpressed[BUTTON_DOWN_IDX]) {
            display_decBrightness();
        } else if (justpressed[BUTTON_MODE_IDX]) {
            buzzerActive = !buzzerActive;
            if (!buzzerActive && buzzerState) { // buzzer is now "playing" - silence it
                buzzerState = 0;
                digitalWrite(BUZZ_CTL, 0);
            }
            display_showBuzzState(buzzerActive);
        }
#ifdef HAS_BUTTON_BATT
        else if (justpressed[BUTTON_BATT_IDX]) {
            display_showBattState(battPercent);
        }
#endif // HAS_BUTTON_BATT
    }
}

void setup() {
#ifdef DEBUG_SERIAL
    // DEBUG: initialize serial
    Serial.begin(9600);
    Serial.println("reset");
#endif

#ifdef TEST_SPI
    test_spi();
#else
    // DISPLAY: setup display control
    display_setup();
#endif

    // setup buzzer pin
    pinMode(BUZZ_CTL, OUTPUT);

    buttons_setup();

#ifdef HAS_VIN_SENSE
    pinMode(VIN_SENSE_PIN, INPUT);
#endif

    // start with "-601 seconds" to display -10:00 at startup
    time_offset = -600999L - millis();

}

/// update displayed time
static void update_time_and_buzzer(long int curr_time) {
    long int curr_secs = curr_time + time_offset;
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
            } else if (curr_ms >= buzzerOff && buzzerState == 1) {
                buzzerState = 0;
                digitalWrite(BUZZ_CTL, 0);
            }
        }
    }
}

void loop() {
    long int curr_time = millis();

    // handle button presses
    buttons_update(curr_time);
    check_button_state(curr_time);

    // update time
    update_time_and_buzzer(curr_time);

    //TODO: voltage sense
    voltage_update(curr_time);

}
