`timescale 1ns / 1ps

module vga_controller(
    input logic clk_100MHz,
    input logic reset,
    output logic hsync,
    output logic vsync,
    output logic p_tick,
    output logic video_on,
    output logic [9:0] x,
    output logic [9:0] y
);
    parameter HD = 640;
    parameter HF = 48;
    parameter HB = 16;
    parameter HR = 96;
    parameter HMAX = HD+HF+HB+HR -1;

    parameter VD = 480;
    parameter VF = 10;
    parameter VB = 33;
    parameter VR = 2;
    parameter VMAX = VD+VF+VB+VR-1;

 
    logic [1:0] r_25MHz;
    logic clk_25MHz;

    always_ff @(posedge clk_100MHz or posedge reset) begin
        if (reset) begin
            r_25MHz <= 0;
    end
    else begin
        r_25MHz <= r_25MHz + 1'b1;
        
    end
    end
    assign clk_25MHz = (r_25MHz == 0 ) ? 1 : 0 ;
    logic hsync_reg, vsync_reg, hsync_reg_next, vsync_reg_next;
    logic [9:0] hreg , vreg, hreg_next, vreg_next;

    always_ff@(posedge clk_100MHz or posedge reset) begin
        if (reset) begin
        hsync_reg <= 1'b0;
        vsync_reg <= 1'b0;
        hreg <= 10'b0;
        vreg <= 10'b0;
        end
        else begin
        hsync_reg <= hsync_reg_next;
        vsync_reg <= vsync_reg_next;
        hreg <= hreg_next;
        vreg <= vreg_next;
        end
    end
    

    always_comb begin
        hreg_next = hreg;
        vreg_next = vreg;
    if (clk_25MHz) begin
        if (hreg == HMAX) begin
            hreg_next = 10'b0;
            if (vreg == VMAX)
                vreg_next = 10'b0;
            else
                vreg_next = vreg + 1'b1;
        end
        else begin
            hreg_next = hreg + 1'b1;
        end
    end
    end


    
    

   //assign hsync_reg = (hreg => (HD + HB) && hreg <= (HD + HB + HR - 1));
  // assign vsync_reg = (vreg => (VD + VB) && vreg <= (VD + VB + VR - 1));
   assign video_on = (hreg >= 0 && hreg <= HD - 1) && (vreg >= 0 && vreg <= VD - 1 );
   
   assign p_tick = clk_25MHz;
   assign x = hreg;
   assign y = vreg;
   assign hsync_reg_next = (hreg >= (HD + HB) && hreg <= (HD + HB + HR - 1));
   assign vsync_reg_next = (vreg >= (VD + VB) && vreg <= (VD + VB + VR - 1));
   assign hsync = hsync_reg;
   assign vsync = vsync_reg;
   endmodule