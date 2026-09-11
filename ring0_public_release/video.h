/* =========================================================================
 * A.A OS - Bare-Metal Ring 0 Video Playback Engine Header
 * Direct Silicon BGA/VBE Linear Framebuffer, DRAM Double-Buffering,
 * Hardware PIT IRQ0 Frame Pacing, PC Speaker Audio Sync, Freestanding Math
 * ========================================================================= */

#ifndef _AAS_VIDEO_H_
#define _AAS_VIDEO_H_

#ifndef _KERNEL_TYPES_DEFINED_
#define _KERNEL_TYPES_DEFINED_
typedef unsigned char      uint8_t;
typedef signed char        int8_t;
typedef unsigned short     uint16_t;
typedef signed short       int16_t;
typedef unsigned int       uint32_t;
typedef int                int32_t;
typedef unsigned long long uint64_t;
typedef long long          int64_t;
#endif

/* Physical Memory Layout Constants */
#define VIDEO_DRAM_BACKBUFFER_PHYS 0x00800000 /* 8 MB Mark (4 MB Capacity) */
#define VIDEO_BACKBUFFER_CAPACITY  0x00400000 /* 4,194,304 Bytes (4 MB) */
#define VIDEO_DEFAULT_LFB_PHYS     0xFD000000 /* Default Bochs/QEMU BGA BAR0 */

/* Bochs Graphics Adapter (BGA) / VBE Dispi Ports & Registers */
#define VBE_DISPI_IOPORT_INDEX           0x01CE
#define VBE_DISPI_IOPORT_DATA            0x01CF

#define VBE_DISPI_INDEX_ID               0x00
#define VBE_DISPI_INDEX_XRES             0x01
#define VBE_DISPI_INDEX_YRES             0x02
#define VBE_DISPI_INDEX_BPP              0x03
#define VBE_DISPI_INDEX_ENABLE           0x04
#define VBE_DISPI_INDEX_BANK             0x05
#define VBE_DISPI_INDEX_VIRT_WIDTH       0x06
#define VBE_DISPI_INDEX_VIRT_HEIGHT      0x07
#define VBE_DISPI_INDEX_X_OFFSET         0x08
#define VBE_DISPI_INDEX_Y_OFFSET         0x09
#define VBE_DISPI_INDEX_VIDEO_MEMORY_64K 0x0A

/* BGA Hardware Version Signatures */
#define VBE_DISPI_ID0                    0xB0C0
#define VBE_DISPI_ID1                    0xB0C1
#define VBE_DISPI_ID2                    0xB0C2
#define VBE_DISPI_ID3                    0xB0C3
#define VBE_DISPI_ID4                    0xB0C4
#define VBE_DISPI_ID5                    0xB0C5

/* BGA Mode Enable Flags */
#define VBE_DISPI_DISABLED               0x00
#define VBE_DISPI_ENABLED                0x01
#define VBE_DISPI_GETCAPS                0x02
#define VBE_DISPI_8BIT_DAC               0x20
#define VBE_DISPI_LFB_ENABLED            0x40
#define VBE_DISPI_NOCLEARMEM             0x80

/* Color Depth Values */
#define VBE_DISPI_BPP_4                  0x04
#define VBE_DISPI_BPP_8                  0x08
#define VBE_DISPI_BPP_15                 0x0F
#define VBE_DISPI_BPP_16                 0x10
#define VBE_DISPI_BPP_24                 0x18
#define VBE_DISPI_BPP_32                 0x20

/* 32bpp Color Packing Macros */
#define COLOR_RGB(r, g, b)       (((uint32_t)((r) & 0xFF) << 16) | ((uint32_t)((g) & 0xFF) << 8) | ((uint32_t)((b) & 0xFF)))
#define COLOR_ARGB(a, r, g, b)   (((uint32_t)((a) & 0xFF) << 24) | ((uint32_t)((r) & 0xFF) << 16) | ((uint32_t)((g) & 0xFF) << 8) | ((uint32_t)((b) & 0xFF)))

/* Freestanding Q16.16 Fixed-Point Definitions */
typedef int32_t fixed_t;

#define FIXED_SHIFT         16
#define FIXED_ONE           (1 << FIXED_SHIFT)          /* 65536 (1.0) */
#define FIXED_HALF          (1 << (FIXED_SHIFT - 1))    /* 32768 (0.5) */
#define FIXED_PI            205887                      /* 3.14159265 * 65536 */
#define FIXED_TWO_PI        411775                      /* 6.28318531 * 65536 */
#define FIXED_HALF_PI       102944                      /* 1.57079633 * 65536 */
#define FIXED_MAX           0x7FFFFFFF
#define FIXED_MIN           (-0x7FFFFFFF - 1)

#define INT_TO_FIXED(x)     ((fixed_t)((int32_t)(x) << FIXED_SHIFT))
#define FIXED_TO_INT(x)     ((int32_t)((x) >> FIXED_SHIFT))
#define FIXED_TO_INT_ROUND(x) (((x) + FIXED_HALF) >> FIXED_SHIFT)
#define FIXED_ADD(a, b)     ((a) + (b))
#define FIXED_SUB(a, b)     ((a) - (b))

/* 3D Fixed-Point Vector Struct */
typedef struct {
    fixed_t x;
    fixed_t y;
    fixed_t z;
} vec3_fixed_t;

/* Video Mode Information Structure */
struct video_mode_info {
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t pitch_bytes;
    uint32_t pitch_dwords;
    uint32_t total_pixels;
    uint32_t total_bytes;
    uint32_t total_dwords;
    uint32_t vram_phys_addr;
    uint32_t dram_backbuffer_phys;
    uint8_t  is_active;
    uint8_t  pci_bus;
    uint8_t  pci_dev;
    uint8_t  pci_func;
    uint16_t bga_version;
};

/* Audio Sound Event Definition */
typedef struct {
    uint32_t frame_start;
    uint32_t frame_end;
    uint32_t freq_hz;
} video_sound_event_t;

/* Video Audio Engine State Structure */
typedef struct {
    int ac97_available;
    int audio_muted;
    uint32_t current_event_idx;
    uint32_t total_events;
    const video_sound_event_t* soundtrack;
} video_audio_engine_t;

/* Interactive Action Enumeration */
typedef enum {
    VIDEO_ACT_NONE = 0,
    VIDEO_ACT_EXIT,
    VIDEO_ACT_PAUSE_TOGGLE,
    VIDEO_ACT_MUTE_TOGGLE,
    VIDEO_ACT_RESTART,
    VIDEO_ACT_SPEED_UP,
    VIDEO_ACT_SLOW_DOWN,
    VIDEO_ACT_OSD_TOGGLE,
    VIDEO_ACT_SWITCH_STREAM
} video_action_t;

/* Video Engine Playback State */
typedef enum {
    VIDEO_STATE_IDLE = 0,
    VIDEO_STATE_PLAYING,
    VIDEO_STATE_PAUSED
} video_state_enum_t;

/* Procedural Stream IDs */
typedef enum {
    STREAM_SHOWCASE = 0,
    STREAM_SPHERE_3D = 1,
    STREAM_PLASMA_WAVE = 2,
    STREAM_MATRIX_RAIN = 3
} video_stream_id_t;

/* Matrix Rain Column Structure */
typedef struct {
    fixed_t  y_head;
    fixed_t  speed;
    uint16_t length;
    uint16_t seed;
    uint8_t  active;
    uint8_t  respawn_delay;
} matrix_column_t;

/* Global Video Engine State Structure */
struct video_engine_state {
    struct video_mode_info mode;
    volatile uint32_t*     vram_ptr;
    volatile uint32_t*     backbuffer_ptr;
    uint32_t               current_frame;
    uint32_t               total_frames;
    uint32_t               target_fps;
    uint32_t               start_tick;
    uint32_t               rendered_frames_total;
    uint32_t               dropped_frames_total;
    uint32_t               last_frame_tick;
    uint8_t                is_playing;
    uint8_t                is_paused;
    uint8_t                active_stream_id;
    uint8_t                show_hud;
    video_audio_engine_t   audio;
};

/* Pure Assembly Video Engine Routines (vga_driver.asm / memorymanagement.asm) */
extern void asm_video_blit_lfb(void* dest_vram, const void* src_dram, uint32_t dword_count);
extern void asm_video_clear_lfb(void* dest_buf, uint32_t color, uint32_t dword_count);
extern void asm_video_draw_rect_lfb(void* dest_buf, uint32_t pitch_dwords, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);

/* Video Engine Core API */
int  video_engine_init(uint32_t width, uint32_t height, uint32_t bpp);
void video_engine_shutdown(void);
void video_engine_clear_buffer(uint32_t color);
void video_engine_draw_pixel(uint32_t x, uint32_t y, uint32_t color);
void video_engine_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void video_engine_draw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color);
void video_engine_draw_circle(int32_t cx, int32_t cy, int32_t radius, uint32_t color);
void video_engine_blit_frame(void);
void video_engine_pace_frame(uint32_t frame_index, uint32_t target_fps);
void video_play_stream(int stream_id, uint32_t target_fps);
void video_print_info(void);
void video_stop_playback(void);
int  video_is_playing(void);
int  video_is_active(void);
const struct video_mode_info* video_get_current_mode(void);

/* Hardware PCI & BGA Primitives */
int      video_pci_probe_lfb(uint32_t* out_lfb_base, uint8_t* out_bus, uint8_t* out_dev, uint8_t* out_func);
int      bga_set_video_mode(uint32_t width, uint32_t height, uint32_t bpp);
void     video_engine_restore_text_mode(void);

/* PIT Timing & Interactive Control API */
void     video_pit_init_1000hz(void);
void     video_pit_restore_rate(uint32_t freq_hz);
void     video_sync_pace_frame(uint32_t start_tick, uint32_t frame_index, uint32_t target_fps);
video_action_t video_poll_interactive_input(void);

/* Audio Synchronization API */
void     video_audio_speaker_tone(uint32_t freq_hz);
void     video_audio_speaker_mute(void);
void     video_audio_init(video_audio_engine_t* audio, const video_sound_event_t* track, uint32_t track_count);
void     video_audio_update_frame(video_audio_engine_t* audio, uint32_t frame_index);
void     video_audio_shutdown(video_audio_engine_t* audio);

/* Procedural Stream Renderers */
void     video_render_stream_frame(int stream_id, uint32_t frame_index, uint32_t target_fps);
void     video_render_sphere_raycaster(uint32_t frame_index, uint32_t target_fps);
void     video_render_plasma_wave(uint32_t frame_index, uint32_t target_fps);
void     video_render_matrix_rain(uint32_t frame_index, uint32_t target_fps);
void     video_render_showcase_demo(uint32_t frame_index, uint32_t target_fps);
void     video_render_hud(uint32_t frame_index, uint32_t total_frames, uint32_t target_fps, uint32_t elapsed_ms);

/* Glyph and String Drawing Primitives */
void     video_draw_glyph_32bpp(uint32_t px, uint32_t py, uint8_t char_code, uint32_t color);
void     video_draw_string_32bpp(uint32_t px, uint32_t py, const char* str, uint32_t color);

/* Kernel CLI Command Handlers */
void     cmd_video_play(const char* args);
void     cmd_video_info(void);
void     cmd_video_stop(void);

#endif /* _AAS_VIDEO_H_ */
