// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//

module user_rom #(
  parameter type obi_req_t = logic,
  parameter type obi_rsp_t = logic
) (
  input  logic     clk_i,
  input  logic     rst_ni,
  input  obi_req_t obi_req_i,
  output obi_rsp_t obi_rsp_o
);

  localparam int unsigned USER_ROM_WORD_ADDR_WIDTH = 10;

  obi_req_t      obi_req_d, obi_req_q;
  logic              err_d,     err_q;
  logic     [31:0] rdata_d,   rdata_q;

  function automatic logic [31:0] get_rom_word(input logic [USER_ROM_WORD_ADDR_WIDTH-1:0] word_idx);
    // Words are little-endian so software can print the ROM contents as a C string.
    unique case (word_idx)
      10'd0:  return 32'h7564_6152; // "Radu"
      10'd1:  return 32'h6E6F_432D; // "-Con"
      10'd2:  return 32'h6E61_7473; // "stan"
      10'd3:  return 32'h206E_6974; // "tin "
      10'd4:  return 32'h7465_7243; // "Cret"
      10'd5:  return 32'h4120_2C75; // "u, A"
      10'd6:  return 32'h6178_656C; // "lexa"
      10'd7:  return 32'h7265_646E; // "nder"
      10'd8:  return 32'h7775_4820; // " Huw"
      10'd9:  return 32'h7265_6C69; // "iler"
      10'd10: return 32'h0000_0000; // zero terminator
      default: return 32'h0000_0000;
    endcase
  endfunction

  assign obi_req_d = obi_req_i;

  always_comb begin : obi_response
    obi_rsp_o         = '0;
    obi_rsp_o.gnt     = 1'b1;
    obi_rsp_o.rvalid  = obi_req_q.req;
    obi_rsp_o.r.err   = err_q;
    obi_rsp_o.r.rid   = obi_req_q.a.aid;
    obi_rsp_o.r.rdata = rdata_q;
  end

  always_comb begin
    err_d   = 1'b0;
    rdata_d = '0;

    if (obi_req_i.req) begin
      if (obi_req_i.a.we) begin
        err_d   = 1'b1;
        rdata_d = 32'hBADCAB1E;
      end else begin
        rdata_d = get_rom_word(obi_req_i.a.addr[11:2]);
      end
    end
  end

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (~rst_ni) begin
      obi_req_q <= '0;
      err_q     <= '0;
      rdata_q   <= '0;
    end else begin
      obi_req_q <= obi_req_d;
      err_q     <= err_d;
      rdata_q   <= rdata_d;
    end
  end

endmodule
