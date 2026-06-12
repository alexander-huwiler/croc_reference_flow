// Copyright (c) 2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//

#include "config.h"
#include "print.h"
#include "soc_ctrl.h"
#include "uart.h"
#include "util.h"

#define SHA256_BENCH_ITERS 32u
#define MAX_MSG_LEN        255u
#define MAX_BLOCKS         5u

static const uint32_t bench_lengths[] = {3u, 55u, 56u, 64u, 128u, 255u};

static uint8_t bench_msg[MAX_MSG_LEN];
static uint32_t bench_blocks[MAX_BLOCKS][16];
static uint32_t bench_digest[8];
static uint32_t bench_schedule[64];

static const uint32_t abc_digest[8] = {
    0xba7816bf, 0x8f01cfea, 0x414140de, 0x5dae2223,
    0xb00361a3, 0x96177a9c, 0xb410ff61, 0xf20015ad,
};

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

static inline uint32_t read_mcycle32() {
    uint32_t cycles;
    asm volatile("csrr %0, mcycle" : "=r"(cycles)::"memory");
    return cycles;
}

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

static void store_byte_be(uint32_t blocks[MAX_BLOCKS][16], uint32_t byte_index, uint8_t value) {
    uint32_t block_idx = byte_index >> 6;
    uint32_t word_idx = (byte_index >> 2) & 0xfu;
    uint32_t byte_in_word = byte_index & 0x3u;
    blocks[block_idx][word_idx] |= (uint32_t)value << (24u - (8u * byte_in_word));
}

static uint32_t build_padded_blocks(
    const uint8_t *msg,
    uint32_t msg_len,
    uint32_t blocks[MAX_BLOCKS][16]
) {
    uint32_t num_blocks = (msg_len + 9u + 63u) / 64u;
    uint64_t bit_len = (uint64_t)msg_len * 8u;

    CHECK_ASSERT_EOC(1u, num_blocks <= MAX_BLOCKS);

    for (uint32_t block_idx = 0; block_idx < MAX_BLOCKS; ++block_idx) {
        for (uint32_t word_idx = 0; word_idx < 16u; ++word_idx) {
            blocks[block_idx][word_idx] = 0u;
        }
    }

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

static void fill_message(uint32_t msg_len) {
    if (msg_len == 3u) {
        bench_msg[0] = 'a';
        bench_msg[1] = 'b';
        bench_msg[2] = 'c';
        return;
    }

    for (uint32_t i = 0; i < msg_len; ++i) {
        bench_msg[i] = (uint8_t)(((i * 37u) + (msg_len * 11u) + 0x13u) & 0xffu);
    }
}

static void sha256_sw_compress(const uint32_t block[16], uint32_t state[8]) {
    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];
    uint32_t f = state[5];
    uint32_t g = state[6];
    uint32_t h = state[7];

    for (uint32_t i = 0; i < 16u; ++i) {
        bench_schedule[i] = block[i];
    }
    for (uint32_t i = 16u; i < 64u; ++i) {
        bench_schedule[i] =
            small_sigma1(bench_schedule[i - 2u]) +
            bench_schedule[i - 7u] +
            small_sigma0(bench_schedule[i - 15u]) +
            bench_schedule[i - 16u];
    }

    for (uint32_t i = 0; i < 64u; ++i) {
        uint32_t t1 = h + big_sigma1(e) + ((e & f) ^ ((~e) & g)) + sha256_k[i] + bench_schedule[i];
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

static void sha256_sw_hash_blocks(const uint32_t *blocks, uint32_t num_blocks, uint32_t digest[8]) {
    uint32_t state[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
    };

    for (uint32_t block_idx = 0; block_idx < num_blocks; ++block_idx) {
        sha256_sw_compress(&blocks[block_idx * 16u], state);
    }

    for (uint32_t i = 0; i < 8u; ++i) {
        digest[i] = state[i];
    }
}

static void check_abc_digest() {
    for (uint32_t i = 0; i < 8u; ++i) {
        CHECK_ASSERT_EOC(5u + i, bench_digest[i] == abc_digest[i]);
    }
}

static void print_case_metrics(uint32_t msg_len, uint32_t num_blocks, uint32_t sw_cycles) {
    uint32_t sw_cycles_per_hash = sw_cycles / SHA256_BENCH_ITERS;
    uint32_t sw_cycles_per_byte_x100 = (sw_cycles * 100u) / (SHA256_BENCH_ITERS * msg_len);

    printf("SHA SW len %x\n", msg_len);
    printf("  blocks %x\n", num_blocks);
    printf("  iters %x\n", SHA256_BENCH_ITERS);
    printf("  cycles total %x\n", sw_cycles);
    printf("  cycles/hash %x\n", sw_cycles_per_hash);
    printf("  cycles/byte x100 %x\n", sw_cycles_per_byte_x100);
}

static void run_bench_case(uint32_t case_idx, uint32_t msg_len) {
    uint32_t num_blocks;
    uint32_t iter_blocks;
    uint32_t sw_cycles;
    uint32_t start_cycles;

    fill_message(msg_len);
    num_blocks = build_padded_blocks(bench_msg, msg_len, bench_blocks);
    sha256_sw_hash_blocks(&bench_blocks[0][0], num_blocks, bench_digest);

    if (msg_len == 3u) {
        check_abc_digest();
    }

    start_cycles = read_mcycle32();
    for (uint32_t iter = 0; iter < SHA256_BENCH_ITERS; ++iter) {
        iter_blocks = build_padded_blocks(bench_msg, msg_len, bench_blocks);
        sha256_sw_hash_blocks(&bench_blocks[0][0], iter_blocks, bench_digest);
    }
    sw_cycles = read_mcycle32() - start_cycles;

    CHECK_ASSERT_EOC(20u + case_idx, iter_blocks == num_blocks);
    CHECK_ASSERT_EOC(30u + case_idx, bench_digest[0] != 0u);
    print_case_metrics(msg_len, num_blocks, sw_cycles);
    uart_write_flush();
}

int main() {
    uart_init();

    for (uint32_t case_idx = 0; case_idx < (sizeof(bench_lengths) / sizeof(bench_lengths[0])); ++case_idx) {
        run_bench_case(case_idx, bench_lengths[case_idx]);
    }

    finish_with_code(0u);
}
