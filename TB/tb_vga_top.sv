`timescale 1ns / 1ps

module tb_vga_top;

    //----------------------------------------------------------------
    // DUT connections
    //----------------------------------------------------------------
    localparam NUM_OBJECTS = 8;

    logic        clk_100MHz;
    logic        reset;
    logic [15:0] write_addr;
    logic [31:0] write_data;
    logic        write_enable;
    logic [11:0] background_color;

    logic        hsync, vsync;
    logic [3:0]  vga_red, vga_green, vga_blue;

    // Known reference colors, RGB444, matching the top level's
    // [11:8]=R [7:4]=G [3:0]=B convention
    localparam [11:0] BG    = 12'h000;
    localparam [11:0] RED   = 12'hF00;
    localparam [11:0] GREEN = 12'h0F0;
    localparam [11:0] BLUE  = 12'h00F;

    int pass_count = 0;
    int fail_count = 0;

    vga_top #(
        .NUM_OBJECTS (NUM_OBJECTS)
    ) dut (
        .clk_100MHz       (clk_100MHz),
        .reset            (reset),
        .write_addr       (write_addr),
        .write_data       (write_data),
        .write_enable     (write_enable),
        .background_color (background_color),
        .hsync            (hsync),
        .vsync            (vsync),
        .vga_red          (vga_red),
        .vga_green        (vga_green),
        .vga_blue         (vga_blue)
    );

    //----------------------------------------------------------------
    // Clock generation: 100MHz -> 10ns period -> toggle every 5ns
    //----------------------------------------------------------------
    initial clk_100MHz = 1'b0;
    always #5 clk_100MHz = ~clk_100MHz;

    //----------------------------------------------------------------
    // Task: write_reg
    // Performs one synchronous register write, matching exactly how
    // register_system expects to see write_enable pulse for one
    // clock cycle. addr_in is a full 16-bit address: object slot
    // index * 16, plus the field offset (0x0/0x4/0x8/0xC) within
    // that slot -- see object_array's addr[15:4]/addr[3:0] split.
    //----------------------------------------------------------------
    task automatic write_reg(input logic [15:0] addr_in, input logic [31:0] data_in);
        @(posedge clk_100MHz);
        write_addr   <= addr_in;
        write_data   <= data_in;
        write_enable <= 1'b1;
        @(posedge clk_100MHz);
        write_enable <= 1'b0;
    endtask

    //----------------------------------------------------------------
    // Task: wait_and_check
    // Blocks until the VGA scan's CURRENT position (read directly off
    // the DUT's internal pixel_x/pixel_y wires via hierarchical
    // reference -- valid in simulation, not something you'd do in
    // real RTL) reaches the requested (target_x, target_y). Once
    // there, samples the actual RGB output and compares it against
    // expected_color.
    //
    // IMPORTANT: checkpoints must be called in the same order the
    // raster scan will actually encounter them (increasing y, and
    // increasing x within a row) -- otherwise this task will block
    // until the NEXT frame wraps around to that position instead of
    // failing outright, which just wastes simulation time rather
    // than indicating a real bug.
    //----------------------------------------------------------------
    task automatic wait_and_check(
        input int target_x,
        input int target_y,
        input logic [11:0] expected_color,
        input string label
    );
        logic [11:0] actual_color;

        while (!(dut.pixel_x == target_x && dut.pixel_y == target_y)) begin
            @(posedge clk_100MHz);
        end

        #1; // allow combinational logic (compositor + blanking mux) to settle
        actual_color = {vga_red, vga_green, vga_blue};

        if (actual_color === expected_color) begin
            $display("[PASS] %-42s (x=%0d,y=%0d) expected=%h actual=%h",
                      label, target_x, target_y, expected_color, actual_color);
            pass_count++;
        end else begin
            $display("[FAIL] %-42s (x=%0d,y=%0d) expected=%h actual=%h",
                      label, target_x, target_y, expected_color, actual_color);
            fail_count++;
        end
    endtask

    //----------------------------------------------------------------
    // Main test sequence
    //----------------------------------------------------------------
    initial begin
        $dumpfile("tb_vga_top.vcd");
        $dumpvars(0, tb_vga_top);

        //--------------------------------------------------------------
        // 1) Reset
        //--------------------------------------------------------------
        reset            = 1'b1;
        write_addr       = 16'b0;
        write_data       = 32'b0;
        write_enable     = 1'b0;
        background_color = BG;

        repeat (5) @(posedge clk_100MHz);
        reset = 1'b0;
        repeat (5) @(posedge clk_100MHz);

        //--------------------------------------------------------------
        // 2) Configure objects
        //    Address = (slot_index * 16) + field_offset
        //    field_offset: 0x0=x  0x4=y  0x8=w/h  0xC=color/enable/shape
        //--------------------------------------------------------------
        $display("---- Configuring objects ----");

        // Object 0: red rectangle, x=100,y=100,w=50,h=50
        write_reg(0*16 + 'h0, 32'd100);              // x
        write_reg(0*16 + 'h4, 32'd100);              // y
        write_reg(0*16 + 'h8, {16'd50, 16'd50});     // w,h
        write_reg(0*16 + 'hC, {17'b0, 2'b00, 1'b1, RED}); // shape=rect, enable=1, color=RED

        // Object 1: green circle, center=(400,300), radius=30
        write_reg(1*16 + 'h0, 32'd400);
        write_reg(1*16 + 'h4, 32'd300);
        write_reg(1*16 + 'h8, {16'd30, 16'd0});      // w=radius, h unused
        write_reg(1*16 + 'hC, {17'b0, 2'b01, 1'b1, GREEN}); // shape=circle

        // Object 2: blue rectangle, x=600,y=200,w=100,h=50
        // (deliberately crosses the x=639 visible/blanking boundary)
        write_reg(2*16 + 'h0, 32'd600);
        write_reg(2*16 + 'h4, 32'd200);
        write_reg(2*16 + 'h8, {16'd100, 16'd50});
        write_reg(2*16 + 'hC, {17'b0, 2'b00, 1'b1, BLUE});

        $display("---- Objects configured, scanning for checkpoints ----");

        //--------------------------------------------------------------
        // 3) Checkpoints, in raster-scan order (increasing y, then
        //    increasing x within each row)
        //--------------------------------------------------------------

        // -- Rectangle 0 boundary tests, row y=100 (top edge) --
        wait_and_check(99,  100, BG,    "Rect0_row1_left_of_left_edge");
        wait_and_check(100, 100, RED,   "Rect0_row1_top_left_corner");
        wait_and_check(125, 100, RED,   "Rect0_row1_top_edge_inside");
        wait_and_check(150, 100, BG,    "Rect0_row1_right_of_right_edge");

        // -- Rectangle 0 boundary tests, row y=125 (mid-height) --
        wait_and_check(99,  125, BG,    "Rect0_row2_left_of_left_edge");
        wait_and_check(100, 125, RED,   "Rect0_row2_left_edge_inside");
        wait_and_check(125, 125, RED,   "Rect0_row2_center");
        wait_and_check(149, 125, RED,   "Rect0_row2_rightmost_inside_pixel");
        wait_and_check(150, 125, BG,    "Rect0_row2_just_outside_right_edge");

        // -- Rectangle 0: just below the bottom edge --
        wait_and_check(125, 150, BG,    "Rect0_row3_below_bottom_edge");

        // -- Object 2 blanking-mux test, row y=225 --
        wait_and_check(599, 225, BG,    "Obj2_row_left_of_left_edge");
        wait_and_check(620, 225, BLUE,  "Obj2_row_visible_portion");
        wait_and_check(650, 225, BG,    "Obj2_row_hblank_forced_black");

        // -- Circle 1 boundary tests, row y=300 (through the center) --
        wait_and_check(369, 300, BG,    "Circle1_just_outside_left_edge");
        wait_and_check(370, 300, GREEN, "Circle1_left_edge_boundary_inclusive");
        wait_and_check(400, 300, GREEN, "Circle1_center");
        wait_and_check(430, 300, GREEN, "Circle1_right_edge_boundary_inclusive");
        wait_and_check(431, 300, BG,    "Circle1_just_outside_right_edge");

        //--------------------------------------------------------------
        // 4) Summary
        //--------------------------------------------------------------
        $display("---- tb_vga_top finished ----");
        $display("PASS: %0d   FAIL: %0d", pass_count, fail_count);
        if (fail_count == 0)
            $display(">>> ALL TESTS PASSED <<<");
        else
            $display(">>> %0d TEST(S) FAILED -- check the pipeline stage matching the failing label <<<", fail_count);

        $finish;
    end

endmodule
