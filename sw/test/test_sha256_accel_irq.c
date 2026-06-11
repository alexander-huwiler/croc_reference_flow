// Copyright (c) 2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//

#include "config.h"
#include "sha256_accel.h"
#include "util.h"

#define BOOTROM_TRAP_HANDLER 0x02000200
#define SRAM_VEC_INTERRUPT   0x10000008

static volatile int irq_fired = 0;
static volatile uint32_t irq_cause = 0;

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

void croc_interrupt_handler(uint32_t cause) {
    irq_cause = cause;
    if (cause == IRQ_SHA256_ACCEL) {
        sha256_accel_clear_status(SHA256_ACCEL_STATUS_IRQ_PENDING_BIT_MASK);
        irq_fired = 1;
    }
}

int main() {
    uint32_t mtvec;
    uint32_t status;
    uint32_t digest[8];

    asm volatile("csrr %0, mtvec" : "=r"(mtvec));
    CHECK_ASSERT(1, mtvec == BOOTROM_TRAP_HANDLER);
    CHECK_ASSERT(2, *(volatile uint32_t *)SRAM_VEC_INTERRUPT == (uint32_t)croc_interrupt_handler);

    CHECK_ASSERT(3, sha256_accel_get_info() == 0x00010002u);

    sha256_accel_set_irq_enable(0);
    CHECK_ASSERT(4, (sha256_accel_get_ctrl() & SHA256_ACCEL_CTRL_IRQ_EN_BIT_MASK) == 0u);
    sha256_accel_set_irq_enable(1);
    CHECK_ASSERT(5, (sha256_accel_get_ctrl() & SHA256_ACCEL_CTRL_IRQ_EN_BIT_MASK) != 0u);

    sha256_accel_write_block(abc_block);
    for (uint32_t word_idx = 0; word_idx < 16; ++word_idx) {
        CHECK_ASSERT((int)(6u + word_idx), sha256_accel_read_block_word(word_idx) == abc_block[word_idx]);
    }

    sha256_accel_clear_status(
        SHA256_ACCEL_STATUS_DONE_BIT_MASK |
        SHA256_ACCEL_STATUS_DIGEST_VALID_BIT_MASK |
        SHA256_ACCEL_STATUS_IRQ_PENDING_BIT_MASK |
        SHA256_ACCEL_STATUS_START_ERR_BIT_MASK);

    irq_fired = 0;
    irq_cause = 0;

    set_interrupt_enable(1, IRQ_SHA256_ACCEL);
    set_global_irq_enable(1);

    sha256_accel_start();

    for (volatile int iter = 0; iter < 50000 && !irq_fired; ++iter)
        ;

    CHECK_ASSERT(22, irq_fired == 1);
    CHECK_ASSERT(23, irq_cause == IRQ_SHA256_ACCEL);

    set_global_irq_enable(0);
    set_interrupt_enable(0, IRQ_SHA256_ACCEL);

    status = sha256_accel_get_status();
    CHECK_ASSERT(24, (status & SHA256_ACCEL_STATUS_DONE_BIT_MASK) != 0u);
    CHECK_ASSERT(25, (status & SHA256_ACCEL_STATUS_DIGEST_VALID_BIT_MASK) != 0u);
    CHECK_ASSERT(26, (status & SHA256_ACCEL_STATUS_IRQ_PENDING_BIT_MASK) == 0u);
    CHECK_ASSERT(27, (status & SHA256_ACCEL_STATUS_START_ERR_BIT_MASK) == 0u);

    sha256_accel_read_digest(digest);
    for (uint32_t word_idx = 0; word_idx < 8; ++word_idx) {
        CHECK_ASSERT((int)(28u + word_idx), digest[word_idx] == abc_digest[word_idx]);
    }

    sha256_accel_set_irq_enable(0);
    CHECK_ASSERT(36, (sha256_accel_get_ctrl() & SHA256_ACCEL_CTRL_IRQ_EN_BIT_MASK) == 0u);

    sha256_accel_clear_status(
        SHA256_ACCEL_STATUS_DONE_BIT_MASK |
        SHA256_ACCEL_STATUS_DIGEST_VALID_BIT_MASK |
        SHA256_ACCEL_STATUS_IRQ_PENDING_BIT_MASK |
        SHA256_ACCEL_STATUS_START_ERR_BIT_MASK);

    status = sha256_accel_get_status();
    CHECK_ASSERT(37, (status & SHA256_ACCEL_STATUS_DONE_BIT_MASK) == 0u);
    CHECK_ASSERT(38, (status & SHA256_ACCEL_STATUS_DIGEST_VALID_BIT_MASK) == 0u);
    CHECK_ASSERT(39, (status & SHA256_ACCEL_STATUS_IRQ_PENDING_BIT_MASK) == 0u);

    return 0;
}
