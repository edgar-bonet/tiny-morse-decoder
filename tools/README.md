# Tooling for tiny-morse-decoder

This directory contains the following files, which were used during the
development of tiny-morse-decoder:

* raw-morse-code.h: Morse code in "raw", human-readable form; included
  by the two following programs
* make-code-table.c: generates the `morse_code[]` array used in
  tiny-morse-decoder.c
* auto-test.ino: tests the complete program using an Arduino.

These are described below.

## raw-morse-code.h

This is a version of the Morse code hand-written in a human-readable
form:

```c
static const raw_code_t raw_code[] = {
    /* letters */
    {'A', ".-"   }, {'B', "-..." }, {'C', "-.-." }, {'D', "-.."  },
    {'E', "."    }, {'F', "..-." }, {'G', "--."  }, {'H', "...." },
    {'I', ".."   }, {'J', ".---" }, {'K', "-.-"  }, {'L', ".-.." },
    {'M', "--"   }, {'N', "-."   }, {'O', "---"  }, {'P', ".--." },
    {'Q', "--.-" }, {'R', ".-."  }, {'S', "..."  }, {'T', "-"    },
    {'U', "..-"  }, {'V', "...-" }, {'W', ".--"  }, {'X', "-..-" },
    {'Y', "-.--" }, {'Z', "--.." },
    /* digits */
    {'0', "-----"}, {'1', ".----"}, {'2', "..---"}, {'3', "...--"},
    {'4', "....-"}, {'5', "....."}, {'6', "-...."}, {'7', "--..."},
    {'8', "---.."}, {'9', "----."},
    /* punctuation */
    {'.', ".-.-.-" }, {',', "--..--" }, {'?', "..--.." },
    {'\'', ".----."}, {'!', "-.-.--" }, {'/', "-..-."  },
    {'(', "-.--."  }, {')', "-.--.-" }, {'&', ".-..."  },
    {':', "---..." }, {';', "-.-.-." }, {'=', "-...-"  },
    {'+', ".-.-."  }, {'-', "-....-" }, {'_', "..--.-" },
    {'"', ".-..-." }, {'$', "...-..-"}, {'@', ".--.-." }
};
```

The file also defines the type `raw_code_t` and the macro
`RAW_CODE_LENGTH`.

## make-code-table.c

As explained in [Internals of tiny-morse-decoder](../internals.md), the
Morse code is stored in a `morse_code[]` array using a non-trivial but
efficient format. This array was generated programmatically on a PC by
make-code-table.c.

Running the program generates the following output, which was copied
verbatim to [tiny-morse-decoder.c](../tiny-morse-decoder.c):

```c
#define CODE_LENGTH 59

static __flash const uint16_t morse_code[CODE_LENGTH] = {
    363, 694, 221,   0, 375,   0,  61, 853, 214, 726,   0, 109,
    698, 190, 365, 110, 682, 341, 171,  87,  47,  31,  62, 122,
    234, 426, 490, 438,   0,  94,   0, 235, 437,   5,  30,  54,
     14,   1,  27,  26,  15,   3,  85,  22,  29,  10,   6,  42,
     53,  90,  13,   7,   2,  11,  23,  21,  46,  86,  58
};
```

## auto-test.ino

This Arduino program performs a functional test on tiny-morse-decoder.
It should be loaded on an Arduino which is then connected to both a
computer and the microcontroller (ATtiny13A/25/45/85) running
tiny-morse-decoder.c:

computer ↔ Arduino ↔ ATtiny

The Arduino is connected to the computer through USB. It sends
diagnostic information on the serial port at 9600/8N1. The connection to
the ATtiny is as follows:

| Arduino | ATtiny |
|:-------:|:------:|
|   GND   | GND    |
|    5V   | Vcc    |
|     8   | PB0    |
|     9   | PB1    |
|    10   | PB4    |
|    11   | RESET  |
|    13   | PB3    |
|  RX/0   | PB2    |

This program loops through the four available keying speeds. It selects
each speed by selectively grounding or floating the ATtiny's PB0 and PB1
pins, then grounding its RESET pin for 1&nbsp;ms. Each speed is then
tested by sending the list of all known characters with no interword
gaps, followed by random text with randomly placed interword gaps. The
Arduino LED should blink showing the dots and dashes being transmitted.
The ATtiny's output is read by the Arduino's serial port and compared to
what was sent. Any errors are reported on the serial monitor. If there
are no errors, the output of the Arduino should look like:

```text
=== Speed 0: 5 wpm (240 ms) ===
Sending all known characters:
ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,?'!/()&:;=+-_"$@
Sending random text:
NXWY@S G:ZQPJW7I HOIB1,,L ?(9$K M+QJRJG!)7/X"JL P1MFY$UV3LB4/F VQIS(HF 2

=== Speed 1: 8 wpm (150 ms) ===
Sending all known characters:
ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,?'!/()&:;=+-_"$@
[...]
```

Note that the Arduino peripherals are used in a somewhat unconventional
way:

* The serial port is used for two independent links: ATtiny → Arduino
  (reception) and Arduino → computer (emission).
* As pin 13 is not used by the program, the on-board LED is driven
  directly by the ATtiny.
