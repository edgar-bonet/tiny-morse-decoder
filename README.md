# tiny-morse-decoder: Morse decoder for ATtiny13A

This program translates [Morse code][], read directly from a [straight
key][], into a stream of ASCII characters delivered as a logic-level
[asynchronous serial][] signal. It was inspired by the [Morse challenge
for ATtiny experts][challenge] by Tom Boyd, but I slightly bent the
rules of the challenge:

1. Whereas the challenge requires the somewhat overpowered ATtiny85
   microcontroller, this program targets the much smaller [ATtiny13A][],
   although it does also work on the attinies 25, 45 and 85.
2. It's written in plain C and compiled with gcc, instead of going
   through the Arduino IDE.

[Morse code]: https://en.wikipedia.org/wiki/Morse_code
[straight key]: https://en.wikipedia.org/wiki/Telegraph_key
[asynchronous serial]: https://en.wikipedia.org/wiki/Asynchronous_serial_communication
[challenge]: http://sheepdogguides.com/arduino/aht8d-ATtiny%20Morse%20chall.htm
[ATtiny13A]: https://www.microchip.com/wwwproducts/en/ATtiny13A

## Features

* Debounces the key.
* Understands 54 characters: 26 letters, 10 digits and 18 punctuation
  symbols.
* 4 selectable keying speeds, from 5 to 18 words per minute. A change in
  selected speed requires a reset to be effective.
* On reset, flashes an “invitation to transmit” code on an LED at the
  selected speed. This is intended as a visual indication of the keying
  speed the user is expected to match.

## Usage

Connect:

* a voltage source (2.7 to 5.5 V) between Vcc and GND
* a straight key (or push button) between PB4 and GND
* an LED, with a suitable current-limiting resistor, or a
  self-oscillating buzzer between PB3 and GND
* a logic-level serial monitor to PB2.

Optionally, pins PB0 and/or PB1 can be connected to ground in order to
select a keying speed as per the following table:

|   PB1    |   PB0    | speed (wpm) |
|:--------:|:--------:|:-----------:|
| floating | floating |     5       |
| floating | grounded |     8       |
| grounded | floating |    12       |
| grounded | grounded |    18       |

In order to facilitate changing the selected speed, it is suggested to
add:

* a pair of DIP switches between PB0 and ground and between PB1 and GND,
  or an equivalent 4 positions coded rotary switch
* a push button between RESET and GND.

Below is the schematic of the suggested circuit:

![](img/schematic.svg)

The circuit can be easily breadboarded. If, however, you want something
more durable, check this [kit sold by Tom Boyd][kit], which is based on
a professionally built PCB.

The “logic-level serial monitor” mentioned above can be anything that is
able to process a logic-level asynchronous serial signal. It should be
configured to 9600/8N1, i.e. 9600&nbsp;baud, 8&nbsp;data bits, no
parity, one stop bit. Typically one would use a USB to TTL serial cable
connected to a computer running a serial terminal emulator, like
[putty][] or [GNU screen][]. On a Linux terminal, one can simply type
something like

```text
stty -F /dev/ttyUSB0 raw 9600 && cat /dev/ttyUSB0
```

An Arduino running a “do nothing” sketch can be used as an alternative
to the USB to TTL serial cable: power the ATtiny from the Arduino GND
and 5V pins, then connect the ATtiny serial output (PB2) to the Arduino
TX pin. When doing this, it is important that the Arduino sketch does
not initialize its serial port. The Arduino serial monitor can then be
used as an alternative to the serial terminal emulator.

[kit]: http://sheepdogguides.com/elec/pcb/PCB271-ATtiny%20Morse%20brd1.htm
[putty]: https://www.chiark.greenend.org.uk/~sgtatham/putty/
[GNU screen]: https://www.gnu.org/software/screen/

## Compiling and uploading

You need GNU make, avr-gcc, avrdude and an ISP programmer. An Arduino
[can be used as an ISP programmer][arduino-isp].

To compile, type

```text
make MCU=<mcu name>
```

where `<mcu_name>` should be either `attiny13a`, `attiny25`, `attiny45`
or `attiny85`. Simply typing

```text
make
```

compiles for the default target, which is the ATtiny13A.

To upload, edit the Makefile, set the `PROGRAMMER` variable to match
your programmer, connect the programmer to the microcontroller and to
the computer, then type

```text
make MCU=<mcu_name> upload
```

If uploading to an ATtiny13A, you can omit `MCU=attiny13a`.

Alternatively, gcc and avrdude can be called directly as:

```text
avr-gcc -mmcu=attiny13a -std=gnu11 -fshort-enums -Os \
    tiny-morse-decoder.c -o tiny-morse-decoder.elf
avrdude -p attiny13 -c usbasp -U tiny-morse-decoder.elf
```

But make sure to replace `-c usbasp` by the avrdude options appropriate
for your programmer.

Another alternative is to open the dummy file tiny-morse-decoder.ino
with the Arduino IDE. For this, you will need a board support package
matching the microcontroller you are using, in order to be able to
select it in the “Board” menu. The code provided by the board support
package will not be used.

[arduino-isp]: https://www.arduino.cc/en/Tutorial/ArduinoISP

## Files

This repository contains the following files and directories:

* [README.md](README.md): this document
* [tiny-morse-decoder.c](tiny-morse-decoder.c): program source
* [Makefile](Makefile): Makefile for GNU make
* [internals.md](internals.md): explanation of the program internals
* [img](img/) directory: images for the documentation
* [tools](tools/) directory: tooling for code generation and testing
