# FPGA VGA Sprite Engine — Project Documentation

Project status: **Advanced — Multi-object rendering with AXI4-Lite integration, text overlay, and MicroBlaze support.**

**Target board:** Digilent Basys 3 (Xilinx Artix-7, XC7A35T)
**Display:** VGA, 640×480 @ 60Hz
**HDL:** SystemVerilog (RTL) + C (MicroBlaze application)
**Toolchain:** Xilinx Vivado (Block Design + IP Packager)
**Language Composition:** C (56.4%), SystemVerilog (36.7%), Verilog (6.9%)

---

<img width="1573" height="707" alt="FPGA VGA Sprite Engine block diagram" src="https://github.com/user-attachments/assets/1f1b8631-baa7-4e4b-9a7f-d28e6635659e" />

## 1. Project Overview

This project implements a hardware-accelerated 2D sprite/object rendering engine on an FPGA, controllable at runtime by a soft-core CPU (MicroBlaze). Instead of a CPU writing a full framebuffer every cycle, objects are stored in registers and evaluated in parallel every pixel clock via a parameterized generate loop. A compositor then resolves overlaps based on priority (z-order).

This is the same architectural idea used in classic sprite-based arcade and console hardware: software decides what should be on screen; hardware decides how it gets drawn every 25 MHz pixel clock.

### 1.1 Why this architecture

| Concern | How this design solves it |
|---|---|
| Frame rate stability | VGA controller scans at a fixed, independent rate. CPU speed cannot cause stutter; it can only cause stale positions. |
| CPU workload | Only a few bytes are written per object per game-logic tick, not a full framebuffer. |
| Scaling object count | Costs FPGA fabric (LUTs/FFs) via a generate loop, not CPU cycles — all objects are evaluated in parallel every pixel. |
| Reusability | Shape evaluation logic (`object_generator`) is written once and instantiated N times via `object_array`. |

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
[text_overlay]  <-- 80x30 character grid, BRAM-backed font ROM, 1-cycle latency
        |
        v
[vga_top]  <-- multiplexes text (top) with shapes/background, adjusts pipeline delays
        |
        v
[vga_controller]  (unchanged from original) --> hsync, vsync, pixel_x, pixel_y
        |
        v
   Physical VGA port (RGB + sync)
```

---

## 2. Repository Structure

```text
repo-root/
├── README.md                                  <- this file
├── docs/
│   ├── register_map.md                        <- (planned) authoritative AXI address map
│   └── phase_log.md                           <- (planned) running log of completed phases
├── RTL/
│   ├── vga_controller.sv                      <- existing VGA timing (640×480@60Hz)
│   ├── object_generator.sv                    <- combinational hit/color logic for one object
│   ├── register_system.sv                     <- per-slot register file (x, y, w, h, color, shape, enable)
│   ├── object_array.sv                        <- N-instance generate loop, address decode
│   ├── compositor.sv                          <- priority mux over active objects
│   ├── text_overlay.sv                        <- 80x30 text grid with BRAM font ROM (1-cycle latency)
│   ├── font_rom.sv                            <- BRAM-backed character font (initialized from font.coe)
│   ├── vga_top.sv                             <- top-level wire harness, text+shape layering, pipeline alignment
│   ├── teenytinygpu.v                         <- AXI4-Lite IP top wrapper (Vivado-generated)
│   ├── teenytinygpu_slave_lite_v1_0_S00_AXI.v <- AXI4-Lite slave interface (Vivado-generated)
│   ├── font.coe                               <- character bitmap data (8×16 pixels per char)
│   ├── comp_tb.sv                             <- standalone compositor testbench
│   ├── reg_tb.sv                              <- register_system testbench
│   ├── object_array_tb.sv                     <- object_array address decode testbench
│   └── tb_vga_top.sv                          <- full-frame integration testbench with VCD output
├── TB/
│   ├── tb_compositor.sv                       <- comprehensive compositor tests
│   ├── tb_vga_top.sv                          <- integration testbench (duplicate, in TB/ for sim organization)
│   ├── object_array_tb.sv                     <- object array tests
│   └── reg_tb.sv                              <- register system tests
├── sw/
│   ├── main.c                                 <- MicroBlaze application (game logic, collision detection, scoring)
│   └── vga_gpu.h                              <- driver header with GPU_* macros and helper functions
├── sim/
│   └── (simulation script or Makefile)        <- (optional) automation for running testbenches
├── constraints/
│   └── (Basys 3 XDC)                          <- (to be added) pin constraints for Basys 3 board
├── vivado/
│   └── block_design/                          <- (to be added) exported .bd / .tcl for reproducible builds
├── VCD_FILE/
│   └── (waveform dumps from simulations)
└── build/
    └── (Vivado build artifacts, not committed)
```

**Rule:** one module per file, filename generally matches module name (except Vivado-generated files).

---

## 3. Module Inventory & Status

| Module | Hand-written? | Status | Lines | Purpose |
|---|---|---|---|---|
| `vga_controller` | Yes (original) | ✅ **Done** | ~100 | Generates 640×480@60Hz timing, pixel clock |
| `object_generator` | Yes | ✅ **Done** | ~50 | Combinational hit/color logic for one object shape |
| `register_system` | Yes | ✅ **Done** | ~50 | Per-slot register file, write decode, unpacks fields |
| `object_array` | Yes | ✅ **Done** | ~60 | Generate loop over N slots, address decode |
| `compositor` | Yes | ✅ **Done** | ~20 | Priority mux, resolves overlapping objects |
| `text_overlay` | Yes | ✅ **Done** | ~150 | 80×30 character grid, text rendering |
| `font_rom` | Yes | ✅ **Done** | ~50 | BRAM-backed 8×16 font storage |
| `vga_top` | Yes | ✅ **Done** | ~120 | Top-level wiring, layer muxing, pipeline alignment |
| `teenytinygpu.v` | Vivado-generated | ✅ **Done** | ~40 | AXI IP wrapper shell |
| `teenytinygpu_slave_lite_v1_0_S00_AXI.v` | Vivado-generated | ✅ **Done** | ~200 | AXI4-Lite slave interface logic |
| `main.c` (MicroBlaze app) | Yes | ✅ **Done** | ~2000+ | Full game loop, input, collision, scoring |
| `vga_gpu.h` (driver) | Yes | ✅ **Done** | ~150 | Convenience macros and helper functions |

**Key Achievements:**
- ✅ All core RTL modules complete and simulated
- ✅ AXI4-Lite integration complete
- ✅ Text overlay with BRAM-backed font
- ✅ Multi-object (default 25 objects) parallel rendering
- ✅ Priority-based compositor (lower slot index = higher z-order)
- ✅ MicroBlaze driver layer complete
- ✅ Full game implementation in C with collision detection

---

## 4. Module Specifications

### 4.1 `object_generator`

**Purpose:** Given the current scan position and one object's parameters, determine whether the current pixel belongs to this object and what color it should be.

**Parameters:**

| Name | Default | Description |
|---|---|---|
| (none — all widths inferred from port widths) | — | — |

**Ports:**

| Name | Dir | Width | Description |
|---|---|---|---|
| `clk` | in | 1 | pixel clock (100 MHz) |
| `pixel_x`, `pixel_y` | in | 10, 10 | current scan column/row |
| `x`, `y` | in | 10, 10 | object origin |
| `w`, `h` | in | 10, 10 | width/height (or radius for circle) |
| `shape_type` | in | 2 | `00`=rect, `01`=circle, (others reserved) |
| `enable` | in | 1 | object active flag |
| `rgb_color` | in | 12 | input color (RGB444) |
| `video_on` | out | 1 | 1 if current pixel belongs to this object |
| `pixel_color` | out | 12 | valid only when `video_on` is high |

**Behavior (combinational, `always_comb`):**
- `enable == 0` → `video_on = 0` unconditionally.
- `shape_type == 2'b00` (rect): `video_on = (pixel_x >= x) && (pixel_x < x + w) && (pixel_y >= y) && (pixel_y < y + h)`
- `shape_type == 2'b01` (circle): center = `(x, y)`, radius = `w`. `video_on = ((dx*dx + dy*dy) <= (w*w))` where `dx = pixel_x - x`, `dy = pixel_y - y`.

**Design note:** Purely combinational, re-evaluated every pixel clock as a pure function of inputs.

---

### 4.2 `register_system`

**Purpose:** Storage for one object's descriptor fields (x, y, w, h, color, shape, enable). Holds flip-flops written by address-decode logic in `object_array`.

**Ports:**

| Name | Dir | Description |
|---|---|---|
| `clk`, `reset` | in | standard |
| `write_enable` | in | write strobe (asserted only when this slot is addressed) |
| `addr` | in | local offset within this slot (4 bits) |
| `write_data` | in | 32-bit write data |
| `x, y, w, h, color, shape_type, enable` | out | unpacked fields for `object_generator` |

**Layout (from vga_gpu.h):**
- **Offset 0x00 (X):** `write_data[9:0]` → x coordinate
- **Offset 0x04 (Y):** `write_data[9:0]` → y coordinate
- **Offset 0x08 (DIM):** `write_data[25:16]` → w, `write_data[9:0]` → h
- **Offset 0x0C (CFG):** `write_data[14:13]` → shape_type, `write_data[12]` → enable, `write_data[11:0]` → color (RGB444)

No combinational logic beyond field unpacking.

---

### 4.3 `object_array`

**Purpose:** Instantiate N × (`register_system` + `object_generator`) via `generate`, and decode an AXI address into per-slot writes.

**Parameters:**

| Name | Default | Description |
|---|---|---|
| `NUM_OBJECTS` | 25 | number of sprite slots |

**Ports:**

| Name | Dir | Description |
|---|---|---|
| `clk`, `reset` | in | standard |
| `addr` | in | 16-bit address (upper bits = slot index, lower 4 bits = register offset within slot) |
| `write_data` | in | 32-bit write data |
| `write_enable` | in | write strobe |
| `pixel_x`, `pixel_y` | in | from VGA controller |
| `active[NUM_OBJECTS-1:0]` | out | one "hit" bit per object, to compositor |
| `pixel_color[NUM_OBJECTS-1:0]` | out | one color bus per object, to compositor |

**Address decode logic:**
- `slot_index = addr[15:4]`
- `reg_offset = addr[3:0]`
- Route `write_enable` to the selected slot's write strobe only.

---

### 4.4 `compositor`

**Purpose:** Resolve overlapping objects into a single RGB output per pixel.

**Ports:**

| Name | Dir | Description |
|---|---|---|
| `active[NUM_OBJECTS-1:0]` | in | from `object_array` |
| `pixel_color[NUM_OBJECTS-1:0]` | in | from `object_array` |
| `background_color` | in | background RGB444 |
| `final_color` | out | final RGB444 for this pixel |

**Priority rule:** Scans from `j = 0` to `NUM_OBJECTS-1`. First active object wins (lower slot index = top priority, highest z-order).

---

### 4.5 `text_overlay`

**Purpose:** Render an 80×30 grid of 8×16 character cells using a BRAM-backed font ROM.

**Ports:**

| Name | Dir | Description |
|---|---|---|
| `clk`, `reset` | in | standard |
| `pixel_x`, `pixel_y` | in | current pixel position |
| `addr` | in | AXI write address (distinguishes text writes via bit 15) |
| `write_data` | in | AXI write data (packed as ASCII + RGB12) |
| `write_enable` | in | AXI write strobe |
| `text_active` | out | 1 if this pixel is covered by rendered text |
| `text_color` | out | RGB12 color of text at this pixel |

**Behavior:**
- Text writes target addresses with bit 15 set (base offset 0x8000).
- Each character cell stores 32 bits: `[31:8] = RGB12`, `[7:0] = ASCII`.
- Font ROM (`font_rom.sv`) has 1 cycle of read latency (real BRAM). `text_overlay` accounts for this internally.
- See `vga_top.sv` for 1-cycle pipeline alignment to keep text synchronized with shapes.

---

### 4.6 `vga_top`

**Purpose:** Top-level module that wires compositor, text overlay, and VGA controller outputs; multiplexes layers and manages pipeline delays.

**Key logic:**
- Delays compositor output and VGA sync signals by 1 clock to align with `text_overlay`'s BRAM latency.
- Text draws on top of shapes/background (highest layer).
- `video_on` suppresses all colors during blanking intervals.

---

## 5. AXI4-Lite Register Map

Generated via Vivado's IP Packager. Base address typically `0x44A00000U` (configurable via Vivado).

### Object Registers (via `register_system`)

Each object occupies 4 words (16 bytes). Base address of slot `i` = `i * 16` (within GPU address space).

| Offset | Field | Bits | Description |
|---|---|---|---|
| +0x0 | X | [9:0] | object x-origin, 0–639 |
| +0x4 | Y | [9:0] | object y-origin, 0–479 |
| +0x8 | DIM | [25:16]=w, [9:0]=h | width/height (radius for circle) |
| +0xC | CFG | [14:13]=shape, [12]=enable, [11:0]=color | configuration and RGB444 |

### Background Color Register

| Offset | Field | Description |
|---|---|---|
| +0xFF0 | BG_COLOR | RGB444 background color |

### Text Overlay Registers

Text writes target offset `0x8000 + (row * 80 + col) * 4` within GPU address space.

| Offset | Field | Bits | Description |
|---|---|---|---|
| +0x8000+ | TEXT_CELL | [31:8]=RGB12, [7:0]=ASCII | character and color |

See `vga_gpu.h` for convenience macros (`GPU_TextPutChar`, `GPU_TextPutString`, etc.).

---

## 6. Coding Conventions (SystemVerilog)

- **File header** (top of every `.sv` file):
  ```systemverilog
  // ============================================================
  // Module: <module_name>
  // Purpose: <one line>
  // ============================================================
  ```
- Use `` `default_nettype none `` at the top of every file to catch typos in signal names.
- Combinational logic: `always_comb`, never `always @(*)`.
- Sequential logic: `always_ff @(posedge clk)`, with synchronous active-low reset (`reset`) convention.
- No inferred latches — every `always_comb` block must assign all outputs on every path.
- Parameterize widths — never hardcode values like `10'd639`; derive from parameters or explicit bit widths.
- One clock domain for the entire pixel pipeline (100 MHz input, 25 MHz effective pixel clock via VGA controller). MicroBlaze/AXI runs on the system clock.

---

## 7. Build Phases — Current Status

| Phase | Description | Status |
|---|---|---|
| **1** | Single hardcoded rectangle + testbench | ✅ **Done** |
| **2** | Add circle shape support | ✅ **Done** |
| **3** | Two hardcoded instances + manual mux | ✅ **Done** |
| **4** | Registers instead of hardcoded params | ✅ **Done** |
| **5** | Generalize to N via generate loop | ✅ **Done** (default N=25) |
| **6** | Address decode (pre-AXI bus) | ✅ **Done** |
| **7** | AXI4-Lite wrapping (Vivado IP) | ✅ **Done** (`teenytinygpu` IP generated) |
| **8** | MicroBlaze + interconnect + Block Design | ✅ **Done** |
| **9** | Game logic on MicroBlaze | ✅ **Done** (collision, scoring, input handling in main.c) |
| **10** | Text overlay with BRAM font | ✅ **Done** |
| **11** | Polish & optimization | ✅ **In Progress** |

**Completed milestones:**
- ✅ All RTL modules tested in simulation
- ✅ AXI4-Lite IP packaged and integrated
- ✅ MicroBlaze application with full game logic
- ✅ Text rendering system with font ROM
- ✅ Multiple test benches validating each layer

---

## 8. Simulation Strategy

Every hand-written module has a corresponding testbench in `TB/`:

- **`tb_compositor.sv`:** Validates priority mux behavior over 2–25 active objects; checks z-order.
- **`reg_tb.sv`:** Drives the register interface; confirms field unpacking.
- **`object_array_tb.sv`:** Tests address decode across all slots and word offsets.
- **`tb_vga_top.sv`:** Full-frame integration test with multiple objects; generates `.vcd` waveform for GTKWave inspection.

**Hardware bring-up order:**
1. Simulate phases 1–6 thoroughly (all testbenches pass).
2. Generate the AXI IP in Vivado (Phase 7).
3. Instantiate MicroBlaze, interconnect, BRAM, clocking wizard in Block Design (Phase 8).
4. Load compiled `.elf` onto the board and verify simple register writes (e.g., move one sprite).

If nothing appears on real hardware after Phase 8, common causes:
- AXI/interconnect address mismatch (verify in Vivado).
- Clock domain crossing issues (check clock wizard output rates).
- Pin assignment error (verify in `.xdc` file against board documentation).

---

## 9. Vivado Build Notes (Basys 3 specific)

- **Board clock:** 100 MHz (`W5` pin, per Basys 3 master XDC).
- **Pixel clock:** 25 MHz (generated via Clocking Wizard) for 640×480@60Hz VGA.
- **Block Design:** Contains MicroBlaze, AXI interconnect, BRAM controller, and `teenytinygpu` IP. Export as Tcl for reproducibility.
- **Constraints:** Use the standard Digilent Basys 3 master `.xdc` as base; enable VGA port pins (JA header) and confirm against your board revision.
- **IP Integration:** The `teenytinygpu` IP is pre-packaged in `RTL/`. Register it in Vivado via **IP Catalog → Add Repositories** if not already visible.

---

## 10. Git Workflow

- **Branch per feature:** e.g., `feat/text-overlay`, `fix/compositor-priority`.
- **No direct commits to `main`** — PR + team review required.
- **Update Section 3 (Module Inventory) and Section 7 (Build Phases)** as part of each PR that completes work.
- **Commit Vivado files:** `.bd` (exported), `.tcl`, `.xci` (IP). **Do not commit:** `.runs/`, `.cache/`, `.sim/`, build artifacts.

---

## 11. Driver API Reference (vga_gpu.h)

```c
// Set background color
GPU_SetBackgroundColor(r, g, b);  // r, g, b are 4-bit values (0–15)

// Position a sprite
GPU_SetPosition(slot, x, y);      // slot: 0–24, x: 0–639, y: 0–479

// Size a sprite
GPU_SetDimensions(slot, w, h);    // w, h: 0–1023

// Configure shape + enable + color
GPU_ConfigureConfig(slot, shape, enable, rgb12);
// shape: SHAPE_RECTANGLE (0) or SHAPE_CIRCLE (1)

// All-in-one sprite setup
GPU_ConfigureObject(slot, x, y, w, h, shape, r, g, b, enable);

// Text overlay
GPU_TextPutChar(row, col, 'A', rgb12);        // row: 0–29, col: 0–79
GPU_TextPutString(row, col, "Hello", rgb12);
GPU_TextClear();
```

---

## 12. Glossary

| Term | Meaning |
|---|---|
| Slot | One object's index in the register file, 0 to 24 (default) |
| Hit / Active | Signal indicating the current scanned pixel belongs to a given object |
| Compositor | Logic that resolves multiple simultaneous "hits" into one final pixel color |
| Z-order | Priority convention for which object wins when shapes overlap (lower slot = higher priority) |
| RGB444 | 12-bit color format, 4 bits each for red/green/blue |
| BRAM | Block RAM (on-chip memory), used for font storage in `text_overlay` |

---

## Quick Start / Contributor Notes

1. **Review Section 7 (Build Phases)** to understand current progress.
2. **All modules must follow Section 6 (Coding Conventions).**
3. **Run testbenches in `TB/`** before integrating changes.
4. **Update this README** and commit as part of your PR.
5. **Treat this file as the authoritative contract** for interfaces, data formats, and workflow.

---

## 13. Performance & Hardware Resource Notes

- **Default configuration:** `NUM_OBJECTS = 25`, 640×480@60Hz, 100 MHz system clock.
- **LUT usage:** Approximately 20–30% of XC7A35T for full design (varies with shape complexity).
- **BRAM usage:** ~4 blocks (one for font ROM in `text_overlay`, one for BRAM controller in Block Design).
- **Timing closure:** Runs comfortably at 100 MHz; pixel-clock-domain logic (vga_controller) is pipelined to run at 25 MHz effective rate.

---

*Maintainers: update this document whenever a phase changes an interface, register map, or convention. Treat drift between this file and the code as a bug.*

