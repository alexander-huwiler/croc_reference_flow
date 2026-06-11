// Copyright (c) 2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//

#include "config.h"
#include "sha256_accel.h"
#include "soc_ctrl.h"
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

static const uint32_t sha256_two_block_msg[2][16] = {
    {
        0x61626364, 0x62636465, 0x63646566, 0x64656667,
        0x65666768, 0x66676869, 0x6768696a, 0x68696a6b,
        0x696a6b6c, 0x6a6b6c6d, 0x6b6c6d6e, 0x6c6d6e6f,
        0x6d6e6f70, 0x6e6f7071, 0x80000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x000001c0,
    },
};

static const uint32_t sha256_two_block_digest[8] = {
    0x248d6a61, 0xd20638b8, 0xe5c02693, 0x0c3e6039,
    0xa33ce459, 0x64ff2167, 0xf6ecedd4, 0x19db06c1,
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

static void clear_accel_status_all(void) {
    sha256_accel_clear_status(
        SHA256_ACCEL_STATUS_DONE_BIT_MASK |
        SHA256_ACCEL_STATUS_DIGEST_VALID_BIT_MASK |
        SHA256_ACCEL_STATUS_IRQ_PENDING_BIT_MASK |
        SHA256_ACCEL_STATUS_START_ERR_BIT_MASK);
}

static void check_digest(uint32_t base_code, const uint32_t digest[8], const uint32_t expected[8]) {
    for (int i = 0; i < 8; ++i) {
        CHECK_ASSERT_EOC(base_code + (uint32_t)i, digest[i] == expected[i]);
    }
}

int main() {
    uint32_t digest[8];
    uint32_t status;

    clear_accel_status_all();

    // A fresh hash of the same block should always restart from the SHA-256 IV.
    status = sha256_accel_hash_block(abc_block, digest);
    CHECK_ASSERT_EOC(1, (status & SHA256_ACCEL_STATUS_START_ERR_BIT_MASK) == 0u);
    CHECK_ASSERT_EOC(2, (status & SHA256_ACCEL_STATUS_DIGEST_VALID_BIT_MASK) != 0u);
    check_digest(10, digest, abc_digest);

    status = sha256_accel_hash_block(abc_block, digest);
    CHECK_ASSERT_EOC(3, (status & SHA256_ACCEL_STATUS_START_ERR_BIT_MASK) == 0u);
    CHECK_ASSERT_EOC(4, (status & SHA256_ACCEL_STATUS_DIGEST_VALID_BIT_MASK) != 0u);
    check_digest(20, digest, abc_digest);

    // After a chained multi-block hash, a plain START must still reset state.
    clear_accel_status_all();
    status = sha256_accel_hash_blocks(&sha256_two_block_msg[0][0], 2, digest);
    CHECK_ASSERT_EOC(5, (status & SHA256_ACCEL_STATUS_START_ERR_BIT_MASK) == 0u);
    CHECK_ASSERT_EOC(6, (status & SHA256_ACCEL_STATUS_DIGEST_VALID_BIT_MASK) != 0u);
    check_digest(30, digest, sha256_two_block_digest);

    status = sha256_accel_hash_block(abc_block, digest);
    CHECK_ASSERT_EOC(7, (status & SHA256_ACCEL_STATUS_START_ERR_BIT_MASK) == 0u);
    CHECK_ASSERT_EOC(8, (status & SHA256_ACCEL_STATUS_DIGEST_VALID_BIT_MASK) != 0u);
    check_digest(40, digest, abc_digest);

    // START while BUSY should not hang and should raise START_ERR.
    clear_accel_status_all();
    sha256_accel_write_block(abc_block);
    sha256_accel_start();
    do {
        status = sha256_accel_get_status();
    } while ((status & SHA256_ACCEL_STATUS_BUSY_BIT_MASK) == 0u);

    sha256_accel_start();
    status = sha256_accel_wait_done();
    CHECK_ASSERT_EOC(9, (status & SHA256_ACCEL_STATUS_BUSY_BIT_MASK) == 0u);
    CHECK_ASSERT_EOC(10, (status & SHA256_ACCEL_STATUS_DONE_BIT_MASK) != 0u);
    CHECK_ASSERT_EOC(11, (status & SHA256_ACCEL_STATUS_DIGEST_VALID_BIT_MASK) != 0u);
    CHECK_ASSERT_EOC(12, (status & SHA256_ACCEL_STATUS_START_ERR_BIT_MASK) != 0u);
    sha256_accel_read_digest(digest);
    check_digest(50, digest, abc_digest);

    clear_accel_status_all();
    status = sha256_accel_get_status();
    CHECK_ASSERT_EOC(13, (status & SHA256_ACCEL_STATUS_DONE_BIT_MASK) == 0u);
    CHECK_ASSERT_EOC(14, (status & SHA256_ACCEL_STATUS_DIGEST_VALID_BIT_MASK) == 0u);
    CHECK_ASSERT_EOC(15, (status & SHA256_ACCEL_STATUS_IRQ_PENDING_BIT_MASK) == 0u);
    CHECK_ASSERT_EOC(16, (status & SHA256_ACCEL_STATUS_START_ERR_BIT_MASK) == 0u);

    finish_with_code(0);
}
