/* hardware pinout connection defines. May change with hardware revision */
#ifndef _HW_H
#define _HW_H

#define CURR_HW_REVISION 1

#if CURR_HW_REVISION == 1
/*
 * Arduino UNO Rev 3
 */
#define DISPLAY_LATCH_PIN       10 /* also SPI SS port */

/* handled automatically by SPI library */
#define DISPLAY_SPI_MOSI_PIN    11
#define UNUSED_SPI_MISO         12
#define DISPLAY_SPI_SCK_PIN     13


#endif // CURR_HW_REVISION == 1


#endif // _HW_H

