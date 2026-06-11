// Copyright (c) 2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//

#include "config.h"
#include "sha256_accel.h"
#include "soc_ctrl.h"
#include "util.h"

#define MAX_MSG_LEN 255u
#define MAX_BLOCKS  5u

static const uint32_t sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

void *memcpy(void *dst, const void *src, unsigned long n) {
    uint8_t *dst_bytes = (uint8_t *)dst;
    const uint8_t *src_bytes = (const uint8_t *)src;
    for (unsigned long i = 0; i < n; ++i) {
        dst_bytes[i] = src_bytes[i];
    }
    return dst;
}

static void finish_with_code(uint32_t exit_code) {
    *reg32(SOCCTRL_BASE_ADDR, SOC_CTRL_CORESTATUS_REG_OFFSET) = (exit_code << 1) | 1u;
    while (1) {
        wfi();
    }
}

#define CHECK_ASSERT_EOC(ret, cond) \
    do { \
        if (!(cond)) finish_with_code(ret); \
    } while (0)

static inline uint32_t rotr32(uint32_t value, uint32_t shift) {
    return (value >> shift) | (value << (32u - shift));
}

static inline uint32_t big_sigma0(uint32_t value) {
    return rotr32(value, 2) ^ rotr32(value, 13) ^ rotr32(value, 22);
}

static inline uint32_t big_sigma1(uint32_t value) {
    return rotr32(value, 6) ^ rotr32(value, 11) ^ rotr32(value, 25);
}

static inline uint32_t small_sigma0(uint32_t value) {
    return rotr32(value, 7) ^ rotr32(value, 18) ^ (value >> 3);
}

static inline uint32_t small_sigma1(uint32_t value) {
    return rotr32(value, 17) ^ rotr32(value, 19) ^ (value >> 10);
}

static void clear_accel_status_all() {
    sha256_accel_clear_status(
        SHA256_ACCEL_STATUS_DONE_BIT_MASK |
        SHA256_ACCEL_STATUS_DIGEST_VALID_BIT_MASK |
        SHA256_ACCEL_STATUS_IRQ_PENDING_BIT_MASK |
        SHA256_ACCEL_STATUS_START_ERR_BIT_MASK);
}

static void store_byte_be(uint32_t blocks[MAX_BLOCKS][16], uint32_t byte_index, uint8_t value) {
    uint32_t block_idx = byte_index >> 6;
    uint32_t word_idx = (byte_index >> 2) & 0xfu;
    uint32_t byte_in_word = byte_index & 0x3u;
    blocks[block_idx][word_idx] |= (uint32_t)value << (24u - (8u * byte_in_word));
}

static uint32_t build_padded_blocks(const uint8_t *msg, uint32_t msg_len, uint32_t blocks[MAX_BLOCKS][16]) {
    uint32_t num_blocks = (msg_len + 9u + 63u) / 64u;
    uint64_t bit_len = (uint64_t)msg_len * 8u;

    for (uint32_t block_idx = 0; block_idx < MAX_BLOCKS; ++block_idx) {
        for (uint32_t word_idx = 0; word_idx < 16u; ++word_idx) {
            blocks[block_idx][word_idx] = 0u;
        }
    }

    CHECK_ASSERT_EOC(1, num_blocks <= MAX_BLOCKS);

    for (uint32_t i = 0; i < msg_len; ++i) {
        store_byte_be(blocks, i, msg[i]);
    }

    store_byte_be(blocks, msg_len, 0x80u);

    for (uint32_t i = 0; i < 8u; ++i) {
        uint32_t shift = 8u * (7u - i);
        store_byte_be(blocks, (num_blocks * 64u) - 8u + i, (uint8_t)(bit_len >> shift));
    }

    return num_blocks;
}

static void sha256_sw_compress(const uint32_t block[16], uint32_t state[8]) {
    uint32_t w[64];
    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];
    uint32_t f = state[5];
    uint32_t g = state[6];
    uint32_t h = state[7];

    for (uint32_t i = 0; i < 16u; ++i) {
        w[i] = block[i];
    }
    for (uint32_t i = 16u; i < 64u; ++i) {
        w[i] = small_sigma1(w[i - 2u]) + w[i - 7u] + small_sigma0(w[i - 15u]) + w[i - 16u];
    }

    for (uint32_t i = 0; i < 64u; ++i) {
        uint32_t t1 = h + big_sigma1(e) + ((e & f) ^ ((~e) & g)) + sha256_k[i] + w[i];
        uint32_t t2 = big_sigma0(a) + ((a & b) ^ (a & c) ^ (b & c));

        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

static void sha256_sw_hash_message(const uint8_t *msg, uint32_t msg_len, uint32_t digest[8]) {
    uint32_t blocks[MAX_BLOCKS][16];
    uint32_t state[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
    };
    uint32_t num_blocks = build_padded_blocks(msg, msg_len, blocks);

    for (uint32_t block_idx = 0; block_idx < num_blocks; ++block_idx) {
        sha256_sw_compress(blocks[block_idx], state);
    }

    for (uint32_t i = 0; i < 8u; ++i) {
        digest[i] = state[i];
    }
}

static void fill_message(uint8_t *msg, uint32_t msg_len, uint32_t pattern) {
    for (uint32_t i = 0; i < msg_len; ++i) {
        if (pattern == 0u) {
            msg[i] = (uint8_t)(((i * 37u) + (msg_len * 11u) + 0x13u) & 0xffu);
        } else if (pattern == 1u) {
            msg[i] = (uint8_t)(0xffu - (((i * 29u) + msg_len + 0x41u) & 0xffu));
        } else {
            msg[i] = (uint8_t)((((i * 17u) ^ (i >> 1)) + (msg_len * 23u) + 0x5au) & 0xffu);
        }
    }
}

static void run_case(uint32_t case_base, uint32_t pattern, uint32_t msg_len) {
    uint8_t msg[MAX_MSG_LEN];
    uint32_t blocks[MAX_BLOCKS][16];
    uint32_t sw_digest[8];
    uint32_t hw_digest[8];
    uint32_t status;
    uint32_t num_blocks;

    fill_message(msg, msg_len, pattern);
    sha256_sw_hash_message(msg, msg_len, sw_digest);
    num_blocks = build_padded_blocks(msg, msg_len, blocks);

    clear_accel_status_all();
    status = sha256_accel_hash_blocks(&blocks[0][0], num_blocks, hw_digest);

    CHECK_ASSERT_EOC(case_base + 0u, (status & SHA256_ACCEL_STATUS_BUSY_BIT_MASK) == 0u);
    CHECK_ASSERT_EOC(case_base + 1u, (status & SHA256_ACCEL_STATUS_DONE_BIT_MASK) != 0u);
    CHECK_ASSERT_EOC(case_base + 2u, (status & SHA256_ACCEL_STATUS_DIGEST_VALID_BIT_MASK) != 0u);
    CHECK_ASSERT_EOC(case_base + 3u, (status & SHA256_ACCEL_STATUS_START_ERR_BIT_MASK) == 0u);

    for (uint32_t i = 0; i < 8u; ++i) {
        CHECK_ASSERT_EOC(case_base + 4u + i, hw_digest[i] == sw_digest[i]);
    }

    clear_accel_status_all();
    status = sha256_accel_get_status();
    CHECK_ASSERT_EOC(case_base + 12u, (status & SHA256_ACCEL_STATUS_DONE_BIT_MASK) == 0u);
    CHECK_ASSERT_EOC(case_base + 13u, (status & SHA256_ACCEL_STATUS_DIGEST_VALID_BIT_MASK) == 0u);
    CHECK_ASSERT_EOC(case_base + 14u, (status & SHA256_ACCEL_STATUS_START_ERR_BIT_MASK) == 0u);
    CHECK_ASSERT_EOC(case_base + 15u, (status & SHA256_ACCEL_STATUS_READY_BIT_MASK) != 0u);
}

int main() {
    static const uint32_t boundary_lengths[] = {
        0u, 1u, 2u, 3u, 4u, 7u, 8u, 15u, 16u, 31u, 32u,
        55u, 56u, 57u, 63u, 64u, 65u, 79u, 80u, 95u,
        111u, 119u, 120u, 121u, 127u, 128u, 129u, 159u, 160u,
        191u, 192u, 193u, 255u
    };
    static const uint32_t extended_lengths[] = {
        161u, 167u, 175u, 183u, 191u, 192u, 193u, 207u, 223u, 239u, 247u, 255u
    };
    uint32_t case_idx = 0u;

    CHECK_ASSERT_EOC(2u, (sha256_accel_get_status() & SHA256_ACCEL_STATUS_READY_BIT_MASK) != 0u);

    for (uint32_t msg_len = 0u; msg_len <= 160u; ++msg_len) {
        run_case(10u + (case_idx * 20u), 0u, msg_len);
        ++case_idx;
    }

    for (uint32_t msg_len = 0u; msg_len <= 160u; ++msg_len) {
        run_case(10u + (case_idx * 20u), 1u, msg_len);
        ++case_idx;
    }

    for (uint32_t idx = 0u; idx < (sizeof(extended_lengths) / sizeof(extended_lengths[0])); ++idx) {
        run_case(10u + (case_idx * 20u), 0u, extended_lengths[idx]);
        ++case_idx;
        run_case(10u + (case_idx * 20u), 1u, extended_lengths[idx]);
        ++case_idx;
    }

    for (uint32_t idx = 0u; idx < (sizeof(boundary_lengths) / sizeof(boundary_lengths[0])); ++idx) {
        run_case(10u + (case_idx * 20u), 2u, boundary_lengths[idx]);
        ++case_idx;
    }

    finish_with_code(0u);
}
