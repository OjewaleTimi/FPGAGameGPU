`timescale 1ns / 1ps
//
// font_rom.sv
// 8x16 ASCII bitmap font ROM, ASCII 0x20 (space) .. 0x7E ('~') = 95 glyphs.
// Data is loaded from font8x16.mem (16 bytes per glyph, MSB = leftmost pixel).
// font8x16.mem was generated procedurally (rasterized from a system font),
// NOT copied from an existing font ROM file.
//
// Any char_code outside the printable range is clamped to space (blank glyph),
// so out-of-range reads are always safe.
//
module font_rom (
    input  logic [7:0] char_code,
    input  logic [3:0] row_in_char,   // 0..15, which scanline within the glyph
    output logic [7:0] glyph_row      // 8 pixels, bit7 = leftmost
);

    localparam FIRST_CHAR  = 8'h20;
    localparam LAST_CHAR   = 8'h7E;
    localparam NUM_GLYPHS  = LAST_CHAR - FIRST_CHAR + 1; // 95

    logic [7:0] rom [0 : NUM_GLYPHS*16 - 1];

    initial begin
        $readmemh("font8x16.mem", rom);
    end

    logic [7:0]  safe_code;
    logic [10:0] rom_addr;

    assign safe_code = (char_code >= FIRST_CHAR && char_code <= LAST_CHAR)
                        ? char_code : FIRST_CHAR; // default to space
    assign rom_addr  = (safe_code - FIRST_CHAR) * 16 + row_in_char;
    assign glyph_row = rom[rom_addr];

endmodule