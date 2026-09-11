# Project: Bare-Metal Ring 0 Video Playback Engine for A.A OS

## Architecture
The A.A OS Video Playback Engine is a Ring 0 bare-metal multimedia subsystem operating directly on x86 hardware, featuring:
1. **Display & Linear Framebuffer (LFB)**: Bochs/QEMU BGA (VBE) controller via I/O ports `0x01CE`/`0x01CF` and PCI Bus Master BAR0 MMIO address discovery (e.g. `0xFD000000`).
2. **Tearing-Free Double-Buffering**: System DRAM backbuffer (allocated in dynamic kernel RAM at physical `0x00800000` / heap `0x00200000+`) rendered off-screen, then blitted to VRAM in <1 ms via pure assembly `rep movsd` / SSE streaming non-temporal stores.
3. **Hardware PIT IRQ0 Frame Pacer**: 1000 Hz PIT Channel 0 (1 ms tick resolution) timer synchronization guaranteeing zero-drift 30 FPS / 60 FPS frame swaps via low-power `sti; hlt` loops with non-blocking PS/2 keyboard ring buffer polling (`ESC` / `'q'` / `'Q'`).
4. **Audio Tone Synchronization**: Multi-channel synchronized audio engine binding sound events (acoustic bounce thud, plasma chords, matrix hum) to frame timestamps via Motherboard PC Speaker (PIT Channel 2 / Port `0x61`) and AC'97 PCI DMA audio.
5. **Freestanding Fixed-Point Math & Video Streams**: Q16.16 integer fixed-point math and 256-entry trigonometry sine LUT for 100% genuine procedural video rendering (3D Bouncing Sphere Raycaster with dynamic Lambertian lighting & floor shadows, Multi-Wave Demoscene Plasma, Cyber Matrix Rain, and Full Showcase sequence) compatible with `-mno-80387 -nostdlib`.
6. **Kernel CLI Integration**: Shell commands `video.play [mode] [fps]`, `video.info`, `video.stop` (and aliases `vplay`, `vinfo`, `vstop`) wired into `kernel.c`.

## Feature Inventory
| # | Feature | Description | Milestone | Source |
|---|---------|-------------|-----------|--------|
| 1 | `bga_lfb_init` & BAR0 Probing | Detects BGA hardware, probes PCI BAR0 LFB physical address (e.g. 0xFD000000), sets 32bpp resolution (1024x768 / 800x600 / 640x480) | M1 | Survey / `kernel.c` |
| 2 | DRAM Double-Buffering & Blitter | Allocates 3MB backbuffer in DRAM, provides `_asm_video_blit_lfb` (`rep movsd`) for instant tearing-free blitting | M1 | Survey / `kernel_entry.asm` |
| 3 | PIT IRQ0 Frame Pacer | Configures PIT to 1000 Hz, paces frames at 30/60 FPS with zero cumulative jitter via `sti; hlt` loop | M2 | Survey / `hardware_io.asm` |
| 4 | Audio Synchronization Engine | Synchronizes audio tones with video playback frames via PC Speaker (Port 0x61) and AC'97 sound | M2 | Survey / `kernel.c` |
| 5 | Procedural 3D Raycaster Stream | Renders 3D bouncing sphere with diffuse lighting, specular highlight, and floor shadow in Q16.16 fixed point | M3 | Survey / ORIGINAL_REQUEST |
| 6 | Demoscene Plasma & Matrix Streams | Multi-wave sinusoidal plasma and falling cyber matrix rain with phosphor decay | M3 | Survey / ORIGINAL_REQUEST |
| 7 | Kernel Shell Integration | `video.play`, `video.info`, `video.stop` CLI commands with arguments, help text, and status telemetry | M3 | Survey / `kernel.c` |
| 8 | Opaque-Box E2E Testing Suite | Multi-tier test verification (Tiers 1-4 + Tier 5 adversarial) verifying build, commands, timing, and memory safety | M4 | E2E Testing Track |

## Milestones
| # | Name | Scope | Dependencies | Status |
|---|------|-------|-------------|--------|
| M1 | Video Engine Core & VBE Double-Buffering | BGA mode setting, PCI BAR0 MMIO resolution, DRAM backbuffer allocation, fast assembly blitter (`_asm_video_blit_lfb`), and 32bpp pixel drawing primitives | none | COMPLETE |
| M2 | PIT IRQ0 Frame Pacing & Audio Sync | 1000 Hz PIT timer configuration, frame pacing math (`target_tick`), non-blocking PS/2 input check, PC Speaker & AC'97 sound event sequencer | M1 | COMPLETE |
| M3 | Video Streams & Shell CLI Integration | 3D Raycaster, Multi-Wave Plasma, Matrix Rain generators, `video.play`, `video.info`, `video.stop` CLI handlers in `kernel.c` | M1, M2 | COMPLETE |
| M4 | E2E Verification & Adversarial Forensic Audit | Comprehensive 4-tier E2E testing, Tier 5 adversarial testing, and forensic integrity audit verification | M1, M2, M3 | COMPLETE |
| M5 | Preemptive & Cooperative Multitasking Subsystem | Task Control Blocks (TCB), 8KB stacks, PIT IRQ0 preemption, Round-Robin scheduler, `ps`, `task.*` CLI | M1, M2, M3 | COMPLETE |

## Interface Contracts
### `video_engine.h` ↔ `kernel.c`
- `int video_engine_init(uint32_t width, uint32_t height, uint32_t bpp);`
- `void video_engine_shutdown(void);`
- `void video_engine_clear_buffer(uint32_t color);`
- `void video_engine_draw_pixel(uint32_t x, uint32_t y, uint32_t color);`
- `void video_engine_blit_frame(void);`
- `void video_engine_pace_frame(uint32_t frame_index, uint32_t target_fps);`
- `void video_play_stream(int stream_id, uint32_t target_fps);`
- `void video_print_info(void);`
- `void video_stop_playback(void);`
- `int video_is_playing(void);`

## Code Layout
- `video.h` / `video.c`: Core video engine state, mode setting, backbuffer management, frame pacing, audio sync, procedural rendering streams, and shell commands.
- `kernel_entry.asm` / `video_asm.asm`: High-performance 32-bit assembly blitting routines (`_asm_video_blit_lfb` using `rep movsd` / SSE).
- `kernel.c`: Shell command dispatch (`execute_command`) routing `video.play`, `video.info`, `video.stop` to video engine.
- `build.ps1`: Build pipeline assembling and compiling all components into `os.img`.
- `tests/`: Automated test scripts and verification runners.
