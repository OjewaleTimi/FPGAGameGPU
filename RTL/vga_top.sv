`timescale 1ns / 1ps
//
// vga_top.sv  (updated for BRAM-backed text overlay)
//
// text_overlay's outputs now lag pixel_x/pixel_y by exactly 1 clock
// cycle (the font ROM is a real BRAM with 1 cycle of read latency --
// see font_rom.sv / text_overlay.sv). To keep text aligned with shapes
// and background, this module now delays hsync/vsync/video_on and the
// compositor's output by that same 1 cycle before the final mux. This
// shifts the whole frame later by one 100MHz clock (10ns) -- far below
// one pixel period (40ns at the 25MHz effective pixel rate) and totally
// imperceptible, since hsync/vsync/color all move together.
//
module vga_top #(
    parameter NUM_OBJECTS = 25
)(
    input  logic        clk_100MHz,
    input  logic        reset,

    input  logic [15:0] write_addr,
    input  logic [31:0] write_data,
    input  logic        write_enable,
    input  logic [11:0] background_color,

    output logic        hsync,
    output logic        vsync,
    output logic [3:0]  vga_red,
    output logic [3:0]  vga_green,
    output logic [3:0]  vga_blue
);

    logic [9:0] pixel_x, pixel_y;
    logic       video_on;
    logic       p_tick;
    logic       hsync_raw, vsync_raw;

    vga_controller vga_ctrl_inst (
        .clk_100MHz (clk_100MHz),
        .reset      (reset),
        .hsync      (hsync_raw),
        .vsync      (vsync_raw),
        .p_tick     (p_tick),
        .video_on   (video_on),
        .x          (pixel_x),
        .y          (pixel_y)
    );

    logic [NUM_OBJECTS-1:0] active;
    logic [11:0]            pixel_color [NUM_OBJECTS-1:0];

    object_array #(
        .NUM_OBJECTS (NUM_OBJECTS)
    ) obj_array_inst (
        .clk          (clk_100MHz),
        .reset        (reset),
        .pixel_x      (pixel_x),
        .pixel_y      (pixel_y),
        .write_data   (write_data),
        .addr         (write_addr),
        .write_enable (write_enable),
        .active       (active),
        .pixel_color  (pixel_color)
    );

    logic [11:0] composited_color;

    compositor #(
        .NUM_OBJECTS (NUM_OBJECTS)
    ) compositor_inst (
        .active           (active),
        .pixel_color      (pixel_color),
        .background_color (background_color),
        .final_color      (composited_color)
    );

    // ---- Text overlay: BRAM font, 1 cycle of latency ----
    logic        text_active;
    logic [11:0] text_color;

    text_overlay #(
        .TEXT_COLS (80),
        .TEXT_ROWS (30)
    ) text_overlay_inst (
        .clk          (clk_100MHz),
        .reset        (reset),
        .pixel_x      (pixel_x),
        .pixel_y      (pixel_y),
        .addr         (write_addr),
        .write_data   (write_data),
        .write_enable (write_enable),
        .text_active  (text_active),
        .text_color   (text_color)
    );

    // ---- 1-cycle pipeline register: realign shapes/background/sync
    //      with the BRAM-delayed text_active/text_color ----
    logic        video_on_d1, hsync_d1, vsync_d1;
    logic [11:0] composited_color_d1;

    always_ff @(posedge clk_100MHz) begin
        video_on_d1          <= video_on;
        hsync_d1             <= hsync_raw;
        vsync_d1             <= vsync_raw;
        composited_color_d1  <= composited_color;
    end

    // Text draws on top of everything else
    logic [11:0] layered_color;
    assign layered_color = text_active ? text_color : composited_color_d1;

    logic [11:0] rgb_out;
    assign rgb_out = video_on_d1 ? layered_color : 12'h000;

    assign hsync     = hsync_d1;
    assign vsync     = vsync_d1;
    assign vga_red   = rgb_out[11:8];
    assign vga_green = rgb_out[7:4];
    assign vga_blue  = rgb_out[3:0];

endmodule