#ifndef _BUTTONS_H
#define _BUTTONS_H

#include <Arduino.h>

/* configuration */
#define BUTTON_DEBOUNCE          10 // button debouncer, how many ms to debounce, 5+ ms is usually plenty
#define BUTTON_FIRST_REPEAT     500 // how much to wait before first auto-repeat (in ms)
#define BUTTON_NEXT_REPEAT      100 // how much to wait before next auto-repeat  (in ms)

#ifdef __cplusplus 
extern "C" {
#endif

extern byte pressed[];
extern byte justpressed[];
extern byte justreleased[];


// setup function
void buttons_setup();

// run it in main loop
void buttons_update();

#ifdef __cplusplus 
}
#endif
#endif //_BUTTONS_H
