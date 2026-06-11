// Copyright (c) 2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//

#include "config.h"
#include "soc_ctrl.h"
#include "sha256_accel.h"
#include "util.h"

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

int main() {
    uint32_t digest[8];
    uint32_t status;

    status = sha256_accel_get_status();
    CHECK_ASSERT_EOC(1, (status & SHA256_ACCEL_STATUS_READY_BIT_MASK) != 0u);
    CHECK_ASSERT_EOC(2, (status & SHA256_ACCEL_STATUS_BUSY_BIT_MASK) == 0u);

    // Chaining without any previous digest state should fail cleanly.
    sha256_accel_write_block(sha256_two_block_msg[1]);
    sha256_accel_start_chain(1);
    status = sha256_accel_get_status();
    CHECK_ASSERT_EOC(3, (status & SHA256_ACCEL_STATUS_START_ERR_BIT_MASK) != 0u);
    CHECK_ASSERT_EOC(4, (status & SHA256_ACCEL_STATUS_BUSY_BIT_MASK) == 0u);
    sha256_accel_clear_status(SHA256_ACCEL_STATUS_START_ERR_BIT_MASK);

    status = sha256_accel_process_block(sha256_two_block_msg[0], 0, 0);
    CHECK_ASSERT_EOC(5, (status & SHA256_ACCEL_STATUS_START_ERR_BIT_MASK) == 0u);
    CHECK_ASSERT_EOC(6, (status & SHA256_ACCEL_STATUS_DIGEST_VALID_BIT_MASK) != 0u);

    status = sha256_accel_process_block(sha256_two_block_msg[1], 1, digest);
    CHECK_ASSERT_EOC(7, (status & SHA256_ACCEL_STATUS_BUSY_BIT_MASK) == 0u);
    CHECK_ASSERT_EOC(8, (status & SHA256_ACCEL_STATUS_DIGEST_VALID_BIT_MASK) != 0u);
    CHECK_ASSERT_EOC(9, (status & SHA256_ACCEL_STATUS_START_ERR_BIT_MASK) == 0u);

    for (int i = 0; i < 8; ++i) {
        CHECK_ASSERT_EOC(10 + i, digest[i] == sha256_two_block_digest[i]);
    }

    sha256_accel_clear_status(
        SHA256_ACCEL_STATUS_DONE_BIT_MASK |
        SHA256_ACCEL_STATUS_DIGEST_VALID_BIT_MASK |
        SHA256_ACCEL_STATUS_IRQ_PENDING_BIT_MASK |
        SHA256_ACCEL_STATUS_START_ERR_BIT_MASK);

    status = sha256_accel_get_status();
    CHECK_ASSERT_EOC(18, (status & SHA256_ACCEL_STATUS_DONE_BIT_MASK) == 0u);
    CHECK_ASSERT_EOC(19, (status & SHA256_ACCEL_STATUS_DIGEST_VALID_BIT_MASK) == 0u);

    finish_with_code(0);
}
