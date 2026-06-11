// Copyright 2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <stdint.h>

// Current programming model:
// - write one already padded 512-bit block into BLOCK[0..15]
// - pulse CTRL.START for a fresh hash from the standard SHA-256 IV
// - pulse CTRL.START | CTRL.CHAIN for continuation blocks of the same message
// - poll STATUS.DONE or use the optional interrupt
// - read DIGEST[0..7]

#define SHA256_ACCEL_CTRL_REG_OFFSET      0x00
#define SHA256_ACCEL_STATUS_REG_OFFSET    0x04
#define SHA256_ACCEL_INFO_REG_OFFSET      0x08

#define SHA256_ACCEL_BLOCK_REG_OFFSET(i)  (0x40 + (4 * (i)))
#define SHA256_ACCEL_DIGEST_REG_OFFSET(i) (0x80 + (4 * (i)))

#define SHA256_ACCEL_CTRL_START_BIT       0
#define SHA256_ACCEL_CTRL_IRQ_EN_BIT      1
#define SHA256_ACCEL_CTRL_CHAIN_BIT       2

#define SHA256_ACCEL_CTRL_START_BIT_MASK  (1u << SHA256_ACCEL_CTRL_START_BIT)
#define SHA256_ACCEL_CTRL_IRQ_EN_BIT_MASK (1u << SHA256_ACCEL_CTRL_IRQ_EN_BIT)
#define SHA256_ACCEL_CTRL_CHAIN_BIT_MASK  (1u << SHA256_ACCEL_CTRL_CHAIN_BIT)

#define SHA256_ACCEL_STATUS_READY_BIT        0
#define SHA256_ACCEL_STATUS_BUSY_BIT         1
#define SHA256_ACCEL_STATUS_DONE_BIT         2
#define SHA256_ACCEL_STATUS_DIGEST_VALID_BIT 3
#define SHA256_ACCEL_STATUS_IRQ_PENDING_BIT  4
#define SHA256_ACCEL_STATUS_START_ERR_BIT    5

#define SHA256_ACCEL_STATUS_READY_BIT_MASK        (1u << SHA256_ACCEL_STATUS_READY_BIT)
#define SHA256_ACCEL_STATUS_BUSY_BIT_MASK         (1u << SHA256_ACCEL_STATUS_BUSY_BIT)
#define SHA256_ACCEL_STATUS_DONE_BIT_MASK         (1u << SHA256_ACCEL_STATUS_DONE_BIT)
#define SHA256_ACCEL_STATUS_DIGEST_VALID_BIT_MASK (1u << SHA256_ACCEL_STATUS_DIGEST_VALID_BIT)
#define SHA256_ACCEL_STATUS_IRQ_PENDING_BIT_MASK  (1u << SHA256_ACCEL_STATUS_IRQ_PENDING_BIT)
#define SHA256_ACCEL_STATUS_START_ERR_BIT_MASK    (1u << SHA256_ACCEL_STATUS_START_ERR_BIT)

uint32_t sha256_accel_get_ctrl();
uint32_t sha256_accel_get_status();
uint32_t sha256_accel_get_info();

void sha256_accel_set_irq_enable(int enable);
void sha256_accel_start();
void sha256_accel_start_chain(int chain);
void sha256_accel_clear_status(uint32_t clear_mask);

void sha256_accel_write_block_word(uint32_t index, uint32_t value);
uint32_t sha256_accel_read_block_word(uint32_t index);
void sha256_accel_write_block(const uint32_t block[16]);

uint32_t sha256_accel_read_digest_word(uint32_t index);
void sha256_accel_read_digest(uint32_t digest[8]);

uint32_t sha256_accel_wait_done();
uint32_t sha256_accel_process_block(const uint32_t block[16], int chain, uint32_t digest[8]);
uint32_t sha256_accel_hash_block(const uint32_t block[16], uint32_t digest[8]);
uint32_t sha256_accel_hash_blocks(const uint32_t *blocks, uint32_t num_blocks, uint32_t digest[8]);
