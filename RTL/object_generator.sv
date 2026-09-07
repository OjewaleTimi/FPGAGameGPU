module object_generator (
    input  logic clk,
    input  logic [9:0] pixel_x, pixel_y,   
    input  logic [9:0] x, y,
    input  logic [9:0] w, h,
    input  logic [1:0] shape_type,
    input  logic [11:0] rgb_color,
    input  logic enable,
    output logic video_on,
    output logic [11:0] pixel_color
);

    logic [9:0] dx, dy;
    logic [19:0] dist_sq, radius_sq;

    always_comb begin
        pixel_color = rgb_color;
        video_on = 1'b0;

        case (shape_type)
            2'b00: // rectangle
                video_on = enable && (pixel_x >= x) && (pixel_x < x + w) &&
                                      (pixel_y >= y) && (pixel_y < y + h);

            2'b01: begin // circle: x,y = center, w = radius
                dx = (pixel_x < x) ? (x - pixel_x) : (pixel_x - x);
                dy = (pixel_y < y) ? (y - pixel_y) : (pixel_y - y);
                dist_sq   = dx*dx + dy*dy;
                radius_sq = w*w;
                video_on = enable && (dist_sq <= radius_sq);
            end

            default: video_on = 1'b0;
        endcase
    end
endmodule