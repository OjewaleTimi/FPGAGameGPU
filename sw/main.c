#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* ==========================================================================
 * BASYS 3 ARCADE - LITE (7 games)
 * Snake, Air Hockey, Pac-Man, Pong, Breakout, Tetris, Frogger
 * UART logging removed to save code size; the on-screen UI covers it.
 * ========================================================================== */

/* ==========================================================================
 * HARDWARE BASE ADDRESSES & REGISTER INTERFACE
 * ========================================================================== */
#define GPU_BASEADDR        ((uintptr_t)0x44A00000U)
#define UART_BASEADDR       ((uintptr_t)0x40600000U)
#define GPU_BG_COLOR_OFFSET ((uintptr_t)0x0FF0U)
#define GPU_NUM_SLOTS       25

#define OBJ_X(i)    (*(volatile uint32_t *)(GPU_BASEADDR + ((uintptr_t)(i) << 4) + 0x00U))
#define OBJ_Y(i)    (*(volatile uint32_t *)(GPU_BASEADDR + ((uintptr_t)(i) << 4) + 0x04U))
#define OBJ_DIM(i)  (*(volatile uint32_t *)(GPU_BASEADDR + ((uintptr_t)(i) << 4) + 0x08U))
#define OBJ_CFG(i)  (*(volatile uint32_t *)(GPU_BASEADDR + ((uintptr_t)(i) << 4) + 0x0CU))

#define UART_RX_FIFO        (*(volatile uint32_t *)(UART_BASEADDR + 0x00U))
#define UART_STATUS_REG     (*(volatile uint32_t *)(UART_BASEADDR + 0x08U))
#define UART_RX_VALID       0x01U

#define SHAPE_RECTANGLE     0U
#define SHAPE_CIRCLE        1U

#define PACK_DIM(w, h)      ((((uint32_t)(w) & 0x3FFU) << 16) | ((uint32_t)(h) & 0x3FFU))
#define PACK_CFG(r, g, b, shape, en) \
    ((((uint32_t)(shape) & 0x03U) << 13) | \
     ((en ? 1U : 0U) << 12) | \
     (((uint32_t)(r) & 0x0FU) << 8) | \
     (((uint32_t)(g) & 0x0FU) << 4) | \
     (((uint32_t)(b) & 0x0FU) << 0))

#define SCREEN_W            640
#define SCREEN_H            480

/* ==========================================================================
 * ON-SCREEN TEXT OVERLAY (80x30 grid of 8x16 cells, CP437 glyphs)
 * ========================================================================== */
#define GPU_TEXT_BASE_OFFSET   ((uintptr_t)0x8000U)
#define GPU_TEXT_COLS          80
#define GPU_TEXT_ROWS          30

#define TEXT_CELL(row, col) \
    (*(volatile uint32_t *)(GPU_BASEADDR + GPU_TEXT_BASE_OFFSET + \
        (((uintptr_t)(row) * GPU_TEXT_COLS + (uintptr_t)(col)) << 2)))

#define PACK_RGB12(r, g, b) \
    ((((uint32_t)(r) & 0x0FU) << 8) | (((uint32_t)(g) & 0x0FU) << 4) | ((uint32_t)(b) & 0x0FU))

#define PACK_TEXT(ascii, r, g, b) \
    ((PACK_RGB12((r), (g), (b)) << 8) | ((uint32_t)(ascii) & 0xFFU))

#define C_WHITE    15, 15, 15
#define C_GRAY      8,  8,  8
#define C_RED      15,  2,  2
#define C_GREEN     2, 15,  2
#define C_BLUE      5,  9, 15
#define C_YELLOW   15, 15,  0
#define C_CYAN      0, 15, 15
#define C_MAGENTA  15,  4, 15

#define CH_BOX_TL  ((char)0xC9)
#define CH_BOX_TR  ((char)0xBB)
#define CH_BOX_BL  ((char)0xC8)
#define CH_BOX_BR  ((char)0xBC)
#define CH_BOX_H   ((char)0xCD)
#define CH_BOX_V   ((char)0xBA)
#define CH_LINE_H  ((char)0xC4)

static int text_len(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static void gpu_text_putc(int row, int col, char ch, uint8_t r, uint8_t g, uint8_t b) {
    if (row < 0 || row >= GPU_TEXT_ROWS || col < 0 || col >= GPU_TEXT_COLS) return;
    TEXT_CELL(row, col) = PACK_TEXT(ch, r, g, b);
}

static int gpu_text_puts(int row, int col, const char *s, uint8_t r, uint8_t g, uint8_t b) {
    int c = col;
    while (*s && c < GPU_TEXT_COLS) {
        gpu_text_putc(row, c, *s, r, g, b);
        s++;
        c++;
    }
    return c;
}

static void gpu_text_fill(int row, int col, int count, char ch, uint8_t r, uint8_t g, uint8_t b) {
    for (int i = 0; i < count; i++) gpu_text_putc(row, col + i, ch, r, g, b);
}

static void gpu_text_clear_rect(int row, int col, int rows, int cols) {
    for (int y = 0; y < rows; y++) gpu_text_fill(row + y, col, cols, ' ', 0, 0, 0);
}

static void gpu_text_clear(void) {
    for (int r = 0; r < GPU_TEXT_ROWS; r++) {
        for (int c = 0; c < GPU_TEXT_COLS; c++) {
            TEXT_CELL(r, c) = PACK_TEXT(' ', 0, 0, 0);
        }
    }
}

static int gpu_text_center(int row, const char *s, uint8_t r, uint8_t g, uint8_t b) {
    return gpu_text_puts(row, (GPU_TEXT_COLS - text_len(s)) / 2, s, r, g, b);
}

static void gpu_text_center_clear(int row, const char *s) {
    gpu_text_fill(row, (GPU_TEXT_COLS - text_len(s)) / 2, text_len(s), ' ', 0, 0, 0);
}

static int gpu_text_put_uint_pad(int row, int col, uint32_t v, int width,
                                 uint8_t r, uint8_t g, uint8_t b) {
    char buf[10];
    if (width > 10) width = 10;
    for (int i = width - 1; i >= 0; i--) { buf[i] = (char)('0' + (v % 10)); v /= 10; }
    for (int i = 0; i < width; i++) gpu_text_putc(row, col + i, buf[i], r, g, b);
    return col + width;
}

static void gpu_text_box(int row, int col, int rows, int cols, uint8_t r, uint8_t g, uint8_t b) {
    gpu_text_putc(row, col, CH_BOX_TL, r, g, b);
    gpu_text_fill(row, col + 1, cols - 2, CH_BOX_H, r, g, b);
    gpu_text_putc(row, col + cols - 1, CH_BOX_TR, r, g, b);
    for (int y = 1; y < rows - 1; y++) {
        gpu_text_putc(row + y, col, CH_BOX_V, r, g, b);
        gpu_text_putc(row + y, col + cols - 1, CH_BOX_V, r, g, b);
    }
    gpu_text_putc(row + rows - 1, col, CH_BOX_BL, r, g, b);
    gpu_text_fill(row + rows - 1, col + 1, cols - 2, CH_BOX_H, r, g, b);
    gpu_text_putc(row + rows - 1, col + cols - 1, CH_BOX_BR, r, g, b);
}

/* ==========================================================================
 * SHARED SYSTEM UTILITIES
 * ========================================================================== */
static void delay_cycles(volatile uint32_t count) {
    while (count--) { __asm__ __volatile__("nop"); }
}

static void flush_uart_rx(void) {
    while (UART_STATUS_REG & UART_RX_VALID) {
        (void)UART_RX_FIFO;
    }
}

static void gpu_set_bg(uint32_t rgb444) {
    *(volatile uint32_t *)(GPU_BASEADDR + GPU_BG_COLOR_OFFSET) = rgb444;
}

static void gpu_reset_all(uint16_t bg_rgb444) {
    gpu_set_bg(bg_rgb444);
    for (int i = 0; i < GPU_NUM_SLOTS; i++) {
        OBJ_CFG(i) = 0x00000000U;
    }
    gpu_text_clear();
}

static void flash_bg(uint32_t flash, uint32_t normal, int times, uint32_t delay) {
    for (int i = 0; i < times; i++) {
        gpu_set_bg(flash);
        delay_cycles(delay);
        gpu_set_bg(normal);
        delay_cycles(delay);
    }
}

static uint32_t rng_state = 0x1337CAFEU;
static uint32_t rng_next(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

static int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static bool aabb_overlap(int ax, int ay, int aw, int ah,
                         int bx, int by, int bw, int bh) {
    return (ax < bx + bw) && (ax + aw > bx) &&
           (ay < by + bh) && (ay + ah > by);
}

/* ==========================================================================
 * ARCADE UI KIT: key input, blinking prompts, panels, READY banner
 * ========================================================================== */
#define BLINK_POLL_DELAY    150000U
#define BLINK_HALF_PERIOD   40U

#define READY_ROW           17

/* Modal dialog panel uses shape slots 23 and 24 */
#define PANEL_ROW0          8
#define PANEL_COL0          22
#define PANEL_ROWS          14
#define PANEL_COLS          36
#define PANEL_SLOT_BORDER   23
#define PANEL_SLOT_FILL     24
#define PANEL_PX_X          (PANEL_COL0 * 8 - 8)
#define PANEL_PX_Y          (PANEL_ROW0 * 16 - 8)
#define PANEL_PX_W          (PANEL_COLS * 8 + 16)
#define PANEL_PX_H          (PANEL_ROWS * 16 + 16)

static char to_upper(char c) {
    return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
}

static char wait_key_blink(int row, const char *msg, uint8_t r, uint8_t g, uint8_t b) {
    int len = msg ? text_len(msg) : 0;
    int col = (GPU_TEXT_COLS - len) / 2;
    uint32_t polls = 0;
    bool on = false;

    flush_uart_rx();
    while (1) {
        if (msg && (polls % BLINK_HALF_PERIOD) == 0) {
            on = !on;
            if (on) gpu_text_puts(row, col, msg, r, g, b);
            else    gpu_text_fill(row, col, len, ' ', 0, 0, 0);
        }
        if (UART_STATUS_REG & UART_RX_VALID) {
            return to_upper((char)(UART_RX_FIFO & 0xFFU));
        }
        polls++;
        delay_cycles(BLINK_POLL_DELAY);
    }
}

static void panel_show(void) {
    OBJ_X(PANEL_SLOT_BORDER)   = PANEL_PX_X;
    OBJ_Y(PANEL_SLOT_BORDER)   = PANEL_PX_Y;
    OBJ_DIM(PANEL_SLOT_BORDER) = PACK_DIM(PANEL_PX_W, PANEL_PX_H);
    OBJ_CFG(PANEL_SLOT_BORDER) = PACK_CFG(0, 10, 15, SHAPE_RECTANGLE, true);

    OBJ_X(PANEL_SLOT_FILL)     = PANEL_PX_X + 4;
    OBJ_Y(PANEL_SLOT_FILL)     = PANEL_PX_Y + 4;
    OBJ_DIM(PANEL_SLOT_FILL)   = PACK_DIM(PANEL_PX_W - 8, PANEL_PX_H - 8);
    OBJ_CFG(PANEL_SLOT_FILL)   = PACK_CFG(0, 0, 4, SHAPE_RECTANGLE, true);

    gpu_text_clear_rect(PANEL_ROW0, PANEL_COL0, PANEL_ROWS, PANEL_COLS);
    gpu_text_box(PANEL_ROW0, PANEL_COL0, PANEL_ROWS, PANEL_COLS, C_CYAN);
}

static void panel_hide(void) {
    OBJ_CFG(PANEL_SLOT_BORDER) = 0x00000000U;
    OBJ_CFG(PANEL_SLOT_FILL)   = 0x00000000U;
    gpu_text_clear_rect(PANEL_ROW0, PANEL_COL0, PANEL_ROWS, PANEL_COLS);
}

static void ready_banner(const char *msg) {
    gpu_text_center(READY_ROW, msg, C_YELLOW);
    delay_cycles(10000000U);
    gpu_text_center_clear(READY_ROW, msg);
}

/* Returns true if the player chose to quit to the menu. */
static bool pause_screen(void) {
    char k;
    panel_show();
    gpu_text_center(12, "PAUSED", C_YELLOW);
    gpu_text_center(15, "P : RESUME", C_WHITE);
    gpu_text_center(17, "Q : MAIN MENU", C_WHITE);
    do {
        k = wait_key_blink(0, 0, 0, 0, 0);
    } while (k != 'P' && k != 'Q');
    panel_hide();
    return k == 'Q';
}

/* Shared GAME OVER dialog. Returns true = play again, false = menu. */
static bool game_over_screen(uint32_t score, uint32_t best, bool new_best,
                             const char *stat_label, uint32_t stat_value) {
    char k;
    panel_show();
    gpu_text_center(10, "G A M E   O V E R", C_RED);

    gpu_text_puts(12, 33, "SCORE", C_WHITE);
    gpu_text_put_uint_pad(12, 41, score, 6, C_YELLOW);
    gpu_text_puts(13, 33, "BEST", C_WHITE);
    gpu_text_put_uint_pad(13, 41, best, 6, C_CYAN);
    gpu_text_puts(14, 33, stat_label, C_WHITE);
    gpu_text_put_uint_pad(14, 41, stat_value, 6, C_GREEN);

    gpu_text_center(18, "R : PLAY AGAIN", C_WHITE);
    gpu_text_center(19, "Q : MAIN MENU", C_WHITE);

    do {
        k = wait_key_blink(16, new_best ? "** NEW HIGH SCORE **" : 0, C_YELLOW);
    } while (k != 'R' && k != 'Q');

    panel_hide();
    return k == 'R';
}

/* ==========================================================================
 * GAME 1: SNAKE
 * ========================================================================== */
#define SNAKE_GRID_SIZE     16
#define SNAKE_GRID_W        (SCREEN_W / SNAKE_GRID_SIZE)
#define SNAKE_GRID_H        (SCREEN_H / SNAKE_GRID_SIZE)
#define SNAKE_MAX_SEGS      64
#define SNAKE_FOOD_SLOT     0

typedef struct { int16_t x, y; } snake_pt_t;
typedef enum { SNAKE_FOOD_NORM = 0, SNAKE_FOOD_GOLD = 1 } snake_food_t;

static snake_pt_t   s_body[SNAKE_MAX_SEGS];
static int          s_len = 4;
static int          s_dx = 1, s_dy = 0;
static int          s_next_dx = 1, s_next_dy = 0;
static snake_pt_t   s_food;
static snake_food_t s_food_type = SNAKE_FOOD_NORM;
static uint32_t     s_food_count = 0;
static uint32_t     s_score = 0;
static uint32_t     s_high_score = 0;
static uint32_t     s_delay = 700000U;
static uint32_t     s_frame = 0;

static void snake_draw_hud(void) {
    gpu_text_puts(0, 1, "SCORE", C_WHITE);
    gpu_text_put_uint_pad(0, 7, s_score, 6, C_YELLOW);
    gpu_text_center(0, "SNAKE", C_GREEN);
    gpu_text_puts(0, 62, "HI", C_WHITE);
    gpu_text_put_uint_pad(0, 65, s_high_score, 6, C_CYAN);

    gpu_text_puts(29, 1, "LEN", C_WHITE);
    gpu_text_put_uint_pad(29, 5, (uint32_t)s_len, 3, C_GREEN);
    gpu_text_center(29, "P:PAUSE  Q:MENU", C_GRAY);
}

static void snake_spawn_food(void) {
    s_food_count++;
    s_food_type = ((s_food_count % 4) == 0) ? SNAKE_FOOD_GOLD : SNAKE_FOOD_NORM;
    bool on_body;
    do {
        on_body = false;
        s_food.x = (int16_t)((rng_next() % (SNAKE_GRID_W - 4)) + 2);
        s_food.y = (int16_t)((rng_next() % (SNAKE_GRID_H - 4)) + 2);
        for (int i = 0; i < s_len; i++) {
            if (s_body[i].x == s_food.x && s_body[i].y == s_food.y) {
                on_body = true;
                break;
            }
        }
    } while (on_body);
}

static void snake_render(void) {
    s_frame++;
    uint16_t radius = SNAKE_GRID_SIZE / 2;
    uint8_t fr = 15, fg = 1, fb = 1;

    if (s_food_type == SNAKE_FOOD_GOLD) {
        radius = (s_frame & 0x02) ? (radius + 1) : (radius - 1);
        fr = 15; fg = 14; fb = 0;
    }

    OBJ_X(SNAKE_FOOD_SLOT)   = (uint32_t)(s_food.x * SNAKE_GRID_SIZE + (SNAKE_GRID_SIZE / 2));
    OBJ_Y(SNAKE_FOOD_SLOT)   = (uint32_t)(s_food.y * SNAKE_GRID_SIZE + (SNAKE_GRID_SIZE / 2));
    OBJ_DIM(SNAKE_FOOD_SLOT) = PACK_DIM(radius, 0);
    OBJ_CFG(SNAKE_FOOD_SLOT) = PACK_CFG(fr, fg, fb, SHAPE_CIRCLE, true);

    OBJ_X(1)   = (uint32_t)(s_body[0].x * SNAKE_GRID_SIZE);
    OBJ_Y(1)   = (uint32_t)(s_body[0].y * SNAKE_GRID_SIZE);
    OBJ_DIM(1) = PACK_DIM(SNAKE_GRID_SIZE, SNAKE_GRID_SIZE);
    OBJ_CFG(1) = PACK_CFG(4, 15, 4, SHAPE_RECTANGLE, true);

    /* Chunk compression of the body into the remaining hardware slots */
    int slot = 2;
    int seg = 1;
    while (seg < s_len && slot < GPU_NUM_SLOTS) {
        int min_x = s_body[seg].x, max_x = s_body[seg].x;
        int min_y = s_body[seg].y, max_y = s_body[seg].y;
        int run = 1;

        while ((seg + run) < s_len) {
            int px = s_body[seg + run - 1].x, py = s_body[seg + run - 1].y;
            int cx = s_body[seg + run].x, cy = s_body[seg + run].y;

            if ((cx == px && (cy == py + 1 || cy == py - 1)) ||
                (cy == py && (cx == px + 1 || cx == px - 1))) {
                bool same_h = (min_y == max_y) && (cy == min_y);
                bool same_v = (min_x == max_x) && (cx == min_x);
                if (same_h || same_v) {
                    if (cx < min_x) min_x = cx;
                    if (cx > max_x) max_x = cx;
                    if (cy < min_y) min_y = cy;
                    if (cy > max_y) max_y = cy;
                    run++;
                    continue;
                }
            }
            break;
        }

        OBJ_X(slot)   = (uint32_t)(min_x * SNAKE_GRID_SIZE);
        OBJ_Y(slot)   = (uint32_t)(min_y * SNAKE_GRID_SIZE);
        OBJ_DIM(slot) = PACK_DIM((max_x - min_x + 1) * SNAKE_GRID_SIZE,
                                 (max_y - min_y + 1) * SNAKE_GRID_SIZE);
        OBJ_CFG(slot) = PACK_CFG(0, 12, 14, SHAPE_RECTANGLE, true);
        slot++;
        seg += run;
    }

    while (slot < GPU_NUM_SLOTS) {
        OBJ_CFG(slot++) = 0x00000000U;
    }
}

static bool snake_session(void) {
    gpu_reset_all(0x111U);
    s_len = 4;
    s_dx = 1; s_dy = 0;
    s_next_dx = 1; s_next_dy = 0;
    s_score = 0;
    s_delay = 700000U;
    uint32_t best_at_start = s_high_score;

    for (int i = 0; i < s_len; i++) {
        s_body[i].x = 15 - i;
        s_body[i].y = 15;
    }
    snake_spawn_food();
    snake_render();
    snake_draw_hud();

    ready_banner("READY!");
    flush_uart_rx();

    while (1) {
        while (UART_STATUS_REG & UART_RX_VALID) {
            char c = (char)(UART_RX_FIFO & 0xFFU);
            if (c == 'q' || c == 'Q') return false;
            if (c == 'p' || c == 'P') { if (pause_screen()) return false; continue; }
            if ((c == 'w' || c == 'W') && s_dy == 0) { s_next_dx =  0; s_next_dy = -1; }
            if ((c == 's' || c == 'S') && s_dy == 0) { s_next_dx =  0; s_next_dy =  1; }
            if ((c == 'a' || c == 'A') && s_dx == 0) { s_next_dx = -1; s_next_dy =  0; }
            if ((c == 'd' || c == 'D') && s_dx == 0) { s_next_dx =  1; s_next_dy =  0; }
        }

        s_dx = s_next_dx;
        s_dy = s_next_dy;

        snake_pt_t next_head = { (int16_t)(s_body[0].x + s_dx), (int16_t)(s_body[0].y + s_dy) };
        if (next_head.x >= SNAKE_GRID_W) next_head.x = 0;
        else if (next_head.x < 0)        next_head.x = SNAKE_GRID_W - 1;
        if (next_head.y >= SNAKE_GRID_H) next_head.y = 0;
        else if (next_head.y < 0)        next_head.y = SNAKE_GRID_H - 1;

        bool dead = false;
        if (s_len >= 5) {
            for (int i = 1; i < s_len - 1; i++) {
                if (s_body[i].x == next_head.x && s_body[i].y == next_head.y) {
                    dead = true;
                    break;
                }
            }
        }

        if (dead) {
            flash_bg(0x800U, 0x111U, 2, 2000000U);
            return game_over_screen(s_score, s_high_score, s_score > best_at_start,
                                    "LENGTH", (uint32_t)s_len);
        }

        bool ate = (next_head.x == s_food.x && next_head.y == s_food.y);
        for (int i = s_len - 1; i > 0; i--) {
            s_body[i] = s_body[i - 1];
        }
        s_body[0] = next_head;

        if (ate) {
            s_score += (s_food_type == SNAKE_FOOD_GOLD) ? 50 : 10;
            if (s_score > s_high_score) s_high_score = s_score;
            if (s_len < SNAKE_MAX_SEGS) {
                s_body[s_len] = s_body[s_len - 1];
                s_len++;
            }
            if (s_delay > 350000U) s_delay -= 20000U;
            snake_spawn_food();
            snake_draw_hud();
        }

        snake_render();
        delay_cycles(s_delay);
    }
}

static void play_snake(void) {
    while (snake_session()) { }
}

/* ==========================================================================
 * GAME 2: AIR HOCKEY (2 players)
 * ========================================================================== */
#define HOCKEY_GOAL_W       20
#define HOCKEY_PAD_W        16
#define HOCKEY_PAD_H        64
#define HOCKEY_PUCK_R       8
#define HOCKEY_MAX_SPD      12
#define HOCKEY_WIN_SCORE    7

#define OBJ_P1              0
#define OBJ_P2              1
#define OBJ_PUCK            2
#define OBJ_GOAL_L          3
#define OBJ_GOAL_R          4
#define OBJ_CENTER_LINE     5

typedef struct { int16_t x, y, vx, vy; } hk_actor_t;
static hk_actor_t hk_p1, hk_p2, hk_puck;
static uint32_t   hk_s1 = 0, hk_s2 = 0;
static uint32_t   hk_wins1 = 0, hk_wins2 = 0;

static void hockey_draw_hud(void) {
    gpu_text_puts(0, 33, "P1", C_GREEN);
    gpu_text_put_uint_pad(0, 36, hk_s1, 2, C_WHITE);
    gpu_text_puts(0, 39, "-", C_GRAY);
    gpu_text_put_uint_pad(0, 41, hk_s2, 2, C_WHITE);
    gpu_text_puts(0, 44, "P2", C_YELLOW);

    gpu_text_puts(29, 3, "P1: WASD", C_GREEN);
    gpu_text_center(29, "FIRST TO 7   P:PAUSE  Q:MENU", C_GRAY);
    gpu_text_puts(29, 68, "P2: IJKL", C_YELLOW);
}

static void hockey_draw_arena(void) {
    OBJ_X(OBJ_GOAL_L)   = 0;
    OBJ_Y(OBJ_GOAL_L)   = 0;
    OBJ_DIM(OBJ_GOAL_L) = PACK_DIM(HOCKEY_GOAL_W, SCREEN_H);
    OBJ_CFG(OBJ_GOAL_L) = PACK_CFG(0, 0, 15, SHAPE_RECTANGLE, true);

    OBJ_X(OBJ_GOAL_R)   = (uint32_t)(SCREEN_W - HOCKEY_GOAL_W);
    OBJ_Y(OBJ_GOAL_R)   = 0;
    OBJ_DIM(OBJ_GOAL_R) = PACK_DIM(HOCKEY_GOAL_W, SCREEN_H);
    OBJ_CFG(OBJ_GOAL_R) = PACK_CFG(15, 0, 0, SHAPE_RECTANGLE, true);

    OBJ_X(OBJ_CENTER_LINE)   = (uint32_t)(SCREEN_W / 2 - 2);
    OBJ_Y(OBJ_CENTER_LINE)   = 0;
    OBJ_DIM(OBJ_CENTER_LINE) = PACK_DIM(4, SCREEN_H);
    OBJ_CFG(OBJ_CENTER_LINE) = PACK_CFG(8, 8, 10, SHAPE_RECTANGLE, true);

    for (int i = 6; i < GPU_NUM_SLOTS; i++) {
        OBJ_CFG(i) = 0x00000000U;
    }
}

static void hockey_render_actors(void) {
    OBJ_X(OBJ_P1)   = (uint32_t)hk_p1.x;
    OBJ_Y(OBJ_P1)   = (uint32_t)hk_p1.y;
    OBJ_DIM(OBJ_P1) = PACK_DIM(HOCKEY_PAD_W, HOCKEY_PAD_H);
    OBJ_CFG(OBJ_P1) = PACK_CFG(0, 15, 0, SHAPE_RECTANGLE, true);

    OBJ_X(OBJ_P2)   = (uint32_t)hk_p2.x;
    OBJ_Y(OBJ_P2)   = (uint32_t)hk_p2.y;
    OBJ_DIM(OBJ_P2) = PACK_DIM(HOCKEY_PAD_W, HOCKEY_PAD_H);
    OBJ_CFG(OBJ_P2) = PACK_CFG(15, 15, 0, SHAPE_RECTANGLE, true);

    OBJ_X(OBJ_PUCK)   = (uint32_t)hk_puck.x;
    OBJ_Y(OBJ_PUCK)   = (uint32_t)hk_puck.y;
    OBJ_DIM(OBJ_PUCK) = PACK_DIM(HOCKEY_PUCK_R, 0);
    OBJ_CFG(OBJ_PUCK) = PACK_CFG(15, 15, 15, SHAPE_CIRCLE, true);
}

static bool match_over_screen(int winner) {
    char k;
    panel_show();
    gpu_text_center(10, "M A T C H   O V E R", C_RED);
    if (winner == 1) gpu_text_center(12, "PLAYER 1 WINS!", C_GREEN);
    else             gpu_text_center(12, "PLAYER 2 WINS!", C_YELLOW);

    gpu_text_puts(14, 33, "P1", C_GREEN);
    gpu_text_put_uint_pad(14, 36, hk_s1, 2, C_WHITE);
    gpu_text_puts(14, 39, "-", C_GRAY);
    gpu_text_put_uint_pad(14, 41, hk_s2, 2, C_WHITE);
    gpu_text_puts(14, 44, "P2", C_YELLOW);

    gpu_text_center(18, "R : REMATCH", C_WHITE);
    gpu_text_center(19, "Q : MAIN MENU", C_WHITE);

    do {
        k = wait_key_blink(16, "** GOOD GAME **", C_YELLOW);
    } while (k != 'R' && k != 'Q');

    panel_hide();
    return k == 'R';
}

static bool hockey_session(void) {
    gpu_reset_all(0x123U);
    hk_s1 = 0; hk_s2 = 0;

    hk_p1.x = HOCKEY_GOAL_W + 20;
    hk_p1.y = (SCREEN_H / 2) - (HOCKEY_PAD_H / 2);
    hk_p1.vx = 0; hk_p1.vy = 0;

    hk_p2.x = SCREEN_W - HOCKEY_GOAL_W - HOCKEY_PAD_W - 20;
    hk_p2.y = (SCREEN_H / 2) - (HOCKEY_PAD_H / 2);
    hk_p2.vx = 0; hk_p2.vy = 0;

    hk_puck.x = SCREEN_W / 2;
    hk_puck.y = SCREEN_H / 2;
    hk_puck.vx = 6; hk_puck.vy = 4;

    hockey_draw_arena();
    hockey_render_actors();
    hockey_draw_hud();

    ready_banner("READY!");
    flush_uart_rx();

    while (1) {
        while (UART_STATUS_REG & UART_RX_VALID) {
            char c = (char)(UART_RX_FIFO & 0xFFU);
            if (c == 'q' || c == 'Q') return false;
            if (c == 'p' || c == 'P') { if (pause_screen()) return false; continue; }
            if (c == 'w' || c == 'W') hk_p1.vy = -12;
            if (c == 's' || c == 'S') hk_p1.vy =  12;
            if (c == 'a' || c == 'A') hk_p1.vx = -12;
            if (c == 'd' || c == 'D') hk_p1.vx =  12;

            if (c == 'i' || c == 'I') hk_p2.vy = -12;
            if (c == 'k' || c == 'K') hk_p2.vy =  12;
            if (c == 'j' || c == 'J') hk_p2.vx = -12;
            if (c == 'l' || c == 'L') hk_p2.vx =  12;
        }

        hk_p1.x += hk_p1.vx; hk_p1.y += hk_p1.vy;
        hk_p2.x += hk_p2.vx; hk_p2.y += hk_p2.vy;
        hk_p1.vx = (hk_p1.vx * 3) / 4; hk_p1.vy = (hk_p1.vy * 3) / 4;
        hk_p2.vx = (hk_p2.vx * 3) / 4; hk_p2.vy = (hk_p2.vy * 3) / 4;

        hk_p1.x = clampi(hk_p1.x, HOCKEY_GOAL_W, (SCREEN_W / 2) - HOCKEY_PAD_W);
        hk_p1.y = clampi(hk_p1.y, 0, SCREEN_H - HOCKEY_PAD_H);
        hk_p2.x = clampi(hk_p2.x, SCREEN_W / 2, SCREEN_W - HOCKEY_GOAL_W - HOCKEY_PAD_W);
        hk_p2.y = clampi(hk_p2.y, 0, SCREEN_H - HOCKEY_PAD_H);

        hk_puck.x += hk_puck.vx;
        hk_puck.y += hk_puck.vy;

        if (hk_puck.y - HOCKEY_PUCK_R <= 0) {
            hk_puck.y = HOCKEY_PUCK_R;
            hk_puck.vy = -hk_puck.vy;
        }
        if (hk_puck.y + HOCKEY_PUCK_R >= SCREEN_H) {
            hk_puck.y = SCREEN_H - HOCKEY_PUCK_R;
            hk_puck.vy = -hk_puck.vy;
        }

        int px = hk_puck.x - HOCKEY_PUCK_R;
        int py = hk_puck.y - HOCKEY_PUCK_R;
        int pd = HOCKEY_PUCK_R * 2;

        if (aabb_overlap(px, py, pd, pd, hk_p1.x, hk_p1.y, HOCKEY_PAD_W, HOCKEY_PAD_H)) {
            hk_puck.x = hk_p1.x + HOCKEY_PAD_W + HOCKEY_PUCK_R;
            hk_puck.vx = (hk_puck.vx < 0) ? -hk_puck.vx : hk_puck.vx;
            hk_puck.vx += (hk_p1.vx / 2);
            hk_puck.vy += (hk_p1.vy / 2);
        }
        if (aabb_overlap(px, py, pd, pd, hk_p2.x, hk_p2.y, HOCKEY_PAD_W, HOCKEY_PAD_H)) {
            hk_puck.x = hk_p2.x - HOCKEY_PUCK_R;
            hk_puck.vx = (hk_puck.vx > 0) ? -hk_puck.vx : hk_puck.vx;
            hk_puck.vx += (hk_p2.vx / 2);
            hk_puck.vy += (hk_p2.vy / 2);
        }

        hk_puck.vx = clampi(hk_puck.vx, -HOCKEY_MAX_SPD, HOCKEY_MAX_SPD);
        hk_puck.vy = clampi(hk_puck.vy, -HOCKEY_MAX_SPD, HOCKEY_MAX_SPD);

        int goal_scored = 0;
        if (hk_puck.x - HOCKEY_PUCK_R <= HOCKEY_GOAL_W) {
            hk_s2++;
            goal_scored = 1;
        } else if (hk_puck.x + HOCKEY_PUCK_R >= SCREEN_W - HOCKEY_GOAL_W) {
            hk_s1++;
            goal_scored = -1;
        }

        if (goal_scored != 0) {
            const char *banner = (goal_scored == -1) ? "PLAYER 1 SCORES!" : "PLAYER 2 SCORES!";
            hockey_render_actors();
            hockey_draw_hud();
            if (goal_scored == -1) gpu_text_center(13, banner, C_GREEN);
            else                   gpu_text_center(13, banner, C_YELLOW);

            flash_bg((goal_scored == -1) ? 0x008U : 0x800U, 0x123U, 3, 2000000U);
            gpu_text_center_clear(13, banner);

            if (hk_s1 >= HOCKEY_WIN_SCORE || hk_s2 >= HOCKEY_WIN_SCORE) {
                int winner = (hk_s1 >= HOCKEY_WIN_SCORE) ? 1 : 2;
                if (winner == 1) hk_wins1++; else hk_wins2++;
                return match_over_screen(winner);
            }

            hk_puck.x = SCREEN_W / 2;
            hk_puck.y = SCREEN_H / 2;
            hk_puck.vx = 6 * goal_scored;
            hk_puck.vy = 4;
            delay_cycles(10000000U);
        }

        hockey_render_actors();
        delay_cycles(400000U);
    }
}

static void play_air_hockey(void) {
    while (hockey_session()) { }
}

/* ==========================================================================
 * GAME 3: PAC-MAN
 * ========================================================================== */
#define PAC_CELL            20
#define PAC_NUM_WALLS       12
#define PAC_MAX_GHOSTS      4
#define PAC_MAX_PELLETS     8

typedef struct { int16_t x, y, w, h; } pac_wall_t;
typedef struct { int16_t x, y, dx, dy; } pac_actor_t;
typedef struct { int16_t x, y; bool active; } pac_dot_t;

static const pac_wall_t PAC_WALLS[PAC_NUM_WALLS] = {
    {0, 0, 640, 20}, {0, 460, 640, 20}, {0, 0, 20, 480}, {620, 0, 20, 480},
    {60, 60, 100, 60}, {480, 60, 100, 60}, {60, 360, 100, 60}, {480, 360, 100, 60},
    {240, 60, 160, 40}, {240, 380, 160, 40}, {220, 200, 200, 40}, {280, 140, 80, 40}
};

static pac_actor_t pac_man;
static pac_actor_t pac_ghosts[PAC_MAX_GHOSTS];
static pac_dot_t   pac_pellets[PAC_MAX_PELLETS];
static pac_dot_t   pac_energizer;
static int16_t     pac_qdx = 0, pac_qdy = 0;
static uint32_t    pac_score = 0;
static uint32_t    pac_high_score = 0;
static uint32_t    pac_frightened = 0;
static uint32_t    pac_tick = 0;
static uint32_t    pac_level = 1;
static int         pac_active_ghosts = 2;
static int         pac_active_pellets = 6;
static uint32_t    pac_speed = 400000U;

static void pac_draw_hud(void) {
    gpu_text_puts(0, 3, "SCORE", C_WHITE);
    gpu_text_put_uint_pad(0, 9, pac_score, 6, C_YELLOW);
    gpu_text_puts(0, 35, "LEVEL", C_WHITE);
    gpu_text_put_uint_pad(0, 41, pac_level, 2, C_GREEN);
    gpu_text_puts(0, 62, "HI", C_WHITE);
    gpu_text_put_uint_pad(0, 65, pac_high_score, 6, C_CYAN);

    gpu_text_center(29, "P:PAUSE  Q:MENU", C_GRAY);
}

static void pac_add_score(uint32_t pts) {
    pac_score += pts;
    if (pac_score > pac_high_score) pac_high_score = pac_score;
}

static bool pac_hits_wall(int16_t px, int16_t py, int16_t size) {
    for (int i = 0; i < PAC_NUM_WALLS; i++) {
        if (px < (PAC_WALLS[i].x + PAC_WALLS[i].w) &&
           (px + size) > PAC_WALLS[i].x &&
            py < (PAC_WALLS[i].y + PAC_WALLS[i].h) &&
           (py + size) > PAC_WALLS[i].y) {
            return true;
        }
    }
    return false;
}

static void pac_spawn_pellet(int idx) {
    int16_t tx, ty;
    do {
        tx = (int16_t)(((rng_next() % 26) + 3) * PAC_CELL);
        ty = (int16_t)(((rng_next() % 18) + 3) * PAC_CELL);
    } while (pac_hits_wall(tx, ty, 8));
    pac_pellets[idx].x = tx;
    pac_pellets[idx].y = ty;
    pac_pellets[idx].active = true;
}

static void pac_spawn_energizer(void) {
    int16_t tx, ty;
    do {
        tx = (int16_t)(((rng_next() % 26) + 3) * PAC_CELL);
        ty = (int16_t)(((rng_next() % 18) + 3) * PAC_CELL);
    } while (pac_hits_wall(tx, ty, 16));
    pac_energizer.x = tx;
    pac_energizer.y = ty;
    pac_energizer.active = true;
}

static void pac_init_level(void) {
    pac_frightened = 0;
    pac_active_ghosts = pac_level + 1;
    if (pac_active_ghosts > PAC_MAX_GHOSTS) pac_active_ghosts = PAC_MAX_GHOSTS;
    pac_active_pellets = 10 - (pac_active_ghosts * 2);

    pac_speed = 400000U - (pac_level * 15000U);
    if (pac_speed < 180000U) pac_speed = 180000U;

    pac_man.x = 300; pac_man.y = 340;
    pac_man.dx = 0;  pac_man.dy = 0;
    pac_qdx = 0;     pac_qdy = 0;

    pac_ghosts[0].x = 200; pac_ghosts[0].y = 160; pac_ghosts[0].dx = 4;  pac_ghosts[0].dy = 0;
    pac_ghosts[1].x = 420; pac_ghosts[1].y = 160; pac_ghosts[1].dx = -4; pac_ghosts[1].dy = 0;
    pac_ghosts[2].x = 300; pac_ghosts[2].y = 260; pac_ghosts[2].dx = 0;  pac_ghosts[2].dy = 4;
    pac_ghosts[3].x = 300; pac_ghosts[3].y = 160; pac_ghosts[3].dx = 0;  pac_ghosts[3].dy = -4;

    pac_spawn_energizer();
    for (int i = 0; i < pac_active_pellets; i++) {
        pac_spawn_pellet(i);
    }

    for (int i = 0; i < PAC_NUM_WALLS; i++) {
        OBJ_X(i)   = (uint32_t)PAC_WALLS[i].x;
        OBJ_Y(i)   = (uint32_t)PAC_WALLS[i].y;
        OBJ_DIM(i) = PACK_DIM(PAC_WALLS[i].w, PAC_WALLS[i].h);
        OBJ_CFG(i) = PACK_CFG(1, 4, 15, SHAPE_RECTANGLE, true);
    }
}

static void pac_update_ghost_ai(int g) {
    bool at_int = ((pac_ghosts[g].x % PAC_CELL) == 0) && ((pac_ghosts[g].y % PAC_CELL) == 0);
    bool blocked = pac_hits_wall(pac_ghosts[g].x + pac_ghosts[g].dx, pac_ghosts[g].y + pac_ghosts[g].dy, 16);

    if (at_int || blocked) {
        static const int16_t dirs[4][2] = { {0, -4}, {0, 4}, {-4, 0}, {4, 0} };
        int16_t tx = pac_man.x, ty = pac_man.y;

        if (g == 1) { tx += (pac_man.dx * 4); ty += (pac_man.dy * 4); }
        else if (g == 2) { tx -= 40; ty -= 40; }
        else if (g == 3) {
            int32_t d2 = (pac_man.x - pac_ghosts[g].x) * (pac_man.x - pac_ghosts[g].x) +
                         (pac_man.y - pac_ghosts[g].y) * (pac_man.y - pac_ghosts[g].y);
            if (d2 < 20000) { tx = 40; ty = 40; }
        }

        int best_dir = -1, fallback = -1;
        int32_t best_score = pac_frightened ? -1 : 0x7FFFFFFF;

        for (int i = 0; i < 4; i++) {
            int16_t nx = pac_ghosts[g].x + dirs[i][0];
            int16_t ny = pac_ghosts[g].y + dirs[i][1];
            bool is_rev = (dirs[i][0] == -pac_ghosts[g].dx) && (dirs[i][1] == -pac_ghosts[g].dy);

            if (pac_hits_wall(nx, ny, 16)) continue;
            if (is_rev) { fallback = i; continue; }

            int32_t d2 = (nx - tx) * (nx - tx) + (ny - ty) * (ny - ty);
            if (pac_frightened) {
                if (d2 > best_score) { best_score = d2; best_dir = i; }
            } else {
                if (d2 < best_score) { best_score = d2; best_dir = i; }
            }
        }

        if (best_dir == -1) best_dir = fallback;
        if (best_dir != -1) {
            pac_ghosts[g].dx = dirs[best_dir][0];
            pac_ghosts[g].dy = dirs[best_dir][1];
        }
    }

    if (!pac_hits_wall(pac_ghosts[g].x + pac_ghosts[g].dx, pac_ghosts[g].y + pac_ghosts[g].dy, 16)) {
        pac_ghosts[g].x += pac_ghosts[g].dx;
        pac_ghosts[g].y += pac_ghosts[g].dy;
    }
}

static void pac_render(void) {
    pac_tick++;
    int slot = 12;

    OBJ_X(slot)   = (uint32_t)(pac_man.x + 8);
    OBJ_Y(slot)   = (uint32_t)(pac_man.y + 8);
    OBJ_DIM(slot) = PACK_DIM(8, 0);
    OBJ_CFG(slot) = PACK_CFG(15, 14, 0, SHAPE_CIRCLE, true);
    slot++;

    bool mouth_open = (pac_tick & 0x04) != 0;
    if (mouth_open && (pac_man.dx != 0 || pac_man.dy != 0)) {
        OBJ_X(slot)   = (uint32_t)(pac_man.x + 8 + (pac_man.dx / 2) - 3);
        OBJ_Y(slot)   = (uint32_t)(pac_man.y + 8 + (pac_man.dy / 2) - 3);
        OBJ_DIM(slot) = PACK_DIM(6, 6);
        OBJ_CFG(slot) = PACK_CFG(0, 0, 0, SHAPE_RECTANGLE, true);
    } else {
        OBJ_CFG(slot) = 0x00000000U;
    }
    slot++;

    const uint8_t g_cols[4][3] = { {15, 0, 0}, {15, 7, 11}, {0, 14, 15}, {15, 8, 0} };
    for (int g = 0; g < pac_active_ghosts; g++) {
        OBJ_X(slot)   = (uint32_t)pac_ghosts[g].x;
        OBJ_Y(slot)   = (uint32_t)pac_ghosts[g].y;
        OBJ_DIM(slot) = PACK_DIM(16, 16);
        if (pac_frightened > 0) {
            if (pac_frightened < 30 && (pac_tick & 0x02)) OBJ_CFG(slot) = PACK_CFG(15, 15, 15, SHAPE_RECTANGLE, true);
            else                                          OBJ_CFG(slot) = PACK_CFG(2, 2, 14, SHAPE_RECTANGLE, true);
        } else {
            OBJ_CFG(slot) = PACK_CFG(g_cols[g][0], g_cols[g][1], g_cols[g][2], SHAPE_RECTANGLE, true);
        }
        slot++;

        OBJ_X(slot)   = (uint32_t)(pac_ghosts[g].x + 8 + (pac_ghosts[g].dx / 2));
        OBJ_Y(slot)   = (uint32_t)(pac_ghosts[g].y + 6 + (pac_ghosts[g].dy / 2));
        OBJ_DIM(slot) = PACK_DIM(2, 0);
        if (pac_frightened > 0) OBJ_CFG(slot) = 0x00000000U;
        else                    OBJ_CFG(slot) = PACK_CFG(15, 15, 15, SHAPE_CIRCLE, true);
        slot++;
    }

    if (pac_energizer.active) {
        uint16_t rad = (pac_tick & 0x02) ? 6 : 5;
        OBJ_X(slot)   = (uint32_t)(pac_energizer.x + 8);
        OBJ_Y(slot)   = (uint32_t)(pac_energizer.y + 8);
        OBJ_DIM(slot) = PACK_DIM(rad, 0);
        OBJ_CFG(slot) = PACK_CFG(15, 10, 0, SHAPE_CIRCLE, true);
    } else {
        OBJ_CFG(slot) = 0x00000000U;
    }
    slot++;

    for (int i = 0; i < pac_active_pellets; i++) {
        if (pac_pellets[i].active) {
            OBJ_X(slot)   = (uint32_t)(pac_pellets[i].x + 4);
            OBJ_Y(slot)   = (uint32_t)(pac_pellets[i].y + 4);
            OBJ_DIM(slot) = PACK_DIM(2, 0);
            OBJ_CFG(slot) = PACK_CFG(14, 14, 14, SHAPE_CIRCLE, true);
        } else {
            OBJ_CFG(slot) = 0x00000000U;
        }
        slot++;
    }

    while (slot < GPU_NUM_SLOTS) {
        OBJ_CFG(slot++) = 0x00000000U;
    }
}

static bool pacman_session(void) {
    gpu_reset_all(0x000U);
    pac_level = 1;
    pac_score = 0;
    uint32_t best_at_start = pac_high_score;
    pac_init_level();
    pac_render();
    pac_draw_hud();

    ready_banner("READY!");
    flush_uart_rx();

    while (1) {
        while (UART_STATUS_REG & UART_RX_VALID) {
            char c = (char)(UART_RX_FIFO & 0xFFU);
            if (c == 'q' || c == 'Q') return false;
            if (c == 'p' || c == 'P') { if (pause_screen()) return false; continue; }
            if (c == 'w' || c == 'W') { pac_qdx =  0; pac_qdy = -4; }
            if (c == 's' || c == 'S') { pac_qdx =  0; pac_qdy =  4; }
            if (c == 'a' || c == 'A') { pac_qdx = -4; pac_qdy =  0; }
            if (c == 'd' || c == 'D') { pac_qdx =  4; pac_qdy =  0; }
        }

        bool hud_dirty = false;

        if (pac_qdx != 0 || pac_qdy != 0) {
            if (!pac_hits_wall(pac_man.x + pac_qdx, pac_man.y + pac_qdy, 16)) {
                pac_man.dx = pac_qdx;
                pac_man.dy = pac_qdy;
            }
        }
        if (!pac_hits_wall(pac_man.x + pac_man.dx, pac_man.y + pac_man.dy, 16)) {
            pac_man.x += pac_man.dx;
            pac_man.y += pac_man.dy;
        }

        for (int i = 0; i < pac_active_pellets; i++) {
            if (pac_pellets[i].active) {
                int16_t dx = (pac_man.x + 8) - (pac_pellets[i].x + 4);
                int16_t dy = (pac_man.y + 8) - (pac_pellets[i].y + 4);
                if ((dx * dx + dy * dy) < 144) {
                    pac_add_score(10);
                    pac_pellets[i].active = false;
                    hud_dirty = true;
                }
            }
        }

        if (pac_energizer.active) {
            int16_t dx = (pac_man.x + 8) - (pac_energizer.x + 8);
            int16_t dy = (pac_man.y + 8) - (pac_energizer.y + 8);
            if ((dx * dx + dy * dy) < 200) {
                pac_add_score(50);
                pac_frightened = 150 - (pac_level * 10);
                if (pac_frightened < 50) pac_frightened = 50;
                pac_energizer.active = false;
                hud_dirty = true;
            }
        }

        bool level_done = !pac_energizer.active;
        for (int i = 0; i < pac_active_pellets; i++) {
            if (pac_pellets[i].active) {
                level_done = false;
                break;
            }
        }

        if (level_done) {
            pac_draw_hud();
            gpu_text_center(13, "LEVEL CLEARED!", C_GREEN);
            flash_bg(0x060U, 0x000U, 3, 2000000U);
            gpu_text_center_clear(13, "LEVEL CLEARED!");
            pac_level++;
            pac_init_level();
            pac_render();
            pac_draw_hud();
            ready_banner("READY!");
            flush_uart_rx();
            continue;
        }

        if (pac_frightened > 0) pac_frightened--;

        bool caught = false;
        for (int g = 0; g < pac_active_ghosts; g++) {
            pac_update_ghost_ai(g);
            int16_t c_dx = (pac_man.x + 8) - (pac_ghosts[g].x + 8);
            int16_t c_dy = (pac_man.y + 8) - (pac_ghosts[g].y + 8);
            if ((c_dx * c_dx + c_dy * c_dy) < 180) {
                if (pac_frightened > 0) {
                    pac_add_score(200);
                    hud_dirty = true;
                    pac_ghosts[g].x = 300;
                    pac_ghosts[g].y = 200;
                } else {
                    caught = true;
                    break;
                }
            }
        }

        if (hud_dirty) pac_draw_hud();

        if (caught) {
            flash_bg(0x800U, 0x000U, 3, 2500000U);
            return game_over_screen(pac_score, pac_high_score, pac_score > best_at_start,
                                    "LEVEL", pac_level);
        }

        pac_render();
        delay_cycles(pac_speed);
    }
}

static void play_pacman(void) {
    while (pacman_session()) { }
}

/* ==========================================================================
 * SHARED HELPERS FOR GAMES 4-7
 * Shapes must not use slots 23/24 (pause / game-over panel).
 * ========================================================================== */
static char poll_key(void) {
    if (UART_STATUS_REG & UART_RX_VALID) {
        return to_upper((char)(UART_RX_FIFO & 0xFFU));
    }
    return 0;
}

typedef enum { SK_NONE = 0, SK_QUIT, SK_RESUMED } sk_t;

/* Shared keys: P = pause, Q = quit. After SK_RESUMED the caller redraws. */
static sk_t sys_key(char c) {
    if (c == 'Q') return SK_QUIT;
    if (c == 'P') return pause_screen() ? SK_QUIT : SK_RESUMED;
    return SK_NONE;
}

static void shape_off(int slot) {
    OBJ_CFG(slot) = 0x00000000U;
}

static void shape_rect(int slot, int x, int y, int w, int h,
                       uint8_t r, uint8_t g, uint8_t b) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCREEN_W) w = SCREEN_W - x;
    if (y + h > SCREEN_H) h = SCREEN_H - y;
    if (w <= 0 || h <= 0) { shape_off(slot); return; }
    OBJ_X(slot)   = (uint32_t)x;
    OBJ_Y(slot)   = (uint32_t)y;
    OBJ_DIM(slot) = PACK_DIM(w, h);
    OBJ_CFG(slot) = PACK_CFG(r, g, b, SHAPE_RECTANGLE, true);
}

static void shape_circ(int slot, int cx, int cy, int rad,
                       uint8_t r, uint8_t g, uint8_t b) {
    if (cx < 0 || cy < 0 || rad <= 0) { shape_off(slot); return; }
    OBJ_X(slot)   = (uint32_t)cx;
    OBJ_Y(slot)   = (uint32_t)cy;
    OBJ_DIM(slot) = PACK_DIM(rad, 0);
    OBJ_CFG(slot) = PACK_CFG(r, g, b, SHAPE_CIRCLE, true);
}

/* ==========================================================================
 * GAME 4: PONG (1 player vs CPU)
 * ========================================================================== */
#define PONG_PAD_W      10
#define PONG_PAD_H      64
#define PONG_BALL_R     6
#define PONG_WIN        7
#define PONG_P1X        24
#define PONG_P2X        (SCREEN_W - 24 - PONG_PAD_W)
#define PONG_FRAME      400000U

static uint32_t pg_wins_you = 0, pg_wins_cpu = 0;
static int pg_bx, pg_by, pg_bvx, pg_bvy, pg_spd;

static void pong_draw_static(void) {
    for (int r = 1; r < GPU_TEXT_ROWS - 1; r += 2) {
        gpu_text_putc(r, 39, (char)0xB3, C_GRAY);
    }
    gpu_text_puts(29, 3, "YOU: W/S", C_GREEN);
    gpu_text_center(29, "FIRST TO 7   P:PAUSE  Q:MENU", C_GRAY);
    gpu_text_puts(29, 72, "CPU", C_YELLOW);
}

static void pong_draw_score(int a, int b) {
    gpu_text_put_uint_pad(1, 32, (uint32_t)a, 2, C_GREEN);
    gpu_text_put_uint_pad(1, 46, (uint32_t)b, 2, C_YELLOW);
}

static void pong_serve(int dir) {
    pg_bx = SCREEN_W / 2;
    pg_by = SCREEN_H / 2;
    pg_spd = 6;
    pg_bvx = dir * pg_spd;
    pg_bvy = (int)((rng_next() % 5) + 1);
    if (rng_next() & 1U) pg_bvy = -pg_bvy;
}

static bool pong_over_screen(int you, int cpu) {
    char k;
    panel_show();
    gpu_text_center(10, "M A T C H   O V E R", C_RED);
    if (you > cpu) gpu_text_center(12, "YOU WIN!", C_GREEN);
    else           gpu_text_center(12, "CPU WINS!", C_YELLOW);
    gpu_text_puts(14, 33, "YOU", C_GREEN);
    gpu_text_put_uint_pad(14, 37, (uint32_t)you, 2, C_WHITE);
    gpu_text_puts(14, 40, "-", C_GRAY);
    gpu_text_put_uint_pad(14, 42, (uint32_t)cpu, 2, C_WHITE);
    gpu_text_puts(14, 45, "CPU", C_YELLOW);
    gpu_text_center(18, "R : REMATCH", C_WHITE);
    gpu_text_center(19, "Q : MAIN MENU", C_WHITE);
    do {
        k = wait_key_blink(16, you > cpu ? "** GOOD GAME **" : "** TRY AGAIN **", C_YELLOW);
    } while (k != 'R' && k != 'Q');
    panel_hide();
    return k == 'R';
}

static bool pong_session(void) {
    gpu_reset_all(0x001U);
    int s1 = 0, s2 = 0;
    int p1y = SCREEN_H / 2 - PONG_PAD_H / 2, p1v = 0;
    int p2y = p1y;

    pong_serve((rng_next() & 1U) ? 1 : -1);
    pong_draw_static();
    pong_draw_score(s1, s2);
    shape_rect(0, PONG_P1X, p1y, PONG_PAD_W, PONG_PAD_H, 0, 15, 0);
    shape_rect(1, PONG_P2X, p2y, PONG_PAD_W, PONG_PAD_H, 15, 15, 0);
    shape_circ(2, pg_bx, pg_by, PONG_BALL_R, 15, 15, 15);

    ready_banner("READY!");
    flush_uart_rx();

    while (1) {
        char c;
        while ((c = poll_key()) != 0) {
            sk_t sk = sys_key(c);
            if (sk == SK_QUIT) return false;
            if (sk == SK_RESUMED) { pong_draw_static(); pong_draw_score(s1, s2); continue; }
            if (c == 'W') p1v = -14;
            if (c == 'S') p1v =  14;
        }

        p1y += p1v;
        p1v = (p1v * 3) / 4;
        p1y = clampi(p1y, 0, SCREEN_H - PONG_PAD_H);

        int target = (pg_bvx > 0) ? pg_by : SCREEN_H / 2;
        int mid = p2y + PONG_PAD_H / 2;
        if (target > mid + 6)      p2y += 4;
        else if (target < mid - 6) p2y -= 4;
        p2y = clampi(p2y, 0, SCREEN_H - PONG_PAD_H);

        pg_bx += pg_bvx;
        pg_by += pg_bvy;
        if (pg_by - PONG_BALL_R <= 0)        { pg_by = PONG_BALL_R;            pg_bvy = -pg_bvy; }
        if (pg_by + PONG_BALL_R >= SCREEN_H) { pg_by = SCREEN_H - PONG_BALL_R; pg_bvy = -pg_bvy; }

        int bl = pg_bx - PONG_BALL_R, bt = pg_by - PONG_BALL_R, bd = PONG_BALL_R * 2;
        if (pg_bvx < 0 && aabb_overlap(bl, bt, bd, bd, PONG_P1X, p1y, PONG_PAD_W, PONG_PAD_H)) {
            if (pg_spd < 11) pg_spd++;
            pg_bx = PONG_P1X + PONG_PAD_W + PONG_BALL_R;
            pg_bvx = pg_spd;
            pg_bvy = ((pg_by - (p1y + PONG_PAD_H / 2)) * 7) / (PONG_PAD_H / 2 + PONG_BALL_R) + p1v / 4;
            pg_bvy = clampi(pg_bvy, -9, 9);
        } else if (pg_bvx > 0 && aabb_overlap(bl, bt, bd, bd, PONG_P2X, p2y, PONG_PAD_W, PONG_PAD_H)) {
            if (pg_spd < 11) pg_spd++;
            pg_bx = PONG_P2X - PONG_BALL_R;
            pg_bvx = -pg_spd;
            pg_bvy = ((pg_by - (p2y + PONG_PAD_H / 2)) * 7) / (PONG_PAD_H / 2 + PONG_BALL_R);
            pg_bvy = clampi(pg_bvy, -9, 9);
        }

        int scored = 0;
        if (pg_bx - PONG_BALL_R <= 0)             { s2++; scored = -1; }
        else if (pg_bx + PONG_BALL_R >= SCREEN_W) { s1++; scored =  1; }

        if (scored != 0) {
            pong_draw_score(s1, s2);
            flash_bg((scored > 0) ? 0x060U : 0x800U, 0x001U, 2, 1500000U);

            if (s1 >= PONG_WIN || s2 >= PONG_WIN) {
                if (s1 > s2) pg_wins_you++; else pg_wins_cpu++;
                return pong_over_screen(s1, s2);
            }
            pong_serve(-scored);
            delay_cycles(6000000U);
        }

        shape_rect(0, PONG_P1X, p1y, PONG_PAD_W, PONG_PAD_H, 0, 15, 0);
        shape_rect(1, PONG_P2X, p2y, PONG_PAD_W, PONG_PAD_H, 15, 15, 0);
        shape_circ(2, pg_bx, pg_by, PONG_BALL_R, 15, 15, 15);
        delay_cycles(PONG_FRAME);
    }
}

static void play_pong(void) {
    while (pong_session()) { }
}

/* ==========================================================================
 * GAME 5: BREAKOUT
 * ========================================================================== */
#define BRK_COLS        14
#define BRK_ROWS        6
#define BRK_C0          5
#define BRK_R0          3
#define BRK_PAD_Y       440
#define BRK_PAD_H       10
#define BRK_BALL_R      4
#define BRK_CEIL        18
#define BRK_FRAME       350000U

static uint16_t brk_alive[BRK_ROWS];
static const uint8_t BRK_RGB[BRK_ROWS][3] = {
    {15, 2, 2}, {15, 8, 0}, {15, 15, 0}, {2, 15, 2}, {0, 15, 15}, {5, 5, 15}
};
static const uint8_t BRK_PTS[BRK_ROWS] = { 60, 50, 40, 30, 20, 10 };
static uint32_t brk_score = 0, brk_high = 0, brk_level = 1, brk_lives = 3;
static int brk_bx, brk_by, brk_bvx, brk_bvy, brk_spd, brk_hits;
static int brk_px, brk_pw;
static bool brk_launched;

static void brk_draw_brick(int r, int c) {
    bool on = ((brk_alive[r] >> c) & 1U) != 0;
    int row = BRK_R0 + r, col = BRK_C0 + c * 5;
    gpu_text_fill(row, col, 4, on ? (char)0xDB : ' ',
                  BRK_RGB[r][0], BRK_RGB[r][1], BRK_RGB[r][2]);
}

static void brk_draw_bricks(void) {
    for (int r = 0; r < BRK_ROWS; r++)
        for (int c = 0; c < BRK_COLS; c++) brk_draw_brick(r, c);
}

static void brk_reset_bricks(void) {
    for (int r = 0; r < BRK_ROWS; r++) brk_alive[r] = (1U << BRK_COLS) - 1U;
}

static void brk_draw_hud(void) {
    gpu_text_puts(0, 1, "SCORE", C_WHITE);
    gpu_text_put_uint_pad(0, 7, brk_score, 6, C_YELLOW);
    gpu_text_puts(0, 24, "LEVEL", C_WHITE);
    gpu_text_put_uint_pad(0, 30, brk_level, 2, C_GREEN);
    gpu_text_puts(0, 38, "BALLS", C_WHITE);
    gpu_text_put_uint_pad(0, 44, brk_lives, 1, C_RED);
    gpu_text_puts(0, 62, "HI", C_WHITE);
    gpu_text_put_uint_pad(0, 65, brk_high, 6, C_CYAN);
    gpu_text_center(29, "A/D:MOVE  SPACE:LAUNCH  P:PAUSE  Q:MENU", C_GRAY);
}

static void brk_hint(bool on) {
    if (on) gpu_text_center(20, "PRESS SPACE TO LAUNCH", C_GRAY);
    else    gpu_text_center_clear(20, "PRESS SPACE TO LAUNCH");
}

static bool brk_at(int px, int py, int *rr, int *cc) {
    if (py < BRK_R0 * 16 || py >= (BRK_R0 + BRK_ROWS) * 16) return false;
    if (px < BRK_C0 * 8  || px >= (BRK_C0 + BRK_COLS * 5) * 8) return false;
    int r = (py - BRK_R0 * 16) / 16;
    int c = (px - BRK_C0 * 8) / 40;
    if (!((brk_alive[r] >> c) & 1U)) return false;
    *rr = r; *cc = c;
    return true;
}

static bool brk_cleared(void) {
    for (int r = 0; r < BRK_ROWS; r++) if (brk_alive[r]) return false;
    return true;
}

static void brk_render(void) {
    shape_rect(0, brk_px, BRK_PAD_Y, brk_pw, BRK_PAD_H, 15, 15, 15);
    shape_circ(1, brk_bx, brk_by, BRK_BALL_R, 15, 15, 0);
}

static void brk_new_ball(void) {
    brk_launched = false;
    brk_px = SCREEN_W / 2 - brk_pw / 2;
    brk_bx = brk_px + brk_pw / 2;
    brk_by = BRK_PAD_Y - BRK_BALL_R - 1;
    brk_bvx = 0; brk_bvy = 0;
    brk_hint(true);
}

static bool breakout_session(void) {
    gpu_reset_all(0x001U);
    brk_score = 0; brk_level = 1; brk_lives = 3;
    uint32_t best_at_start = brk_high;
    int pv = 0;
    brk_pw = 80; brk_spd = 5; brk_hits = 0;

    brk_reset_bricks();
    brk_draw_bricks();
    brk_draw_hud();
    brk_new_ball();
    brk_render();

    ready_banner("READY!");
    flush_uart_rx();

    while (1) {
        char c;
        while ((c = poll_key()) != 0) {
            sk_t sk = sys_key(c);
            if (sk == SK_QUIT) return false;
            if (sk == SK_RESUMED) {
                brk_draw_bricks(); brk_draw_hud();
                if (!brk_launched) brk_hint(true);
                continue;
            }
            if (c == 'A') pv = -16;
            if (c == 'D') pv =  16;
            if (c == ' ' && !brk_launched) {
                brk_launched = true;
                brk_bvx = (rng_next() & 1U) ? 2 : -2;
                brk_bvy = -brk_spd;
                brk_hint(false);
            }
        }

        brk_px = clampi(brk_px + pv, 0, SCREEN_W - brk_pw);
        pv = (pv * 3) / 4;

        if (!brk_launched) {
            brk_bx = brk_px + brk_pw / 2;
            brk_by = BRK_PAD_Y - BRK_BALL_R - 1;
        } else {
            int nx = brk_bx + brk_bvx, ny = brk_by + brk_bvy;

            if (nx - BRK_BALL_R < 0)              { nx = BRK_BALL_R;              brk_bvx = -brk_bvx; }
            else if (nx + BRK_BALL_R > SCREEN_W)  { nx = SCREEN_W - BRK_BALL_R;   brk_bvx = -brk_bvx; }
            if (ny - BRK_BALL_R < BRK_CEIL)       { ny = BRK_CEIL + BRK_BALL_R;   brk_bvy = -brk_bvy; }

            int ex  = nx + (brk_bvx > 0 ? BRK_BALL_R : -BRK_BALL_R);
            int ey  = ny + (brk_bvy > 0 ? BRK_BALL_R : -BRK_BALL_R);
            int exo = brk_bx + (brk_bvx > 0 ? BRK_BALL_R : -BRK_BALL_R);
            int eyo = brk_by + (brk_bvy > 0 ? BRK_BALL_R : -BRK_BALL_R);
            int rr, cc;
            if (brk_at(ex, ey, &rr, &cc)) {
                int r2, c2;
                bool hit_x = brk_at(ex, eyo, &r2, &c2);
                bool hit_y = brk_at(exo, ey, &r2, &c2);

                brk_alive[rr] &= (uint16_t)~(1U << cc);
                brk_draw_brick(rr, cc);
                brk_score += BRK_PTS[rr];
                if (brk_score > brk_high) brk_high = brk_score;
                brk_draw_hud();

                if (hit_x && !hit_y)      { brk_bvx = -brk_bvx; nx = brk_bx; }
                else if (hit_y && !hit_x) { brk_bvy = -brk_bvy; ny = brk_by; }
                else                      { brk_bvx = -brk_bvx; brk_bvy = -brk_bvy; nx = brk_bx; ny = brk_by; }

                if (++brk_hits % 10 == 0 && brk_spd < 8) brk_spd++;
                brk_bvy = (brk_bvy < 0) ? -brk_spd : brk_spd;
            }

            if (brk_bvy > 0 && ny + BRK_BALL_R >= BRK_PAD_Y && brk_by + BRK_BALL_R <= BRK_PAD_Y + 6 &&
                nx + BRK_BALL_R >= brk_px && nx - BRK_BALL_R <= brk_px + brk_pw) {
                int off = nx - (brk_px + brk_pw / 2);
                brk_bvx = (off * 7) / (brk_pw / 2 + BRK_BALL_R) + pv / 6;
                brk_bvx = clampi(brk_bvx, -7, 7);
                if (brk_bvx == 0) brk_bvx = (rng_next() & 1U) ? 1 : -1;
                brk_bvy = -brk_spd;
                ny = BRK_PAD_Y - BRK_BALL_R;
            }

            brk_bx = nx; brk_by = ny;

            if (brk_cleared()) {
                brk_render();
                gpu_text_center(17, "LEVEL CLEARED!", C_GREEN);
                flash_bg(0x060U, 0x001U, 3, 1500000U);
                gpu_text_center_clear(17, "LEVEL CLEARED!");
                brk_level++;
                brk_score += 100;
                if (brk_score > brk_high) brk_high = brk_score;
                brk_reset_bricks();
                brk_draw_bricks();
                brk_pw = (brk_level >= 3) ? 64 : 80;
                brk_spd = 5 + (int)(brk_level - 1) / 2;
                if (brk_spd > 8) brk_spd = 8;
                brk_hits = 0;
                brk_new_ball();
                brk_draw_hud();
                brk_render();
                flush_uart_rx();
                continue;
            }

            if (brk_by - BRK_BALL_R > SCREEN_H) {
                brk_lives--;
                brk_draw_hud();
                flash_bg(0x800U, 0x001U, 2, 1500000U);
                if (brk_lives == 0) {
                    return game_over_screen(brk_score, brk_high, brk_score > best_at_start,
                                            "LEVEL", brk_level);
                }
                brk_new_ball();
                flush_uart_rx();
            }
        }

        brk_render();
        delay_cycles(BRK_FRAME);
    }
}

static void play_breakout(void) {
    while (breakout_session()) { }
}

/* ==========================================================================
 * GAME 6: TETRIS
 * ========================================================================== */
#define TT_W        10
#define TT_H        20
#define TT_R0       3
#define TT_C0       30
#define TT_FRAME    300000U

static uint8_t  tt_board[TT_H][TT_W];
static uint8_t  tt_shown[TT_H][TT_W];
static int      tt_type, tt_rot, tt_px, tt_py, tt_next;
static uint32_t tt_score = 0, tt_high = 0, tt_lines = 0, tt_level = 1;

static const uint8_t TT_BASE[7][4][2] = {
    {{0,1},{1,1},{2,1},{3,1}},      /* I */
    {{0,0},{1,0},{0,1},{1,1}},      /* O */
    {{1,0},{0,1},{1,1},{2,1}},      /* T */
    {{1,0},{2,0},{0,1},{1,1}},      /* S */
    {{0,0},{1,0},{1,1},{2,1}},      /* Z */
    {{0,0},{0,1},{1,1},{2,1}},      /* J */
    {{2,0},{0,1},{1,1},{2,1}}       /* L */
};
static const uint8_t TT_SIZE[7] = { 4, 2, 3, 3, 3, 3, 3 };
static const uint8_t TT_PAL[9][3] = {
    {0,0,0}, {0,15,15}, {15,15,0}, {11,2,15}, {2,15,2}, {15,2,2}, {3,4,15}, {15,8,0}, {6,6,6}
};

static void tt_put(int row, int col, uint8_t idx) {
    char ch = (idx == 0) ? ' ' : ((idx == 8) ? (char)0xB0 : (char)0xDB);
    gpu_text_putc(row, col,     ch, TT_PAL[idx][0], TT_PAL[idx][1], TT_PAL[idx][2]);
    gpu_text_putc(row, col + 1, ch, TT_PAL[idx][0], TT_PAL[idx][1], TT_PAL[idx][2]);
}

static void tt_cells(int type, int rot, int px, int py, int xs[4], int ys[4]) {
    int sz = TT_SIZE[type];
    for (int i = 0; i < 4; i++) {
        int x = TT_BASE[type][i][0], y = TT_BASE[type][i][1];
        for (int r = 0; r < rot; r++) {
            int nx = sz - 1 - y;
            int ny = x;
            x = nx; y = ny;
        }
        xs[i] = px + x;
        ys[i] = py + y;
    }
}

static bool tt_fits(int type, int rot, int px, int py) {
    int xs[4], ys[4];
    tt_cells(type, rot, px, py, xs, ys);
    for (int i = 0; i < 4; i++) {
        if (xs[i] < 0 || xs[i] >= TT_W || ys[i] >= TT_H) return false;
        if (ys[i] >= 0 && tt_board[ys[i]][xs[i]]) return false;
    }
    return true;
}

static void tt_add_score(uint32_t pts) {
    tt_score += pts;
    if (tt_score > tt_high) tt_high = tt_score;
}

static void tt_draw_stats(void) {
    gpu_text_put_uint_pad(5, 8, tt_score, 6, C_YELLOW);
    gpu_text_put_uint_pad(9, 8, tt_lines, 4, C_GREEN);
    gpu_text_put_uint_pad(13, 8, tt_level, 2, C_CYAN);
    gpu_text_put_uint_pad(17, 8, tt_high, 6, C_WHITE);
}

static void tt_draw_next(void) {
    gpu_text_clear_rect(6, 54, 4, 8);
    for (int i = 0; i < 4; i++) {
        tt_put(6 + TT_BASE[tt_next][i][1], 54 + 2 * TT_BASE[tt_next][i][0], (uint8_t)(tt_next + 1));
    }
}

static void tt_draw_static(void) {
    gpu_text_center(0, "TETRIS", C_CYAN);
    gpu_text_box(TT_R0 - 1, TT_C0 - 1, TT_H + 2, TT_W * 2 + 2, C_BLUE);
    gpu_text_puts(4, 8, "SCORE", C_WHITE);
    gpu_text_puts(8, 8, "LINES", C_WHITE);
    gpu_text_puts(12, 8, "LEVEL", C_WHITE);
    gpu_text_puts(16, 8, "BEST", C_WHITE);
    gpu_text_puts(4, 54, "NEXT", C_WHITE);
    gpu_text_puts(14, 54, "A/D    MOVE", C_GRAY);
    gpu_text_puts(15, 54, "W      ROTATE", C_GRAY);
    gpu_text_puts(16, 54, "S      SOFT DROP", C_GRAY);
    gpu_text_puts(17, 54, "SPACE  HARD DROP", C_GRAY);
    gpu_text_puts(18, 54, "P      PAUSE", C_GRAY);
    gpu_text_puts(19, 54, "Q      MENU", C_GRAY);
}

static void tt_invalidate(void) {
    for (int y = 0; y < TT_H; y++)
        for (int x = 0; x < TT_W; x++) tt_shown[y][x] = 0xFF;
}

static void tt_render(void) {
    uint8_t disp[TT_H][TT_W];
    int xs[4], ys[4];

    for (int y = 0; y < TT_H; y++)
        for (int x = 0; x < TT_W; x++) disp[y][x] = tt_board[y][x];

    int gy = tt_py;
    while (tt_fits(tt_type, tt_rot, tt_px, gy + 1)) gy++;
    tt_cells(tt_type, tt_rot, tt_px, gy, xs, ys);
    for (int i = 0; i < 4; i++)
        if (ys[i] >= 0 && disp[ys[i]][xs[i]] == 0) disp[ys[i]][xs[i]] = 8;
    tt_cells(tt_type, tt_rot, tt_px, tt_py, xs, ys);
    for (int i = 0; i < 4; i++)
        if (ys[i] >= 0) disp[ys[i]][xs[i]] = (uint8_t)(tt_type + 1);

    for (int y = 0; y < TT_H; y++)
        for (int x = 0; x < TT_W; x++)
            if (disp[y][x] != tt_shown[y][x]) {
                tt_put(TT_R0 + y, TT_C0 + 2 * x, disp[y][x]);
                tt_shown[y][x] = disp[y][x];
            }
}

static bool tt_spawn(void) {
    tt_type = tt_next;
    tt_next = (int)(rng_next() % 7U);
    tt_rot = 0;
    tt_px = 3;
    tt_py = 0;
    tt_draw_next();
    return tt_fits(tt_type, tt_rot, tt_px, tt_py);
}

/* Locks the piece, clears lines, scores, spawns. Returns false on top-out. */
static bool tt_lock_and_spawn(void) {
    int xs[4], ys[4];
    tt_cells(tt_type, tt_rot, tt_px, tt_py, xs, ys);
    for (int i = 0; i < 4; i++)
        if (ys[i] >= 0) tt_board[ys[i]][xs[i]] = (uint8_t)(tt_type + 1);

    int cleared = 0;
    for (int y = TT_H - 1; y >= 0; ) {
        bool full = true;
        for (int x = 0; x < TT_W; x++) if (!tt_board[y][x]) { full = false; break; }
        if (full) {
            cleared++;
            for (int yy = y; yy > 0; yy--)
                for (int x = 0; x < TT_W; x++) tt_board[yy][x] = tt_board[yy - 1][x];
            for (int x = 0; x < TT_W; x++) tt_board[0][x] = 0;
        } else {
            y--;
        }
    }
    if (cleared > 0) {
        static const uint16_t line_pts[5] = { 0, 40, 100, 300, 1200 };
        tt_add_score((uint32_t)line_pts[cleared] * tt_level);
        tt_lines += (uint32_t)cleared;
        tt_level = 1 + tt_lines / 10;
    }
    tt_draw_stats();
    return tt_spawn();
}

static bool tetris_session(void) {
    gpu_reset_all(0x001U);
    for (int y = 0; y < TT_H; y++)
        for (int x = 0; x < TT_W; x++) tt_board[y][x] = 0;
    tt_invalidate();
    tt_score = 0; tt_lines = 0; tt_level = 1;
    uint32_t best_at_start = tt_high;
    int grav = 0;

    tt_next = (int)(rng_next() % 7U);
    (void)tt_spawn();
    tt_draw_static();
    tt_draw_stats();
    tt_render();

    ready_banner("READY!");
    flush_uart_rx();

    while (1) {
        char c;
        bool topped = false;
        while ((c = poll_key()) != 0) {
            sk_t sk = sys_key(c);
            if (sk == SK_QUIT) return false;
            if (sk == SK_RESUMED) {
                tt_invalidate();
                tt_draw_static(); tt_draw_stats(); tt_draw_next();
                continue;
            }
            if (c == 'A' && tt_fits(tt_type, tt_rot, tt_px - 1, tt_py)) tt_px--;
            if (c == 'D' && tt_fits(tt_type, tt_rot, tt_px + 1, tt_py)) tt_px++;
            if (c == 'W') {
                static const int kicks[5] = { 0, -1, 1, -2, 2 };
                int nr = (tt_rot + 1) & 3;
                for (int k = 0; k < 5; k++) {
                    if (tt_fits(tt_type, nr, tt_px + kicks[k], tt_py)) {
                        tt_rot = nr;
                        tt_px += kicks[k];
                        break;
                    }
                }
            }
            if (c == 'S') {
                if (tt_fits(tt_type, tt_rot, tt_px, tt_py + 1)) { tt_py++; tt_add_score(1); grav = 0; }
            }
            if (c == ' ') {
                uint32_t dist = 0;
                while (tt_fits(tt_type, tt_rot, tt_px, tt_py + 1)) { tt_py++; dist++; }
                tt_add_score(2U * dist);
                if (!tt_lock_and_spawn()) { topped = true; break; }
                grav = 0;
            }
        }

        if (!topped) {
            int g_frames = 18 - 2 * ((int)tt_level - 1);
            if (g_frames < 2) g_frames = 2;
            if (++grav >= g_frames) {
                grav = 0;
                if (tt_fits(tt_type, tt_rot, tt_px, tt_py + 1)) tt_py++;
                else if (!tt_lock_and_spawn()) topped = true;
            }
        }

        if (topped) {
            tt_draw_stats();
            flash_bg(0x800U, 0x001U, 2, 1500000U);
            return game_over_screen(tt_score, tt_high, tt_score > best_at_start,
                                    "LINES", tt_lines);
        }

        tt_render();
        delay_cycles(TT_FRAME);
    }
}

static void play_tetris(void) {
    while (tetris_session()) { }
}

/* ==========================================================================
 * GAME 7: FROGGER
 * Screen is 15 lanes of 32 px. Lane 0 = HUD, 1 = homes, 2-6 = river,
 * 7 = median, 8-12 = road, 13 = start, 14 = HUD.
 * ========================================================================== */
#define FG_LANE_H       32
#define FG_HOMES        5
#define FG_TIME_MAX     1200
#define FG_BG           0x050U
#define FG_FRAME        450000U
#define FG_SLOT_FROG    22

static const int8_t  FG_SPD[13] = { 0, 0, -2, 3, -1, 2, -3, 0, -2, 3, -2, 2, -4 };
static const uint8_t FG_W[13]   = { 0, 0, 96, 64, 128, 64, 96, 0, 32, 32, 64, 64, 32 };
static const uint8_t FG_RGB[13][3] = {
    {0,0,0}, {0,0,0},
    {9,5,1}, {2,12,4}, {9,5,1}, {2,12,4}, {9,5,1},
    {0,0,0},
    {15,2,2}, {15,15,0}, {12,12,15}, {15,8,0}, {15,4,15}
};

static int      fg_x[13][2];
static int      fg_fx, fg_fy, fg_best_lane, fg_time, fg_bar_shown;
static bool     fg_home[FG_HOMES];
static uint32_t fg_score = 0, fg_high = 0, fg_level = 1, fg_lives = 3;

static int fg_home_col(int k) { return 6 + 15 * k; }

static int fg_speed(int lane) {
    int s = FG_SPD[lane];
    int bonus = ((int)fg_level - 1) / 2;
    if (s > 0) s += bonus; else if (s < 0) s -= bonus;
    return clampi(s, -6, 6);
}

static void fg_draw_hud(void) {
    gpu_text_puts(0, 1, "SCORE", C_WHITE);
    gpu_text_put_uint_pad(0, 7, fg_score, 6, C_YELLOW);
    gpu_text_center(0, "FROGGER", C_WHITE);
    gpu_text_puts(0, 62, "HI", C_WHITE);
    gpu_text_put_uint_pad(0, 65, fg_high, 6, C_CYAN);

    gpu_text_puts(29, 1, "FROGS", C_WHITE);
    gpu_text_put_uint_pad(29, 7, fg_lives, 1, C_GREEN);
    gpu_text_puts(29, 10, "TIME", C_WHITE);
    gpu_text_puts(29, 68, "LEVEL", C_WHITE);
    gpu_text_put_uint_pad(29, 74, fg_level, 2, C_CYAN);
    fg_bar_shown = -1;
}

static void fg_draw_timer(void) {
    int n = (fg_time * 40) / FG_TIME_MAX;
    if (n == fg_bar_shown) return;
    fg_bar_shown = n;
    for (int i = 0; i < 40; i++) {
        if (i < n) {
            if (n > 20)      gpu_text_putc(29, 15 + i, (char)0xDB, C_GREEN);
            else if (n > 10) gpu_text_putc(29, 15 + i, (char)0xDB, C_YELLOW);
            else             gpu_text_putc(29, 15 + i, (char)0xDB, C_RED);
        } else {
            gpu_text_putc(29, 15 + i, ' ', 0, 0, 0);
        }
    }
}

static void fg_draw_homes(void) {
    for (int k = 0; k < FG_HOMES; k++) {
        int col = fg_home_col(k);
        gpu_text_fill(2, col, 6, (char)0xDB, 0, 3, 12);
        gpu_text_fill(3, col, 6, (char)0xDB, 0, 3, 12);
        if (fg_home[k]) {
            gpu_text_puts(2, col + 2, "\x01\x01", C_GREEN);
            gpu_text_puts(3, col + 2, "\x01\x01", C_GREEN);
        }
    }
}

static void fg_reset_frog(void) {
    fg_fx = SCREEN_W / 2 - 12;
    fg_fy = 13 * FG_LANE_H + 4;
    fg_best_lane = 13;
    fg_time = FG_TIME_MAX;
}

static void fg_init_lanes(void) {
    for (int l = 2; l <= 12; l++) {
        if (FG_W[l] == 0) continue;
        fg_x[l][0] = (l * 97) % SCREEN_W;
        fg_x[l][1] = (fg_x[l][0] + SCREEN_W / 2) % SCREEN_W;
    }
}

static void fg_render(void) {
    shape_rect(0, 0, 2 * FG_LANE_H, SCREEN_W, 5 * FG_LANE_H, 0, 3, 12);
    shape_rect(1, 0, 8 * FG_LANE_H, SCREEN_W, 5 * FG_LANE_H, 3, 3, 3);
    for (int l = 2; l <= 12; l++) {
        if (FG_W[l] == 0) continue;
        int slot = (l <= 6) ? (2 + (l - 2) * 2) : (12 + (l - 8) * 2);
        for (int k = 0; k < 2; k++) {
            shape_rect(slot + k, fg_x[l][k], l * FG_LANE_H + 4, FG_W[l], 24,
                       FG_RGB[l][0], FG_RGB[l][1], FG_RGB[l][2]);
        }
    }
    shape_circ(FG_SLOT_FROG, fg_fx + 12, fg_fy + 12, 10, 2, 15, 2);
}

/* Returns true when the player loses their last frog. */
static bool fg_die(void) {
    fg_render();
    flash_bg(0x800U, FG_BG, 2, 1200000U);
    fg_lives--;
    fg_draw_hud();
    if (fg_lives == 0) return true;
    fg_reset_frog();
    flush_uart_rx();
    return false;
}

static void fg_add_score(uint32_t pts) {
    fg_score += pts;
    if (fg_score > fg_high) fg_high = fg_score;
}

static bool frogger_session(void) {
    gpu_reset_all(FG_BG);
    fg_score = 0; fg_level = 1; fg_lives = 3;
    uint32_t best_at_start = fg_high;
    for (int k = 0; k < FG_HOMES; k++) fg_home[k] = false;

    fg_init_lanes();
    fg_reset_frog();
    fg_draw_homes();
    fg_draw_hud();
    fg_draw_timer();
    fg_render();

    ready_banner("READY!");
    flush_uart_rx();

    while (1) {
        char c;
        while ((c = poll_key()) != 0) {
            sk_t sk = sys_key(c);
            if (sk == SK_QUIT) return false;
            if (sk == SK_RESUMED) { fg_draw_homes(); fg_draw_hud(); fg_draw_timer(); continue; }
            if (c == 'W') fg_fy -= FG_LANE_H;
            if (c == 'S' && fg_fy < 13 * FG_LANE_H) fg_fy += FG_LANE_H;
            if (c == 'A') fg_fx -= FG_LANE_H;
            if (c == 'D') fg_fx += FG_LANE_H;
            fg_fx = clampi(fg_fx, 0, SCREEN_W - 24);
            fg_fy = clampi(fg_fy, FG_LANE_H + 4, 13 * FG_LANE_H + 4);
            int lane = fg_fy / FG_LANE_H;
            if (lane < fg_best_lane) {
                fg_add_score(10);
                fg_best_lane = lane;
                fg_draw_hud();
            }
        }

        for (int l = 2; l <= 12; l++) {
            if (FG_W[l] == 0) continue;
            int sp = fg_speed(l);
            for (int k = 0; k < 2; k++) {
                fg_x[l][k] += sp;
                if (sp > 0 && fg_x[l][k] >= SCREEN_W)         fg_x[l][k] = -FG_W[l];
                else if (sp < 0 && fg_x[l][k] + FG_W[l] <= 0) fg_x[l][k] = SCREEN_W;
            }
        }

        bool dead = false;
        int lane = fg_fy / FG_LANE_H;

        if (lane >= 2 && lane <= 6) {
            int fcx = fg_fx + 12;
            bool on_log = false;
            for (int k = 0; k < 2; k++)
                if (fcx >= fg_x[lane][k] && fcx <= fg_x[lane][k] + FG_W[lane]) on_log = true;
            if (on_log) {
                fg_fx += fg_speed(lane);
                if (fg_fx < 0 || fg_fx > SCREEN_W - 24) dead = true;
            } else {
                dead = true;
            }
        } else if (lane >= 8 && lane <= 12) {
            for (int k = 0; k < 2; k++)
                if (aabb_overlap(fg_fx + 4, fg_fy + 4, 16, 16,
                                 fg_x[lane][k], lane * FG_LANE_H + 4, FG_W[lane], 24)) dead = true;
        } else if (lane == 1) {
            int fcx = fg_fx + 12;
            int hit = -1;
            for (int k = 0; k < FG_HOMES; k++) {
                int x0 = fg_home_col(k) * 8;
                if (fcx >= x0 && fcx < x0 + 48) hit = k;
            }
            if (hit < 0 || fg_home[hit]) {
                dead = true;
            } else {
                fg_home[hit] = true;
                fg_add_score(50 + (uint32_t)(fg_time / 20));
                fg_draw_homes();
                fg_draw_hud();
                bool all = true;
                for (int k = 0; k < FG_HOMES; k++) if (!fg_home[k]) all = false;
                if (all) {
                    fg_render();
                    gpu_text_center(8, "LEVEL CLEARED!", C_YELLOW);
                    fg_add_score(500);
                    flash_bg(0x060U, FG_BG, 3, 1500000U);
                    gpu_text_center_clear(8, "LEVEL CLEARED!");
                    fg_level++;
                    for (int k = 0; k < FG_HOMES; k++) fg_home[k] = false;
                    fg_draw_homes();
                    fg_draw_hud();
                }
                fg_reset_frog();
                flush_uart_rx();
            }
        }

        if (!dead) {
            if (--fg_time <= 0) dead = true;
            else fg_draw_timer();
        }

        if (dead) {
            if (fg_die()) {
                return game_over_screen(fg_score, fg_high, fg_score > best_at_start,
                                        "LEVEL", fg_level);
            }
            fg_draw_timer();
        }

        fg_render();
        delay_cycles(FG_FRAME);
    }
}

static void play_frogger(void) {
    while (frogger_session()) { }
}

/* ==========================================================================
 * TOP-LEVEL MENU (7 games)
 * ========================================================================== */
static void menu_entry(int row, int kcol, char key, const char *name,
                       uint8_t nr, uint8_t ng, uint8_t nb,
                       int lblcol, const char *lbl, uint32_t val) {
    char kb[4] = { '[', key, ']', 0 };
    gpu_text_puts(row, kcol, kb, C_YELLOW);
    gpu_text_puts(row, kcol + 4, name, nr, ng, nb);
    if (lbl) {
        gpu_text_puts(row, lblcol, lbl, C_GRAY);
        gpu_text_put_uint_pad(row, lblcol + text_len(lbl) + 1, val, 6, C_CYAN);
    }
}

static void menu_wins(int row, int col, uint32_t a, uint32_t b) {
    gpu_text_puts(row, col, "WINS", C_GRAY);
    gpu_text_put_uint_pad(row, col + 5, a, 2, C_GREEN);
    gpu_text_puts(row, col + 7, "-", C_GRAY);
    gpu_text_put_uint_pad(row, col + 8, b, 2, C_YELLOW);
}

static void draw_menu(void) {
    gpu_reset_all(0x002U);
    flush_uart_rx();

    gpu_text_box(1, 2, 28, 76, C_BLUE);
    gpu_text_center(3, "B A S Y S   3   A R C A D E", C_YELLOW);
    gpu_text_center(4, "F P G A   R E T R O   P L A T F O R M", C_CYAN);
    gpu_text_fill(6, 20, 40, CH_LINE_H, C_GRAY);
    gpu_text_center(8, "SELECT YOUR GAME", C_WHITE);

    /* Left column: 1-4 */
    menu_entry(10, 6, '1', "SNAKE",      C_GREEN,   22, "HI", s_high_score);
    menu_entry(12, 6, '2', "AIR HOCKEY", C_YELLOW,  22, NULL, 0);
    menu_wins (12, 22, hk_wins1, hk_wins2);
    menu_entry(14, 6, '3', "PAC-MAN",    C_MAGENTA, 22, "HI", pac_high_score);
    menu_entry(16, 6, '4', "PONG",       C_WHITE,   22, NULL, 0);
    menu_wins (16, 22, pg_wins_you, pg_wins_cpu);

    /* Right column: 5-7 */
    menu_entry(10, 40, '5', "BREAKOUT",  C_RED,     61, "HI", brk_high);
    menu_entry(12, 40, '6', "TETRIS",    C_CYAN,    61, "HI", tt_high);
    menu_entry(14, 40, '7', "FROGGER",   C_GREEN,   61, "HI", fg_high);

    gpu_text_fill(20, 20, 40, CH_LINE_H, C_GRAY);
    gpu_text_center(21, "CONTROLS", C_WHITE);
    gpu_text_center(22, "MOVE :  W A S D        FIRE / ACTION :  SPACE", C_GRAY);
    gpu_text_center(23, "AIR HOCKEY P2 :  I J K L        TETRIS ROTATE :  W", C_GRAY);
    gpu_text_center(24, "P = PAUSE        Q = QUIT TO MENU", C_GRAY);

    gpu_text_center(27, "BASYS 3  -  TEENYTINYGPU", C_GRAY);
}

static char wait_menu_choice(void) {
    char k;
    do {
        k = wait_key_blink(25, "PRESS 1-7 TO START", C_WHITE);
    } while (k < '1' || k > '7');
    return k;
}

int main(void) {
    while (1) {
        draw_menu();
        char choice = wait_menu_choice();

        switch (choice) {
            case '1': play_snake();      break;
            case '2': play_air_hockey(); break;
            case '3': play_pacman();     break;
            case '4': play_pong();       break;
            case '5': play_breakout();   break;
            case '6': play_tetris();     break;
            case '7': play_frogger();    break;
            default:                     break;
        }
    }
    return 0;
}
