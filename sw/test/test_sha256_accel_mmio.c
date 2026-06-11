// Copyright (c) 2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//

#include "config.h"
#include "sha256_accel.h"
#include "util.h"

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

static inline void write_sha_reg8(int offs, uint8_t value) {
    *reg8(SHA256_ACCEL_BASE_ADDR, offs) = value;
}

int main() {
    uint32_t status = sha256_accel_get_status();
    CHECK_ASSERT(1, (status & SHA256_ACCEL_STATUS_READY_BIT_MASK) != 0u);

    // Byte-enable behavior on block registers.
    write_sha_reg8(SHA256_ACCEL_BLOCK_REG_OFFSET(0) + 0, 0x11);
    write_sha_reg8(SHA256_ACCEL_BLOCK_REG_OFFSET(0) + 1, 0x22);
    write_sha_reg8(SHA256_ACCEL_BLOCK_REG_OFFSET(0) + 2, 0x33);
    write_sha_reg8(SHA256_ACCEL_BLOCK_REG_OFFSET(0) + 3, 0x44);
    CHECK_ASSERT(2, sha256_accel_read_block_word(0) == 0x44332211u);

    write_sha_reg8(SHA256_ACCEL_BLOCK_REG_OFFSET(0) + 1, 0x66);
    CHECK_ASSERT(3, sha256_accel_read_block_word(0) == 0x44336611u);

    write_sha_reg8(SHA256_ACCEL_BLOCK_REG_OFFSET(1) + 0, 0xaa);
    write_sha_reg8(SHA256_ACCEL_BLOCK_REG_OFFSET(1) + 1, 0xbb);
    write_sha_reg8(SHA256_ACCEL_BLOCK_REG_OFFSET(1) + 2, 0xcc);
    write_sha_reg8(SHA256_ACCEL_BLOCK_REG_OFFSET(1) + 3, 0xdd);
    CHECK_ASSERT(4, sha256_accel_read_block_word(1) == 0xddccbbaau);

    // Byte write on CTRL must allow enabling/disabling IRQ without pulsing START.
    write_sha_reg8(SHA256_ACCEL_CTRL_REG_OFFSET, 0x02);
    CHECK_ASSERT(5, (sha256_accel_get_ctrl() & SHA256_ACCEL_CTRL_IRQ_EN_BIT_MASK) != 0u);
    CHECK_ASSERT(6, (sha256_accel_get_status() & SHA256_ACCEL_STATUS_BUSY_BIT_MASK) == 0u);

    write_sha_reg8(SHA256_ACCEL_CTRL_REG_OFFSET, 0x00);
    CHECK_ASSERT(7, (sha256_accel_get_ctrl() & SHA256_ACCEL_CTRL_IRQ_EN_BIT_MASK) == 0u);

    // Hash a known-good block after the MMIO scratch accesses.
    uint32_t digest[8];
    status = sha256_accel_hash_block(abc_block, digest);
    CHECK_ASSERT(8, (status & SHA256_ACCEL_STATUS_DONE_BIT_MASK) != 0u);
    CHECK_ASSERT(9, (status & SHA256_ACCEL_STATUS_DIGEST_VALID_BIT_MASK) != 0u);
    CHECK_ASSERT(10, (status & SHA256_ACCEL_STATUS_START_ERR_BIT_MASK) == 0u);
    for (int i = 0; i < 8; ++i) {
        CHECK_ASSERT(11 + i, digest[i] == abc_digest[i]);
    }

    // Byte write to STATUS must clear sticky status bits.
    write_sha_reg8(SHA256_ACCEL_STATUS_REG_OFFSET, 0x0c);
    status = sha256_accel_get_status();
    CHECK_ASSERT(19, (status & SHA256_ACCEL_STATUS_DONE_BIT_MASK) == 0u);
    CHECK_ASSERT(20, (status & SHA256_ACCEL_STATUS_DIGEST_VALID_BIT_MASK) == 0u);
    CHECK_ASSERT(21, (status & SHA256_ACCEL_STATUS_READY_BIT_MASK) != 0u);

    return 0;
}
