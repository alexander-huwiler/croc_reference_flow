// Copyright (c) 2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//

#include "config.h"
#include "sha256_accel.h"
#include "soc_ctrl.h"
#include "util.h"

static const uint32_t empty_msg_blocks[1][16] = {
    {
        0x80000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
};

static const uint32_t empty_msg_digest[8] = {
    0xe3b0c442, 0x98fc1c14, 0x9afbf4c8, 0x996fb924,
    0x27ae41e4, 0x649b934c, 0xa495991b, 0x7852b855,
};

static const uint32_t message_digest_blocks[1][16] = {
    {
        0x6d657373, 0x61676520, 0x64696765, 0x73748000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000070,
    },
};

static const uint32_t message_digest_digest[8] = {
    0xf7846f55, 0xcf23e14e, 0xebeab5b4, 0xe1550cad,
    0x5b509e33, 0x48fbc4ef, 0xa3a1413d, 0x393cb650,
};

static const uint32_t alphabet_blocks[1][16] = {
    {
        0x61626364, 0x65666768, 0x696a6b6c, 0x6d6e6f70,
        0x71727374, 0x75767778, 0x797a8000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x000000d0,
    },
};

static const uint32_t alphabet_digest[8] = {
    0x71c480df, 0x93d6ae2f, 0x1efad144, 0x7c66c952,
    0x5e316218, 0xcf51fc8d, 0x9ed832f2, 0xdaf18b73,
};

static const uint32_t a56_blocks[2][16] = {
    {
        0x61616161, 0x61616161, 0x61616161, 0x61616161,
        0x61616161, 0x61616161, 0x61616161, 0x61616161,
        0x61616161, 0x61616161, 0x61616161, 0x61616161,
        0x61616161, 0x61616161, 0x80000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x000001c0,
    },
};

static const uint32_t a56_digest[8] = {
    0xb35439a4, 0xac6f0948, 0xb6d6f9e3, 0xc6af0f5f,
    0x590ce20f, 0x1bde7090, 0xef797068, 0x6ec6738a,
};

static const uint32_t a55_blocks[1][16] = {
    {
        0x61616161, 0x61616161, 0x61616161, 0x61616161,
        0x61616161, 0x61616161, 0x61616161, 0x61616161,
        0x61616161, 0x61616161, 0x61616161, 0x61616161,
        0x61616161, 0x61616180, 0x00000000, 0x000001b8,
    },
};

static const uint32_t a55_digest[8] = {
    0x9f4390f8, 0xd30c2dd9, 0x2ec9f095, 0xb65e2b9a,
    0xe9b0a925, 0xa5258e24, 0x1c9f1e91, 0x0f734318,
};

static const uint32_t a63_blocks[2][16] = {
    {
        0x61616161, 0x61616161, 0x61616161, 0x61616161,
        0x61616161, 0x61616161, 0x61616161, 0x61616161,
        0x61616161, 0x61616161, 0x61616161, 0x61616161,
        0x61616161, 0x61616161, 0x61616161, 0x61616180,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x000001f8,
    },
};

static const uint32_t a63_digest[8] = {
    0x7d3e74a0, 0x5d7db15b, 0xce4ad9ec, 0x0658ea98,
    0xe3f06eee, 0xcf16b4c6, 0xfff2da45, 0x7ddc2f34,
};

static const uint32_t a64_blocks[2][16] = {
    {
        0x61616161, 0x61616161, 0x61616161, 0x61616161,
        0x61616161, 0x61616161, 0x61616161, 0x61616161,
        0x61616161, 0x61616161, 0x61616161, 0x61616161,
        0x61616161, 0x61616161, 0x61616161, 0x61616161,
    },
    {
        0x80000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000200,
    },
};

static const uint32_t a64_digest[8] = {
    0xffe054fe, 0x7ae0cb6d, 0xc65c3af9, 0xb61d5209,
    0xf439851d, 0xb43d0ba5, 0x997337df, 0x154668eb,
};

static const uint32_t a65_blocks[2][16] = {
    {
        0x61616161, 0x61616161, 0x61616161, 0x61616161,
        0x61616161, 0x61616161, 0x61616161, 0x61616161,
        0x61616161, 0x61616161, 0x61616161, 0x61616161,
        0x61616161, 0x61616161, 0x61616161, 0x61616161,
    },
    {
        0x61800000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000208,
    },
};

static const uint32_t a65_digest[8] = {
    0x635361c4, 0x8bb9eab1, 0x4198e76e, 0xa8ab7f1a,
    0x41685d6a, 0xd62aa914, 0x6d301d4f, 0x17eb0ae0,
};

static const uint32_t a120_blocks[3][16] = {
    {
        0x61616161, 0x61616161, 0x61616161, 0x61616161,
        0x61616161, 0x61616161, 0x61616161, 0x61616161,
        0x61616161, 0x61616161, 0x61616161, 0x61616161,
        0x61616161, 0x61616161, 0x61616161, 0x61616161,
    },
    {
        0x61616161, 0x61616161, 0x61616161, 0x61616161,
        0x61616161, 0x61616161, 0x61616161, 0x61616161,
        0x61616161, 0x61616161, 0x61616161, 0x61616161,
        0x61616161, 0x61616161, 0x80000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x000003c0,
    },
};

static const uint32_t a120_digest[8] = {
    0x2f3d3354, 0x32c70b58, 0x0af0e8e1, 0xb3674a7c,
    0x020d683a, 0xa5f73aaa, 0xedfdc55a, 0xf904c21c,
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

static uint32_t run_case(const uint32_t *blocks, uint32_t num_blocks,
                         const uint32_t expected_digest[8], uint32_t err_base) {
    uint32_t digest[8];
    uint32_t status = sha256_accel_hash_blocks(blocks, num_blocks, digest);

    CHECK_ASSERT_EOC(err_base + 0, (status & SHA256_ACCEL_STATUS_BUSY_BIT_MASK) == 0u);
    CHECK_ASSERT_EOC(err_base + 1, (status & SHA256_ACCEL_STATUS_DONE_BIT_MASK) != 0u);
    CHECK_ASSERT_EOC(err_base + 2, (status & SHA256_ACCEL_STATUS_DIGEST_VALID_BIT_MASK) != 0u);
    CHECK_ASSERT_EOC(err_base + 3, (status & SHA256_ACCEL_STATUS_START_ERR_BIT_MASK) == 0u);

    for (int i = 0; i < 8; ++i) {
        CHECK_ASSERT_EOC(err_base + 4 + (uint32_t)i, digest[i] == expected_digest[i]);
    }

    sha256_accel_clear_status(
        SHA256_ACCEL_STATUS_DONE_BIT_MASK |
        SHA256_ACCEL_STATUS_DIGEST_VALID_BIT_MASK |
        SHA256_ACCEL_STATUS_IRQ_PENDING_BIT_MASK |
        SHA256_ACCEL_STATUS_START_ERR_BIT_MASK);

    status = sha256_accel_get_status();
    CHECK_ASSERT_EOC(err_base + 12, (status & SHA256_ACCEL_STATUS_DONE_BIT_MASK) == 0u);
    CHECK_ASSERT_EOC(err_base + 13, (status & SHA256_ACCEL_STATUS_DIGEST_VALID_BIT_MASK) == 0u);

    return status;
}

int main() {
    uint32_t status = sha256_accel_get_status();

    CHECK_ASSERT_EOC(1, (status & SHA256_ACCEL_STATUS_READY_BIT_MASK) != 0u);
    CHECK_ASSERT_EOC(2, (status & SHA256_ACCEL_STATUS_BUSY_BIT_MASK) == 0u);

    run_case(&empty_msg_blocks[0][0], 1, empty_msg_digest, 10);
    run_case(&message_digest_blocks[0][0], 1, message_digest_digest, 30);
    run_case(&alphabet_blocks[0][0], 1, alphabet_digest, 50);
    run_case(&a56_blocks[0][0], 2, a56_digest, 70);
    run_case(&a55_blocks[0][0], 1, a55_digest, 90);
    run_case(&a63_blocks[0][0], 2, a63_digest, 110);
    run_case(&a64_blocks[0][0], 2, a64_digest, 130);
    run_case(&a65_blocks[0][0], 2, a65_digest, 150);
    run_case(&a120_blocks[0][0], 3, a120_digest, 170);

    finish_with_code(0);
}
