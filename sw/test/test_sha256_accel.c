// Copyright (c) 2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//

#include "config.h"
#include "soc_ctrl.h"
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

// Report directly to the testbench instead of relying on the bootrom return path.
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
    // Keep the polled MMIO status in a register; a volatile stack spill caused a false
    // failure in this simulation setup.
    uint32_t status;

    status = sha256_accel_get_status();
    CHECK_ASSERT_EOC(1, (status & SHA256_ACCEL_STATUS_READY_BIT_MASK) != 0u);
    CHECK_ASSERT_EOC(2, (status & SHA256_ACCEL_STATUS_BUSY_BIT_MASK) == 0u);

    status = sha256_accel_hash_block(abc_block, digest);

    CHECK_ASSERT_EOC(3, (status & SHA256_ACCEL_STATUS_BUSY_BIT_MASK) == 0u);
    CHECK_ASSERT_EOC(4, (status & SHA256_ACCEL_STATUS_DIGEST_VALID_BIT_MASK) != 0u);
    CHECK_ASSERT_EOC(5, (status & SHA256_ACCEL_STATUS_START_ERR_BIT_MASK) == 0u);

    for (int i = 0; i < 8; ++i) {
        CHECK_ASSERT_EOC(6 + i, digest[i] == abc_digest[i]);
    }

    sha256_accel_clear_status(
        SHA256_ACCEL_STATUS_DONE_BIT_MASK |
        SHA256_ACCEL_STATUS_DIGEST_VALID_BIT_MASK |
        SHA256_ACCEL_STATUS_IRQ_PENDING_BIT_MASK |
        SHA256_ACCEL_STATUS_START_ERR_BIT_MASK);

    status = sha256_accel_get_status();
    CHECK_ASSERT_EOC(14, (status & SHA256_ACCEL_STATUS_DONE_BIT_MASK) == 0u);
    CHECK_ASSERT_EOC(15, (status & SHA256_ACCEL_STATUS_DIGEST_VALID_BIT_MASK) == 0u);

    finish_with_code(0);
}
