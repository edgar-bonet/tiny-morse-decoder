/*
 * Automated test for tiny-morse-decoder.
 *
 * Connect to the ATtiny and see the diagnostic information sent to
 * the serial monitor at 9600/8N1.
 *
 * Connections:
 *   Arduino  ATtiny
 *     GND --- GND
 *      5V --- Vcc                  ┌──┬┬──┐
 *       8 --- PB0     11 ─── RESET ┤1 └┘ 8├ Vcc ─── 5V
 *       9 --- PB1     13 ───── PB3 ┤2    7├ PB2 ─── RX
 *      10 --- PB4     10 ───── PB4 ┤3    6├ PB1 ───  9
 *      11 --- RESET  GND ───── GND ┤4    5├ PB0 ───  8
 *      13 --- PB3                  └──────┘
 *    RX/0 --- PB2
 *
 * Copyright (c) 2018 Edgar Bonet Orozco.
 * This file is part of tiny-morse-decoder, licensed under the terms of
 * the MIT license. See file LICENSE for details.
 */

#include "raw-morse-code.h"

/* Pins connected to the ATtiny. */
const uint8_t SPEED_SELECT_PINS[2] = {8, 9};
const uint8_t KEY_PIN = 10;
const uint8_t RESET_PIN = 11;

/* Selectable keying rates. */
const uint8_t KEY_RATES[4] = {5, 8, 12, 18};

/*
 * The ATtiny is controlled in an "open drain" mode, by grounding and
 * floating its inputs. This open drain control can be achieved on the
 * Arduino through pinMode():
 *  - OUTPUT sets the pin to LOW by default, effectively grounding it
 *  - INPUT sets the pin to high impedance, effectively floating it.
 * We alias pinMode() to openDrainWrite() to make our intent clearer.
 */
#define GROUND OUTPUT
#define FLOAT  INPUT
#define openDrainWrite(pin, value) pinMode((pin), (value))

void setup()
{
    Serial.begin(9600);
    Serial.println();
    pinMode(0, INPUT_PULLUP);  // pullup on RX
}

/* Send a character in Morse. */
static void send(const char *code, uint8_t dot_time)
{
    for (const char *p = code; *p; p++) {
        openDrainWrite(KEY_PIN, GROUND);
        switch (*p) {
            case '.': delay(dot_time); break;
            case '-': delay(3 * dot_time); break;
        }
        openDrainWrite(KEY_PIN, FLOAT);
        delay(dot_time);  // interelement gap
    }
    delay(2 * dot_time);  // intercharacter - interelement gap
}

/* Send a character and make sure it was correctly understood. */
static bool test(const raw_code_t *character, uint8_t dot_time)
{
    Serial.write(character->c);
    if (character->c == ' ')
        delay(4 * dot_time);  // interword - intercharacter gap
    else
        send(character->code, dot_time);
    delay(0.5 * dot_time);
    if (!Serial.available()) {
        Serial.print(" -> TIMEOUT");
        return false;
    }
    char received = Serial.read();
    if (received != character->c) {
        Serial.println();
        Serial.print("ERROR: expecting '");
        Serial.print(character->c);
        Serial.print("', received '");
        Serial.print(received);
        Serial.print("'");
        return false;
    }
    return true;
}

void loop()
{
    static uint8_t rate_index = 0;
    uint8_t key_rate = KEY_RATES[rate_index];
    uint8_t dot_time = (1200 + key_rate/2) / key_rate;

    /* Select the speed. */
    Serial.print("=== Speed ");
    Serial.print(rate_index);
    Serial.print(": ");
    Serial.print(key_rate);
    Serial.print(" wpm (");
    Serial.print(dot_time);
    Serial.println(" ms) ===");
    openDrainWrite(SPEED_SELECT_PINS[0], rate_index&1 ? GROUND : FLOAT);
    openDrainWrite(SPEED_SELECT_PINS[1], rate_index&2 ? GROUND : FLOAT);

    /* Reset the target. */
    openDrainWrite(RESET_PIN, GROUND);
    delay(1);
    openDrainWrite(RESET_PIN, FLOAT);
    delay(100 + 10*dot_time);  // wait for the invitation

    /* Send the whole code. */
    Serial.println("Sending all known characters:");
    Serial.flush();
    while (Serial.read() != -1) ;
    for (size_t i = 0; i < RAW_CODE_LENGTH; i++) {
        if (!test(&raw_code[i], dot_time))
            break;
    }
    Serial.println();

    /* Send some random text. */
    Serial.println("Sending random text:");
    Serial.flush();
    while (Serial.read() != -1) ;
    for (int i = 0; i < 72; i++) {
        size_t j = random(RAW_CODE_LENGTH);
        if (!test(&raw_code[j], dot_time))
            break;
        if (i < 71 && random(5) == 0) {
            static const raw_code_t space = {' ', NULL};
            if (!test(&space, dot_time))
                break;
            i++;
        }
    }
    Serial.println();
    Serial.println();

    /* Next time try a different keying rate. */
    rate_index = (rate_index + 1) % 4;
}
