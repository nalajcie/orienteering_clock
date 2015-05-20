#include "test.h"
#include "display_control.h"
#include "hw.h"
#include <Arduino.h>

#ifdef TEST_SPI
// tests SPI transfers (writing and reading) 
void test_spi()
{
    // TODO: hadcoded, move to defines  
    pinMode(9, OUTPUT);
    pinMode(10, OUTPUT);
    pinMode(11, OUTPUT);
    pinMode(12, INPUT);
    pinMode(13, OUTPUT);

    setupSPI();
    digitalWrite(9, LOW);
    //digitalWrite(10, LOW);
    //shiftOut(11, 13, MSBFIRST, 0x0F);
    //shiftOut(11, 13, MSBFIRST, 0xF0);

    digitalWrite(10, HIGH);
    byte incoming0 = spiTransfer(0x0F);
    byte incoming1 = spiTransfer(0xF0);
    byte incoming2 = spiTransfer(0x0F);
    byte incoming3 = spiTransfer(0xF0);
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
#endif

