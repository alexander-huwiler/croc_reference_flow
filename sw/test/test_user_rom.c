// Copyright (c) 2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//

#include "config.h"
#include "util.h"

int main() {
    static const uint32_t expected_words[] = {
        0x75646152, // "Radu"
        0x6E6F432D, // "-Con"
        0x6E617473, // "stan"
        0x206E6974, // "tin "
        0x74657243, // "Cret"
        0x41202C75, // "u, A"
        0x6178656C, // "lexa"
        0x7265646E, // "nder"
        0x77754820, // " Huw"
        0x72656C69, // "iler"
        0x00000000, // zero terminator
    };

    for (uint32_t word_idx = 0; word_idx < sizeof(expected_words) / sizeof(expected_words[0]); ++word_idx) {
        uint32_t word = *reg32(USER_ROM_BASE_ADDR, (int)(word_idx * 4u));
        CHECK_ASSERT((int)(word_idx + 1u), word == expected_words[word_idx]);
    }

    return 0;
}
