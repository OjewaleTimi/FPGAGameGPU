`timescale 1ns / 1ps

module vga_controller(
    input  logic       clk_100MHz,
    input  logic       reset,
    output logic       hsync,
    output logic       vsync,
    output logic       p_tick,
    output logic       video_on,
    output logic [9:0] x,
    output logic [9:0] y
);
    // Standard 640x480 @ 60Hz timing constants
    localparam HD   = 640;             // Visible horizontal display width
    localparam HF   = 16;              // Horizontal Front Porch
    localparam HR   = 96;              // Horizontal Sync Pulse
    localparam HB   = 48;              // Horizontal Back Porch
    localparam HMAX = HD + HF + HR + HB - 1; // 799

    localparam VD   = 480;             // Visible vertical display height
    localparam VF   = 10;              // Vertical Front Porch
    localparam VR   = 2;               // Vertical Sync Pulse
    localparam VB   = 33;              // Vertical Back Porch
    localparam VMAX = VD + VF + VR + VB - 1; // 524

    // 100MHz / 4 = 25MHz pixel rate tick generator
    logic [1:0] pixel_cnt;
    always_ff @(posedge clk_100MHz or posedge reset) begin
        if (reset)
            pixel_cnt <= 2'b00;
        else
            pixel_cnt <= pixel_cnt + 1'b1;
    end
    assign p_tick = (pixel_cnt == 2'b11);

    // Screen coordinate sweep counters
    logic [9:0] h_count_reg, h_count_next;
    logic [9:0] v_count_reg, v_count_next;

    always_ff @(posedge clk_100MHz or posedge reset) begin
        if (reset) begin
            h_count_reg <= 10'd0;
            v_count_reg <= 10'd0;
        end else if (p_tick) begin
            h_count_reg <= h_count_next;
            v_count_reg <= v_count_next;
        end
    end

    always_comb begin
        h_count_next = h_count_reg;
        v_count_next = v_count_reg;

        if (h_count_reg == HMAX) begin
            h_count_next = 10'd0;
            if (v_count_reg == VMAX)
                v_count_next = 10'd0;
            else
                v_count_next = v_count_reg + 1'b1;
        end else begin
            h_count_next = h_count_reg + 1'b1;
        end
    end

    // Active-Low Sync Pulse Generation (Standard Negative Polarity)
    assign hsync = ~((h_count_reg >= (HD + HF)) && (h_count_reg < (HD + HF + HR)));
    assign vsync = ~((v_count_reg >= (VD + VF)) && (v_count_reg < (VD + VF + VR)));

    // Active video display area
    assign video_on = (h_count_reg < HD) && (v_count_reg < VD);
    assign x = h_count_reg;
    assign y = v_count_reg;

endmodule