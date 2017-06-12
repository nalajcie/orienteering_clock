#include "hw.h"
#include "test.h"
#include "display_control.h"
#include "buttons.h"
#include "mem.h"

#define VERSION_STR F("1.2.2")

//#define DEBUG_SERIAL

#define BUZZER_SHORT_BEEP  100 // in ms
#define BUZZER_LONG_BEEP   350 // in ms

#define SET_HOLD_MS        3000 // 3 seconds

// raw values read by analog input
#define VOLTAGE_0          670
#define VOLTAGE_100        870

#define BATT_HISTERESIS          1

#define VOLTAGE_SENSE_EVERY_MS        500 // 0.5 second

static int8_t buzzerState = 0;
static int8_t buzzerActive = 1;
static int16_t battPercent = 0;

// difference between "real time" and "millis()" in milliseconds
static long time_offset;

static uint8_t voltage_sense_enabled = 1;
static uint8_t countdown_mode = 0;

static const uint8_t battery_level_percent[]    = {0, 10, 30, 50, 255};
static const uint8_t battery_level_brightness[] = {1, BRIGHTNESS_CRITICAL, BRIGHTNESS_LOW, BRIGHTNESS_WARN, MAX_BRIGHTNESS};

static void voltage_update(long int curr_time) {
#ifdef HAS_VIN_SENSE
    static long int last_volt_sense = 0;
    static uint16_t v_avg = (VOLTAGE_0 + VOLTAGE_100) / 2;
    static uint16_t v_sum_avg = v_avg * 4;
    static uint8_t batt_level = sizeof(battery_level_percent) - 1;

    if (!voltage_sense_enabled) {
        return;
    }

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

    if (battPercent + BATT_HISTERESIS <= battery_level_percent[batt_level - 1]) {
        batt_level -= 1;
    } else if (battPercent - BATT_HISTERESIS > battery_level_percent[batt_level]) {
        batt_level += 1;
    }

    display_setMaxBrightness(battery_level_brightness[batt_level]);

#ifdef DEBUG_SERIAL
    Serial.println(F("BATTERY STATE: "));
    Serial.print(F("\tANALOG 0: "));       Serial.println(v);
    Serial.print(F("\tSUM: "));            Serial.println(v_sum_avg);
    Serial.print(F("\tAVG: "));            Serial.println(v_avg);
    Serial.print(F("\tpercent: "));        Serial.println(battPercent);
    Serial.print(F("\tlevel: "));          Serial.println(batt_level);
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
            mem_set_buzzer_active(buzzerActive);
        }
#ifdef HAS_BUTTON_BATT
        else if (justpressed[BUTTON_BATT_IDX]) {
            display_showBattState(battPercent);
        }
#endif // HAS_BUTTON_BATT
    }
}

void setup() {
    // always initialize serial and output basic info
    Serial.begin(9600);
    Serial.println(F("reset"));
    Serial.print(F("HW REVISION: ")); Serial.println(CURR_HW_REVISION);
    Serial.print(F("VERSION: "));     Serial.println(VERSION_STR);
    mem_setup();
    buzzerActive = mem_was_buzzer_active();

#ifdef TEST_SPI
    test_spi();
#else
    // DISPLAY: setup display control
    display_setup();
#endif
#ifdef TEST_DISPLAY
    for (int i = 0; i < LED_DISPLAYS_CNT; ++i) {
        display_setDP(i, 1);
    }
#endif

    // setup buzzer pin
    pinMode(BUZZ_CTL, OUTPUT);

    buttons_setup();

#ifdef HAS_VIN_SENSE
    pinMode(VIN_SENSE_PIN, INPUT);

#ifdef HAS_BUTTON_BATT
    // overriding VIN sense capability:
    const uint8_t batt_not_pressed = digitalRead(BUTTON_BATT);
    if (!batt_not_pressed) {
        voltage_sense_enabled = 0;
    }
#endif // HAS_BUTTON_BATT
#endif // HAS_VIN_SENSE

    // if SET button is pressed at startup - start in countdown mode
    const uint8_t mode_not_pressed = digitalRead(BUTTON_SET);
    if (mode_not_pressed) {
        // start with "-601 seconds" to display -10:00 at startup
        time_offset = -600999L - millis();
    } else {
        // start with "-3000 seconds"
        countdown_mode = !mode_not_pressed;
        time_offset = -3000000L - millis();
    }

#ifdef DEBUG_SERIAL
    Serial.print(F("Startup took: ")); Serial.println(millis());
#endif
}

/// update displayed time
static void update_time_and_buzzer(long int curr_time) {
    long int curr_secs = curr_time + time_offset;
    long int curr_ms = curr_secs % 1000;
    curr_secs = curr_secs / 1000;
    unsigned int curr_mins = abs(curr_secs / 60);
    unsigned int only_secs = abs(curr_secs % 60);

    if (countdown_mode) {
        // in countdown mode we're displaying centiseconds on smaller display
        display_setValue(abs(curr_secs), abs(curr_ms/10), 0);
    } else {
        display_setValue(curr_mins, only_secs, (curr_secs < 0));
    }

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

#ifdef TEST_DISPLAY
static void test_display(long int curr_time) {
    static long int last_time = 0;
    static int val = 0;

    if (curr_time - last_time > 1000) {
        last_time = curr_time;
        val =  (val + 1) % 10;
        display_setValue(val*1000+val*100+val*10+val, val*10+val, 0);
    }
}
#endif

void loop() {
    long int curr_time = millis();

#ifdef TEST_DISPLAY
    // factory testing: display
    test_display(curr_time);
#else

    // handle button presses
    buttons_update(curr_time);
    check_button_state(curr_time);

    // update time
    update_time_and_buzzer(curr_time);

    // voltage sense
    voltage_update(curr_time);

    // saving stats to EEPROM
    mem_update(curr_time);
#endif
}
