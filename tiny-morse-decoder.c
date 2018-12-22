/*
 * tiny-morse-decoder.c: Morse decoder for ATtiny13A/25/45/85.
 *
 * Connect:
 *  - a voltage source (2.7 to 5.5 V) between Vcc and GND
 *  - a straight key (or push button) between PB4 and GND
 *  - an LED, with a suitable current-limiting resistor, or a
 *    self-oscillating buzzer between PB3 and GND
 *  - a logic-level serial monitor to PB2
 *
 * Optionally, ground either or both PB0 and PB1 to select the keying
 * speed according to the following table:
 *
 *        PB1       PB0    speed (wpm)
 *      ------------------------------
 *      floating  floating     5
 *      floating  grounded     8
 *      grounded  floating    12
 *      grounded  grounded    18
 *
 * Speed changes are only effective after a reset.
 *
 * Copyright (c) 2018 Edgar Bonet Orozco.
 * This file is part of tiny-morse-decoder, licensed under the terms of
 * the MIT license. See file LICENSE for details.
 */

#include <stdbool.h>
#include <avr/io.h>
#include <avr/power.h>
#include <util/atomic.h>

/*
 * Pinout. It can be changed, but beware that PB0 and PB1 are reserved
 * for selecting the keying speed.
 */
#define KEY_PIN  PB4
#define LED_PIN  PB3
#define TX_PIN   PB2

/* Available keying rates in words per minute. */
#define KEY_RATE_0  5
#define KEY_RATE_1  8
#define KEY_RATE_2 12
#define KEY_RATE_3 18

/* Baud rate of the serial data output. */
#define BAUD_RATE 9600

/* Compatibility with ATtiny25/45/85. */
#if __AVR_ATtiny25__ || __AVR_ATtiny45__ || __AVR_ATtiny85__
#  define F_CPU 8000000  // internal clock with prescaler = 1
#  define TIMSK0 TIMSK
#  define TIFR0 TIFR
#  undef  TIM0_COMPA_vect
#  define TIM0_COMPA_vect TIMER0_COMPA_vect
#  undef  TIM0_COMPB_vect
#  define TIM0_COMPB_vect TIMER0_COMPB_vect
#elif __AVR_ATtiny13A__
#  define F_CPU 9600000  // internal clock with prescaler = 1
#else
#  error "Unsupported MCU."
#endif

/* Timing calculations. */
#define TIMER_TOP ((int)((float)F_CPU/8/BAUD_RATE+0.5) - 1)
#define TIC_FREQ ((float)F_CPU/8/(TIMER_TOP+1))           // in Hz
#define DOT_TIME(rate) ((uint16_t)(1.2/(rate)*TIC_FREQ))  // in tics
#define DEBOUNCE_TIME  ((uint16_t)(0.01*TIC_FREQ+0.5))    // in tics


/***********************************************************************
 * Timekeeping.
 *
 * The timer is configured to generate two interrupts at 9.6 kHz:
 *  - TIM0_COMPA for counting system tics
 *  - TIM0_COMPB for driving the software UART.
 */

static void init_timer(void)
{
    OCR0A  = TIMER_TOP;    // period = 125 * 8 = 1000 CPU cycles
    OCR0B  = TIMER_TOP/2;  // interleave TIM0_COMPB and TIM0_COMPA
    TCCR0A = _BV(WGM01);   // clear timer on compare match
    TCCR0B = _BV(CS01);    // clock at F_CPU / 8
    TIMSK0 = _BV(OCIE0A);  // enable TIM0_COMPA interrupt
}

/*
 * This variable is our "clock". It is incremented at 9.6 kHz:
 *  - resolution: 104.2 us
 *  - roll-over period: 6.83 s
 */
static volatile uint16_t system_tics;

/* Routine servicing the TIM0_COMPA interrupt. */
ISR(TIM0_COMPA_vect)
{
    system_tics++;
}

/*
 * Return the current time in "tics". Note that system_tics being two
 * bytes, it has to be read with interrupts disabled in order to avoid
 * race conditions, hence the ATOMIC_BLOCK.
 */
static uint16_t tics(void)
{
    uint16_t system_tics_copy;
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
        system_tics_copy = system_tics;
    }
    return system_tics_copy;
}

/*
 * Return true if the timeout has expired. Owing to the rules of modular
 * arithmetics, this computation is rollover-safe as long as the timeout
 * is no more than 32767 tics (about 3.4 seconds) into the future.
 */
static bool expired(uint16_t now, uint16_t timeout)
{
    return (int16_t) (now - timeout) >= 0;
}

/*
 * Active wait for the given number of tics. This is a blocking function
 * to be called only during program startup.
 */
static void delay(uint16_t tick_count)
{
    uint16_t timeout = tics() + tick_count;
    while (!expired(tics(), timeout)) ;
}


/***********************************************************************
 * Keying speed selection.
 */

static __flash const uint16_t dot_times[] = {
    DOT_TIME(KEY_RATE_3), DOT_TIME(KEY_RATE_2),
    DOT_TIME(KEY_RATE_1), DOT_TIME(KEY_RATE_0)
};

/*
 * Delays corresponding to 1, 2 and 3 "Morse time units", the unit being
 * the length of a dot, in tics.
 */
static uint16_t delay_1u, delay_2u, delay_3u;

/*
 * Initialize the globals above from the user selection. Only additions
 * are done here because an expression like `3 * delay_1u' compiles to a
 * flash-hungry multiplication.
 */
static void set_delays()
{
    uint8_t speed = PINB & 0x03;  // selection set from PB0 and PB1
    delay_1u = dot_times[speed];
    delay_2u = delay_1u + delay_1u;
    delay_3u = delay_2u + delay_1u;
}


/***********************************************************************
 * Edge detector.
 */

typedef enum {NO_EDGE, RISE, FALL} edge_t;

/*
 * Return true if the key is held down, false otherwise. Note that the
 * port access is so efficient that there is no point in buffering the
 * reading, as it would make the code larger.
 */
static bool key_down(void)
{
    return (PINB & _BV(KEY_PIN)) == 0;
}

/*
 * Return RISE or FALL if an edge is detected, NO_EDGE otherwise.
 *
 * This is a straight implementation of a finite-state machine. See the
 * documentation in the accompanying file internals.md for a description
 * of the state machine.
 */
static edge_t get_edge(void)
{
    static enum {UP, DOWN, BOUNCING} state;
    static uint16_t timeout;
    uint16_t now = tics();
    switch (state) {
        case UP:
            if (key_down()) {
                state = DOWN;
                PORTB |= _BV(LED_PIN);  // LED on
                return FALL;
            }
            break;
        case DOWN:
            if (!key_down()) {
                state = BOUNCING;
                timeout = now + DEBOUNCE_TIME;
            }
            break;
        case BOUNCING:
            if (key_down()) {
                state = DOWN;
            } else if (expired(now, timeout)) {
                state = UP;
                PORTB &= ~_BV(LED_PIN);  // LED off
                return RISE;
            }
            break;
    }
    return NO_EDGE;
}


/***********************************************************************
 * Tokenizer.
 */

typedef enum {NO_SYMBOL, DOT, DASH, END_OF_CHAR, END_OF_WORD} symbol_t;

/*
 * Return the next detected symbol, if any, NO_SYMBOL otherwise.
 *
 * This is also a finite-state machine described in internals.md.
 */
static symbol_t tokenize(edge_t edge)
{
    static enum {
        INTERWORD, SHORT, LONG, INTERELEMENT, INTERCHARACTER
    } state;
    static uint16_t timeout;
    uint16_t now = tics();
    switch (state) {
        case INTERWORD:
            if (edge == FALL) {
                state = SHORT;
                timeout = now + delay_2u;
            }
            break;
        case SHORT:
            if (edge == RISE) {
                state = INTERELEMENT;
                timeout = now + delay_2u;
                return DOT;
            } else if (expired(now, timeout)) {
                state = LONG;
            }
            break;
        case LONG:
            if (edge == RISE) {
                state = INTERELEMENT;
                timeout = now + delay_2u;
                return DASH;
            }
            break;
        case INTERELEMENT:
            if (edge == FALL) {
                state = SHORT;
                timeout = now + delay_2u;
            } else if (expired(now, timeout)) {
                state = INTERCHARACTER;
                timeout = now + delay_3u;
                return END_OF_CHAR;
            }
            break;
        case INTERCHARACTER:
            if (edge == FALL) {
                state = SHORT;
                timeout = now + delay_2u;
            } else if (expired(now, timeout)) {
                state = INTERWORD;
                return END_OF_WORD;
            }
            break;
    }
    return NO_SYMBOL;
}


/***********************************************************************
 * Decoder.
 */

/* === Generated code. See the accompanying "tools" directory. === */
#define CODE_LENGTH 59

static __flash const uint16_t morse_code[CODE_LENGTH] = {
    363, 694, 221,   0, 375,   0,  61, 853, 214, 726,   0, 109,
    698, 190, 365, 110, 682, 341, 171,  87,  47,  31,  62, 122,
    234, 426, 490, 438,   0,  94,   0, 235, 437,   5,  30,  54,
     14,   1,  27,  26,  15,   3,  85,  22,  29,  10,   6,  42,
     53,  90,  13,   7,   2,  11,  23,  21,  46,  86,  58
};
/* === End of generated code. === */

/*
 * Convert a "code number" to an ASCII character. The ASCII code is 32
 * (ASCII space) + the index of the code number in the morse_code array.
 * Exception: code number 0 means '_'.
 */
static char code_to_char(uint16_t code)
{
    int i;  // array index

    // Lookup the code in the array.
    for (i = 0; i < CODE_LENGTH; i++) {
        if (morse_code[i] == code)
            break;
    }

    if (i == 0)                // 0 means '_'
        return '_';
    else if (i < CODE_LENGTH)  // generic case
        return ' ' + i;
    else                       // not found: return "invalid"
        return '#';
}

/*
 * Decode a stream of DOT, DASH, END_OF_CHAR, END_OF_WORD symbols.
 * Returns the decoded character, or 0 if the current symbol is neither
 * END_OF_CHAR nor END_OF_WORD.
 *
 * This works by translating the symbols to a bit stream per:
 *   DOT -> 1
 *   DASH -> 0, 1.
 * The bit stream is actually a "code number" serialized least
 * significant bit first. Upon receiving END_OF_CHAR, this number is
 * converted to an ASCII character.
 */
static char decode(symbol_t symbol)
{
    static uint16_t code = 0, bitmask = 1;
    switch (symbol) {
        case NO_SYMBOL:
            break;
        case DASH:
            bitmask <<= 1;    // add a 0 to the bit stream
            /* fallthrough */
        case DOT:
            code |= bitmask;  // add a 1
            bitmask <<= 1;
            break;
        case END_OF_CHAR: {
                char c = code_to_char(code);
                code = 0;  // reset, to get ready for the next character
                bitmask = 1;
                return c;
            }
        case END_OF_WORD:
            return ' ';
    }
    return 0;  // not a full character yet
}


/***********************************************************************
 * Software UART transmitter.
 */

static void init_uart()
{
    PORTB |= _BV(TX_PIN);  // TX idles HIGH
    DDRB  |= _BV(TX_PIN);  // TX is an output
}

/*
 * The shift register is 16-bits because it must hold not only the data
 * bits, but also the start and stop bits.
 */
static volatile int16_t uart_shift_register;

ISR(TIM0_COMPB_vect)
{
    /*
     * Take a local copy of the global uart_shift_register. Since it is
     * volatile, working directly on that global variable would involve
     * multiple memory reads and writes, which would make the code
     * slower and 12 bytes larger.
     */
    int16_t shift_register = uart_shift_register;

    /* Send the current bit -- least significant first. */
    if (shift_register & 1)
        PORTB |= _BV(TX_PIN);
    else
        PORTB &= ~_BV(TX_PIN);

    /* Shift. */
    shift_register >>= 1;

    /*
     * If we are done, disable the interrupt. As a micro-optimization,
     * we test only the low byte of the shift register. Note that this
     * would be incorrect if we ever send NUL or 0x80, but we only send
     * printable ASCII.
     */
    if ((uint8_t) shift_register == 0)
        TIMSK0 &= ~_BV(OCIE0B);

    uart_shift_register = shift_register;
}

/*
 * Send a character. It is assumed that the sending is slow enough that
 * we do not need to check whether another character is still being
 * transmitted. This assumption would only break at an unrealistic rate
 * of 288 wpm when sending "EE".
 */
static void uart_putchar(char c)
{
    uart_shift_register = (0x0100 | (uint8_t)c) << 1;
    TIFR0 = _BV(OCF0B);     // clear the interrupt flag
    TIMSK0 |= _BV(OCIE0B);  // enable the interrupt
}


/***********************************************************************
 * Main program.
 */

/* Send an invitation to transmit. */
static void invite(void)
{
    uint8_t code = 22;  // -.- = K = Invitation to transmit
    while (code) {
        PORTB |= _BV(LED_PIN);  // LED on
        if ((code & 1) == 0) {  // dash
            delay(delay_2u);
            code >>= 1;
        }
        delay(delay_1u);
        code >>= 1;
        PORTB &= ~_BV(LED_PIN);  // LED off
        delay(delay_1u);
    }
}

int main(void)
{
    /* Set the clock prescaler to 1. */
    clock_prescale_set(clock_div_1);

    /* Enable internal pull-ups. */
    PORTB = _BV(PB0)
          | _BV(PB1)
          | _BV(KEY_PIN);

    DDRB  |= _BV(LED_PIN);  // LED_PIN as output
    init_timer();
    init_uart();
    set_delays();
    sei();
    invite();
    for (;;) {
        edge_t edge = get_edge();
        symbol_t sym = tokenize(edge);
        char c = decode(sym);
        if (c)
            uart_putchar(c);
    }
}
