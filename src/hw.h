/* hardware pinout connection defines. May change with hardware revision */
#ifndef _HW_H
#define _HW_H

#define CURR_HW_REVISION 1


#if CURR_HW_REVISION == 1
/***** Arduino UNO Rev 3 ******/
#define DISPLAY_G_PIN            9 /* for quickly holding every output of high-side driver LOW */
#define DISPLAY_LATCH_PIN       10 /* also SPI SS port */

/* handled automatically by SPI library */
#define DISPLAY_SPI_MOSI_PIN    11
#define UNUSED_SPI_MISO         12
#define DISPLAY_SPI_SCK_PIN     13

/* buttons - the analog 0-3 pins are also known as 14-17 */
#define BUTTON_UP               14
#define BUTTON_DOWN             15
#define BUTTON_SET              16
#define BUTTON_MODE             17

/* indexes in the table */
#define BUTTON_UP_IDX           0
#define BUTTON_DOWN_IDX         1
#define BUTTON_SET_IDX          2
#define BUTTON_MODE_IDX         3

#endif // CURR_HW_REVISION == 1


#endif // _HW_H

