module comp_tb;
   logic [7:0] active;
   logic [11:0] pixel_color [7:0];
   logic [11:0] background_color;
   logic [11:0] final_color;

   compositor comp1 (
    .active(active),
    .pixel_color(pixel_color),
    .background_color(background_color),
    .final_color(final_color)
   );


   initial begin
    #5 
    active = 8'b0000000;
    pixel_color[5] = 12'h000;
    background_color = 12'h000;
    
    #10 
    active = 8'b01101010;
    pixel_color[1] = 12'h435;
    background_color = 12'h200;

    #10
    if(final_color == 12'h435) 
     $display("Successful");
     else 
    begin
        $display("Failed, this is it %d and %d", final_color, pixel_color[1]);
        $finish;
   end
end
endmodule