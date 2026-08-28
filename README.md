# FPGA VGA Sprite Engine — Project Documentation

**Target board:** Digilent Basys 3 (Xilinx Artix-7, XC7A35T)
**Display:** VGA, 640×480 @ 60Hz
**HDL:** SystemVerilog (RTL) + C (MicroBlaze application)
**Toolchain:** Xilinx Vivado (Block Design + IP Packager)

---

## 1. Project Overview

This project implements a **hardware-accelerated 2D sprite/object rendering engine** on an FPGA, controllable at runtime by a soft-core CPU (MicroBlaze). Instead of a CPU writing pixels into a framebuffer, the CPU writes small **object descriptors** (position, size, color, shape, enable) into a memory-mapped register file. Dedicated parallel hardware then redraws every object, every pixel, every frame, continuously and independently of CPU speed.

This is the same architectural idea used in classic sprite-based arcade and console hardware: software decides *what* should be on screen; hardware decides *how it gets drawn*, every 25 MHz pixel clock tick, with zero per-pixel CPU involvement.

### 1.1 Why this architecture

| Concern | How this design solves it |
|---|---|
| Frame rate stability | VGA controller scans at a fixed, independent rate. CPU speed cannot cause stutter — it can only cause stale positions. |
| CPU workload | Only a few bytes are written per object per game-logic tick, not a full framebuffer. |
| Scaling object count | Costs FPGA fabric (LUTs/FFs) via a generate loop, not CPU cycles — all objects are evaluated in parallel every pixel. |
| Reusability | Shape hit-test logic (`primitive_object`) is written once and instantiated N times. |

### 1.2 High-level data flow

```
[Game logic in C, running on MicroBlaze]
        |
        | AXI4-Lite writes (object descriptors)
        v
[AXI4-Lite Wrapper]  <-- Vivado-generated, houses our register logic
        |
        v
[object_array]  --- generate loop, N instances ---
        |
        +--> [Object Register File slot 0] --> [primitive_object 0] --> hit0, color0
        +--> [Object Register File slot 1] --> [primitive_object 1] --> hit1, color1
        +--> ...                                                        ...
        +--> [Object Register File slot N-1] --> [primitive_object N-1] --> hitN-1, colorN-1
        |
        v
[Priority Mux / Compositor]  <-- picks winning object per pixel, else background
        |
        v
[VGA-Out Top Wrapper]  <-- combines compositor RGB with VGA controller timing
        |
        v
[VGA Controller]  (existing, unchanged) --> hsync, vsync, pixel_x, pixel_y
        |
        v
   Physical VGA port (RGB + sync)
```

---

## 2. Repository Structure

```
repo-root/
├── README.md                      <- this file
├── docs/
│   ├── register_map.md            <- authoritative AXI address map (Section 5)
│   └── phase_log.md               <- running log of completed phases (Section 7)
├── rtl/
│   ├── vga_controller.sv          <- existing, unchanged
│   ├── primitive_object.sv
│   ├── object_reg_slot.sv
│   ├── object_array.sv
│   ├── compositor.sv
│   ├── vga_top.sv
│   └── axi_object_array_wrapper/  <- Vivado-generated IP, do not hand-edit outside user-logic section
├── sim/
│   ├── tb_primitive_object.sv
│   ├── tb_object_array.sv
│   └── tb_vga_top.sv
├── sw/
│   └── microblaze_app/
│       └── main.c
├── constraints/
│   └── basys3_vga.xdc
└── vivado/
    └── block_design/              <- exported .bd / .tcl for reproducible builds
```

**Rule:** one module per file, filename == module name.

---

## 3. Module Inventory

| Module | Hand-written RTL? | Status | Owner |
|---|---|---|---|
| `vga_controller` | Yes (already built) | ✅ Done | — |
| `primitive_object` | Yes | ⬜ Phase 1–2 | — |
| `object_reg_slot` | Yes | ⬜ Phase 4 | — |
| `object_array` | Yes | ⬜ Phase 5–6 | — |
| `compositor` | Yes | ⬜ Phase 3 (manual), Phase 5 (generalized) | — |
| `vga_top` | Yes | ⬜ Phase 1 (wire-through), grows each phase | — |
| AXI4-Lite wrapper | Vivado-generated, we fill user-logic | ⬜ Phase 7 | — |
| MicroBlaze + interconnect + BRAM + clocking wizard | Vivado Block Design GUI | ⬜ Phase 8 | — |

Update the Status column as your team completes each phase. This table is the fastest way for anyone joining the project to see where things stand.

---

## 4. Module Specifications

### 4.1 `primitive_object`

**Purpose:** Given the current scan position and one object's parameters, determine whether the current pixel belongs to this object and what color it should be.

**Parameters:**
| Name | Default | Description |
|---|---|---|
| `PIXEL_X_WIDTH` | 10 | bits needed for 0–639 |
| `PIXEL_Y_WIDTH` | 9 | bits needed for 0–479 |
| `COLOR_WIDTH` | 12 | RGB444 packed color |

**Ports:**
| Name | Dir | Width | Description |
|---|---|---|---|
| `clk` | in | 1 | pixel clock |
| `pixel_x` | in | `PIXEL_X_WIDTH` | current scan column |
| `pixel_y` | in | `PIXEL_Y_WIDTH` | current scan row |
| `obj_x`, `obj_y` | in | 10/9 | object origin |
| `obj_w`, `obj_h` | in | 10/9 | width/height (or radius for circle, `obj_w` reused) |
| `obj_color` | in | `COLOR_WIDTH` | RGB444 |
| `shape_type` | in | 2 | `00`=rect, `01`=circle, `10`=triangle |
| `enable` | in | 1 | object active flag |
| `hit` | out | 1 | 1 if current pixel belongs to this object |
| `color_out` | out | `COLOR_WIDTH` | valid only when `hit` is high |

**Behavior (combinational, `always_comb`):**
- `enable == 0` → `hit = 0` unconditionally.
- `shape_type == 2'b00` (rect): `hit = (pixel_x >= obj_x) && (pixel_x < obj_x + obj_w) && (pixel_y >= obj_y) && (pixel_y < obj_y + obj_h)`
- `shape_type == 2'b01` (circle): center = `(obj_x, obj_y)`, radius = `obj_w`. `hit = (dx*dx + dy*dy) <= (obj_w*obj_w)` where `dx = pixel_x - obj_x`, `dy = pixel_y - obj_y`.
- `shape_type == 2'b10` (triangle): edge-function test against three vertices derived from `obj_x, obj_y, obj_w, obj_h` (define exact vertex convention in code comments before implementing — this is a team decision point).

**Design note:** keep this module purely combinational. No clocked state inside — it should behave like a pure function of its inputs, re-evaluated every pixel clock.

---

### 4.2 `object_reg_slot`

**Purpose:** Storage only — holds one object's descriptor fields as flip-flops, written by the address-decode logic in `object_array`.

**Ports:**
| Name | Dir | Description |
|---|---|---|
| `clk`, `rst_n` | in | standard |
| `wr_en` | in | write strobe, asserted only when this slot is addressed |
| `wr_word_sel` | in | which packed word is being written (see Section 5) |
| `wr_data` | in | 32-bit write data |
| `obj_x, obj_y, obj_w, obj_h, obj_color, shape_type, enable` | out | unpacked fields for `primitive_object` |

No combinational logic beyond field unpacking. This module intentionally does nothing "smart."

---

### 4.3 `object_array`

**Purpose:** Instantiate N × (`object_reg_slot` + `primitive_object`) via `generate`, and decode a simple `[addr, wdata, we]` bus into per-slot writes.

**Parameters:**
| Name | Default | Description |
|---|---|---|
| `N_OBJECTS` | 8 | number of sprite slots |
| `ADDR_WIDTH` | `$clog2(N_OBJECTS*8)` | derived from register map, Section 5 |

**Ports (pre-AXI, Phase 6 form):**
| Name | Dir | Description |
|---|---|---|
| `clk`, `rst_n` | in | standard |
| `addr` | in | byte address, decoded to slot + word |
| `wdata` | in | 32-bit write data |
| `we` | in | write enable |
| `pixel_x`, `pixel_y` | in | from VGA controller |
| `hit[N_OBJECTS-1:0]` | out | one hit bit per object, to compositor |
| `color[N_OBJECTS-1:0]` | out | one color bus per object, to compositor |

**Address decode logic:** `slot_index = addr[ADDR_WIDTH-1:3]`, `word_sel = addr[2]` (given 2 words = 8 bytes per object; adjust if register map changes). Route `we` to the selected slot's `wr_en` only.

---

### 4.4 `compositor`

**Purpose:** Resolve overlapping objects into a single RGB output per pixel.

**Ports:**
| Name | Dir | Description |
|---|---|---|
| `hit[N_OBJECTS-1:0]` | in | from `object_array` |
| `color[N_OBJECTS-1:0]` | in | from `object_array` |
| `bg_color` | in | background RGB444 |
| `rgb_out` | out | final RGB444 for this pixel |

**Priority rule:** lowest slot index wins (object 0 drawn "on top"). Document this explicitly in code — it is your z-order convention and must be consistent with how the team assigns slot numbers to game entities (e.g., convention: slot 0 = player, higher slots = background elements, or vice versa — **team decision, record it here once made**).

---

### 4.5 `vga_top`

**Purpose:** Wire the compositor's RGB output together with the existing `vga_controller`'s timing signals (`hsync`, `vsync`, `pixel_x`, `pixel_y`) to drive the physical VGA port.

No new logic beyond signal routing — this module should stay thin.

---

## 5. AXI4-Lite Register Map

Each object occupies **2 words (8 bytes)**. Base address of slot `i` = `i * 8`.

### Word 0 — Position / Control (offset +0x0)
| Bits | Field | Description |
|---|---|---|
| `[9:0]` | `x` | object x-origin, 0–639 |
| `[18:10]` | `y` | object y-origin, 0–479 |
| `[19]` | `enable` | 1 = object active |
| `[21:20]` | `shape_type` | `00`=rect, `01`=circle, `10`=triangle |
| `[31:22]` | reserved | write 0 |

### Word 1 — Size / Color (offset +0x4)
| Bits | Field | Description |
|---|---|---|
| `[9:0]` | `w` | width (rect/triangle) or radius (circle) |
| `[18:10]` | `h` | height (unused for circle) |
| `[30:19]` | `color` | RGB444: `[30:27]`=R, `[26:23]`=G, `[22:19]`=B |
| `[31]` | reserved | write 0 |

**Total address space:** `N_OBJECTS * 8` bytes. For `N_OBJECTS = 8`, that's 64 bytes → 6-bit address decode.

> ⚠️ **Status: draft, not final.** Confirm this layout before Phase 7 (AXI wrapping) — changing it afterward means re-generating the packaged IP. Record any changes in `docs/register_map.md` with a revision note.

---

## 6. Coding Conventions (SystemVerilog)

- **File header** (top of every `.sv` file):
  ```systemverilog
  // ============================================================
  // Module: <module_name>
  // Purpose: <one line>
  // Author: <name>        Date: <YYYY-MM-DD>
  // ============================================================
  ```
- Use `` `default_nettype none `` at the top of every file to catch typos in signal names.
- Combinational logic: `always_comb`, never `always @(*)`.
- Sequential logic: `always_ff @(posedge clk)`, with **explicit synchronous or asynchronous reset convention chosen once for the whole project** and documented here (recommend: synchronous, active-low `rst_n`, for BRAM/MicroBlaze compatibility on Artix-7).
- No inferred latches — every `always_comb` block must assign all outputs on every path.
- Use `typedef struct packed` for object descriptor fields where it improves readability, but keep AXI-facing widths raw `logic [31:0]` to match the register map exactly.
- Parameterize widths (`PIXEL_X_WIDTH`, `N_OBJECTS`, etc.) — never hardcode `10'd639` deep inside logic; derive from parameters.
- One clock domain for the entire pixel pipeline (pixel clock). MicroBlaze/AXI side runs on system clock — **cross this boundary only at the AXI wrapper**, not inside `object_array`. Flag this explicitly during Phase 7 design review.

---

## 7. Build Phases

Track progress here or in `docs/phase_log.md`. Each phase changes exactly one variable from the previous — if something breaks, you know which layer to look in.

- [ ] **Phase 1 — Single hardcoded shape.** Write `primitive_object` with hardcoded rect parameters, wire directly into existing `vga_controller` output. No `object_array`, no compositor. Goal: prove the hit-test-to-VGA-output concept in simulation.
- [ ] **Phase 2 — Add shape_type.** Extend `primitive_object` to support circle and triangle via `case(shape_type)`. Still one hardcoded instance.
- [ ] **Phase 3 — Two hardcoded instances + manual mux.** Instantiate `primitive_object` twice, write compositor logic by hand (`if/else` chain) to prove overlap/priority handling.
- [ ] **Phase 4 — Registers instead of hardcoded params.** Introduce `object_reg_slot`; drive it from a testbench (not AXI) to confirm objects can change position mid-simulation.
- [ ] **Phase 5 — Generalize to N via generate loop.** Write `object_array` combining N × (`object_reg_slot` + `primitive_object`), and generalize the compositor's priority mux to loop over N.
- [ ] **Phase 6 — Address decode (no AXI yet).** Add a plain `[addr, wdata, we]` bus driven from testbench, routed to the correct slot. Test thoroughly — off-by-one errors here are the most common bug class in this project.
- [ ] **Phase 7 — AXI4-Lite wrapping.** Use Vivado's "Create and Package IP" wizard; drop Phase 6's decode logic into the generated slave template's user-logic section.
- [ ] **Phase 8 — MicroBlaze integration.** Wire MicroBlaze + AXI interconnect + BRAM + clocking wizard in the Block Design GUI. Write a minimal C program that writes one register and confirms an object appears/moves on real hardware.
- [ ] **Phase 9 — Game logic on MicroBlaze.** Input handling, collision detection (using CPU-side mirror of object x/y/w/h — no pixel readback needed), scoring, game state machine.
- [ ] **Phase 10 — Polish.** Tune `N_OBJECTS`, timing closure on circle/triangle math at target `N`, finalize z-order convention, write user-facing game instructions.

---

## 8. Simulation Strategy

- Every hand-written module gets its own testbench in `sim/`, before it is ever integrated into a larger block.
- `tb_primitive_object.sv`: sweep `pixel_x`/`pixel_y` across the full frame for one hardcoded object per shape type; assert `hit` matches expected geometry at boundary pixels (off-by-one edges are the most likely bug).
- `tb_object_array.sv`: drive the `[addr, wdata, we]` bus with writes to every slot and every word, confirm each slot's unpacked fields update correctly and no cross-slot bleed occurs.
- `tb_vga_top.sv`: full-frame simulation with multiple objects; dump to `.vcd` and visually inspect via GTKWave or Vivado's waveform viewer before ever touching real hardware.
- **Hardware bring-up order:** get Phase 1–6 fully proven in simulation before generating the AXI IP. If nothing appears on real hardware after Phase 8, the bug is almost certainly in AXI/interconnect wiring, not rendering logic — because rendering logic is already proven.

---

## 9. Vivado Build Notes (Basys 3 specific)

- Board clock: 100 MHz (`W5` pin, per Basys 3 master XDC).
- Pixel clock for 640×480@60Hz VGA: 25.175 MHz (25 MHz is an acceptable approximation) — generate via Clocking Wizard IP, not a manual counter, once MicroBlaze/AXI is in the design (keeps timing closure clean).
- Use the standard Digilent Basys 3 master XDC as your base constraints file; uncomment VGA port pins (`JA`/dedicated VGA header — confirm against your specific `vga_controller`'s existing constraints file if one already exists, and reuse it unchanged).
- MicroBlaze + AXI interconnect + BRAM controller + clocking wizard: added via **Block Design GUI** (`Create Block Design` → `Add IP`), not hand-instantiated. Export block design as Tcl (`File > Export > Export Block Design`) and commit the `.tcl` to `vivado/block_design/` so the design is reproducible by teammates.

---

## 10. Git Workflow

- Branch per phase or per module (e.g., `phase3-compositor`, `feat/primitive-object-triangle`).
- No direct commits to `main` — PR + at least one teammate review, since RTL bugs are expensive to trace once integrated.
- Update Section 3 (Module Inventory) status column and `docs/phase_log.md` as part of the PR that completes a phase, not as an afterthought.
- Commit Vivado-generated files (`.bd`, exported `.tcl`, `.xci` for IP) — but **not** Vivado's build artifacts (`.runs/`, `.cache/`, `.sim/`) — add these to `.gitignore`.

---

## 11. Glossary

| Term | Meaning |
|---|---|
| Slot | One object's index in the register file, 0 to N_OBJECTS-1 |
| Hit | Signal indicating the current scanned pixel belongs to a given object |
| Compositor | Logic that resolves multiple simultaneous "hits" into one final pixel color |
| Z-order | The priority convention for which object wins when shapes overlap |
| RGB444 | 12-bit color format, 4 bits each for red/green/blue |

---

*Maintainers: update this document as the authoritative source whenever a phase changes an interface, address map, or convention. Treat drift between this file and the code as a bug.*
