// Copyright 2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//

#include "sha256_accel.h"
#include "config.h"
#include "util.h"

static inline uint32_t read_ctrl_reg() {
    return *reg32(SHA256_ACCEL_BASE_ADDR, SHA256_ACCEL_CTRL_REG_OFFSET);
}

static inline uint32_t read_status_reg() {
    return *reg32(SHA256_ACCEL_BASE_ADDR, SHA256_ACCEL_STATUS_REG_OFFSET);
}

static inline uint32_t read_info_reg() {
    return *reg32(SHA256_ACCEL_BASE_ADDR, SHA256_ACCEL_INFO_REG_OFFSET);
}

static inline void write_ctrl_reg(uint32_t value) {
    *reg32(SHA256_ACCEL_BASE_ADDR, SHA256_ACCEL_CTRL_REG_OFFSET) = value;
}

static inline void write_status_reg(uint32_t value) {
    *reg32(SHA256_ACCEL_BASE_ADDR, SHA256_ACCEL_STATUS_REG_OFFSET) = value;
}

static inline void write_block_reg(uint32_t index, uint32_t value) {
    *reg32(SHA256_ACCEL_BASE_ADDR, SHA256_ACCEL_BLOCK_REG_OFFSET(index)) = value;
}

static inline uint32_t read_block_reg(uint32_t index) {
    return *reg32(SHA256_ACCEL_BASE_ADDR, SHA256_ACCEL_BLOCK_REG_OFFSET(index));
}

static inline uint32_t read_digest_reg(uint32_t index) {
    return *reg32(SHA256_ACCEL_BASE_ADDR, SHA256_ACCEL_DIGEST_REG_OFFSET(index));
}

uint32_t sha256_accel_get_ctrl() {
    return read_ctrl_reg();
}

uint32_t sha256_accel_get_status() {
    return read_status_reg();
}

uint32_t sha256_accel_get_info() {
    return read_info_reg();
}

void sha256_accel_set_irq_enable(int enable) {
    uint32_t ctrl = read_ctrl_reg();
    if (enable)
        ctrl |= SHA256_ACCEL_CTRL_IRQ_EN_BIT_MASK;
    else
        ctrl &= ~SHA256_ACCEL_CTRL_IRQ_EN_BIT_MASK;
    write_ctrl_reg(ctrl);
}

void sha256_accel_start() {
    uint32_t ctrl = read_ctrl_reg() & SHA256_ACCEL_CTRL_IRQ_EN_BIT_MASK;
    ctrl |= SHA256_ACCEL_CTRL_START_BIT_MASK;
    write_ctrl_reg(ctrl);
    fence();
}

void sha256_accel_start_chain(int chain) {
    uint32_t ctrl = read_ctrl_reg() & SHA256_ACCEL_CTRL_IRQ_EN_BIT_MASK;
    ctrl |= SHA256_ACCEL_CTRL_START_BIT_MASK;
    if (chain) {
        ctrl |= SHA256_ACCEL_CTRL_CHAIN_BIT_MASK;
    }
    write_ctrl_reg(ctrl);
    fence();
}

void sha256_accel_clear_status(uint32_t clear_mask) {
    const uint32_t writable_mask =
        SHA256_ACCEL_STATUS_DONE_BIT_MASK |
        SHA256_ACCEL_STATUS_DIGEST_VALID_BIT_MASK |
        SHA256_ACCEL_STATUS_IRQ_PENDING_BIT_MASK |
        SHA256_ACCEL_STATUS_START_ERR_BIT_MASK;
    write_status_reg(clear_mask & writable_mask);
}

void sha256_accel_write_block_word(uint32_t index, uint32_t value) {
    if (index >= 16) return;
    write_block_reg(index, value);
}

uint32_t sha256_accel_read_block_word(uint32_t index) {
    if (index >= 16) return 0;
    return read_block_reg(index);
}

void sha256_accel_write_block(const uint32_t block[16]) {
    for (uint32_t i = 0; i < 16; ++i) {
        write_block_reg(i, block[i]);
    }
}

uint32_t sha256_accel_read_digest_word(uint32_t index) {
    if (index >= 8) return 0;
    return read_digest_reg(index);
}

void sha256_accel_read_digest(uint32_t digest[8]) {
    for (uint32_t i = 0; i < 8; ++i) {
        digest[i] = read_digest_reg(i);
    }
}

uint32_t sha256_accel_wait_done() {
    uint32_t status;
    do {
        status = read_status_reg();
    } while (((status & SHA256_ACCEL_STATUS_DONE_BIT_MASK) == 0u) &&
             (((status & SHA256_ACCEL_STATUS_START_ERR_BIT_MASK) == 0u) ||
              ((status & SHA256_ACCEL_STATUS_BUSY_BIT_MASK) != 0u)));
    return status;
}

uint32_t sha256_accel_process_block(const uint32_t block[16], int chain, uint32_t digest[8]) {
    sha256_accel_write_block(block);
    sha256_accel_start_chain(chain);
    uint32_t status = sha256_accel_wait_done();
    if (digest != 0) {
        sha256_accel_read_digest(digest);
    }
    return status;
}

uint32_t sha256_accel_hash_block(const uint32_t block[16], uint32_t digest[8]) {
    return sha256_accel_process_block(block, 0, digest);
}

uint32_t sha256_accel_hash_blocks(const uint32_t *blocks, uint32_t num_blocks, uint32_t digest[8]) {
    uint32_t status = 0;

    for (uint32_t block_idx = 0; block_idx < num_blocks; ++block_idx) {
        uint32_t *digest_out = (block_idx + 1u == num_blocks) ? digest : 0;
        status = sha256_accel_process_block(&blocks[block_idx * 16], block_idx != 0u, digest_out);
        if ((status & SHA256_ACCEL_STATUS_START_ERR_BIT_MASK) != 0u) {
            break;
        }
    }
    return status;
}
