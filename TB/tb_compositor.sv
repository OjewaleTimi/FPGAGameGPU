`timescale 1ns / 1ps

module tb_compositor;

    localparam NUM_OBJECTS = 8;

    // DUT inputs
    logic [NUM_OBJECTS-1:0] active;
    logic [11:0]            pixel_color [NUM_OBJECTS-1:0];
    logic [11:0]            background_color;

    // DUT output
    logic [11:0] final_color;

    // Track pass/fail counts so we get a summary at the end
    int pass_count = 0;
    int fail_count = 0;

    //----------------------------------------------------------------
    // Instantiate the DUT (device under test)
    //----------------------------------------------------------------
    compositor #(
        .NUM_OBJECTS (NUM_OBJECTS)
    ) dut (
        .active           (active),
        .pixel_color      (pixel_color),
        .background_color (background_color),
        .final_color      (final_color)
    );

    //----------------------------------------------------------------
    // Helper task: apply a scenario, wait for combinational logic to
    // settle, then compare final_color against what we expect.
    //----------------------------------------------------------------
    task automatic check_result(input string test_name, input logic [11:0] expected);
        #1; // small delay lets always_comb settle before we read final_color
        if (final_color === expected) begin
            $display("[PASS] %-40s expected=%h actual=%h", test_name, expected, final_color);
            pass_count++;
        end else begin
            $display("[FAIL] %-40s expected=%h actual=%h", test_name, expected, final_color);
            fail_count++;
        end
    endtask

    //----------------------------------------------------------------
    // Helper task: clear every object to inactive with distinct
    // "canary" colors, so if the wrong slot ever wins by mistake,
    // it's immediately obvious from the printed color which slot
    // leaked through.
    //----------------------------------------------------------------
    task automatic reset_objects();
        int k;
        for (k = 0; k < NUM_OBJECTS; k = k + 1) begin
            active[k]      = 1'b0;
            pixel_color[k] = 12'h100 * k;  // slot 0=0x000, slot1=0x100, slot2=0x200, etc.
        end
    endtask

    //----------------------------------------------------------------
    // Test sequence
    //----------------------------------------------------------------
    initial begin
        // ------------------------------------------------------------
        // VCD dump setup -- must happen BEFORE any signal changes, so
        // put it as the very first thing in the testbench.
        //   $dumpfile : names the output file GTKWave will open
        //   $dumpvars(0, tb_compositor) : dumps EVERY signal inside
        //     this testbench module and everything below it in the
        //     hierarchy (including the compositor DUT itself), since
        //     the first argument (0) means "unlimited depth".
        // ------------------------------------------------------------
        $dumpfile("tb_compositor.vcd");
        $dumpvars(0, tb_compositor);

        $display("---- tb_compositor starting ----");

        background_color = 12'hFFF; // white, easy to distinguish from any slot color
        reset_objects();

        // Test 1: nothing active -> background should pass through
        check_result("Test1_all_inactive_shows_background", 12'hFFF);

        // Test 2: only slot 0 active
        reset_objects();
        active[0] = 1'b1;
        check_result("Test2_only_slot0_active", pixel_color[0]);

        // Test 3: only slot 7 active
        reset_objects();
        active[7] = 1'b1;
        check_result("Test3_only_slot7_active", pixel_color[7]);

        // Test 4: slot 0 AND slot 7 active -> highest index (7) must win
        reset_objects();
        active[0] = 1'b1;
        active[7] = 1'b1;
        check_result("Test4_slot0_and_slot7_overlap_slot7_wins", pixel_color[7]);

        // Test 5: ALL slots active -> slot 7 (highest) must still win
        reset_objects();
        active = {NUM_OBJECTS{1'b1}};
        check_result("Test5_all_slots_active_slot7_wins", pixel_color[7]);

        // Test 6: middle slots overlap (2 and 4) -> slot 4 must win,
        // proving the priority logic isn't just "first or last bit",
        // but genuinely "highest active index amongst whichever are set"
        reset_objects();
        active[2] = 1'b1;
        active[4] = 1'b1;
        check_result("Test6_slot2_and_slot4_overlap_slot4_wins", pixel_color[4]);

        //--------------------------------------------------------------
        // Summary
        //--------------------------------------------------------------
        $display("---- tb_compositor finished ----");
        $display("PASS: %0d   FAIL: %0d", pass_count, fail_count);
        if (fail_count == 0)
            $display(">>> ALL TESTS PASSED <<<");
        else
            $display(">>> %0d TEST(S) FAILED -- check compositor.sv <<<", fail_count);

        $finish;
    end

endmodule
