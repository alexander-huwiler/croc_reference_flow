// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//

module sha256_accel #(
  parameter type obi_req_t = logic,
  parameter type obi_rsp_t = logic
) (
  input  logic     clk_i,
  input  logic     rst_ni,
  input  obi_req_t obi_req_i,
  output obi_rsp_t obi_rsp_o,
  output logic     interrupt_o
);

  import sha256_accel_pkg::*;

  // Milestone-2 implementation:
  // - software provides fully padded 512-bit SHA-256 blocks
  // - each START command performs one 64-round compression
  // - CTRL.CHAIN lets software continue from the previous digest state for
  //   multi-block messages
  // - a plain START always reinitializes to the standard SHA-256 IV

  localparam logic [31:0] SHA256_H_INIT_0 = 32'h6a09e667;
  localparam logic [31:0] SHA256_H_INIT_1 = 32'hbb67ae85;
  localparam logic [31:0] SHA256_H_INIT_2 = 32'h3c6ef372;
  localparam logic [31:0] SHA256_H_INIT_3 = 32'ha54ff53a;
  localparam logic [31:0] SHA256_H_INIT_4 = 32'h510e527f;
  localparam logic [31:0] SHA256_H_INIT_5 = 32'h9b05688c;
  localparam logic [31:0] SHA256_H_INIT_6 = 32'h1f83d9ab;
  localparam logic [31:0] SHA256_H_INIT_7 = 32'h5be0cd19;

  logic [31:0] block_d  [0:15];
  logic [31:0] block_q  [0:15];
  logic [31:0] digest_d [0:7];
  logic [31:0] digest_q [0:7];
  logic [31:0] hash_state_d [0:7];
  logic [31:0] hash_state_q [0:7];
  logic [31:0] w_mem_d  [0:15];
  logic [31:0] w_mem_q  [0:15];

  logic [31:0] a_d, a_q;
  logic [31:0] b_d, b_q;
  logic [31:0] c_d, c_q;
  logic [31:0] d_d, d_q;
  logic [31:0] e_d, e_q;
  logic [31:0] f_d, f_q;
  logic [31:0] g_d, g_q;
  logic [31:0] h_d, h_q;

  logic [5:0] round_d, round_q;

  logic     irq_enable_d,   irq_enable_q;
  logic     busy_d,         busy_q;
  logic     done_d,         done_q;
  logic     digest_valid_d, digest_valid_q;
  logic     irq_pending_d,  irq_pending_q;
  logic     start_error_d,  start_error_q;
  logic     state_valid_d,  state_valid_q;

  obi_req_t      obi_req_d, obi_req_q;
  logic              err_d,     err_q;
  logic     [31:0] rdata_d,   rdata_q;

  logic [31:0] be_mask;

  function automatic logic [31:0] rotr32(input logic [31:0] value, input int unsigned shift);
    return (value >> shift) | (value << (32 - shift));
  endfunction

  function automatic logic [31:0] big_sigma0(input logic [31:0] value);
    return rotr32(value, 2) ^ rotr32(value, 13) ^ rotr32(value, 22);
  endfunction

  function automatic logic [31:0] big_sigma1(input logic [31:0] value);
    return rotr32(value, 6) ^ rotr32(value, 11) ^ rotr32(value, 25);
  endfunction

  function automatic logic [31:0] small_sigma0(input logic [31:0] value);
    return rotr32(value, 7) ^ rotr32(value, 18) ^ (value >> 3);
  endfunction

  function automatic logic [31:0] small_sigma1(input logic [31:0] value);
    return rotr32(value, 17) ^ rotr32(value, 19) ^ (value >> 10);
  endfunction

  function automatic logic [31:0] sha256_k(input logic [5:0] idx);
    unique case (idx)
      6'd0:  return 32'h428a2f98;
      6'd1:  return 32'h71374491;
      6'd2:  return 32'hb5c0fbcf;
      6'd3:  return 32'he9b5dba5;
      6'd4:  return 32'h3956c25b;
      6'd5:  return 32'h59f111f1;
      6'd6:  return 32'h923f82a4;
      6'd7:  return 32'hab1c5ed5;
      6'd8:  return 32'hd807aa98;
      6'd9:  return 32'h12835b01;
      6'd10: return 32'h243185be;
      6'd11: return 32'h550c7dc3;
      6'd12: return 32'h72be5d74;
      6'd13: return 32'h80deb1fe;
      6'd14: return 32'h9bdc06a7;
      6'd15: return 32'hc19bf174;
      6'd16: return 32'he49b69c1;
      6'd17: return 32'hefbe4786;
      6'd18: return 32'h0fc19dc6;
      6'd19: return 32'h240ca1cc;
      6'd20: return 32'h2de92c6f;
      6'd21: return 32'h4a7484aa;
      6'd22: return 32'h5cb0a9dc;
      6'd23: return 32'h76f988da;
      6'd24: return 32'h983e5152;
      6'd25: return 32'ha831c66d;
      6'd26: return 32'hb00327c8;
      6'd27: return 32'hbf597fc7;
      6'd28: return 32'hc6e00bf3;
      6'd29: return 32'hd5a79147;
      6'd30: return 32'h06ca6351;
      6'd31: return 32'h14292967;
      6'd32: return 32'h27b70a85;
      6'd33: return 32'h2e1b2138;
      6'd34: return 32'h4d2c6dfc;
      6'd35: return 32'h53380d13;
      6'd36: return 32'h650a7354;
      6'd37: return 32'h766a0abb;
      6'd38: return 32'h81c2c92e;
      6'd39: return 32'h92722c85;
      6'd40: return 32'ha2bfe8a1;
      6'd41: return 32'ha81a664b;
      6'd42: return 32'hc24b8b70;
      6'd43: return 32'hc76c51a3;
      6'd44: return 32'hd192e819;
      6'd45: return 32'hd6990624;
      6'd46: return 32'hf40e3585;
      6'd47: return 32'h106aa070;
      6'd48: return 32'h19a4c116;
      6'd49: return 32'h1e376c08;
      6'd50: return 32'h2748774c;
      6'd51: return 32'h34b0bcb5;
      6'd52: return 32'h391c0cb3;
      6'd53: return 32'h4ed8aa4a;
      6'd54: return 32'h5b9cca4f;
      6'd55: return 32'h682e6ff3;
      6'd56: return 32'h748f82ee;
      6'd57: return 32'h78a5636f;
      6'd58: return 32'h84c87814;
      6'd59: return 32'h8cc70208;
      6'd60: return 32'h90befffa;
      6'd61: return 32'ha4506ceb;
      6'd62: return 32'hbef9a3f7;
      6'd63: return 32'hc67178f2;
      default: return 32'h0;
    endcase
  endfunction

  assign obi_req_d = obi_req_i;

  for (genvar i = 0; i < 32 / 8; ++i) begin : gen_write_mask
    assign be_mask[8*i +: 8] = {8{obi_req_i.a.be[i]}};
  end

  always_comb begin : obi_response
    obi_rsp_o         = '0;
    obi_rsp_o.gnt     = 1'b1;
    obi_rsp_o.rvalid  = obi_req_q.req;
    obi_rsp_o.r.err   = err_q;
    obi_rsp_o.r.rid   = obi_req_q.a.aid;
    obi_rsp_o.r.rdata = rdata_q;
  end

  always_comb begin
    logic       start_cmd;
    logic       chain_cmd;
    logic [5:0] reg_word_idx;
    logic [31:0] w_t;
    logic [31:0] t1;
    logic [31:0] t2;
    logic [31:0] a_next;
    logic [31:0] b_next;
    logic [31:0] c_next;
    logic [31:0] d_next;
    logic [31:0] e_next;
    logic [31:0] f_next;
    logic [31:0] g_next;
    logic [31:0] h_next;

    err_d          = '0;
    rdata_d        = '0;
    irq_enable_d   = irq_enable_q;
    busy_d         = busy_q;
    done_d         = done_q;
    digest_valid_d = digest_valid_q;
    irq_pending_d  = irq_pending_q;
    start_error_d  = start_error_q;
    state_valid_d  = state_valid_q;
    round_d        = round_q;

    a_d = a_q;
    b_d = b_q;
    c_d = c_q;
    d_d = d_q;
    e_d = e_q;
    f_d = f_q;
    g_d = g_q;
    h_d = h_q;

    for (int unsigned i = 0; i < 16; ++i) begin
      block_d[i] = block_q[i];
      w_mem_d[i] = w_mem_q[i];
    end
    for (int unsigned i = 0; i < 8; ++i) begin
      digest_d[i] = digest_q[i];
      hash_state_d[i] = hash_state_q[i];
    end

    start_cmd = 1'b0;
    chain_cmd = 1'b0;
    reg_word_idx = '0;
    w_t          = '0;
    t1           = '0;
    t2           = '0;
    a_next       = '0;
    b_next       = '0;
    c_next       = '0;
    d_next       = '0;
    e_next       = '0;
    f_next       = '0;
    g_next       = '0;
    h_next       = '0;

    if (obi_req_i.req) begin
      if (obi_req_i.a.we) begin
        unique case ({obi_req_i.a.addr[SHA256_ACCEL_INT_ADDR_WIDTH-1:2], 2'b00})
          SHA256_ACCEL_CTRL_OFFSET: begin
            irq_enable_d = (irq_enable_q & ~be_mask[1]) | (obi_req_i.a.wdata[1] & be_mask[1]);
            start_cmd    = obi_req_i.a.wdata[0] & be_mask[0];
            chain_cmd    = obi_req_i.a.wdata[2] & be_mask[2];
          end
          SHA256_ACCEL_STATUS_OFFSET: begin
            if (obi_req_i.a.wdata[2] & be_mask[2]) done_d = 1'b0;
            if (obi_req_i.a.wdata[3] & be_mask[3]) digest_valid_d = 1'b0;
            if (obi_req_i.a.wdata[4] & be_mask[4]) irq_pending_d = 1'b0;
            if (obi_req_i.a.wdata[5] & be_mask[5]) start_error_d = 1'b0;
          end
          default: begin
            if ((obi_req_i.a.addr[SHA256_ACCEL_INT_ADDR_WIDTH-1:2] >= SHA256_ACCEL_BLOCK0_OFFSET[SHA256_ACCEL_INT_ADDR_WIDTH-1:2]) &&
                (obi_req_i.a.addr[SHA256_ACCEL_INT_ADDR_WIDTH-1:2] <= SHA256_ACCEL_BLOCK15_OFFSET[SHA256_ACCEL_INT_ADDR_WIDTH-1:2])) begin
              reg_word_idx = obi_req_i.a.addr[5:2];
              block_d[reg_word_idx] = (block_q[reg_word_idx] & ~be_mask) | (obi_req_i.a.wdata & be_mask);
            end else begin
              err_d = 1'b1;
            end
          end
        endcase
      end else begin
        unique case ({obi_req_i.a.addr[SHA256_ACCEL_INT_ADDR_WIDTH-1:2], 2'b00})
          SHA256_ACCEL_CTRL_OFFSET: begin
            rdata_d = {29'h0, 1'b0, irq_enable_q, 1'b0};
          end
          SHA256_ACCEL_STATUS_OFFSET: begin
            rdata_d = {26'h0, start_error_q, irq_pending_q, digest_valid_q, done_q, busy_q, ~busy_q};
          end
          SHA256_ACCEL_INFO_OFFSET: begin
            rdata_d = SHA256_ACCEL_INFO;
          end
          default: begin
            if ((obi_req_i.a.addr[SHA256_ACCEL_INT_ADDR_WIDTH-1:2] >= SHA256_ACCEL_BLOCK0_OFFSET[SHA256_ACCEL_INT_ADDR_WIDTH-1:2]) &&
                (obi_req_i.a.addr[SHA256_ACCEL_INT_ADDR_WIDTH-1:2] <= SHA256_ACCEL_BLOCK15_OFFSET[SHA256_ACCEL_INT_ADDR_WIDTH-1:2])) begin
              reg_word_idx = obi_req_i.a.addr[5:2];
              rdata_d = block_q[reg_word_idx];
            end else if ((obi_req_i.a.addr[SHA256_ACCEL_INT_ADDR_WIDTH-1:2] >= SHA256_ACCEL_DIGEST0_OFFSET[SHA256_ACCEL_INT_ADDR_WIDTH-1:2]) &&
                         (obi_req_i.a.addr[SHA256_ACCEL_INT_ADDR_WIDTH-1:2] <= SHA256_ACCEL_DIGEST7_OFFSET[SHA256_ACCEL_INT_ADDR_WIDTH-1:2])) begin
              reg_word_idx = obi_req_i.a.addr[4:2];
              rdata_d = digest_q[reg_word_idx];
            end else begin
              rdata_d = 32'hBADCAB1E;
              err_d   = 1'b1;
            end
          end
        endcase
      end
    end

    if (busy_q) begin
      if (start_cmd) begin
        start_error_d = 1'b1;
      end

      if (round_q < 16) begin
        w_t = w_mem_q[round_q];
      end else begin
        reg_word_idx       = round_q[3:0];
        w_t                = small_sigma1(w_mem_q[(round_q-2)  & 6'h0f]) +
                             w_mem_q[(round_q-7)  & 6'h0f] +
                             small_sigma0(w_mem_q[(round_q-15) & 6'h0f]) +
                             w_mem_q[(round_q-16) & 6'h0f];
        w_mem_d[reg_word_idx] = w_t;
      end

      t1 = h_q + big_sigma1(e_q) + ((e_q & f_q) ^ ((~e_q) & g_q)) + sha256_k(round_q) + w_t;
      t2 = big_sigma0(a_q) + ((a_q & b_q) ^ (a_q & c_q) ^ (b_q & c_q));

      a_next = t1 + t2;
      b_next = a_q;
      c_next = b_q;
      d_next = c_q;
      e_next = d_q + t1;
      f_next = e_q;
      g_next = f_q;
      h_next = g_q;

      a_d = a_next;
      b_d = b_next;
      c_d = c_next;
      d_d = d_next;
      e_d = e_next;
      f_d = f_next;
      g_d = g_next;
      h_d = h_next;

      if (round_q == 6'd63) begin
        busy_d         = 1'b0;
        done_d         = 1'b1;
        digest_valid_d = 1'b1;
        irq_pending_d  = 1'b1;
        state_valid_d  = 1'b1;
        round_d        = '0;

        digest_d[0] = hash_state_q[0] + a_next;
        digest_d[1] = hash_state_q[1] + b_next;
        digest_d[2] = hash_state_q[2] + c_next;
        digest_d[3] = hash_state_q[3] + d_next;
        digest_d[4] = hash_state_q[4] + e_next;
        digest_d[5] = hash_state_q[5] + f_next;
        digest_d[6] = hash_state_q[6] + g_next;
        digest_d[7] = hash_state_q[7] + h_next;

        hash_state_d[0] = hash_state_q[0] + a_next;
        hash_state_d[1] = hash_state_q[1] + b_next;
        hash_state_d[2] = hash_state_q[2] + c_next;
        hash_state_d[3] = hash_state_q[3] + d_next;
        hash_state_d[4] = hash_state_q[4] + e_next;
        hash_state_d[5] = hash_state_q[5] + f_next;
        hash_state_d[6] = hash_state_q[6] + g_next;
        hash_state_d[7] = hash_state_q[7] + h_next;
      end else begin
        round_d = round_q + 1'b1;
      end
    end else if (start_cmd) begin
      if (chain_cmd && ~state_valid_q) begin
        start_error_d = 1'b1;
      end else begin
        busy_d         = 1'b1;
        done_d         = 1'b0;
        digest_valid_d = 1'b0;
        irq_pending_d  = 1'b0;
        start_error_d  = 1'b0;
        round_d        = '0;

        if (chain_cmd) begin
          a_d = hash_state_q[0];
          b_d = hash_state_q[1];
          c_d = hash_state_q[2];
          d_d = hash_state_q[3];
          e_d = hash_state_q[4];
          f_d = hash_state_q[5];
          g_d = hash_state_q[6];
          h_d = hash_state_q[7];
        end else begin
          state_valid_d   = 1'b1;
          hash_state_d[0] = SHA256_H_INIT_0;
          hash_state_d[1] = SHA256_H_INIT_1;
          hash_state_d[2] = SHA256_H_INIT_2;
          hash_state_d[3] = SHA256_H_INIT_3;
          hash_state_d[4] = SHA256_H_INIT_4;
          hash_state_d[5] = SHA256_H_INIT_5;
          hash_state_d[6] = SHA256_H_INIT_6;
          hash_state_d[7] = SHA256_H_INIT_7;

          a_d = SHA256_H_INIT_0;
          b_d = SHA256_H_INIT_1;
          c_d = SHA256_H_INIT_2;
          d_d = SHA256_H_INIT_3;
          e_d = SHA256_H_INIT_4;
          f_d = SHA256_H_INIT_5;
          g_d = SHA256_H_INIT_6;
          h_d = SHA256_H_INIT_7;
        end

        for (int unsigned i = 0; i < 16; ++i) begin
          w_mem_d[i] = block_q[i];
        end
        for (int unsigned i = 0; i < 8; ++i) begin
          digest_d[i] = '0;
        end
      end
    end
  end

  assign interrupt_o = irq_enable_q && irq_pending_q;

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (~rst_ni) begin
      irq_enable_q   <= '0;
      busy_q         <= '0;
      done_q         <= '0;
      digest_valid_q <= '0;
      irq_pending_q  <= '0;
      start_error_q  <= '0;
      state_valid_q  <= '0;
      round_q        <= '0;
      a_q            <= '0;
      b_q            <= '0;
      c_q            <= '0;
      d_q            <= '0;
      e_q            <= '0;
      f_q            <= '0;
      g_q            <= '0;
      h_q            <= '0;
      obi_req_q      <= '0;
      err_q          <= '0;
      rdata_q        <= '0;
      block_q      <= '{default: '0};
      w_mem_q      <= '{default: '0};
      digest_q     <= '{default: '0};
      hash_state_q <= '{default: '0};
    end else begin
      irq_enable_q   <= irq_enable_d;
      busy_q         <= busy_d;
      done_q         <= done_d;
      digest_valid_q <= digest_valid_d;
      irq_pending_q  <= irq_pending_d;
      start_error_q  <= start_error_d;
      state_valid_q  <= state_valid_d;
      round_q        <= round_d;
      a_q            <= a_d;
      b_q            <= b_d;
      c_q            <= c_d;
      d_q            <= d_d;
      e_q            <= e_d;
      f_q            <= f_d;
      g_q            <= g_d;
      h_q            <= h_d;
      obi_req_q      <= obi_req_d;
      err_q          <= err_d;
      rdata_q        <= rdata_d;
      block_q      <= block_d;
      w_mem_q      <= w_mem_d;
      digest_q     <= digest_d;
      hash_state_q <= hash_state_d;
    end
  end

endmodule
