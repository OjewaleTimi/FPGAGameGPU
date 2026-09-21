module reg_tb;

logic clk;
logic reset;
logic [9:0] x, y;
logic [9:0] w, h;
logic write_enable;
logic [31:0] write_data;
logic [1:0] shape_type;
logic [3:0] addr;
logic enable;
logic [11:0] color;

register_system dut (
    .clk(clk),
    .reset(reset),
    .addr(addr),
    .x(x),
    .y(y),
    .w(w),
    .h(h),
    .write_data(write_data),
    .write_enable(write_enable),
    .enable(enable),
    .color(color),
    .shape_type(shape_type)
);

// Clock generation
initial clk = 0;
always #5 clk = ~clk;

initial begin
    // Initialize Inputs
    reset = 1;
    addr = 4'h0;
    write_enable = 1'b0;
    write_data = 32'h0;

    // Wait for the first negative edge, then drop reset
    @(negedge clk);
    reset = 0;

    // Apply data on the negative edge
    @(negedge clk);
    addr = 4'hC;  
    write_data = {17'b0, 2'b01, 1'b1, 12'hFFF};
    write_enable = 1;

    // Wait one clock cycle, then turn off write enable
    @(negedge clk);
    write_enable = 0;

    // Wait one more cycle for the data to propagate out of the DUT
    @(negedge clk);
    
    // Check the result
    if (color == 12'hFFF)
        $display("Successful");
    else
        $display("FAIL: color = %h, expected FFF", color);
        
    $finish;
end

endmodule