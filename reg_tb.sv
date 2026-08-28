module reg_tb;

logic clk;
logic reset;
logic [9:0] x,y;
logic [9:0] w,h;
logic write_enable;
logic [31:0] write_data;
logic [1:0] shape_type;
logic [3:0] addr;
logic enable;
logic [6:0] filler;

register_system dut (
    .clk(clk),
    .addr(addr),
    .x(x),
    .y(y),
    .w(w),
    .write_data(write_data),
    .write_enable(write_enable),
    .enable(enable)
);

initial 
clk = 0;
always #5 clk = ~clk;


initial 
begin
reset = 1;
addr = 4'h0;
write_enable = 1'b0;
write_data = 32'h0;
filler = 6'h00;

#5 
reset = 0;

#10
addr = 4'h8;
write_data = {17'b0, 2'b01, 1'b1, 12'hFFF};
write_enable = 1;

#10
write_enable = 0;


#10
    if (color == 12'hFFF)
    $display("Successful");
    else
        $display("FAIL: w =%h, expected 0x32", w);
    $finish;
  end
endmodule