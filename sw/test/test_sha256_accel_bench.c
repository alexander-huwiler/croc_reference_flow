// Copyright (c) 2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//

#include "config.h"
#include "print.h"
#include "sha256_accel.h"
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

static void print_case_metrics(uint32_t msg_len, uint32_t num_blocks, uint32_t hw_cycles) {
    uint32_t hw_cycles_per_hash = hw_cycles / SHA256_BENCH_ITERS;
    uint32_t hw_cycles_per_byte_x100 = (hw_cycles * 100u) / (SHA256_BENCH_ITERS * msg_len);

    printf("SHA HW len %x\n", msg_len);
    printf("  blocks %x\n", num_blocks);
    printf("  iters %x\n", SHA256_BENCH_ITERS);
    printf("  cycles total %x\n", hw_cycles);
    printf("  cycles/hash %x\n", hw_cycles_per_hash);
    printf("  cycles/byte x100 %x\n", hw_cycles_per_byte_x100);
}

static void run_bench_case(uint32_t case_idx, uint32_t msg_len) {
    uint32_t status = 0u;
    uint32_t num_blocks;
    uint32_t hw_cycles;
    uint32_t start_cycles;

    fill_message(msg_len);

    clear_accel_status_all();
    num_blocks = build_padded_blocks(bench_msg, msg_len, bench_blocks);
    status = sha256_accel_hash_blocks(&bench_blocks[0][0], num_blocks, bench_digest);
    CHECK_ASSERT_EOC(10u + (case_idx * 10u) + 0u, (status & SHA256_ACCEL_STATUS_BUSY_BIT_MASK) == 0u);
    CHECK_ASSERT_EOC(10u + (case_idx * 10u) + 1u, (status & SHA256_ACCEL_STATUS_DIGEST_VALID_BIT_MASK) != 0u);
    CHECK_ASSERT_EOC(10u + (case_idx * 10u) + 2u, (status & SHA256_ACCEL_STATUS_START_ERR_BIT_MASK) == 0u);

    clear_accel_status_all();
    start_cycles = read_mcycle32();
    for (uint32_t iter = 0; iter < SHA256_BENCH_ITERS; ++iter) {
        num_blocks = build_padded_blocks(bench_msg, msg_len, bench_blocks);
        status = sha256_accel_hash_blocks(&bench_blocks[0][0], num_blocks, bench_digest);
    }
    hw_cycles = read_mcycle32() - start_cycles;

    CHECK_ASSERT_EOC(10u + (case_idx * 10u) + 3u, (status & SHA256_ACCEL_STATUS_BUSY_BIT_MASK) == 0u);
    CHECK_ASSERT_EOC(10u + (case_idx * 10u) + 4u, (status & SHA256_ACCEL_STATUS_DIGEST_VALID_BIT_MASK) != 0u);
    CHECK_ASSERT_EOC(10u + (case_idx * 10u) + 5u, (status & SHA256_ACCEL_STATUS_START_ERR_BIT_MASK) == 0u);

    print_case_metrics(msg_len, num_blocks, hw_cycles);
    uart_write_flush();
}

int main() {
    uart_init();

    CHECK_ASSERT_EOC(2u, (sha256_accel_get_status() & SHA256_ACCEL_STATUS_READY_BIT_MASK) != 0u);

    for (uint32_t case_idx = 0; case_idx < (sizeof(bench_lengths) / sizeof(bench_lengths[0])); ++case_idx) {
        run_bench_case(case_idx, bench_lengths[case_idx]);
    }

    clear_accel_status_all();
    finish_with_code(0u);
}
