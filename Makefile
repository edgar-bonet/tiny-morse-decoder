# Makefile for tiny-morse-decoder.

# Avrdude options for accessing the ISP programmer. Format:
#   -c programmer-id -P port -b baudrate
# The following would be appropriate for "Arduino as ISP":
#PROGRAMMER = -c stk500v1 -P /dev/ttyACM0 -b 19200
# The USBasp does not require the -P and -b options.
PROGRAMMER = -c usbasp

MCU    = attiny13a
CFLAGS = -mmcu=$(MCU) -std=gnu11 -fshort-enums -Os -Wall -Wextra -g
TARGET = tiny-morse-decoder.elf
AVRDUDE_OPTS = -p attiny13 $(PROGRAMMER)

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
