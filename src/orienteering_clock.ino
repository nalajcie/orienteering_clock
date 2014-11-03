//#include <toneAC.h>
//#include <Bounce2.h>

#include "display_control.h"

//Pin connected to ST_CP of 74HC595
const int latchPin = 8;
//Pin connected to SH_CP of 74HC595
const int clockPin = 6;
//Pin connected to DS of 74HC595
const int dataPin = 11;
// PINS 9 & 10 connected to a buzzer

const int buzzerPin = 3;

#define DIGITS 1
#define BITS_PER_DIGIT 8
const int digitPins[DIGITS] = {
  2}; //4 common anode pins of the display

const int buttonPin = 4;     // the number of the pushbutton pin
const int ledPin =  13;      // the number of the LED pin

int buttonState = 0;         // variable for reading the pushbutton status
int currVal = 0;

//Bounce debouncer = Bounce();

//holder for infromation you're going to pass to shifting function
byte data = 0; 

unsigned long start;

void setup() {
    DisplayControl.setup();

  for (int i = 0; i < DIGITS; ++i) {
    pinMode(digitPins[i], OUTPUT);
  }

  //set pins to output because they are addressed in the main loop
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);

  pinMode(buttonPin, INPUT);

  /*
   Serial.begin(9600);
   Serial.println("reset");

   start = millis();

   debouncer.attach(buttonPin);
   debouncer.interval(15);
   */
  // declare pin 5 to be an output:
  //  pinMode(led, OUTPUT);
}

unsigned long int pwm_time = 1000;
int duty_cycle = 10;

// up, down: 0-15
void updateDisplay(long val) {
  // power off all digits 
  for(byte i = 0; i < DIGITS; ++i) {  
    //digitalWrite(digitPins[i], LOW);

    byte val_to_send = (val >> (i * BITS_PER_DIGIT)) & (~(1<<BITS_PER_DIGIT));
    //   Serial.print("val_to_send = ");
    //   Serial.println(~val_to_send);

    digitalWrite(latchPin, LOW);
    shiftOut(dataPin, clockPin, MSBFIRST, ((~val_to_send)));
    digitalWrite(latchPin, HIGH);

    digitalWrite(digitPins[i], HIGH);
    delayMicroseconds(duty_cycle * pwm_time / 10);
    digitalWrite(digitPins[i], LOW);
    delayMicroseconds((10-duty_cycle) * pwm_time / 10);

  }
}
int prev_secs = start;
int buzzerActive = 0;
int mul = 1;
void loop() {
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


  //function that blinks all the LEDs
  //gets passed the number of blinks and the pause time
  //blinkAll(1,500);

  // light each pin one by one using a function A
  unsigned long value = (millis() - start);
  unsigned int secs = value / 1000;
  if (secs - prev_secs >= 2) {
    prev_secs = secs;
    duty_cycle = (duty_cycle > 1) ? (duty_cycle - 1) : 10;
  }
  /*
    if (secs > 255) {
   start = millis();
   return;
   }
   */
  //    Serial.print("value = ");
  //    Serial.println(value);
  //updateDisplay(secs);
  updateDisplay(mul);
  mul = mul << 1;
  if (mul > 0xFF)
    mul = 1;

  /*
    // buzzer routine (4 last seconds
   if (secs % 16 == 0 && ((value % 1000) < 20)) {
   toneAC(294, 10, 800, true);
   } else if ((secs % 16 > 12)  && ((value % 1000) < 20)) {
   toneAC(196, 10, 200, true);
   }
   */


  /*   Serial.print("j = ");
   Serial.println(j);
   lightShiftPinA(j);
   delay(1000);*/
  //blinkAll(2,500);  

  // light each pin one by one using a function A
  /*
  for (int j = 4; j < 8; j++) {
   lowerPins(j);
   delay(100);
   }
   */
}

