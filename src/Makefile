# Arduino Make file. Refer to https://github.com/sudar/Arduino-Makefile

# bootstrapping board:
# make burn_bootloader


# every other time:
# make ispload


ifeq ($(OS),Windows_NT)
    # configuration
    ARDMK_DIR	= ../Arduino-Makefile
    ARDUINO_DIR	= ../../../Arduino
    ISP_PROG    = usbasp
    ISP_PORT    = usb
endif

ifeq ($(MAKECMDGOALS),burn_bootloader)
	# first time we have to use lower clock, because we're using internal oscillator
	# burning bootloader will also set the fuses to use external 16MHz osc
	AVRDUDE_OPTS = -v -B 20.96 
endif

BOARD_TAG     = uno
# differs than arduino - does not erease EEPROM
ISP_HIGH_FUSE =0xD6
ISP_LOW_FUSE  =0xFF
ISP_EXT_FUSE  =0x05

include ../Arduino-Makefile/Arduino.mk

