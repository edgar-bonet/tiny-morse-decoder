/*
 * Generate a Morse code table suitable for efficient storage and
 * decoding.
 *
 * Copyright (c) 2018 Edgar Bonet Orozco.
 * This file is part of tiny-morse-decoder, licensed under the terms of
 * the MIT license. See file LICENSE for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "raw-morse-code.h"

/*
 * Intermediate representation: the codes are represented by binary
 * numbers, as in the final representation, but this is an array of
 * (character, code) pairs, just like the raw Morse code array.
 */
static struct {
    char c;
    uint16_t code;
} morse_code[RAW_CODE_LENGTH];

int main(void)
{
    /* Build the intermediate representation. */
    for (size_t i = 0; i < RAW_CODE_LENGTH; i++) {

        /* Copy the character. */
        char c = raw_code[i].c;
        morse_code[i].c = c;

        /* Translate the code. */
        const char *s = raw_code[i].code;
        unsigned int code = 0, bitmask = 1;
        for (const char *p = s; *p; p++) {
            if (*p == '-') {
                bitmask <<= 1;
            } else if (*p != '.') {
                fprintf(stderr, "Found symbol %c in code.\n", *p);
                return EXIT_FAILURE;
            }
            code |= bitmask;
            bitmask <<= 1;
        }
        if (code > UINT16_MAX) {
            fprintf(stderr, "Code too large: %c -> %u\n", c, code);
            return EXIT_FAILURE;
        }
        morse_code[i].code = code;
    }

    /* Print a table of code numbers in ASCII order. */
    const size_t generated_code_length = 'Z' - ' ' + 1;
    printf("#define CODE_LENGTH %zu\n\n", generated_code_length);
    printf("static __flash const uint16_t "
            "morse_code[CODE_LENGTH] = {\n    ");
    for (size_t i = 0; i < generated_code_length; i++) {

        /* Print the code number of this character. */
        char c = i==0 ? '_' : ' ' + i;
        size_t j;
        for (j = 0; j < RAW_CODE_LENGTH; j++)
            if (morse_code[j].c == c) break;
        uint16_t code = j==RAW_CODE_LENGTH ? 0 : morse_code[j].code;
        printf("%3"PRIu16"", code);

        /* Print a separator for nice formatting. */
        if (i == generated_code_length - 1)
            printf("\n");
        else if (i % 12 == 11)
            printf(",\n    ");
        else
            printf(", ");
    }
    printf("};\n");

    /* Be happy. */
    return EXIT_SUCCESS;
}
