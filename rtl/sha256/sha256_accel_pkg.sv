// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//

package sha256_accel_pkg;

  localparam int unsigned SHA256_ACCEL_INT_ADDR_WIDTH = 12;

  localparam logic [SHA256_ACCEL_INT_ADDR_WIDTH-1:0] SHA256_ACCEL_CTRL_OFFSET   = 12'h000;
  localparam logic [SHA256_ACCEL_INT_ADDR_WIDTH-1:0] SHA256_ACCEL_STATUS_OFFSET = 12'h004;
  localparam logic [SHA256_ACCEL_INT_ADDR_WIDTH-1:0] SHA256_ACCEL_INFO_OFFSET   = 12'h008;

  localparam logic [SHA256_ACCEL_INT_ADDR_WIDTH-1:0] SHA256_ACCEL_BLOCK0_OFFSET  = 12'h040;
  localparam logic [SHA256_ACCEL_INT_ADDR_WIDTH-1:0] SHA256_ACCEL_BLOCK15_OFFSET = 12'h07C;

  localparam logic [SHA256_ACCEL_INT_ADDR_WIDTH-1:0] SHA256_ACCEL_DIGEST0_OFFSET = 12'h080;
  localparam logic [SHA256_ACCEL_INT_ADDR_WIDTH-1:0] SHA256_ACCEL_DIGEST7_OFFSET = 12'h09C;

  localparam logic [31:0] SHA256_ACCEL_INFO = 32'h0001_0002;

endpackage
