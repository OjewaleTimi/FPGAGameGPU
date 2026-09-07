module compositor #(
    parameter NUM_OBJECTS = 8
)(
    input logic [NUM_OBJECTS-1:0] active,
    input logic [11:0] pixel_color [NUM_OBJECTS-1:0],
    input logic [11:0] background_color,
    output logic [11:0] final_color
);

       int j;
    always_comb begin
        final_color = background_color;
     for(j = 0; j < NUM_OBJECTS; j = j + 1)  begin
        if(active[j] == 1)begin
            final_color = pixel_color[j];
        end
    end
    end
endmodule 