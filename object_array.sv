module object_array #(parameter NUM_OBJECTS = 8)(
    input logic clk,
    input logic reset,
    input logic [9:0] pixel_x, pixel_y,

    input logic [31:0] write_data,
    input logic [15:0] addr,
    input logic write_enable,

    output logic [NUM_OBJECTS - 1 :0] active,
    output logic [11:0] pixel_color [NUM_OBJECTS - 1 : 0]
    );  
        
    logic [3:0] local_offset;
    logic [12:0] obj_index;

    assign local_offset = addr[3:0];
    assign obj_index = addr[15:4];

    genvar i;
    generate 
        for(i = 0; i < NUM_OBJECTS; i = i + 1) begin : obj_gen
            logic [9:0] x,y,w,h;
            logic enable;
            logic [11:0] color;
            logic [1:0] shape_type;
            logic obj_write_enable;

            assign obj_write_enable = write_enable & (obj_index ==  i);
            
              register_system genn(
                .clk(clk),
                .reset(reset),
                .addr(local_offset),
                .write_enable(obj_write_enable),
                .write_data(write_data),

                .x(x),.y(y),
                .w(w),.h(h),
                .color(color),
                .shape_type(shape_type),
                .enable(enable)
                );

            object_generator inst1(
                .clk(clk),
                .pixel_x(pixel_x),.pixel_y(pixel_y),
                .x(x), .y(y),
                .w(w), .h(h),
                .shape_type(shape_type),
                .enable(enable),
                .video_on(active[i]),
                .rgb_color(color),
                .pixel_color(pixel_color[i])
            );
        end
    endgenerate 


endmodule