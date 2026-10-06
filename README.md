# FPGA VGA Sprite Engine — Project Documentation

A hardware-accelerated 2D sprite rendering engine built for the Digilent Basys 3 board. The project combines a MicroBlaze soft processor, a custom AXI4-Lite GPU peripheral, and a VGA output pipeline to render sprite objects, layered text, and game-state visuals in real time.

Project status: Advanced — multi-object rendering, AXI4-Lite integration, text overlay, and MicroBlaze support are in place.

Target board: Digilent Basys 3 (Xilinx Artix-7, XC7A35T)
Display: VGA, 640×480 @ 60Hz
HDL: SystemVerilog (RTL) + C (MicroBlaze application)
Toolchain: Xilinx Vivado (block design + IP packaging)

---

## Full Implementation Preview

This image shows the complete FPGA VGA sprite engine architecture, including the MicroBlaze-controlled register interface, sprite generation pipeline, compositor, text overlay, and final VGA output path.

<img width="1573" height="707" alt="FPGA VGA Sprite Engine full implementation overview" src="https://github.com/user-attachments/assets/1f1b8631-baa7-4e4b-9a7f-d28e6635659e" />

---

<img width="1280" height="960" alt="Entire FPGA gaming setup with Pacman" src="https://github.com/user-attachments/assets/d6905993-beb7-4716-924e-f34cfa5c7785" />

This image shows the full board setup with the Pacman-style game running on the Basys 3 hardware.

---

## 1. Project Overview

This project implements a hardware-accelerated 2D sprite and object rendering engine on an FPGA, controlled at runtime by a MicroBlaze soft-core processor. Instead of writing a full framebuffer from software for every frame, the CPU writes a compact set of object descriptors to a custom GPU register space. The FPGA hardware evaluates sprite visibility and pixel color for every screen position at video timing rates.

This follows the same core idea used in classic sprite-based arcade machines and early console graphics systems: software decides what should be visible, while hardware determines how those objects are drawn efficiently every pixel clock.

### 1.1 Why this architecture

| Concern | How this design solves it |
|---|---|
| Frame rate stability | The VGA controller scans at a fixed timing independent of CPU speed. |
| CPU workload | Software writes only a few registers per object instead of a full framebuffer each frame. |
| Scaling object count | Object generation is implemented in hardware and replicated via a generate loop. |
| Reusability | One `object_generator` is reused across many sprite slots through `object_array`. |
| Deterministic output | The compositor resolves overlaps with a fixed priority rule and predictable z-order. |

### 1.2 High-level data flow

```text
[Game logic in C, running on MicroBlaze]
        |
        | AXI4-Lite writes (object descriptors)
        v
[AXI4-Lite Slave Interface]  <-- teenytinygpu_slave_lite_v1_0_S00_AXI.v
        |
        v
[object_array]  --- generate loop, N instances (default N=25) ---
        |
        +--> [Slot 0: register_system + object_generator] --> hit0, color0
        +--> [Slot 1: register_system + object_generator] --> hit1, color1
        +--> ...                                             ...
        +--> [Slot N-1: register_system + object_generator] --> hitN-1, colorN-1
        |
        v
[compositor]  <-- resolves overlaps, z-order priority (lower slot wins)
        |
        v
[text_overlay]  <-- 80x30 character grid, BRAM-backed font ROM
        |
        v
[vga_top]  <-- multiplexes text over shapes/background and aligns pipeline delays
        |
        v
[vga_controller]  <-- generates sync timing and pixel coordinates
        |
        v
   Physical VGA port (RGB + sync)
```

---

## 2. Repository Structure

```text
repo-root/
├── README.md                                  <- project documentation
├── docs/
│   ├── register_map.md                        <- authoritative AXI address map (planned)
│   └── phase_log.md                           <- development log (planned)
├── RTL/
│   ├── vga_controller.sv                      <- VGA timing generator (640×480@60Hz)
│   ├── object_generator.sv                    <- combinational hit/color logic for one object
│   ├── register_system.sv                     <- per-slot register file (x, y, w, h, color, shape, enable)
│   ├── object_array.sv                        <- N-instance generate loop and address decode
│   ├── compositor.sv                          <- priority mux for overlap resolution
│   ├── text_overlay.sv                       <- 80×30 text grid with BRAM font ROM
│   ├── font_rom.sv                           <- BRAM-backed character font store
│   ├── vga_top.sv                            <- top-level wiring, layering, and timing alignment
│   ├── teenytinygpu.v                        <- AXI4-Lite IP top wrapper (Vivado-generated)
│   ├── teenytinygpu_slave_lite_v1_0_S00_AXI.v <- AXI4-Lite slave interface (Vivado-generated)
│   ├── font.coe                              <- character bitmap data (8×16 pixels per char)
│   ├── comp_tb.sv                            <- compositor testbench
│   ├── reg_tb.sv                             <- register_system testbench
│   ├── object_array_tb.sv                    <- object_array address decode testbench
│   └── tb_vga_top.sv                         <- full-frame integration testbench with VCD output
├── TB/
│   ├── tb_compositor.sv                      <- comprehensive compositor tests
│   ├── tb_vga_top.sv                         <- integration testbench for sim organization
│   ├── object_array_tb.sv                    <- object array validation
│   └── reg_tb.sv                            <- register system validation
├── sw/
│   ├── main.c                                <- MicroBlaze application with game logic
│   └── vga_gpu.h                             <- GPU driver API and helper macros
├── sim/
│   └── (simulation scripts / Makefiles)      <- optional automation
├── constraints/
│   └── (Basys 3 XDC)                         <- board pin constraints
├── vivado/
│   └── block_design/                         <- exported block design / TCL files
├── VCD_FILE/
│   └── (waveform dumps from simulations)
├── build/
│   └── (Vivado build artifacts, not committed)
└── README.md
```

Rule: one module per file; the filename generally matches the module name, with the exception of Vivado-generated files.

---

## 3. Module Inventory & Status

| Module | Hand-written? | Status | Purpose |
|---|---|---|---|
| `vga_controller` | Yes (original) | ✅ Done | Generates 640×480@60Hz timing and pixel clock |
| `object_generator` | Yes | ✅ Done | Combinational hit/color logic for one sprite object |
| `register_system` | Yes | ✅ Done | Per-slot register file for object parameters |
| `object_array` | Yes | ✅ Done | Generate loop over object slots and address decode |
| `compositor` | Yes | ✅ Done | Resolves overlap priority and chooses final color |
| `text_overlay` | Yes | ✅ Done | Renders an 80×30 character grid with font ROM |
| `font_rom` | Yes | ✅ Done | BRAM-backed 8×16 font storage |
| `vga_top` | Yes | ✅ Done | Top-level wiring, layer muxing, and timing alignment |
| `teenytinygpu.v` | Vivado-generated | ✅ Done | AXI IP wrapper shell |
| `teenytinygpu_slave_lite_v1_0_S00_AXI.v` | Vivado-generated | ✅ Done | AXI4-Lite slave interface logic |
| `main.c` | Yes | ✅ Done | Game loop, input handling, collision detection, scoring |
| `vga_gpu.h` | Yes | ✅ Done | Convenience macros and software driver API |

### Key achievements

- ✅ All core RTL modules implemented and simulated
- ✅ AXI4-Lite integration completed
- ✅ Text overlay with BRAM-backed font ROM implemented
- ✅ Multi-object rendering with a default of 25 sprite slots
- ✅ Priority-based compositor for deterministic z-order behavior
- ✅ MicroBlaze driver layer completed
- ✅ Full game logic with collision detection and score updates implemented

---

## 4. Hardware and Software Interaction

The design is intentionally split into hardware and software responsibilities:

1. The MicroBlaze CPU decides what objects should appear and updates their descriptors.
2. The AXI4-Lite peripheral receives software writes and stores the object metadata.
3. The FPGA renderer checks each object against the current pixel coordinates.
4. The compositor resolves overlaps using a fixed priority rule.
5. The text overlay draws characters over the final scene.
6. The VGA controller outputs the resulting pixels to the display.

This separation allows the CPU to focus on gameplay logic, while the FPGA handles the pixel-by-pixel rendering path at fixed video timing.

---

## 5. Module Specifications

### 5.1 `object_generator`

Purpose: determine whether the current pixel belongs to a given sprite and what color it should output.

Parameters:

| Name | Default | Description |
|---|---|---|
| (none — all widths inferred from port widths) | — | — |

Ports:

| Name | Dir | Width | Description |
|---|---|---:|---|
| `clk` | in | 1 | Pixel clock |
| `pixel_x`, `pixel_y` | in | 10, 10 | Current scan column and row |
| `x`, `y` | in | 10, 10 | Object origin |
| `w`, `h` | in | 10, 10 | Width/height or radius |
| `shape_type` | in | 2 | `00` = rectangle, `01` = circle |
| `enable` | in | 1 | Object active flag |
| `rgb_color` | in | 12 | RGB444 color input |
| `video_on` | out | 1 | Pixel belongs to this object |
| `pixel_color` | out | 12 | Color output for this pixel |

Behavior:

- If `enable == 0`, the object is inactive and never contributes pixels.
- Rectangle mode checks whether the pixel is within the object bounds.
- Circle mode tests whether the pixel lies within the circle radius.
- The logic is fully combinational and reevaluated every pixel clock.

### 5.2 `register_system`

Purpose: store a single object's descriptor fields, including position, dimensions, color, shape, and enable state.

Ports:

| Name | Dir | Description |
|---|---|---|
| `clk`, `reset` | in | Standard clock and reset |
| `write_enable` | in | Asserted only when the slot is addressed |
| `addr` | in | Local register offset within the slot |
| `write_data` | in | 32-bit write data |
| `x, y, w, h, color, shape_type, enable` | out | Unpacked fields for `object_generator` |

Register layout (matching `vga_gpu.h`):

- Offset `0x00 (X)`: `write_data[9:0]` → x coordinate
- Offset `0x04 (Y)`: `write_data[9:0]` → y coordinate
- Offset `0x08 (DIM)`: `write_data[25:16]` → w, `write_data[9:0]` → h
- Offset `0x0C (CFG)`: `write_data[14:13]` → shape, `write_data[12]` → enable, `write_data[11:0]` → color (RGB444)

No combinational logic beyond field unpacking is required.

### 5.3 `object_array`

Purpose: instantiate N copies of `register_system + object_generator` and decode the write address into the correct slot.

Parameters:

| Name | Default | Description |
|---|---|---|
| `NUM_OBJECTS` | 25 | Number of sprite slots |

Ports:

| Name | Dir | Description |
|---|---|---|
| `clk`, `reset` | in | Standard clock and reset |
| `addr` | in | 16-bit address; upper bits select the slot |
| `write_data` | in | 32-bit write data |
| `write_enable` | in | Write strobe |
| `pixel_x`, `pixel_y` | in | Current VGA pixel coordinates |
| `active[NUM_OBJECTS-1:0]` | out | One hit bit per object |
| `pixel_color[NUM_OBJECTS-1:0]` | out | One color bus per object |

Address decode logic:

- `slot_index = addr[15:4]`
- `reg_offset = addr[3:0]`
- `write_enable` is routed only to the selected object slot

### 5.4 `compositor`

Purpose: resolve overlapping sprites into a single final color for each pixel.

Ports:

| Name | Dir | Description |
|---|---|---|
| `active[NUM_OBJECTS-1:0]` | in | Hit vector from `object_array` |
| `pixel_color[NUM_OBJECTS-1:0]` | in | Per-object pixel color |
| `background_color` | in | Background RGB444 color |
| `final_color` | out | Final pixel color |

Priority rule:

- Scan from slot 0 to `NUM_OBJECTS-1`
- The first active object wins
- Lower slot index therefore has higher z-order priority

### 5.5 `text_overlay`

Purpose: render an 80×30 grid of text using a BRAM-backed font ROM.

Ports:

| Name | Dir | Description |
|---|---|---|
| `clk`, `reset` | in | Standard signals |
| `pixel_x`, `pixel_y` | in | Current pixel position |
| `addr` | in | AXI write address for text writes |
| `write_data` | in | Packed ASCII + RGB12 data |
| `write_enable` | in | AXI write strobe |
| `text_active` | out | Indicates whether the pixel is covered by text |
| `text_color` | out | RGB12 color of the text pixel |

Behavior:

- Text writes target addresses with bit 15 set (base offset `0x8000`)
- Each character cell stores 32 bits: `[31:8] = RGB12`, `[7:0] = ASCII`
- The font ROM has one cycle of read latency, and `text_overlay` compensates for it internally
- `vga_top` aligns the text path so it remains synchronized with sprite rendering

### 5.6 `vga_top`

Purpose: connect and align the compositor, text overlay, and VGA controller into a single screen pipeline.

Key logic:

- Delays the compositor output and synchronization signals to match the text overlay latency
- Draws text on top of shapes and background
- Suppresses colors during blanking periods
- Feeds the final RGB value to the VGA controller

---

## 6. AXI4-Lite Register Map

The custom GPU peripheral is generated through Vivado IP Packager. The base address is typically `0x44A00000U`, but it may vary depending on the final block design.

### Object registers

Each object occupies 16 bytes in the GPU address space.

| Offset | Field | Bits | Description |
|---|---|---|---|
| `+0x00` | X | `[9:0]` | object x-origin |
| `+0x04` | Y | `[9:0]` | object y-origin |
| `+0x08` | DIM | `[25:16] = w`, `[9:0] = h` | width and height |
| `+0x0C` | CFG | `[14:13] = shape`, `[12] = enable`, `[11:0] = color` | configuration and RGB444 color |

### Background color register

| Offset | Field | Description |
|---|---|---|
| `+0xFF0` | `BG_COLOR` | RGB444 background color |

### Text overlay registers

Text writes target an address range beginning at `0x8000`.

| Offset | Field | Bits | Description |
|---|---|---|---|
| `+0x8000 + (row * 80 + col) * 4` | `TEXT_CELL` | `[31:8] = RGB12`, `[7:0] = ASCII` | character and text color |

See `sw/vga_gpu.h` for helper macros such as `GPU_TextPutChar`, `GPU_TextPutString`, and related convenience functions.

---

## 7. Driver API Reference (`vga_gpu.h`)

```c
// Set background color
GPU_SetBackgroundColor(r, g, b);  // r, g, b are 4-bit values (0..15)

// Position a sprite
GPU_SetPosition(slot, x, y);      // slot: 0..24, x: 0..639, y: 0..479

// Size a sprite
GPU_SetDimensions(slot, w, h);    // w, h: 0..1023

// Configure shape + enable + color
GPU_ConfigureConfig(slot, shape, enable, rgb12);
// shape: SHAPE_RECTANGLE (0) or SHAPE_CIRCLE (1)

// All-in-one sprite setup
GPU_ConfigureObject(slot, x, y, w, h, shape, r, g, b, enable);

// Text overlay
GPU_TextPutChar(row, col, 'A', rgb12);    // row: 0..29, col: 0..79
GPU_TextPutString(row, col, "Hello", rgb12);
GPU_TextClear();
```

Example:

```c
GPU_SetBackgroundColor(0, 0, 0);
GPU_ConfigureObject(0, 100, 80, 40, 30, SHAPE_RECTANGLE, 15, 0, 0, 1);
GPU_TextPutString(0, 2, "READY", 0xFFF);
```

---

## 8. Coding Conventions (SystemVerilog)

Each SystemVerilog file should follow the project conventions below:

- Add a file-level header comment:

```systemverilog
// ============================================================
// Module: <module_name>
// Purpose: <one line>
// ============================================================
```

- Use `` `default_nettype none `` at the top of every file to catch accidental typos.
- Use `always_comb` for combinational logic rather than `always @(*)`.
- Use `always_ff @(posedge clk)` for sequential logic.
- Use synchronous active-low reset (`reset`) when appropriate.
- Avoid inferred latches; every combinational branch must assign all outputs.
- Prefer parameterized widths over hardcoded literals.
- Keep the pixel pipeline in one clock domain; the MicroBlaze/AXI side is separate.

---

## 9. Build Phases — Current Status

| Phase | Description | Status |
|---|---|---|
| **1** | Single hardcoded rectangle + testbench | ✅ Done |
| **2** | Add circle shape support | ✅ Done |
| **3** | Two hardcoded instances + manual mux | ✅ Done |
| **4** | Registers instead of hardcoded parameters | ✅ Done |
| **5** | Generalize to N via generate loop | ✅ Done (default `N=25`) |
| **6** | Address decode (pre-AXI bus) | ✅ Done |
| **7** | AXI4-Lite wrapping (Vivado IP) | ✅ Done |
| **8** | MicroBlaze + interconnect + block design | ✅ Done |
| **9** | Game logic on MicroBlaze | ✅ Done |
| **10** | Text overlay with BRAM font | ✅ Done |
| **11** | Polish, tuning, and cleanup | 🔄 In progress |

### Completed milestones

- ✅ All RTL modules tested in simulation
- ✅ AXI4-Lite IP packaged and integrated
- ✅ MicroBlaze application with full game logic
- ✅ Text rendering system with font ROM
- ✅ Multiple test benches validating each layer

---

## 10. Simulation Strategy

The project includes focused test benches for each hardware block:

- `tb_compositor.sv`: validates overlap priority and z-order behavior
- `reg_tb.sv`: checks register unpacking and write behavior
- `object_array_tb.sv`: validates address decode and slot routing
- `tb_vga_top.sv`: runs a full-frame integration simulation and emits `.vcd` output for waveform inspection

### Hardware bring-up order

1. Simulate and validate the core pipeline before adding AXI wrapping.
2. Generate the custom IP in Vivado.
3. Instantiate the MicroBlaze, interconnect, BRAM controller, and clocking wizard in the block design.
4. Load a compiled `.elf` onto the board and test simple object movement and register writes.
5. Bring up game logic and text rendering once the base display pipeline is stable.

### Common causes of display failure

If nothing appears on the monitor after the block design is loaded:

- Validate the AXI address map in Vivado.
- Confirm the pixel clock is running at the expected 25 MHz.
- Check the VGA pin assignments against the Basys 3 XDC.
- Verify that the BRAM, interconnect, and custom IP are all connected correctly.

---

## 11. Vivado Build Notes (Basys 3 specific)

- Board clock: 100 MHz on the Basys 3 board clock input
- Pixel clock: 25 MHz generated via the Clocking Wizard for 640×480@60Hz VGA output
- Block design contains MicroBlaze, AXI interconnect, BRAM controller, and the custom GPU peripheral
- Use the standard Digilent Basys 3 master XDC as the starting point and assign the VGA output pins correctly
- If the custom IP is not visible in Vivado, add the repository directory under IP Catalog → Add Repository

---

## 12. Git Workflow

- Use a separate branch for each feature or fix
- Do not commit directly to `main` without review
- Update the Module Inventory and Build Phases sections as part of PRs that change hardware or software behavior
- Commit relevant Vivado files such as `.bd`, `.tcl`, and `.xci` when needed
- Do not commit generated artifacts such as `.runs/`, `.cache/`, `.sim/`, or build output

---

## 13. Performance & Hardware Resource Notes

- Default configuration: `NUM_OBJECTS = 25`, 640×480@60Hz display, 100 MHz system clock
- LUT usage: roughly 20–30% of the XC7A35T fabric depending on complexity
- BRAM usage: a few blocks for font storage and block design memory
- Timing closure: the design runs comfortably at 100 MHz; the VGA pixel pipeline runs at the effective 25 MHz pixel rate

---

## 14. Glossary

| Term | Meaning |
|---|---|
| Slot | One object instance in the object array |
| Hit / Active | A signal indicating the current pixel belongs to a sprite |
| Compositor | Logic that resolves multiple sprite hits into one final pixel |
| Z-order | Priority rule for overlapping objects (lower slot index wins) |
| RGB444 | 12-bit color format using 4 bits per channel |
| BRAM | Block RAM used for font storage and on-chip memory |

---

## Quick Start / Contributor Notes

1. Review the Build Phases section to understand the current project status.
2. Follow the SystemVerilog coding conventions.
3. Run the existing testbenches in `TB/` before integrating changes.
4. Update this README whenever the interface, register map, or workflow changes.
5. Treat this file as the source of truth for architecture, behavior, and contributor expectations.

---

## Maintainer Note

This document should remain synchronized with the codebase. If a module interface, register map, timing assumption, or design convention changes, update the relevant section in this README as part of the same change.

---

The goal of this project is to provide a clean, practical FPGA graphics pipeline that combines programmable logic, software control, and real display output on a low-cost development board.
