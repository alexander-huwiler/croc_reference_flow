// Copyright (c) 2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//

#include "config.h"
#include "print.h"
#include "soc_ctrl.h"
#include "sha256_accel.h"
#include "uart.h"
#include "util.h"

#define SHA256_BENCH_ITERS 64u

static const uint32_t abc_block[16] = {
    0x61626380, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000018,
};

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

static void sha256_sw_hash_one_block(const uint32_t block[16], uint32_t digest[8]) {
    uint32_t w[64];
    uint32_t a = 0x6a09e667;
    uint32_t b = 0xbb67ae85;
    uint32_t c = 0x3c6ef372;
    uint32_t d = 0xa54ff53a;
    uint32_t e = 0x510e527f;
    uint32_t f = 0x9b05688c;
    uint32_t g = 0x1f83d9ab;
    uint32_t h = 0x5be0cd19;

    for (uint32_t i = 0; i < 16; ++i) {
        w[i] = block[i];
    }
    for (uint32_t i = 16; i < 64; ++i) {
        w[i] = small_sigma1(w[i - 2]) + w[i - 7] + small_sigma0(w[i - 15]) + w[i - 16];
    }

    for (uint32_t i = 0; i < 64; ++i) {
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

    digest[0] = 0x6a09e667 + a;
    digest[1] = 0xbb67ae85 + b;
    digest[2] = 0x3c6ef372 + c;
    digest[3] = 0xa54ff53a + d;
    digest[4] = 0x510e527f + e;
    digest[5] = 0x9b05688c + f;
    digest[6] = 0x1f83d9ab + g;
    digest[7] = 0x5be0cd19 + h;
}

static void clear_accel_status_all() {
    sha256_accel_clear_status(
        SHA256_ACCEL_STATUS_DONE_BIT_MASK |
        SHA256_ACCEL_STATUS_DIGEST_VALID_BIT_MASK |
        SHA256_ACCEL_STATUS_IRQ_PENDING_BIT_MASK |
        SHA256_ACCEL_STATUS_START_ERR_BIT_MASK);
}

static void check_digest(uint32_t base_code, const uint32_t digest[8]) {
    for (int i = 0; i < 8; ++i) {
        CHECK_ASSERT_EOC(base_code + (uint32_t)i, digest[i] == abc_digest[i]);
    }
}

int main() {
    uint32_t hw_digest[8];
    uint32_t sw_digest[8];
    uint32_t status;
    uint32_t hw_cycles;
    uint32_t sw_cycles;
    uint32_t start_cycles;

    uart_init();

    clear_accel_status_all();
    status = sha256_accel_hash_block(abc_block, hw_digest);
    CHECK_ASSERT_EOC(1, (status & SHA256_ACCEL_STATUS_BUSY_BIT_MASK) == 0u);
    CHECK_ASSERT_EOC(2, (status & SHA256_ACCEL_STATUS_DIGEST_VALID_BIT_MASK) != 0u);
    CHECK_ASSERT_EOC(3, (status & SHA256_ACCEL_STATUS_START_ERR_BIT_MASK) == 0u);
    check_digest(4, hw_digest);

    sha256_sw_hash_one_block(abc_block, sw_digest);
    check_digest(12, sw_digest);

    clear_accel_status_all();
    start_cycles = read_mcycle32();
    for (uint32_t iter = 0; iter < SHA256_BENCH_ITERS; ++iter) {
        status = sha256_accel_hash_block(abc_block, hw_digest);
    }
    hw_cycles = read_mcycle32() - start_cycles;

    start_cycles = read_mcycle32();
    for (uint32_t iter = 0; iter < SHA256_BENCH_ITERS; ++iter) {
        sha256_sw_hash_one_block(abc_block, sw_digest);
    }
    sw_cycles = read_mcycle32() - start_cycles;

    CHECK_ASSERT_EOC(20, (status & SHA256_ACCEL_STATUS_BUSY_BIT_MASK) == 0u);
    CHECK_ASSERT_EOC(21, (status & SHA256_ACCEL_STATUS_DIGEST_VALID_BIT_MASK) != 0u);
    CHECK_ASSERT_EOC(22, (status & SHA256_ACCEL_STATUS_START_ERR_BIT_MASK) == 0u);
    check_digest(23, hw_digest);
    check_digest(31, sw_digest);

    printf("SHA abc iters %x\n", SHA256_BENCH_ITERS);
    printf("SHA abc hw cycles total %x\n", hw_cycles);
    printf("SHA abc sw cycles total %x\n", sw_cycles);
    printf("SHA abc hw cycles/hash %x\n", hw_cycles / SHA256_BENCH_ITERS);
    printf("SHA abc sw cycles/hash %x\n", sw_cycles / SHA256_BENCH_ITERS);
    uart_write_flush();

    clear_accel_status_all();
    finish_with_code(0);
}
