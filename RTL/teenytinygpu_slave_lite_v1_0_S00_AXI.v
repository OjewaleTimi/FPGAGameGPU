`timescale 1 ns / 1 ps

module teenytinygpu_slave_lite_v1_0_S00_AXI #
(
    parameter integer C_S_AXI_DATA_WIDTH = 32,
    parameter integer C_S_AXI_ADDR_WIDTH = 16
)
(
    input  wire                                  S_AXI_ACLK,
    input  wire                                  S_AXI_ARESETN,
    input  wire [C_S_AXI_ADDR_WIDTH-1 : 0]       S_AXI_AWADDR,
    input  wire [2 : 0]                          S_AXI_AWPROT,
    input  wire                                  S_AXI_AWVALID,
    output wire                                  S_AXI_AWREADY,
    input  wire [C_S_AXI_DATA_WIDTH-1 : 0]       S_AXI_WDATA,
    input  wire [(C_S_AXI_DATA_WIDTH/8)-1 : 0]   S_AXI_WSTRB,
    input  wire                                  S_AXI_WVALID,
    output wire                                  S_AXI_WREADY,
    output wire [1 : 0]                          S_AXI_BRESP,
    output wire                                  S_AXI_BVALID,
    input  wire                                  S_AXI_BREADY,
    input  wire [C_S_AXI_ADDR_WIDTH-1 : 0]       S_AXI_ARADDR,
    input  wire [2 : 0]                          S_AXI_ARPROT,
    input  wire                                  S_AXI_ARVALID,
    output wire                                  S_AXI_ARREADY,
    output wire [C_S_AXI_DATA_WIDTH-1 : 0]       S_AXI_RDATA,
    output wire [1 : 0]                          S_AXI_RRESP,
    output wire                                  S_AXI_RVALID,
    input  wire                                  S_AXI_RREADY,
    
    // --- Physical VGA output pins ---
    output logic                                 hsync,
    output logic                                 vsync,
    output logic [3:0]                           vga_red,
    output logic [3:0]                           vga_green,
    output logic [3:0]                           vga_blue
);

reg axi_awready;
    reg axi_wready;
    reg [1:0] axi_bresp;
    reg axi_bvalid;
    reg axi_arready;
    reg [C_S_AXI_DATA_WIDTH-1:0] axi_rdata;
    reg [1:0] axi_rresp;
    reg axi_rvalid;

    assign S_AXI_AWREADY = axi_awready;
    assign S_AXI_WREADY  = axi_wready;
    assign S_AXI_BRESP   = axi_bresp;
    assign S_AXI_BVALID  = axi_bvalid;
    assign S_AXI_ARREADY = axi_arready;
    assign S_AXI_RDATA   = axi_rdata;
    assign S_AXI_RRESP   = axi_rresp;
    assign S_AXI_RVALID  = axi_rvalid;

    reg [15:0] write_addr_reg;
    reg [31:0] write_data_reg;
    reg        addr_done;
    reg        data_done;
    reg        gpu_wren;
    reg [11:0] background_color_reg;

    // Independent Write Address Handshake
    always @(posedge S_AXI_ACLK) begin
        if (!S_AXI_ARESETN) begin
            axi_awready    <= 1'b0;
            addr_done      <= 1'b0;
            write_addr_reg <= 16'd0;
        end else begin
            if (~axi_awready && S_AXI_AWVALID && ~addr_done) begin
                axi_awready    <= 1'b1;
                addr_done      <= 1'b1;
                write_addr_reg <= S_AXI_AWADDR[15:0];
            end else begin
                axi_awready <= 1'b0;
                if (axi_bvalid && S_AXI_BREADY) begin
                    addr_done <= 1'b0;
                end
            end
        end
    end

    // Independent Write Data Handshake
    always @(posedge S_AXI_ACLK) begin
        if (!S_AXI_ARESETN) begin
            axi_wready     <= 1'b0;
            data_done      <= 1'b0;
            write_data_reg <= 32'd0;
        end else begin
            if (~axi_wready && S_AXI_WVALID && ~data_done) begin
                axi_wready     <= 1'b1;
                data_done      <= 1'b1;
                write_data_reg <= S_AXI_WDATA;
            end else begin
                axi_wready <= 1'b0;
                if (axi_bvalid && S_AXI_BREADY) begin
                    data_done <= 1'b0;
                end
            end
        end
    end

    // Write Response & Strobe Generation
    always @(posedge S_AXI_ACLK) begin
        if (!S_AXI_ARESETN) begin
            axi_bvalid           <= 1'b0;
            axi_bresp            <= 2'b00;
            gpu_wren             <= 1'b0;
            background_color_reg <= 12'h003; // Default Dark Navy Blue
        end else begin
            gpu_wren <= 1'b0;

            // When BOTH address and data have been accepted, commit the write and respond
            if ((axi_awready || addr_done) && (axi_wready || data_done) && ~axi_bvalid) begin
                axi_bvalid <= 1'b1;
                axi_bresp  <= 2'b00; // OKAY
                gpu_wren   <= 1'b1;  // Strobe write to GPU logic

                if (write_addr_reg == 16'h0FF0) begin
                    background_color_reg <= write_data_reg[11:0];
                end
            end else if (S_AXI_BREADY && axi_bvalid) begin
                axi_bvalid <= 1'b0;
            end
        end
    end

    // Read Handshake (Stubbed)
    always @(posedge S_AXI_ACLK) begin
        if (!S_AXI_ARESETN) begin
            axi_arready <= 1'b0;
            axi_rvalid  <= 1'b0;
            axi_rresp   <= 2'b00;
            axi_rdata   <= 32'd0;
        end else begin
            if (~axi_arready && S_AXI_ARVALID) begin
                axi_arready <= 1'b1;
                axi_rvalid  <= 1'b1;
                axi_rresp   <= 2'b00;
                axi_rdata   <= (S_AXI_ARADDR[15:0] == 16'h0FF0) ? {20'd0, background_color_reg} : 32'h00000000;
            end else begin
                axi_arready <= 1'b0;
            end

            if (axi_rvalid && S_AXI_RREADY) begin
                axi_rvalid <= 1'b0;
            end
        end
    end

    // Connect VGA Top
    vga_top #(
        .NUM_OBJECTS(25)
    ) gpu_top (
        .clk_100MHz       (S_AXI_ACLK),
        .reset            (~S_AXI_ARESETN), 
        .write_addr       (write_addr_reg),
        .write_data       (write_data_reg),
        .write_enable     (gpu_wren),
        .background_color (background_color_reg),
        .hsync            (hsync),
        .vsync            (vsync),
        .vga_red          (vga_red),
        .vga_green        (vga_green),
        .vga_blue         (vga_blue)
    );
endmodule