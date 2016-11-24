#ifndef _MEMMORY_H
#define _MEMMORY_H

#ifdef __cplusplus 
extern "C" {
#endif

extern char* serial_number;

// setup function
void mem_setup(void);

// last buzzer state before poweroff
int mem_was_buzzer_active(void);
void mem_set_buzzer_active(int active);

// run it in main loop
void mem_update(long int curr_ms);

#ifdef __cplusplus 
}
#endif
#endif //_MEMMORY_H
