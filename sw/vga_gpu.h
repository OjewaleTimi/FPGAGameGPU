#ifndef VGA_GPU_H
#define VGA_GPU_H

#include <stdint.h>
#include <stdbool.h>
#include "xparameters.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * 1. BASE ADDRESS RESOLUTION
 * ========================================================================= */
#if defined(XPAR_MY_GPU_IP_0_S00_AXI_BASEADDR)
  #define GPU_BASEADDR                XPAR_MY_GPU_IP_0_S00_AXI_BASEADDR
#elif defined(XPAR_MY_GPU_IP_V1_0_0_BASEADDR)
  #define GPU_BASEADDR                XPAR_MY_GPU_IP_V1_0_0_BASEADDR
#elif defined(XPAR_GPU_TOP_0_S00_AXI_BASEADDR)
  #define GPU_BASEADDR                XPAR_GPU_TOP_0_S00_AXI_BASEADDR
#elif defined(XPAR_TEENYTINYGPU_0_BASEADDR)
  #define GPU_BASEADDR                XPAR_TEENYTINYGPU_0_BASEADDR
#else
  #define GPU_BASEADDR                0x44A00000U
#endif

#define GPU_BG_COLOR_REG_OFFSET       0x0FF0U
#define GPU_MAX_OBJECTS               25U  /* matches NUM_OBJECTS in vga_top.sv (was 8 -- stale) */

/* =========================================================================
 * 2. STANDALONE MMIO HARDWARE ACCESSORS
 * ========================================================================= */
#define Xil_Out32(addr, val) \
    (*((volatile uint32_t *)(uintptr_t)(addr)) = (uint32_t)(val))

#define Xil_In32(addr) \
    (*((volatile uint32_t *)(uintptr_t)(addr)))

/* =========================================================================
 * 3. HARDWARE REGISTER OFFSETS (From register_system.sv)
 * ========================================================================= */
#define OBJ_REG_X              0x00U  /* write_data[9:0] = x */
#define OBJ_REG_Y              0x04U  /* write_data[9:0] = y */
#define OBJ_REG_DIM            0x08U  /* [25:16] = width (or radius), [9:0] = height */
#define OBJ_REG_CFG            0x0CU  /* [14:13] = shape, [12] = enable, [11:0] = RGB */

#define SHAPE_RECTANGLE        0x0U   /* 2'b00 */
#define SHAPE_CIRCLE           0x1U   /* 2'b01 */

/* =========================================================================
 * 4. BUS MAPPING & PACKING MACROS
 * ========================================================================= */
#define GPU_MAKE_ADDR(slot, reg_off) \
    (((uint32_t)((slot) & 0x0FFFU) << 4) | ((uint32_t)(reg_off) & 0x0FU))

#define GPU_WRITE_SLOT(slot, reg_off, val) \
    Xil_Out32((uintptr_t)(GPU_BASEADDR) + (uintptr_t)GPU_MAKE_ADDR((slot), (reg_off)), (uint32_t)(val))

#define GPU_READ_SLOT(slot, reg_off) \
    Xil_In32((uintptr_t)(GPU_BASEADDR) + (uintptr_t)GPU_MAKE_ADDR((slot), (reg_off)))

#define PACK_RGB12(r, g, b) \
    ((((uint32_t)(r) & 0x0FU) << 8) | \
     (((uint32_t)(g) & 0x0FU) << 4) | \
     (((uint32_t)(b) & 0x0FU) << 0))

#define PACK_DIM(w, h) \
    ((((uint32_t)(w) & 0x3FFU) << 16) | \
     (((uint32_t)(h) & 0x3FFU) << 0))

#define PACK_CFG(shape, enable, color12) \
    ((((uint32_t)(shape)   & 0x03U) << 13) | \
     (((uint32_t)(enable ? 1 : 0) & 0x01U) << 12) | \
     (((uint32_t)(color12) & 0x0FFFU) << 0))

/* =========================================================================
 * 5. DRIVER FUNCTIONS
 * ========================================================================= */
static inline __attribute__((unused)) void GPU_SetBackgroundColor(uint8_t r, uint8_t g, uint8_t b) {
    Xil_Out32((uintptr_t)(GPU_BASEADDR) + (uintptr_t)GPU_BG_COLOR_REG_OFFSET, PACK_RGB12(r, g, b));
}

static inline __attribute__((unused)) void GPU_SetPosition(uint8_t slot, uint16_t x, uint16_t y) {
    GPU_WRITE_SLOT(slot, OBJ_REG_X, (uint32_t)(x & 0x3FF));
    GPU_WRITE_SLOT(slot, OBJ_REG_Y, (uint32_t)(y & 0x3FF));
}

static inline __attribute__((unused)) void GPU_SetDimensions(uint8_t slot, uint16_t w, uint16_t h) {
    GPU_WRITE_SLOT(slot, OBJ_REG_DIM, PACK_DIM(w, h));
}

static inline __attribute__((unused)) void GPU_ConfigureConfig(uint8_t slot, uint8_t shape, bool enable, uint16_t rgb12) {
    GPU_WRITE_SLOT(slot, OBJ_REG_CFG, PACK_CFG(shape, enable, rgb12));
}

static inline __attribute__((unused)) void GPU_ConfigureObject(uint8_t slot, 
                                                              uint16_t x, uint16_t y,
                                                              uint16_t w, uint16_t h,
                                                              uint8_t shape,
                                                              uint8_t r, uint8_t g, uint8_t b,
                                                              bool enable) {
    GPU_SetPosition(slot, x, y);
    GPU_SetDimensions(slot, w, h);
    GPU_ConfigureConfig(slot, shape, enable, PACK_RGB12(r, g, b));
}

/* =========================================================================
 * 6. TEXT OVERLAY (text_overlay.sv: 80x30 grid of 8x16 character cells)
 *
 * Shares the object bus but claims address bit 15 as its own region, so
 * it never collides with GPU_MAKE_ADDR()'s slot addressing above (that
 * stays well under 0x1000 for any realistic GPU_MAX_OBJECTS). Every text
 * write is a full 32-bit word (ascii + color), so -- unlike a
 * read-modify-write on an object's config register -- there's no need to
 * read anything back here; the AXI slave's read path only returns real
 * data for GPU_BG_COLOR_REG_OFFSET anyway, so text state can't be read
 * back from hardware even if you wanted to.
 * ========================================================================= */
#define GPU_TEXT_BASE_OFFSET   0x8000U
#define GPU_TEXT_COLS          80U
#define GPU_TEXT_ROWS          30U

#define GPU_TEXT_CELL_ADDR(row, col) \
    ((uintptr_t)(GPU_BASEADDR) + (uintptr_t)GPU_TEXT_BASE_OFFSET + \
     (((uintptr_t)(row) * GPU_TEXT_COLS + (uintptr_t)(col)) << 2))

#define PACK_TEXT_CELL(ascii, rgb12) \
    ((((uint32_t)(rgb12)  & 0x0FFFU) << 8) | \
     (((uint32_t)(ascii)  & 0x00FFU) << 0))

static inline __attribute__((unused)) void GPU_TextPutChar(uint8_t row, uint8_t col, char ch, uint16_t rgb12) {
    if (row >= GPU_TEXT_ROWS || col >= GPU_TEXT_COLS) return;
    Xil_Out32(GPU_TEXT_CELL_ADDR(row, col), PACK_TEXT_CELL((uint8_t)ch, rgb12));
}

static inline __attribute__((unused)) void GPU_TextPutString(uint8_t row, uint8_t col, const char *str, uint16_t rgb12) {
    uint16_t c = col;
    while (*str && c < GPU_TEXT_COLS) {
        GPU_TextPutChar(row, (uint8_t)c, *str, rgb12);
        str++;
        c++;
    }
}

static inline __attribute__((unused)) void GPU_TextClear(void) {
    for (uint32_t r = 0; r < GPU_TEXT_ROWS; r++) {
        for (uint32_t c = 0; c < GPU_TEXT_COLS; c++) {
            Xil_Out32(GPU_TEXT_CELL_ADDR(r, c), PACK_TEXT_CELL((uint32_t)' ', 0));
        }
    }
}

#ifdef __cplusplus
}
#endif

#endif /* VGA_GPU_H */
