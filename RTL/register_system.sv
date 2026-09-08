module register_system (
    input  logic clk,
    input  logic reset,
    input  logic [3:0] addr,
    input  logic write_enable,
    input  logic [31:0] write_data,

    output logic [9:0] x, y,
    output logic [9:0] w, h,
    output logic [11:0] color,
    output logic [1:0] shape_type,
    output logic enable
);

    always_ff @(posedge clk or posedge reset) begin
        if (reset) begin
            x          <= 10'b0;
            y          <= 10'b0;
            w          <= 10'b0;
            h          <= 10'b0;
            color      <= 12'b0;
            shape_type <= 2'b0;
            enable     <= 1'b0;
        end
        else if (write_enable) begin
            case (addr)
                4'h0: x <= write_data[9:0];
                4'h4: y <= write_data[9:0];
                4'h8: begin
                    w <= write_data[25:16]; 
                    h <= write_data[9:0];
                end
                4'hC: begin
                    color      <= write_data[11:0];
                    enable     <= write_data[12];
                    shape_type <= write_data[14:13];
                end
                default: ; 
            endcase
        end
    end

endmodule