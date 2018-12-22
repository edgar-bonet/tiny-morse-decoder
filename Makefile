# Makefile for tiny-morse-decoder.

# Avrdude options for accessing the ISP programmer. Format:
#   -c programmer-id -P port -b baudrate
# The following would be appropriate for "Arduino as ISP":
#PROGRAMMER = -c stk500v1 -P /dev/ttyACM0 -b 19200
# The USBasp does not require the -P and -b options.
PROGRAMMER = -c usbasp

# The MCU target can be overridden on the command line, e.g.
#   make MCU=attiny85
MCU    = attiny13a

CFLAGS = -mmcu=$(MCU) -std=gnu11 -fshort-enums -Os -Wall -Wextra -g
TARGET = tiny-morse-decoder.elf

# Avrdude expects the ATtiny13A to be called "attiny13".
ifeq "$(MCU)" "attiny13a"
    AVRDUDE_MCU = attiny13
else
    AVRDUDE_MCU = $(MCU)
endif
AVRDUDE_OPTS = -p $(AVRDUDE_MCU) $(PROGRAMMER)

all: $(TARGET)

list: $(TARGET:%.elf=%.lss)

upload: $(TARGET)
	avrdude $(AVRDUDE_OPTS) -U $<

clean:
	rm -f $(TARGET) $(TARGET:%.elf=%.lss)

%.elf: %.c
	avr-gcc $(CFLAGS) $< -o $@
	avr-size --format=avr --mcu=$(MCU) $@

%.lss: %.elf
	avr-objdump -S $< > $@

.PHONY: all list upload clean
