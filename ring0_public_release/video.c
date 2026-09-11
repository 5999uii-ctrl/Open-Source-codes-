/* =========================================================================
 * A.A OS - Bare-Metal Ring 0 Video Playback Engine
 * Direct Silicon BGA/VBE Linear Framebuffer, DRAM Double-Buffering,
 * Hardware PIT IRQ0 Frame Pacing, PC Speaker Audio Sync, Freestanding Math
 * ========================================================================= */

#include "video.h"

/* Forward declarations of external kernel functions & data */
extern void vga_puts(const char* data);
extern void vga_puts_color(const char* data, uint8_t color);
extern void vga_putc(char c);
extern void vga_putc_color(char c, uint8_t color);
extern void vga_put_uint(uint32_t val);
extern void vga_put_int(int32_t val);
extern void vga_put_hex(uint32_t val);
extern void vga_put_hex8(uint8_t val);
extern void vga_put_hex16(uint16_t val);
extern void vga_clear_screen(void);
static inline uint8_t vga_entry_color(uint8_t fg, uint8_t bg) {
    return (uint8_t)(fg | (bg << 4));
}
extern void dump_memory_hex(uint32_t start_addr, uint32_t length);
extern uint32_t parse_hex(const char* str);
extern int atoi(const char* str);
extern uint32_t strlen(const char* str);
extern int strcmp(const char* s1, const char* s2);
extern int strncmp(const char* s1, const char* s2, uint32_t n);
extern void* memcpy(void* dest, const void* src, uint32_t len);
extern void* memset(void* dest, uint8_t val, uint32_t len);

/* Assembly VGA Text Mode Primitives (vga_driver.asm) */
extern void asm_vga_clear_screen(uint8_t color_attr);
extern void asm_vga_enable_hardware_cursor(uint8_t cursor_start, uint8_t cursor_end);
extern void asm_vga_update_hardware_cursor(uint16_t cursor_offset);

/* Hardware PCI Configuration Space (hardware_io.asm) */
extern uint32_t asm_pci_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
extern void asm_pci_write_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val);

static inline uint32_t pci_read_config_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    return asm_pci_read_dword(bus, slot, func, offset);
}

/* Hardware Port I/O Instructions (hardware_io.asm) */
extern uint8_t asm_inb(uint16_t port);
extern uint16_t asm_inw(uint16_t port);
extern uint32_t asm_inl(uint16_t port);
extern void asm_outb(uint16_t port, uint8_t val);
extern void asm_outw(uint16_t port, uint16_t val);
extern void asm_outl(uint16_t port, uint32_t val);

#ifndef inb
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
#endif

#ifndef outb
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
#endif

#ifndef inw
static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
#endif

#ifndef outw
static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}
#endif

#ifndef inl
static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
#endif

#ifndef outl
static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}
#endif

/* System Timer and Interrupt State */
extern volatile uint32_t system_timer_irq_ticks;

extern int asm_serial_is_data_ready(uint16_t base_port);
extern char asm_serial_read_char(uint16_t base_port);

static inline int serial_has_byte(void) {
    return asm_serial_is_data_ready(0x3F8);
}

static inline char serial_getc(void) {
    return asm_serial_read_char(0x3F8);
}

#define KBD_RING_SIZE 64
extern volatile uint8_t kbd_ring_buf[KBD_RING_SIZE];
extern volatile uint32_t kbd_ring_head;
extern volatile uint32_t kbd_ring_tail;

/* AC'97 Sound Functions */
extern int ac97_find_pci_controller(void);
extern int ac97_init_hardware(void);
extern void ac97_play_tone(uint32_t freq_hz, uint32_t duration_ms);
extern void ac97_stop(void);

/* Global Video Engine State Instance */
static struct video_engine_state g_video_engine;

/* =========================================================================
 * 1. Freestanding Q16.16 Fixed-Point Mathematics & Trigonometry Engine
 * ========================================================================= */

/* 256-entry Q16.16 Sine LUT: round(sin(i * 2*PI / 256) * 65536) */
static const fixed_t sin_lut[256] = {
         0,   1608,   3215,   4821,   6424,   8022,   9616,  11204,
     12785,  14359,  15924,  17479,  19024,  20557,  22078,  23586,
     25079,  26558,  28020,  29465,  30893,  32302,  33692,  35062,
     36410,  37736,  39040,  40319,  41575,  42806,  44011,  45190,
     46341,  47464,  48559,  49624,  50660,  51665,  52639,  53581,
     54491,  55368,  56212,  57022,  57797,  58538,  59243,  59913,
     60547,  61144,  61705,  62228,  62714,  63162,  63571,  63943,
     64276,  64571,  64826,  65043,  65220,  65358,  65457,  65516,
     65536,  65516,  65457,  65358,  65220,  65043,  64826,  64571,
     64276,  63943,  63571,  63162,  62714,  62228,  61705,  61144,
     60547,  59913,  59243,  58538,  57797,  57022,  56212,  55368,
     54491,  53581,  52639,  51665,  50660,  49624,  48559,  47464,
     46341,  45190,  44011,  42806,  41575,  40319,  39040,  37736,
     36410,  35062,  33692,  32302,  30893,  29465,  28020,  26558,
     25079,  23586,  22078,  20557,  19024,  17479,  15924,  14359,
     12785,  11204,   9616,   8022,   6424,   4821,   3215,   1608,
         0,  -1608,  -3215,  -4821,  -6424,  -8022,  -9616, -11204,
    -12785, -14359, -15924, -17479, -19024, -20557, -22078, -23586,
    -25079, -26558, -28020, -29465, -30893, -32302, -33692, -35062,
    -36410, -37736, -39040, -40319, -41575, -42806, -44011, -45190,
    -46341, -47464, -48559, -49624, -50660, -51665, -52639, -53581,
    -54491, -55368, -56212, -57022, -57797, -58538, -59243, -59913,
    -60547, -61144, -61705, -62228, -62714, -63162, -63571, -63943,
    -64276, -64571, -64826, -65043, -65220, -65358, -65457, -65516,
    -65536, -65516, -65457, -65358, -65220, -65043, -64826, -64571,
    -64276, -63943, -63571, -63162, -62714, -62228, -61705, -61144,
    -60547, -59913, -59243, -58538, -57797, -57022, -56212, -55368,
    -54491, -53581, -52639, -51665, -50660, -49624, -48559, -47464,
    -46341, -45190, -44011, -42806, -41575, -40319, -39040, -37736,
    -36410, -35062, -33692, -32302, -30893, -29465, -28020, -26558,
    -25079, -23586, -22078, -20557, -19024, -17479, -15924, -14359,
    -12785, -11204,  -9616,  -8022,  -6424,  -4821,  -3215,  -1608
};

static inline fixed_t fixed_mul(fixed_t a, fixed_t b) {
    return (fixed_t)(((int64_t)a * (int64_t)b) >> FIXED_SHIFT);
}

static inline fixed_t fixed_div(fixed_t a, fixed_t b) {
    if (b == 0) return (a >= 0) ? FIXED_MAX : FIXED_MIN;
    fixed_t res;
    int32_t eax = a << FIXED_SHIFT;
    int32_t edx = a >> FIXED_SHIFT;
    __asm__ __volatile__ (
        "idivl %2"
        : "=a"(res)
        : "a"(eax), "r"(b), "d"(edx)
    );
    return res;
}

static inline fixed_t fixed_sqrt(fixed_t x) {
    if (x <= 0) return 0;
    uint64_t n = (uint64_t)x << FIXED_SHIFT;
    uint64_t root = 0;
    uint64_t bit = (uint64_t)1 << 62;
    
    while (bit > n) {
        bit >>= 2;
    }
    while (bit != 0) {
        if (n >= root + bit) {
            n -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }
    return (fixed_t)root;
}

static inline fixed_t fixed_sin(uint8_t angle) {
    return sin_lut[angle];
}

static inline fixed_t fixed_cos(uint8_t angle) {
    return sin_lut[(uint8_t)(angle + 64)];
}

/* 3D Vector Math Primitives */
static inline vec3_fixed_t vec3_add(vec3_fixed_t a, vec3_fixed_t b) {
    vec3_fixed_t res;
    res.x = a.x + b.x;
    res.y = a.y + b.y;
    res.z = a.z + b.z;
    return res;
}

static inline vec3_fixed_t vec3_sub(vec3_fixed_t a, vec3_fixed_t b) {
    vec3_fixed_t res;
    res.x = a.x - b.x;
    res.y = a.y - b.y;
    res.z = a.z - b.z;
    return res;
}

static inline fixed_t vec3_dot(vec3_fixed_t a, vec3_fixed_t b) {
    return fixed_mul(a.x, b.x) + fixed_mul(a.y, b.y) + fixed_mul(a.z, b.z);
}

static inline vec3_fixed_t vec3_scale(vec3_fixed_t v, fixed_t s) {
    vec3_fixed_t res;
    res.x = fixed_mul(v.x, s);
    res.y = fixed_mul(v.y, s);
    res.z = fixed_mul(v.z, s);
    return res;
}

static inline vec3_fixed_t vec3_normalize(vec3_fixed_t v) {
    fixed_t len_sq = vec3_dot(v, v);
    fixed_t len = fixed_sqrt(len_sq);
    if (len == 0) {
        vec3_fixed_t forward = { 0, 0, FIXED_ONE };
        return forward;
    }
    return vec3_scale(v, fixed_div(FIXED_ONE, len));
}

/* BGA Registers and Probing */
static inline void bga_write_register(uint16_t index, uint16_t data) {
    outw(VBE_DISPI_IOPORT_INDEX, index);
    outw(VBE_DISPI_IOPORT_DATA, data);
}

static inline uint16_t bga_read_register(uint16_t index) {
    outw(VBE_DISPI_IOPORT_INDEX, index);
    return inw(VBE_DISPI_IOPORT_DATA);
}

extern int bga_is_available(void);
extern uint32_t bga_get_framebuffer_physical_address(void);


int video_pci_probe_lfb(uint32_t* out_lfb_base, uint8_t* out_bus, uint8_t* out_dev, uint8_t* out_func) {
    if (!out_lfb_base) return 0;
    
    for (uint32_t bus = 0; bus < 256; bus++) {
        for (uint8_t dev = 0; dev < 32; dev++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint32_t vend_dev = pci_read_config_dword((uint8_t)bus, dev, func, 0x00);
                uint16_t vendor = (uint16_t)(vend_dev & 0xFFFF);
                uint16_t device = (uint16_t)((vend_dev >> 16) & 0xFFFF);
                if (vendor == 0xFFFF || vendor == 0x0000) continue;

                uint32_t class_reg = pci_read_config_dword((uint8_t)bus, dev, func, 0x08);
                uint8_t base_class = (uint8_t)(class_reg >> 24);

                if (base_class == 0x03 || (vendor == 0x1234 && device == 0x1111) || (vendor == 0x80EE && device == 0xBEEF)) {
                    for (uint8_t bar_idx = 0; bar_idx < 6; bar_idx++) {
                        uint32_t bar = pci_read_config_dword((uint8_t)bus, dev, func, 0x10 + (bar_idx * 4));
                        if ((bar & 0x01) == 0 && (bar & 0xFFFFFFF0) >= 0x00100000) {
                            *out_lfb_base = bar & 0xFFFFFFF0;
                            if (out_bus)  *out_bus = (uint8_t)bus;
                            if (out_dev)  *out_dev = dev;
                            if (out_func) *out_func = func;

                            uint32_t cmd_reg = pci_read_config_dword((uint8_t)bus, dev, func, 0x04);
                            asm_pci_write_dword((uint8_t)bus, dev, func, 0x04, cmd_reg | 0x0006);
                            return 1;
                        }
                    }
                }
            }
        }
    }
    
    *out_lfb_base = VIDEO_DEFAULT_LFB_PHYS;
    return 1;
}

int bga_set_video_mode(uint32_t width, uint32_t height, uint32_t bpp) {
    if (!bga_is_available()) return 0;

    bga_write_register(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    bga_write_register(VBE_DISPI_INDEX_XRES, (uint16_t)width);
    bga_write_register(VBE_DISPI_INDEX_YRES, (uint16_t)height);
    bga_write_register(VBE_DISPI_INDEX_BPP,  (uint16_t)bpp);
    bga_write_register(VBE_DISPI_INDEX_VIRT_WIDTH,  (uint16_t)width);
    bga_write_register(VBE_DISPI_INDEX_VIRT_HEIGHT, (uint16_t)height);
    bga_write_register(VBE_DISPI_INDEX_X_OFFSET, 0);
    bga_write_register(VBE_DISPI_INDEX_Y_OFFSET, 0);
    bga_write_register(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED | VBE_DISPI_NOCLEARMEM);

    return 1;
}

void video_engine_restore_text_mode(void) {
    bga_write_register(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    asm_vga_enable_hardware_cursor(14, 15);
    asm_vga_clear_screen(0x07);
    asm_vga_update_hardware_cursor(0);
}

/* =========================================================================
 * 3. Video Engine Core & Drawing Primitives
 * ========================================================================= */

int video_engine_init(uint32_t width, uint32_t height, uint32_t bpp) {
    if (width == 0 || height == 0) {
        width = 1024;
        height = 768;
    }
    if (width > 1920) width = 1024;
    if (height > 1080) height = 768;
    if (bpp != 8 && bpp != 16 && bpp != 24 && bpp != 32) bpp = 32;

    uint32_t total_bytes = width * height * (bpp / 8);
    if (total_bytes > VIDEO_BACKBUFFER_CAPACITY) {
        width = 1024;
        height = 768;
        bpp = 32;
        total_bytes = width * height * 4;
    }

    uint32_t lfb_base = VIDEO_DEFAULT_LFB_PHYS;
    uint8_t bus = 0, dev = 0, func = 0;
    video_pci_probe_lfb(&lfb_base, &bus, &dev, &func);

    if (!bga_is_available()) {
        return 0;
    }

    if (!bga_set_video_mode(width, height, bpp)) {
        return 0;
    }

    g_video_engine.mode.width                = width;
    g_video_engine.mode.height               = height;
    g_video_engine.mode.bpp                  = bpp;
    g_video_engine.mode.pitch_bytes          = width * (bpp / 8);
    g_video_engine.mode.pitch_dwords         = width;
    g_video_engine.mode.total_pixels         = width * height;
    g_video_engine.mode.total_bytes          = total_bytes;
    g_video_engine.mode.total_dwords         = width * height;
    g_video_engine.mode.vram_phys_addr       = lfb_base;
    g_video_engine.mode.dram_backbuffer_phys = VIDEO_DRAM_BACKBUFFER_PHYS;
    g_video_engine.mode.is_active            = 1;
    g_video_engine.mode.pci_bus              = bus;
    g_video_engine.mode.pci_dev              = dev;
    g_video_engine.mode.pci_func             = func;
    g_video_engine.mode.bga_version          = bga_read_register(VBE_DISPI_INDEX_ID);

    g_video_engine.vram_ptr       = (volatile uint32_t*)lfb_base;
    g_video_engine.backbuffer_ptr = (volatile uint32_t*)VIDEO_DRAM_BACKBUFFER_PHYS;
    g_video_engine.current_frame  = 0;
    g_video_engine.total_frames   = 0;
    g_video_engine.target_fps     = 60;
    g_video_engine.rendered_frames_total = 0;
    g_video_engine.dropped_frames_total  = 0;
    g_video_engine.is_playing     = 1;
    g_video_engine.is_paused      = 0;
    g_video_engine.show_hud       = 1;

    asm_video_clear_lfb((void*)VIDEO_DRAM_BACKBUFFER_PHYS, 0x00000000, g_video_engine.mode.total_dwords);
    asm_video_blit_lfb((void*)lfb_base, (const void*)VIDEO_DRAM_BACKBUFFER_PHYS, g_video_engine.mode.total_dwords);

    return 1;
}

void video_engine_shutdown(void) {
    if (!g_video_engine.mode.is_active && !g_video_engine.is_playing) return;
    video_audio_shutdown(&g_video_engine.audio);
    video_pit_restore_rate(100);
    video_engine_restore_text_mode();
    g_video_engine.mode.is_active = 0;
    g_video_engine.is_playing = 0;
    g_video_engine.is_paused = 0;
}

void video_engine_clear_buffer(uint32_t color) {
    if (!g_video_engine.mode.is_active) return;
    asm_video_clear_lfb((void*)g_video_engine.backbuffer_ptr, color, g_video_engine.mode.total_dwords);
}

void video_engine_draw_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!g_video_engine.mode.is_active) return;
    if (x >= g_video_engine.mode.width || y >= g_video_engine.mode.height) return;
    g_video_engine.backbuffer_ptr[y * g_video_engine.mode.width + x] = color;
}

void video_engine_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    if (!g_video_engine.mode.is_active) return;
    if (x >= g_video_engine.mode.width || y >= g_video_engine.mode.height) return;
    if (x + w > g_video_engine.mode.width)  w = g_video_engine.mode.width - x;
    if (y + h > g_video_engine.mode.height) h = g_video_engine.mode.height - y;
    if (w == 0 || h == 0) return;

    asm_video_draw_rect_lfb((void*)g_video_engine.backbuffer_ptr, g_video_engine.mode.width, x, y, w, h, color);
}

void video_engine_draw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color) {
    if (!g_video_engine.mode.is_active) return;
    int32_t dx = (x1 >= x0) ? (x1 - x0) : (x0 - x1);
    int32_t sx = (x0 < x1) ? 1 : -1;
    int32_t dy = (y1 >= y0) ? (y0 - y1) : (y1 - y0);
    int32_t sy = (y0 < y1) ? 1 : -1;
    int32_t err = dx + dy;

    while (1) {
        if (x0 >= 0 && (uint32_t)x0 < g_video_engine.mode.width &&
            y0 >= 0 && (uint32_t)y0 < g_video_engine.mode.height) {
            g_video_engine.backbuffer_ptr[y0 * g_video_engine.mode.width + x0] = color;
        }
        if (x0 == x1 && y0 == y1) break;
        int32_t e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void video_engine_draw_circle(int32_t cx, int32_t cy, int32_t radius, uint32_t color) {
    if (!g_video_engine.mode.is_active || radius <= 0) return;
    int32_t x = radius;
    int32_t y = 0;
    int32_t err = 0;

    while (x >= y) {
        video_engine_draw_pixel(cx + x, cy + y, color);
        video_engine_draw_pixel(cx + y, cy + x, color);
        video_engine_draw_pixel(cx - y, cy + x, color);
        video_engine_draw_pixel(cx - x, cy + y, color);
        video_engine_draw_pixel(cx - x, cy - y, color);
        video_engine_draw_pixel(cx - y, cy - x, color);
        video_engine_draw_pixel(cx + y, cy - x, color);
        video_engine_draw_pixel(cx + x, cy - y, color);

        if (err <= 0) {
            y += 1;
            err += 2 * y + 1;
        }
        if (err > 0) {
            x -= 1;
            err -= 2 * x + 1;
        }
    }
}

void video_engine_blit_frame(void) {
    if (!g_video_engine.mode.is_active) return;
    asm_video_blit_lfb((void*)g_video_engine.vram_ptr, (const void*)g_video_engine.backbuffer_ptr, g_video_engine.mode.total_dwords);
    g_video_engine.rendered_frames_total++;
    g_video_engine.current_frame++;
}

int video_is_playing(void) {
    return g_video_engine.is_playing;
}

int video_is_active(void) {
    return g_video_engine.mode.is_active;
}

const struct video_mode_info* video_get_current_mode(void) {
    return &g_video_engine.mode;
}

/* =========================================================================
 * 4. Embedded 8x16 Bitmap Font Engine & OSD Primitives
 * ========================================================================= */

/* Minimal ASCII 8x16 font glyph patterns for uppercase, lowercase, numbers, symbols */
static const uint8_t font_8x16_basic[96][16] = {
    /* 0x20 ' ' */ {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x21 '!' */ {0x00,0x00,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00,0x00,0x18,0x18,0x00,0x00,0x00},
    /* 0x22 '"' */ {0x00,0x36,0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x23 '#' */ {0x00,0x00,0x36,0x36,0x7F,0x36,0x36,0x36,0x7F,0x36,0x36,0x00,0x00,0x00,0x00,0x00},
    /* 0x24 '$' */ {0x00,0x18,0x3E,0x60,0x3C,0x06,0x7C,0x18,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x25 '%' */ {0x00,0x00,0x62,0x66,0x0C,0x18,0x30,0x66,0x46,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x26 '&' */ {0x00,0x00,0x38,0x6C,0x38,0x76,0xDC,0xCC,0x76,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x27 ''' */ {0x00,0x18,0x18,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x28 '(' */ {0x00,0x0C,0x18,0x30,0x30,0x30,0x30,0x18,0x0C,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x29 ')' */ {0x00,0x30,0x18,0x0C,0x0C,0x0C,0x0C,0x18,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x2A '*' */ {0x00,0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x2B '+' */ {0x00,0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x2C ',' */ {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x2D '-' */ {0x00,0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x2E '.' */ {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x2F '/' */ {0x00,0x02,0x06,0x0C,0x18,0x30,0x60,0xC0,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x30 '0' */ {0x00,0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x31 '1' */ {0x00,0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x32 '2' */ {0x00,0x3C,0x66,0x06,0x0C,0x18,0x30,0x7E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x33 '3' */ {0x00,0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x34 '4' */ {0x00,0x0C,0x1C,0x34,0x64,0x7E,0x04,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x35 '5' */ {0x00,0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x36 '6' */ {0x00,0x1C,0x30,0x60,0x7C,0x66,0x66,0x3C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x37 '7' */ {0x00,0x7E,0x06,0x0C,0x18,0x30,0x30,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x38 '8' */ {0x00,0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x39 '9' */ {0x00,0x3C,0x66,0x66,0x3E,0x06,0x0C,0x38,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x3A ':' */ {0x00,0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x3B ';' */ {0x00,0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x3C '<' */ {0x00,0x06,0x18,0x60,0x60,0x18,0x06,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x3D '=' */ {0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x3E '>' */ {0x00,0x60,0x18,0x06,0x06,0x18,0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x3F '?' */ {0x00,0x3C,0x66,0x06,0x0C,0x18,0x00,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x40 '@' */ {0x00,0x3C,0x42,0x99,0xA5,0xA5,0x9E,0x40,0x3C,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x41 'A' */ {0x00,0x18,0x3C,0x66,0x66,0x7E,0x66,0x66,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x42 'B' */ {0x00,0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x43 'C' */ {0x00,0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x44 'D' */ {0x00,0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x45 'E' */ {0x00,0x7E,0x60,0x60,0x7C,0x60,0x60,0x7E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x46 'F' */ {0x00,0x7E,0x60,0x60,0x7C,0x60,0x60,0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x47 'G' */ {0x00,0x3C,0x66,0x60,0x6E,0x66,0x66,0x3A,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x48 'H' */ {0x00,0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x49 'I' */ {0x00,0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x4A 'J' */ {0x00,0x0E,0x06,0x06,0x06,0x06,0x66,0x3C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x4B 'K' */ {0x00,0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x4C 'L' */ {0x00,0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x4D 'M' */ {0x00,0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x4E 'N' */ {0x00,0x66,0x76,0x7E,0x7E,0x6E,0x66,0x66,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x4F 'O' */ {0x00,0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x50 'P' */ {0x00,0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x51 'Q' */ {0x00,0x3C,0x66,0x66,0x66,0x66,0x6E,0x3C,0x06,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x52 'R' */ {0x00,0x7C,0x66,0x66,0x7C,0x6C,0x66,0x63,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x53 'S' */ {0x00,0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x54 'T' */ {0x00,0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x55 'U' */ {0x00,0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x56 'V' */ {0x00,0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x57 'W' */ {0x00,0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x58 'X' */ {0x00,0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x59 'Y' */ {0x00,0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x5A 'Z' */ {0x00,0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x5B '[' */ {0x00,0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x5C '\' */ {0x00,0x80,0xC0,0x60,0x30,0x18,0x0C,0x06,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x5D ']' */ {0x00,0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x5E '^' */ {0x00,0x18,0x3C,0x66,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x5F '_' */ {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x60 '`' */ {0x00,0x30,0x18,0x0C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x61 'a' */ {0x00,0x00,0x00,0x3C,0x06,0x3E,0x66,0x3E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x62 'b' */ {0x00,0x60,0x60,0x7C,0x66,0x66,0x66,0x7C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x63 'c' */ {0x00,0x00,0x00,0x3C,0x66,0x60,0x66,0x3C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x64 'd' */ {0x00,0x06,0x06,0x3E,0x66,0x66,0x66,0x3E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x65 'e' */ {0x00,0x00,0x00,0x3C,0x66,0x7E,0x60,0x3C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x66 'f' */ {0x00,0x1C,0x30,0x7C,0x30,0x30,0x30,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x67 'g' */ {0x00,0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x3C,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x68 'h' */ {0x00,0x60,0x60,0x7C,0x66,0x66,0x66,0x66,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x69 'i' */ {0x00,0x18,0x00,0x38,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x6A 'j' */ {0x00,0x0C,0x00,0x0C,0x0C,0x0C,0x0C,0x4C,0x38,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x6B 'k' */ {0x00,0x60,0x60,0x66,0x6C,0x78,0x6C,0x66,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x6C 'l' */ {0x00,0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x6D 'm' */ {0x00,0x00,0x00,0x66,0x7F,0x7F,0x6B,0x63,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x6E 'n' */ {0x00,0x00,0x00,0x7C,0x66,0x66,0x66,0x66,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x6F 'o' */ {0x00,0x00,0x00,0x3C,0x66,0x66,0x66,0x3C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x70 'p' */ {0x00,0x00,0x00,0x7C,0x66,0x66,0x7C,0x60,0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x71 'q' */ {0x00,0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x06,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x72 'r' */ {0x00,0x00,0x00,0x7C,0x66,0x60,0x60,0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x73 's' */ {0x00,0x00,0x00,0x3E,0x60,0x3C,0x06,0x7C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x74 't' */ {0x00,0x18,0x18,0x7E,0x18,0x18,0x18,0x0E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x75 'u' */ {0x00,0x00,0x00,0x66,0x66,0x66,0x66,0x3E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x76 'v' */ {0x00,0x00,0x00,0x66,0x66,0x66,0x3C,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x77 'w' */ {0x00,0x00,0x00,0x63,0x6B,0x7F,0x3E,0x36,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x78 'x' */ {0x00,0x00,0x00,0x66,0x3C,0x18,0x3C,0x66,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x79 'y' */ {0x00,0x00,0x00,0x66,0x66,0x66,0x3E,0x06,0x3C,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x7A 'z' */ {0x00,0x00,0x00,0x7E,0x0C,0x18,0x30,0x7E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x7B '{' */ {0x00,0x0E,0x18,0x18,0x70,0x18,0x18,0x0E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x7C '|' */ {0x00,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x7D '}' */ {0x00,0x70,0x18,0x18,0x0E,0x18,0x18,0x70,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x7E '~' */ {0x00,0x76,0xDC,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x7F ' ' */ {0x00,0x7E,0x7E,0x7E,0x7E,0x7E,0x7E,0x7E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}
};

void video_draw_glyph_32bpp(uint32_t px, uint32_t py, uint8_t char_code, uint32_t color) {
    if (!g_video_engine.mode.is_active) return;
    if (char_code < 32 || char_code > 127) char_code = '?';
    const uint8_t* glyph = font_8x16_basic[char_code - 32];
    uint32_t screen_w = g_video_engine.mode.width;
    uint32_t screen_h = g_video_engine.mode.height;

    for (int row = 0; row < 16; row++) {
        uint32_t cur_y = py + row;
        if (cur_y >= screen_h) break;
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            uint32_t cur_x = px + col;
            if (cur_x >= screen_w) break;
            if (bits & (0x80 >> col)) {
                g_video_engine.backbuffer_ptr[cur_y * screen_w + cur_x] = color;
            }
        }
    }
}

void video_draw_string_32bpp(uint32_t px, uint32_t py, const char* str, uint32_t color) {
    if (!str || !g_video_engine.mode.is_active) return;
    uint32_t cur_x = px;
    while (*str) {
        if (*str == '\n') {
            py += 18;
            cur_x = px;
        } else {
            video_draw_glyph_32bpp(cur_x, py, (uint8_t)*str, color);
            cur_x += 9;
        }
        str++;
    }
}

/* =========================================================================
 * 5. PIT IRQ0 Frame Pacing & Non-Blocking Interactive Control
 * ========================================================================= */

void video_pit_init_1000hz(void) {
    uint16_t divisor = 1193; // 1,193,182 / 1000 = 1193 (0x04A9)
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

void video_pit_restore_rate(uint32_t freq_hz) {
    if (freq_hz == 0) freq_hz = 100;
    uint32_t divisor = 1193182 / freq_hz;
    if (divisor > 65535) divisor = 0;
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

static inline uint32_t video_calc_frame_target_tick(uint32_t start_tick, uint32_t frame_index, uint32_t target_fps) {
    if (target_fps == 0) target_fps = 60;
    return start_tick + (uint32_t)(((uint64_t)(frame_index + 1) * 1000) / target_fps);
}

void video_sync_pace_frame(uint32_t start_tick, uint32_t frame_index, uint32_t target_fps) {
    uint32_t target_tick = video_calc_frame_target_tick(start_tick, frame_index, target_fps);

    if (system_timer_irq_ticks >= target_tick) {
        g_video_engine.dropped_frames_total++;
        return;
    }

    while (system_timer_irq_ticks < target_tick) {
        __asm__ volatile ("sti\n\thlt");
    }
}

void video_engine_pace_frame(uint32_t frame_index, uint32_t target_fps) {
    video_sync_pace_frame(g_video_engine.start_tick, frame_index, target_fps);
}

video_action_t video_poll_interactive_input(void) {
    /* 1. Poll Keyboard Ring Buffer from IRQ1 */
    while (kbd_ring_head != kbd_ring_tail) {
        uint8_t scancode = kbd_ring_buf[kbd_ring_tail];
        kbd_ring_tail = (kbd_ring_tail + 1) % KBD_RING_SIZE;

        if (scancode & 0x80) continue;

        switch (scancode) {
            case 0x01: /* ESC */
            case 0x10: /* 'Q' / 'q' */
                return VIDEO_ACT_EXIT;
            case 0x39: /* Space */
                return VIDEO_ACT_PAUSE_TOGGLE;
            case 0x32: /* 'M' */
                return VIDEO_ACT_MUTE_TOGGLE;
            case 0x13: /* 'R' */
                return VIDEO_ACT_RESTART;
            case 0x0D: /* '+' / '=' */
            case 0x4E:
                return VIDEO_ACT_SPEED_UP;
            case 0x0C: /* '-' */
            case 0x4A:
                return VIDEO_ACT_SLOW_DOWN;
            case 0x23: /* 'H' */
            case 0x3B: /* F1 */
                return VIDEO_ACT_OSD_TOGGLE;
            case 0x02: /* '1' */
            case 0x03: /* '2' */
            case 0x04: /* '3' */
            case 0x05: /* '4' */
                return VIDEO_ACT_SWITCH_STREAM;
            default:
                break;
        }
    }

    /* 2. Direct 8042 Keyboard Controller Status Port Fallback Check */
    uint8_t status = inb(0x64);
    if (status & 0x01) {
        if ((status & 0x20) == 0) {
            uint8_t scancode = inb(0x60);
            if (!(scancode & 0x80)) {
                if (scancode == 0x01 || scancode == 0x10) return VIDEO_ACT_EXIT;
                if (scancode == 0x39) return VIDEO_ACT_PAUSE_TOGGLE;
                if (scancode == 0x32) return VIDEO_ACT_MUTE_TOGGLE;
                if (scancode == 0x23 || scancode == 0x3B) return VIDEO_ACT_OSD_TOGGLE;
                if (scancode >= 0x02 && scancode <= 0x05) return VIDEO_ACT_SWITCH_STREAM;
            }
        }
    }

    /* 3. Serial Port COM1 UART Check */
    if (serial_has_byte()) {
        char ch = serial_getc();
        if (ch == 'q' || ch == 'Q' || ch == 27) return VIDEO_ACT_EXIT;
        if (ch == ' ') return VIDEO_ACT_PAUSE_TOGGLE;
        if (ch == 'm' || ch == 'M') return VIDEO_ACT_MUTE_TOGGLE;
        if (ch == 'r' || ch == 'R') return VIDEO_ACT_RESTART;
        if (ch == 'h' || ch == 'H') return VIDEO_ACT_OSD_TOGGLE;
        if (ch == '+' || ch == '=') return VIDEO_ACT_SPEED_UP;
        if (ch == '-') return VIDEO_ACT_SLOW_DOWN;
        if (ch >= '1' && ch <= '4') return VIDEO_ACT_SWITCH_STREAM;
    }

    return VIDEO_ACT_NONE;
}

/* =========================================================================
 * 6. Audio Synchronization Engine (PC Speaker + AC'97 DMA)
 * ========================================================================= */

void video_audio_speaker_tone(uint32_t freq_hz) {
    if (freq_hz == 0) {
        outb(0x61, inb(0x61) & 0xFC);
        return;
    }
    if (freq_hz < 20) freq_hz = 20;
    if (freq_hz > 20000) freq_hz = 20000;

    uint32_t divisor = 1193182 / freq_hz;
    if (divisor > 65535) divisor = 65535;
    if (divisor < 1) divisor = 1;

    outb(0x43, 0xB6);
    outb(0x42, (uint8_t)(divisor & 0xFF));
    outb(0x42, (uint8_t)((divisor >> 8) & 0xFF));

    uint8_t p61 = inb(0x61);
    if ((p61 & 0x03) != 0x03) {
        outb(0x61, p61 | 0x03);
    }
}

void video_audio_speaker_mute(void) {
    outb(0x61, inb(0x61) & 0xFC);
}

void video_audio_init(video_audio_engine_t* audio, const video_sound_event_t* track, uint32_t track_count) {
    if (!audio) return;
    audio->soundtrack = track;
    audio->total_events = track_count;
    audio->current_event_idx = 0;
    audio->audio_muted = 0;
    audio->ac97_available = ac97_find_pci_controller() && ac97_init_hardware();
}

void video_audio_update_frame(video_audio_engine_t* audio, uint32_t frame_index) {
    if (!audio || audio->audio_muted || !audio->soundtrack) {
        video_audio_speaker_mute();
        return;
    }

    while (audio->current_event_idx < audio->total_events &&
           frame_index >= audio->soundtrack[audio->current_event_idx].frame_end) {
        audio->current_event_idx++;
    }

    if (audio->current_event_idx < audio->total_events &&
        frame_index >= audio->soundtrack[audio->current_event_idx].frame_start) {
        uint32_t freq = audio->soundtrack[audio->current_event_idx].freq_hz;
        if (freq > 0) {
            video_audio_speaker_tone(freq);
            if (audio->ac97_available) {
                ac97_play_tone(freq, 50);
            }
        } else {
            video_audio_speaker_mute();
        }
    } else {
        video_audio_speaker_mute();
    }
}

void video_audio_shutdown(video_audio_engine_t* audio) {
    video_audio_speaker_mute();
    if (audio && audio->ac97_available) {
        ac97_stop();
    }
}

/* Soundtracks for Procedural Streams */
static const video_sound_event_t track_sphere[] = {
    {  30,  34, 110 }, /* Bounce 1 Thud (A2) */
    {  34,  36,  82 },
    {  75,  79, 120 }, /* Bounce 2 */
    { 115, 119, 130 }, /* Bounce 3 */
    { 150, 154, 140 }, /* Bounce 4 */
    { 180, 184, 150 }, /* Bounce 5 */
    { 205, 209, 160 }, /* Bounce 6 */
    { 225, 228, 175 }, /* Bounce 7 */
    { 240, 243, 190 }, /* Bounce 8 */
    { 252, 255, 210 }, /* Bounce 9 */
    { 262, 265, 230 }, /* Bounce 10 */
    { 270, 273, 250 }, /* Settling */
    { 280, 300,   0 }  /* Rest */
};

static const video_sound_event_t track_plasma[] = {
    {   0,  15, 220 }, {  15,  30, 261 }, {  30,  45, 330 }, {  45,  60, 440 }, /* Am */
    {  60,  75, 174 }, {  75,  90, 220 }, {  90, 105, 261 }, { 105, 120, 349 }, /* F  */
    { 120, 135, 130 }, { 135, 150, 196 }, { 150, 165, 261 }, { 165, 180, 330 }, /* C  */
    { 180, 195, 196 }, { 195, 210, 246 }, { 210, 225, 293 }, { 225, 240, 392 }, /* G  */
    { 240, 255, 220 }, { 255, 270, 261 }, { 270, 285, 330 }, { 285, 300, 440 }  /* Am */
};

static const video_sound_event_t track_matrix[] = {
    {   0,  10, 880 }, {  10,  20, 1760 }, {  30,  40,  440 }, {  50,  60, 1320 },
    {  80,  90, 880 }, { 100, 110, 1760 }, { 140, 150,  660 }, { 180, 190, 2200 },
    { 220, 230, 880 }, { 250, 260, 1760 }, { 280, 290, 1320 }
};

/* =========================================================================
 * 7. Procedural Video Streams Rendering Implementations
 * ========================================================================= */

/* Stream 1: 3D Raycaster Bouncing Sphere */
void video_render_sphere_raycaster(uint32_t frame_index, uint32_t target_fps) {
    (void)target_fps;
    uint32_t width = g_video_engine.mode.width;
    uint32_t height = g_video_engine.mode.height;
    volatile uint32_t* fb = g_video_engine.backbuffer_ptr;

    /* Physics & Dynamic Orbiting Light */
    uint8_t angle_light = (uint8_t)(frame_index * 2);
    fixed_t light_x = fixed_mul(fixed_cos(angle_light), INT_TO_FIXED(2));
    fixed_t light_y = INT_TO_FIXED(2);
    fixed_t light_z = INT_TO_FIXED(2) + fixed_mul(fixed_sin(angle_light), INT_TO_FIXED(2));
    vec3_fixed_t light_pos = { light_x, light_y, light_z };

    /* Sphere Bouncing Physics (Height oscillating smoothly with gravity bounce simulation) */
    uint8_t bounce_phase = (uint8_t)(frame_index * 4);
    fixed_t sin_val = fixed_sin(bounce_phase);
    if (sin_val < 0) sin_val = -sin_val;
    fixed_t sphere_y = -35000 + fixed_mul(sin_val, 70000);
    fixed_t sphere_radius = 45000; /* ~0.68 in Q16 */
    fixed_t radius_sq = fixed_mul(sphere_radius, sphere_radius);
    vec3_fixed_t sphere_center = { 0, sphere_y, INT_TO_FIXED(3) };

    vec3_fixed_t cam_pos = { 0, 0, 0 };
    fixed_t ground_y = -40000; /* Ground plane Y */

    /* Screen Coordinates Transformation (Step size = 2 for performance scaling on high-res) */
    uint32_t step = (width >= 1024) ? 2 : 1;

    for (uint32_t sy = 0; sy < height; sy += step) {
        fixed_t vy = fixed_div(INT_TO_FIXED((int32_t)(height / 2) - (int32_t)sy), INT_TO_FIXED(height / 2));
        for (uint32_t sx = 0; sx < width; sx += step) {
            fixed_t vx = fixed_div(INT_TO_FIXED((int32_t)sx - (int32_t)(width / 2)), INT_TO_FIXED(height / 2));
            vec3_fixed_t ray_dir = { vx, vy, FIXED_ONE };
            ray_dir = vec3_normalize(ray_dir);

            /* Ray-Sphere Intersection */
            vec3_fixed_t oc = vec3_sub(cam_pos, sphere_center);
            fixed_t b_term = vec3_dot(oc, ray_dir);
            fixed_t c_term = vec3_dot(oc, oc) - radius_sq;
            fixed_t disc = fixed_mul(b_term, b_term) - c_term;

            uint32_t pixel_color;

            if (disc >= 0) {
                /* Sphere Hit! */
                fixed_t sqrt_disc = fixed_sqrt(disc);
                fixed_t t_hit = -b_term - sqrt_disc;
                if (t_hit < 0) t_hit = -b_term + sqrt_disc;

                vec3_fixed_t hit_pt = vec3_add(cam_pos, vec3_scale(ray_dir, t_hit));
                vec3_fixed_t norm = vec3_normalize(vec3_sub(hit_pt, sphere_center));
                vec3_fixed_t light_dir = vec3_normalize(vec3_sub(light_pos, hit_pt));

                /* Lambertian Diffuse (N . L) */
                fixed_t diff = vec3_dot(norm, light_dir);
                if (diff < 0) diff = 0;

                /* Specular Highlight (Phong: R . V) */
                vec3_fixed_t reflect_v = vec3_sub(vec3_scale(norm, fixed_mul(2 * FIXED_ONE, diff)), light_dir);
                vec3_fixed_t view_dir = vec3_scale(ray_dir, -FIXED_ONE);
                fixed_t spec = vec3_dot(reflect_v, view_dir);
                if (spec < 0) spec = 0;
                fixed_t spec4 = fixed_mul(fixed_mul(spec, spec), fixed_mul(spec, spec));
                fixed_t spec16 = fixed_mul(spec4, spec4);

                uint32_t r = 30 + FIXED_TO_INT(fixed_mul(diff, INT_TO_FIXED(210))) + FIXED_TO_INT(fixed_mul(spec16, INT_TO_FIXED(220)));
                uint32_t g = 20 + FIXED_TO_INT(fixed_mul(diff, INT_TO_FIXED(100))) + FIXED_TO_INT(fixed_mul(spec16, INT_TO_FIXED(220)));
                uint32_t b = 40 + FIXED_TO_INT(fixed_mul(diff, INT_TO_FIXED(180))) + FIXED_TO_INT(fixed_mul(spec16, INT_TO_FIXED(220)));
                if (r > 255) r = 255;
                if (g > 255) g = 255;
                if (b > 255) b = 255;
                pixel_color = COLOR_RGB(r, g, b);
            } else if (ray_dir.y < 0) {
                /* Ground Plane Hit */
                fixed_t t_plane = fixed_div(ground_y - cam_pos.y, ray_dir.y);
                if (t_plane > 0) {
                    vec3_fixed_t grd_pt = vec3_add(cam_pos, vec3_scale(ray_dir, t_plane));
                    int32_t gx = FIXED_TO_INT(grd_pt.x * 2);
                    int32_t gz = FIXED_TO_INT(grd_pt.z * 2);
                    int is_dark = (gx + gz) & 1;
                    uint32_t tile_c = is_dark ? 0x001A2530 : 0x002C3E50;

                    /* Soft Shadow Projection on Floor */
                    fixed_t sh_dx = grd_pt.x - sphere_center.x;
                    fixed_t sh_dz = grd_pt.z - sphere_center.z;
                    fixed_t sh_dist_sq = fixed_mul(sh_dx, sh_dx) + fixed_mul(sh_dz, sh_dz);
                    if (sh_dist_sq < 25000000) {
                        tile_c = 0x000A1018; /* In Shadow */
                    }
                    pixel_color = tile_c;
                } else {
                    pixel_color = 0x000A0E17;
                }
            } else {
                /* Sky Background Gradient */
                uint32_t sky_b = 30 + (sy * 40) / height;
                pixel_color = COLOR_RGB(10, 15, sky_b);
            }

            if (step == 1) {
                fb[sy * width + sx] = pixel_color;
            } else {
                fb[sy * width + sx] = pixel_color;
                if (sx + 1 < width) fb[sy * width + sx + 1] = pixel_color;
                if (sy + 1 < height) {
                    fb[(sy + 1) * width + sx] = pixel_color;
                    if (sx + 1 < width) fb[(sy + 1) * width + sx + 1] = pixel_color;
                }
            }
        }
    }
}

/* Stream 2: Multi-Wave Demoscene Plasma */
void video_render_plasma_wave(uint32_t frame_index, uint32_t target_fps) {
    (void)target_fps;
    uint32_t width = g_video_engine.mode.width;
    uint32_t height = g_video_engine.mode.height;
    volatile uint32_t* fb = g_video_engine.backbuffer_ptr;

    uint8_t t1 = (uint8_t)(frame_index * 3);
    uint8_t t2 = (uint8_t)(frame_index * 2);
    uint8_t t3 = (uint8_t)(frame_index * 4);
    uint8_t t4 = (uint8_t)(frame_index * 1);

    uint32_t cx = width / 2;
    uint32_t cy = height / 2;
    uint32_t step = (width >= 1024) ? 2 : 1;

    for (uint32_t y = 0; y < height; y += step) {
        uint8_t angle_y = (uint8_t)((y << 1) + t2);
        fixed_t w2 = sin_lut[angle_y];

        for (uint32_t x = 0; x < width; x += step) {
            uint8_t angle_x = (uint8_t)((x << 1) + t1);
            fixed_t w1 = sin_lut[angle_x];

            uint8_t angle_diag = (uint8_t)((x + y) + t3);
            fixed_t w3 = sin_lut[angle_diag];

            int32_t dx = (int32_t)x - (int32_t)cx;
            int32_t dy = (int32_t)y - (int32_t)cy;
            fixed_t dist = fixed_sqrt(INT_TO_FIXED(dx * dx + dy * dy));
            uint8_t angle_circ = (uint8_t)((FIXED_TO_INT(dist) << 2) + t4);
            fixed_t w4 = sin_lut[angle_circ];

            fixed_t sum = w1 + w2 + w3 + w4;
            int32_t p_idx = (int32_t)(((sum + 262144) >> 11) & 0xFF);

            uint8_t r = (uint8_t)(128 + (fixed_sin((uint8_t)(p_idx + t1)) >> 9));
            uint8_t g = (uint8_t)(128 + (fixed_sin((uint8_t)(p_idx * 2 + t2)) >> 9));
            uint8_t b = (uint8_t)(128 + (fixed_cos((uint8_t)(p_idx + t3)) >> 9));

            uint32_t color = COLOR_RGB(r, g, b);

            if (step == 1) {
                fb[y * width + x] = color;
            } else {
                fb[y * width + x] = color;
                if (x + 1 < width) fb[y * width + x + 1] = color;
                if (y + 1 < height) {
                    fb[(y + 1) * width + x] = color;
                    if (x + 1 < width) fb[(y + 1) * width + x + 1] = color;
                }
            }
        }
    }
}

/* Stream 3: Cyber Matrix Rain */
#define MATRIX_MAX_COLS 128
static matrix_column_t g_matrix_cols[MATRIX_MAX_COLS];
static int g_matrix_initialized = 0;

static void matrix_rain_init(uint32_t num_cols) {
    for (uint32_t i = 0; i < num_cols && i < MATRIX_MAX_COLS; i++) {
        g_matrix_cols[i].y_head = INT_TO_FIXED(-((int32_t)(i * 3) % 40));
        g_matrix_cols[i].speed = 20000 + (fixed_t)((i * 37) % 35000);
        g_matrix_cols[i].length = 12 + (uint16_t)((i * 7) % 20);
        g_matrix_cols[i].seed = (uint16_t)(0x1337 + i * 19);
        g_matrix_cols[i].active = 1;
        g_matrix_cols[i].respawn_delay = 0;
    }
    g_matrix_initialized = 1;
}

void video_render_matrix_rain(uint32_t frame_index, uint32_t target_fps) {
    (void)target_fps;
    (void)frame_index;
    uint32_t width = g_video_engine.mode.width;
    uint32_t height = g_video_engine.mode.height;
    uint32_t num_cols = width / 10;
    if (num_cols > MATRIX_MAX_COLS) num_cols = MATRIX_MAX_COLS;

    if (!g_matrix_initialized) {
        matrix_rain_init(num_cols);
    }

    /* Fast Phosphor Fade of previous frame */
    volatile uint32_t* fb = g_video_engine.backbuffer_ptr;
    uint32_t total = g_video_engine.mode.total_dwords;
    for (uint32_t i = 0; i < total; i++) {
        uint32_t c = fb[i];
        if (c != 0) {
            uint32_t r = (c >> 16) & 0xFF;
            uint32_t g = (c >> 8) & 0xFF;
            uint32_t b = c & 0xFF;
            r = (r * 80) / 100;
            g = (g * 85) / 100;
            b = (b * 80) / 100;
            if (g < 12) {
                fb[i] = 0;
            } else {
                fb[i] = COLOR_RGB(r, g, b);
            }
        }
    }

    /* Render and Advance Column Drops */
    uint32_t rows_max = height / 16;

    for (uint32_t c = 0; c < num_cols; c++) {
        matrix_column_t* col = &g_matrix_cols[c];
        int32_t head_row = FIXED_TO_INT(col->y_head);
        uint32_t px = c * 10;

        for (int32_t r = 0; r <= head_row && r < (int32_t)rows_max; r++) {
            int32_t dist = head_row - r;
            if (dist < 0 || dist > (int32_t)col->length) continue;

            uint32_t py = (uint32_t)r * 16;
            uint8_t char_code = 33 + (uint8_t)((col->seed + r * 13) % 94);

            uint32_t glyph_color;
            if (dist == 0) {
                glyph_color = 0x00E0FFE0; /* Neon White Head */
            } else if (dist <= 2) {
                glyph_color = 0x0050FF70; /* Cyan-Green */
            } else {
                uint32_t g_intensity = 255 - (uint32_t)((dist * 180) / col->length);
                if (g_intensity < 40) g_intensity = 40;
                glyph_color = COLOR_RGB(g_intensity / 5, g_intensity, g_intensity / 6);
            }

            video_draw_glyph_32bpp(px, py, char_code, glyph_color);
        }

        col->y_head += col->speed;
        if (FIXED_TO_INT(col->y_head) - (int32_t)col->length > (int32_t)rows_max) {
            col->y_head = INT_TO_FIXED(-((int32_t)(col->seed % 10)));
            col->seed = (uint16_t)(col->seed * 1103515245 + 12345);
        }
    }
}

/* Stream 4: Rotating Showcase Demo */
void video_render_showcase_demo(uint32_t frame_index, uint32_t target_fps) {
    uint32_t cycle_frames = 180 + 300 + 300 + 300; /* 1080 frames = 18s at 60fps */
    uint32_t local_frame = frame_index % cycle_frames;

    if (local_frame < 180) {
        /* Intro Banner Phase */
        video_engine_clear_buffer(0x000F141D);
        video_draw_string_32bpp(200, 240, "==========================================================", 0x0000FFFF);
        video_draw_string_32bpp(200, 270, "       A.A OS BARE-METAL RING 0 VIDEO ENGINE              ", 0x0000FF00);
        video_draw_string_32bpp(200, 300, "==========================================================", 0x0000FFFF);
        video_draw_string_32bpp(200, 340, " * Mode: VBE 1024x768x32bpp Linear Framebuffer (0xFD000000)", 0x00E0E0E0);
        video_draw_string_32bpp(200, 370, " * Double-Buffer: DRAM 4MB at 0x00800000 (rep movsd blit) ", 0x00E0E0E0);
        video_draw_string_32bpp(200, 400, " * Pacing: Intel 8254 PIT 1000Hz Zero-Drift Hardware Sync  ", 0x00E0E0E0);
        video_draw_string_32bpp(200, 430, " * Audio: PC Speaker Port 0x61 + Intel AC'97 DMA Engine    ", 0x00E0E0E0);
        video_draw_string_32bpp(200, 480, " Loading Showcase: Sphere -> Plasma -> Matrix Rain...    ", 0x00FFFF00);
    } else if (local_frame < 480) {
        /* Phase 1: 3D Raycaster Bouncing Sphere */
        video_render_sphere_raycaster(local_frame - 180, target_fps);
    } else if (local_frame < 780) {
        /* Phase 2: Multi-Wave Demoscene Plasma */
        video_render_plasma_wave(local_frame - 480, target_fps);
    } else {
        /* Phase 3: Cyber Matrix Rain */
        video_render_matrix_rain(local_frame - 780, target_fps);
    }
}

/* Diagnostic OSD HUD Overlay */
void video_render_hud(uint32_t frame_index, uint32_t total_frames, uint32_t target_fps, uint32_t elapsed_ms) {
    (void)total_frames;
    uint32_t width = g_video_engine.mode.width;
    video_engine_draw_rect(0, 0, width, 32, 0x00000000);
    video_engine_draw_line(0, 32, width, 32, 0x0000FFCC);

    char hud_buf[128];
    const char* stream_name = "DEMO SHOWCASE";
    if (g_video_engine.active_stream_id == STREAM_SPHERE_3D) stream_name = "3D SPHERE RAYCASTER";
    else if (g_video_engine.active_stream_id == STREAM_PLASMA_WAVE) stream_name = "DEMOSCENE PLASMA";
    else if (g_video_engine.active_stream_id == STREAM_MATRIX_RAIN) stream_name = "CYBER MATRIX RAIN";

    video_draw_string_32bpp(16, 8, "[A.A OS VIDEO] STREAM: ", 0x0000FFCC);
    video_draw_string_32bpp(210, 8, stream_name, 0x00FFFF00);

    /* FPS and Tick Telemetry */
    video_draw_string_32bpp(440, 8, "| FPS:", 0x00AAAAAA);
    if (target_fps == 60) video_draw_string_32bpp(500, 8, "60.0", 0x0000FF00);
    else video_draw_string_32bpp(500, 8, "30.0", 0x0000FF00);

    video_draw_string_32bpp(550, 8, "| TICKS:", 0x00AAAAAA);
    uint32_t t_sec = elapsed_ms / 1000;
    uint32_t t_ms = elapsed_ms % 1000;
    hud_buf[0] = (char)('0' + (t_sec / 10) % 10);
    hud_buf[1] = (char)('0' + t_sec % 10);
    hud_buf[2] = '.';
    hud_buf[3] = (char)('0' + (t_ms / 100) % 10);
    hud_buf[4] = 's';
    hud_buf[5] = '\0';
    video_draw_string_32bpp(625, 8, hud_buf, 0x0000FFCC);

    video_draw_string_32bpp(width - 240, 8, "[ESC=Exit  SPACE=Pause]", 0x00FFFFFF);
}

void video_render_stream_frame(int stream_id, uint32_t frame_index, uint32_t target_fps) {
    if (stream_id == STREAM_SPHERE_3D) {
        video_render_sphere_raycaster(frame_index, target_fps);
    } else if (stream_id == STREAM_PLASMA_WAVE) {
        video_render_plasma_wave(frame_index, target_fps);
    } else if (stream_id == STREAM_MATRIX_RAIN) {
        video_render_matrix_rain(frame_index, target_fps);
    } else {
        video_render_showcase_demo(frame_index, target_fps);
    }
}

/* =========================================================================
 * 8. Video Playback Main Engine Loop
 * ========================================================================= */

void video_play_stream(int stream_id, uint32_t target_fps) {
    if (target_fps == 0) target_fps = 60;
    if (target_fps > 120) target_fps = 120;

    if (!video_engine_init(1024, 768, 32)) {
        vga_puts_color("[ERROR] Failed to initialize BGA VBE Video Controller!\n", vga_entry_color(12, 0));
        return;
    }

    g_video_engine.active_stream_id = (uint8_t)stream_id;
    g_video_engine.target_fps = target_fps;

    /* Reprogram PIT Channel 0 to 1000 Hz */
    video_pit_init_1000hz();

    /* Setup Audio Track Profile */
    const video_sound_event_t* soundtrack = track_sphere;
    uint32_t track_count = sizeof(track_sphere) / sizeof(track_sphere[0]);
    if (stream_id == STREAM_PLASMA_WAVE) {
        soundtrack = track_plasma;
        track_count = sizeof(track_plasma) / sizeof(track_plasma[0]);
    } else if (stream_id == STREAM_MATRIX_RAIN) {
        soundtrack = track_matrix;
        track_count = sizeof(track_matrix) / sizeof(track_matrix[0]);
    } else if (stream_id == STREAM_SHOWCASE) {
        soundtrack = track_plasma;
        track_count = sizeof(track_plasma) / sizeof(track_plasma[0]);
    }

    video_audio_init(&g_video_engine.audio, soundtrack, track_count);

    uint32_t start_tick = system_timer_irq_ticks;
    g_video_engine.start_tick = start_tick;
    uint32_t frame = 0;
    int is_paused = 0;
    int show_hud = 1;
    g_matrix_initialized = 0;

    while (g_video_engine.is_playing) {
        /* 1. Poll Interactive Controls */
        video_action_t act = video_poll_interactive_input();
        if (act == VIDEO_ACT_EXIT) {
            break;
        } else if (act == VIDEO_ACT_PAUSE_TOGGLE) {
            is_paused = !is_paused;
            g_video_engine.is_paused = (uint8_t)is_paused;
            if (is_paused) {
                video_audio_speaker_mute();
            } else {
                start_tick = system_timer_irq_ticks - (uint32_t)(((uint64_t)frame * 1000) / target_fps);
                g_video_engine.start_tick = start_tick;
            }
        } else if (act == VIDEO_ACT_MUTE_TOGGLE) {
            g_video_engine.audio.audio_muted = !g_video_engine.audio.audio_muted;
            if (g_video_engine.audio.audio_muted) video_audio_speaker_mute();
        } else if (act == VIDEO_ACT_RESTART) {
            frame = 0;
            start_tick = system_timer_irq_ticks;
            g_video_engine.start_tick = start_tick;
            g_video_engine.audio.current_event_idx = 0;
            continue;
        } else if (act == VIDEO_ACT_SPEED_UP) {
            if (target_fps < 120) target_fps += 5;
            g_video_engine.target_fps = target_fps;
            start_tick = system_timer_irq_ticks - (uint32_t)(((uint64_t)frame * 1000) / target_fps);
            g_video_engine.start_tick = start_tick;
        } else if (act == VIDEO_ACT_SLOW_DOWN) {
            if (target_fps > 5) target_fps -= 5;
            g_video_engine.target_fps = target_fps;
            start_tick = system_timer_irq_ticks - (uint32_t)(((uint64_t)frame * 1000) / target_fps);
            g_video_engine.start_tick = start_tick;
        } else if (act == VIDEO_ACT_OSD_TOGGLE) {
            show_hud = !show_hud;
            g_video_engine.show_hud = (uint8_t)show_hud;
        } else if (act == VIDEO_ACT_SWITCH_STREAM) {
            g_video_engine.active_stream_id = (g_video_engine.active_stream_id + 1) % 4;
            stream_id = g_video_engine.active_stream_id;
            g_matrix_initialized = 0;
        }

        /* 2. Paused State Wait */
        if (is_paused) {
            __asm__ volatile ("sti\n\thlt");
            continue;
        }

        /* 3. Audio Update */
        video_audio_update_frame(&g_video_engine.audio, frame);

        /* 4. Render Active Stream */
        video_render_stream_frame(stream_id, frame, target_fps);

        /* 5. Diagnostic HUD */
        if (show_hud) {
            video_render_hud(frame, 0, target_fps, system_timer_irq_ticks - start_tick);
        }

        /* 6. Hardware Fast Blit to VRAM */
        video_engine_blit_frame();

        /* 7. Zero-Drift Frame Pacing */
        video_sync_pace_frame(start_tick, frame, target_fps);

        frame++;
    }

    /* Teardown and Clean Restoration */
    video_engine_shutdown();
}

void video_stop_playback(void) {
    g_video_engine.is_playing = 0;
    video_engine_shutdown();
}

void video_print_info(void) {
    vga_clear_screen();
    vga_puts_color("===============================================================================\n", vga_entry_color(11, 0));
    vga_puts_color("  [VIDEO ENGINE] RING 0 BARE-METAL MULTIMEDIA SUBSYSTEM STATUS                 \n", vga_entry_color(10, 0));
    vga_puts_color("===============================================================================\n", vga_entry_color(11, 0));

    int bga_ok = bga_is_available();
    vga_puts("  * Video Playback State        : ");
    if (g_video_engine.is_playing) vga_puts_color("[ACTIVE / PLAYING]\n", vga_entry_color(10, 0));
    else vga_puts("[IDLE / STOPPED - Native 80x25 VGA Text Mode]\n");

    vga_puts("  * BGA GPU Controller Hardware : ");
    if (bga_ok) {
        uint16_t id = bga_read_register(VBE_DISPI_INDEX_ID);
        vga_puts("0x"); vga_put_hex16(id);
        if (id == 0xB0C5) vga_puts(" (Bochs/QEMU VBE 5.0 Linear Framebuffer)\n");
        else vga_puts(" (Bochs Compatible BGA Controller)\n");
    } else {
        vga_puts_color("[NOT DETECTED]\n", vga_entry_color(12, 0));
    }

    uint32_t lfb_phys = bga_get_framebuffer_physical_address();
    vga_puts("  * Display Resolution          : ");
    vga_put_uint(g_video_engine.mode.width ? g_video_engine.mode.width : 1024);
    vga_puts(" x ");
    vga_put_uint(g_video_engine.mode.height ? g_video_engine.mode.height : 768);
    vga_puts(" (");
    vga_put_uint(g_video_engine.mode.bpp ? g_video_engine.mode.bpp : 32);
    vga_puts(" Bits Per Pixel)\n");

    vga_puts("  * Frame Pitch (Stride)        : 4096 Bytes / Row (3,145,728 Bytes / Frame)\n");
    vga_puts("  * Physical LFB BAR0 MMIO Base : 0x");
    vga_put_hex(lfb_phys ? lfb_phys : VIDEO_DEFAULT_LFB_PHYS);
    vga_puts(" [PCI Display Controller Class 0x03]\n");

    vga_puts("  * System DRAM Backbuffer Base : 0x00800000 [4 MB Dedicated Kernel RAM]\n");
    vga_puts("  * PIT IRQ0 Frame Pacer Timer  : 1000 Hz (1 ms Tick Resolution, 0x04A9 Divisor)\n");
    vga_puts("  * Audio Synchronization Engine: PC Motherboard Speaker (Port 0x61) + AC'97 DMA\n");

    vga_puts_color("-------------------------------------------------------------------------------\n", vga_entry_color(11, 0));
    vga_puts_color("PERFORMANCE & RENDER TELEMETRY:\n", vga_entry_color(14, 0));
    vga_puts("  * Total Frames Rendered       : "); vga_put_uint(g_video_engine.rendered_frames_total); vga_puts(" frames\n");
    vga_puts("  * Total Frames Dropped        : "); vga_put_uint(g_video_engine.dropped_frames_total); vga_puts(" frames\n");
    vga_puts("  * Assembly Blit Mechanism     : _asm_video_blit_lfb (rep movsd ERMS Streaming)\n");
    vga_puts("  * Procedural Streams Supported: sphere (3D), plasma (Wave), matrix (Rain), demo\n");
    vga_puts("  * Available Commands          : 'video.play [mode] [fps]', 'video.info', 'video.stop'\n");
    vga_puts_color("===============================================================================\n", vga_entry_color(11, 0));
}

/* =========================================================================
 * 9. Shell CLI Handlers
 * ========================================================================= */

void cmd_video_play(const char* args) {
    while (*args == ' ') args++;
    
    char stream_str[32];
    int si = 0;
    while (*args && *args != ' ' && si < 31) {
        stream_str[si++] = *args++;
    }
    stream_str[si] = '\0';
    while (*args == ' ') args++;

    uint32_t fps = *args ? (uint32_t)atoi(args) : 60;
    if (fps == 0) fps = 60;
    if (fps > 120) fps = 120;

    int stream_id = STREAM_SHOWCASE;
    if (strcmp(stream_str, "sphere") == 0 || strcmp(stream_str, "3d") == 0 || strcmp(stream_str, "1") == 0) {
        stream_id = STREAM_SPHERE_3D;
    } else if (strcmp(stream_str, "plasma") == 0 || strcmp(stream_str, "wave") == 0 || strcmp(stream_str, "2") == 0) {
        stream_id = STREAM_PLASMA_WAVE;
    } else if (strcmp(stream_str, "matrix") == 0 || strcmp(stream_str, "rain") == 0 || strcmp(stream_str, "3") == 0) {
        stream_id = STREAM_MATRIX_RAIN;
    } else if (strcmp(stream_str, "demo") == 0 || strcmp(stream_str, "all") == 0 || strcmp(stream_str, "4") == 0 || stream_str[0] == '\0') {
        stream_id = STREAM_SHOWCASE;
    } else {
        vga_puts_color("[ERROR] Unknown video stream: '", vga_entry_color(12, 0));
        vga_puts_color(stream_str, vga_entry_color(12, 0));
        vga_puts_color("'\n", vga_entry_color(12, 0));
        vga_puts("Usage: video.play [sphere | plasma | matrix | demo] [fps: 30/60]\n");
        return;
    }

    video_play_stream(stream_id, fps);
}

void cmd_video_info(void) {
    video_print_info();
}

void cmd_video_stop(void) {
    video_stop_playback();
    vga_puts_color("[VIDEO ENGINE] Playback Stopped. 80x25 Text Mode Restored.\n", vga_entry_color(10, 0));
}
