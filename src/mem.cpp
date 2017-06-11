#include "mem.h"
#include "hw.h"

#include <Arduino.h>
#include <EEPROM.h>

//#define DEBUG_EEPROM
#define AUTOSAVE_PERIOD_MS ((uint32_t) 120 * 1000)

#pragma pack(push, 1)
typedef struct const_params_s
{
    uint16_t magic;     // CONST_PARAMS_MAGIC
    char serial_no[16];
    char prod_date[16]; // YYYY-MM-DD

    // todo: min/max voltage?
} const_params_t;
#define CONST_PARAMS_MAGIC    0x574D // 'WM'
#define CONST_PARAMS_MAX_SIZE 64

typedef struct log_params_s
{
    uint16_t magic;               // LOG_PARAMS_MAGIC
    uint16_t poweron_cnt;         // how many times the device was powered on

    uint32_t total_poweron_secs;  // for how many minutes the device was on (note: the newest log entry will have the highest value here)
    uint32_t max_poweron_secs;    // maximum time for which the device was powered on

    uint8_t  buzzer_active;       // last buzzer state


} log_params_t;
#define LOG_PARAMS_MAGIC    0x776D // 'wm'
#define LOG_PARAMS_MAX_SIZE 32

#pragma pack(pop)

#define BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2*!!(condition)]))


static const_params_t consts;
static log_params_t params;

static unsigned int last_log_address = EEPROM.length();
static uint32_t last_log_save_ms = 0;


static void read_const_params(void)
{
    EEPROM.get(0, consts);

    if (consts.magic != CONST_PARAMS_MAGIC) {
        memset(&consts, 0, sizeof(consts));
        Serial.println(F("Empty const config!"));
        Serial.setTimeout(5000); // wait 5 seconds for input
        Serial.print(F("Enter SN: "));
        int ret = Serial.readBytesUntil('\n', consts.serial_no, sizeof(consts.serial_no) - 1);
        if (!ret) {
            return;
        }
        Serial.println();
        Serial.print("Enter production date: ");
        ret = Serial.readBytesUntil('\n', consts.prod_date, sizeof(consts.prod_date) - 1);
        if (!ret) {
            return;
        }

        Serial.println("Saving consts config");
        consts.magic = CONST_PARAMS_MAGIC;
        EEPROM.put(0, consts);
    }

    Serial.print(F("SN: ")); Serial.println(consts.serial_no);
    Serial.print(F("production date: ")); Serial.println(consts.prod_date);
}

static unsigned int next_log_address(unsigned int curr) {
    if ((curr + LOG_PARAMS_MAX_SIZE) >=  EEPROM.length()) {
        return CONST_PARAMS_MAX_SIZE;
    } else {
        return curr + LOG_PARAMS_MAX_SIZE;
    }
}

static void read_log_params(void)
{
    log_params_t tmp_params;
    unsigned int log_start = CONST_PARAMS_MAX_SIZE;

    memset(&params, 0, sizeof(params));
    params.buzzer_active = 1; //default

    while (log_start + LOG_PARAMS_MAX_SIZE < EEPROM.length()) {
        // note: LSB architecture
        uint16_t magic = (uint16_t) EEPROM[log_start + 1] << 8 | EEPROM[log_start];
#ifdef DEBUG_EEPROM
       Serial.print(" magic: "); Serial.println(magic, 16);
#endif

        if (magic == LOG_PARAMS_MAGIC) {
            EEPROM.get(log_start, tmp_params);
#ifdef DEBUG_EEPROM
            Serial.print("  LOG at "); Serial.println(log_start);
            Serial.print("  total_poweron_secs: "); Serial.println(tmp_params.total_poweron_secs);
#endif
            if (params.total_poweron_secs < tmp_params.total_poweron_secs) {
                memcpy(&params, &tmp_params, sizeof(params));
                last_log_address = log_start;
            }
        } else {
            break; // free space, no more logs written
        }
        log_start += LOG_PARAMS_MAX_SIZE;
    }

    params.magic = LOG_PARAMS_MAGIC;
}


static void update_and_save_log(void)
{
    const uint32_t curr_ms = millis();
    if (params.max_poweron_secs < (curr_ms / 1000)) {
        params.max_poweron_secs = curr_ms / 1000;
    }

    params.total_poweron_secs += (curr_ms - last_log_save_ms) / 1000;

    const int log_addr = next_log_address(last_log_address);
    EEPROM.put(log_addr, params);
    last_log_save_ms = curr_ms;
    last_log_address = log_addr;

#ifdef DEBUG_EEPROM
    Serial.print("SAVE LOG at "); Serial.println(log_addr);
    Serial.print("  total_poweron_secs: "); Serial.println(params.total_poweron_secs);
#endif
}



static void print_log_params(log_params_t* p)
{
    Serial.print("poweron_cnt:\t"); Serial.println(p->poweron_cnt);
    Serial.print("total_poweron_secs:\t"); Serial.println(p->total_poweron_secs);
    Serial.print("max_poweron_secs:\t"); Serial.println(p->max_poweron_secs);
    Serial.print("buzzer_active:\t"); Serial.println(p->buzzer_active);
}

// setup function
void mem_setup(void)
{
    BUILD_BUG_ON(sizeof(const_params_t) > CONST_PARAMS_MAX_SIZE);
    BUILD_BUG_ON(sizeof(log_params_t) > LOG_PARAMS_MAX_SIZE);

    read_const_params();
    read_log_params();
    params.poweron_cnt += 1;

    print_log_params(&params);
    update_and_save_log();
}

int mem_was_buzzer_active(void) {
    return params.buzzer_active;
}

void mem_set_buzzer_active(int active) {
    params.buzzer_active = active;
    update_and_save_log();
}


void mem_update(long int curr_ms)
{
    if ((curr_ms - last_log_save_ms) > AUTOSAVE_PERIOD_MS) {
        update_and_save_log();
    }
}

