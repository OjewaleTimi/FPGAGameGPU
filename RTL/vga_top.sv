`timescale 1ns / 1ps
module vga_top #(
    parameter NUM_OBJECTS = 8
)(
    input  logic        clk_100MHz,
    input  logic        reset,

    // --- Object write bus (pre-AXI control interface, Phase 6) ---
    // write_addr[15:4] = which object slot (0 .. NUM_OBJECTS-1)
    // write_addr[3:0]  = which register within that slot (0x0/0x4/0x8/0xC)
    input  logic [15:0] write_addr,
    input  logic [31:0] write_data,
    input  logic        write_enable,

    // the background color logic
    input  logic [11:0] background_color,

    // --- Physical VGA output pins ---
    output logic        hsync,
    output logic        vsync,
    output logic [3:0]  vga_red,
    output logic [3:0]  vga_green,
    output logic [3:0]  vga_blue
);

    //================================================================
    // STAGE 1 -- VGA timing generator (existing module, unchanged)
    //
    // This is the "heartbeat" of the whole design. It free-runs off
    // clk_100MHz and continuously sweeps pixel_x/pixel_y across the
    // full 800x525 timing grid (640x480 visible + porches + sync).
    // Every other stage below reacts to whatever position it reports
    // -- nothing here waits for or synchronizes with software.
    //================================================================
    logic [9:0] pixel_x, pixel_y;
    logic       video_on;  // HIGH only when (pixel_x,pixel_y) is inside the visible 640x480 area
    logic       p_tick;    // internal 25MHz pixel-rate enable tick; not needed outside this module

    vga_controller vga_ctrl_inst (
        .clk_100MHz (clk_100MHz),
        .reset      (reset),
        .hsync      (hsync),
        .vsync      (vsync),
        .p_tick     (p_tick),
        .video_on   (video_on),
        .x          (pixel_x),
        .y          (pixel_y)
    );

    //================================================================
    // STAGE 2 -- Object array
    //
    // Instantiates NUM_OBJECTS parallel copies of (register_system +
    // object_generator). Every single one of them evaluates the SAME
    // (pixel_x, pixel_y) every cycle, independently deciding:
    //   - active[i]      : "is this pixel mine?"
    //   - pixel_color[i] : "if so, what color am I?"
    //
    // All address decoding (which slot a write targets) happens
    // INSIDE object_array -- this top level just passes the raw bus
    // through untouched.
    //================================================================
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

    //================================================================
    // STAGE 3 -- Compositor
    //
    // object_array can legally report MULTIPLE objects as "active"
    // for the same pixel (overlapping shapes). The compositor picks
    // exactly one winner -- see the PRIORITY / Z-ORDER NOTE at the
    // top of this file for exactly which slot wins on overlap.
    // If no object is active, it falls back to whatever this top
    // level's background_color input is currently driving.
    //================================================================
    logic [11:0] composited_color;

    compositor #(
        .NUM_OBJECTS (NUM_OBJECTS)
    ) compositor_inst (
        .active           (active),
        .pixel_color      (pixel_color),
        .background_color (background_color),
        .final_color      (composited_color)
    );

    //================================================================
    // STAGE 4 -- Blanking mux + physical pin mapping
    //
    // WHY THIS STAGE EXISTS: vga_controller's pixel_x/pixel_y sweep
    // all the way up to HMAX/VMAX (799/524), not just the visible
    // 0..639 / 0..479 range -- the extra counts are the horizontal
    // and vertical porches + sync pulses, which are NOT part of the
    // visible image. video_on is the signal that tells us whether we
    // are currently inside the visible area or not.
    //
    // Nothing in object_array or compositor guards against this on
    // its own, so we force the output to black here whenever
    // video_on is low, regardless of what the compositor produced.
    // Skipping this step risks driving unintended color data during
    // the sync timing region, which can look like a corrupted or
    // unstable image on some monitors.
    //
    // COLOR FORMAT: 12-bit RGB444 is split evenly across the Basys
    // 3's three 4-bit VGA DAC channels:
    //   composited_color[11:8] -> Red
    //   composited_color[7:4]  -> Green
    //   composited_color[3:0]  -> Blue
    //================================================================
    logic [11:0] rgb_out;
    assign rgb_out = video_on ? composited_color : 12'h000;

    assign vga_red   = rgb_out[11:8];
    assign vga_green = rgb_out[7:4];
    assign vga_blue  = rgb_out[3:0];

endmodule
