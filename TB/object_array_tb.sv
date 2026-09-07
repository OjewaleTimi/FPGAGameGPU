`timescale 1ns / 1ps

module object_array_tb;

    localparam NUM_OBJECTS = 8;

    logic clk;
    logic reset;
    logic [9:0] pixel_x, pixel_y;
    logic [31:0] write_data;
    logic [15:0] addr;
    logic write_enable;

    logic [NUM_OBJECTS-1:0] active;
    logic [11:0] pixel_color [NUM_OBJECTS-1:0];
    logic [10*NUM_OBJECTS-1:0] x_debug_flat;

    object_array #(.NUM_OBJECTS(NUM_OBJECTS)) dut (
        .clk(clk),
        .reset(reset),
        .pixel_x(pixel_x),
        .pixel_y(pixel_y),
        .write_data(write_data),
        .addr(addr),
        .write_enable(write_enable),
        .active(active),
        .pixel_color(pixel_color),
        .x_debug_flat(x_debug_flat)
    );

    initial clk = 0;
    always #5 clk = ~clk;

    int k;
    logic pass;
    logic [9:0] xk;

    initial begin
        // 1. Reset
        reset = 1;
        write_enable = 0;
        addr = 16'h0;
        write_data = 32'h0;
        pixel_x = 0; pixel_y = 0;
        pass = 1;

        #10;
        reset = 0;

        // 2. Write ONLY to object 2's x register (local offset 0x0)
        //    obj_index=2 -> addr[15:4]=2 -> addr = 2 << 4 = 0x020
        #10;
        addr = 16'h020;
        write_data = 32'd77;
        write_enable = 1;

        #10;
        write_enable = 0;

        // 3. Let the write settle, then check ALL 8 objects
        #10;
        for (k = 0; k < NUM_OBJECTS; k = k + 1) begin
            xk = x_debug_flat[k*10 +: 10];
            if (k == 2) begin
                if (xk !== 10'd77) begin
                    $display("FAIL: object %0d x = %0d, expected 77", k, xk);
                    pass = 0;
                end else begin
                    $display("PASS: object %0d x correctly set to 77", k);
                end
            end else begin
                if (xk !== 10'd0) begin
                    $display("FAIL: object %0d x = %0d, expected 0 (write leaked!)", k, xk);
                    pass = 0;
                end else begin
                    $display("PASS: object %0d x correctly stayed at 0", k);
                end
            end
        end

        #10;
        if (pass)
            $display(">>> OVERALL: PASS - write isolation confirmed <<<");
        else
            $display(">>> OVERALL: FAIL - write leaked to another object <<<");

        $finish;
    end

endmodule