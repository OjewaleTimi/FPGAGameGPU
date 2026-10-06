`timescale 1ns / 1ps
//
// text_overlay.sv
//
// Text plane for teenytinygpu: 8x16 font, 80 columns x 30 rows
// (80*8 = 640, 30*16 = 480 -> exactly covers the VGA frame).
//
// Each cell is one 32-bit word, written over the same bus object_array
// uses:
//   [7:0]   ASCII/CP437 character code (full byte range is valid --
//           the font ROM has a glyph for all 256 codes)
//   [19:8]  foreground color (12-bit RGB444)
//   [31:20] reserved (write 0)
//
// Memory map: bit 15 of the address selects the text region, exactly as
// before.
//   TEXT_BASE = 0x8000
//   cell(row,col) byte address = TEXT_BASE + (row*80 + col)*4
//   row = 0..29, col = 0..79
//
// PIPELINE / LATENCY:
// The character/color cell buffer (cell_mem) is still small enough to
// stay as inferred distributed RAM with a combinational (same-cycle)
// read. The font ROM is now a real Block Memory Generator BRAM
// (font_rom.sv) with 1 cycle of read latency: glyph_row is valid one
// cycle after char_code/row_in_char are presented to it.
//
// To keep the glyph bit and the color it belongs to in lockstep, this
// module registers col_in_char and fg_color by exactly 1 cycle so they
// arrive at the output mux at the same time glyph_row does. The result:
// text_active/text_color are valid ONE CYCLE AFTER pixel_x/pixel_y
// change, not combinationally. vga_top.sv compensates by delaying the
// non-text video path (compositor output, hsync, vsync, video_on) by
// the same 1 cycle, so the whole frame is just shifted 10ns later at
// 100MHz -- imperceptible, and hsync/vsync/color stay aligned with each
// other because they're delayed together.
//
module text_overlay #(
    parameter TEXT_COLS = 80,
    parameter TEXT_ROWS = 30,
    parameter CHAR_W    = 8,
    parameter CHAR_H    = 16
)(
    input  logic        clk,
    input  logic         reset,
    input  logic [9:0]  pixel_x, pixel_y,

    // Same bus shape as object_array's write port
    input  logic [15:0] addr,
    input  logic [31:0] write_data,
    input  logic        write_enable,

    output logic        text_active,   // valid 1 cycle after pixel_x/pixel_y
    output logic [11:0] text_color     // valid 1 cycle after pixel_x/pixel_y
);

    localparam NUM_CELLS   = TEXT_COLS * TEXT_ROWS;       // 2400
    localparam CELL_ADDR_W = $clog2(NUM_CELLS);           // 12 bits

    // ---- Cell memory (char code + fg color per cell) ----
    // Still distributed RAM: small, async read, no latency of its own.
    logic [31:0] cell_mem [0 : NUM_CELLS-1];

    wire                    text_region_sel = addr[15];
    wire [CELL_ADDR_W-1:0]  windex          = addr[CELL_ADDR_W+1:2];

    always_ff @(posedge clk) begin
        if (write_enable && text_region_sel) begin
            cell_mem[windex] <= write_data;
        end
    end

    // ---- Which cell is the current pixel in? (combinational) ----
    wire [6:0] col = pixel_x[9:3];   // pixel_x / 8   -> 0..79
    wire [4:0] row = pixel_y[8:4];   // pixel_y / 16  -> 0..29

    logic [CELL_ADDR_W-1:0] rindex;
    assign rindex = row * TEXT_COLS[CELL_ADDR_W-1:0] + col;

    logic [31:0] cell_data;
    assign cell_data = cell_mem[rindex];

    wire [7:0]  char_code = cell_data[7:0];
    wire [11:0] fg_color  = cell_data[19:8];

    // ---- Font ROM lookup (1 cycle of BRAM latency) ----
    wire [3:0] row_in_char = pixel_y[3:0]; // 0..15
    wire [2:0] col_in_char = pixel_x[2:0]; // 0..7

    logic [7:0] glyph_row; // valid 1 cycle after char_code/row_in_char
    logic [10:0] row_addr = {char_code, row_in_char};
    font_rom font_inst (
        .clka         (clk),
        .addra   (row_addr),
        .douta   (glyph_row)
    );

    // Delay col_in_char and fg_color by 1 cycle to meet glyph_row when
    // it arrives, so the right color lands on the right glyph bit.
    logic [2:0]  col_in_char_d1;
    logic [11:0] fg_color_d1;

    always_ff @(posedge clk) begin
        col_in_char_d1 <= col_in_char;
        fg_color_d1    <= fg_color;
    end

    wire glyph_bit = glyph_row[7 - col_in_char_d1];

    assign text_active = glyph_bit;
    assign text_color  = fg_color_d1;

endmodule