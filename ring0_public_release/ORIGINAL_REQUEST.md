# Original User Request

## Initial Request — 2026-08-30T07:19:19Z

A.A OS me Ring 0 level par bare-metal Video Playback Engine implement karna. Platform raw RGB frame sequence stream, VBE Linear Framebuffer (LFB) double-buffering, aur Hardware PIT IRQ0 timer timing synchronization use karega taake 30/60 FPS par smooth video frames render ho sakein.

Working directory: c:/Users/ALLAH/OneDrive/Desktop/my_os
Integrity mode: development

## Requirements

### R1. Ring 0 Bare-Metal Video Engine & Double-Buffering
- Implement a Ring 0 video decoder and renderer in `kernel.c` / assembly.
- Utilize VBE Linear Framebuffer (LFB) with off-screen double-buffering (`0xFD000000` / VRAM) for tearing-free 32bpp/24bpp frame blitting.

### R2. Hardware PIT IRQ0 Frame Pacing & Audio Sync
- Synchronize frame swaps with Intel 8254 PIT (Programmable Interval Timer) IRQ0 hardware interrupt ticks at target 30 FPS / 60 FPS.
- Integrate with PC Speaker / AC'97 sound driver for synced audio tone playback during video sequence.

### R3. Kernel Shell Integration & Test Video Stream
- Add `video.play`, `video.info`, `video.stop` shell commands to A.A OS.
- Embed a procedural / encoded test video sequence (e.g. animated 3D bouncing sphere / video matrix stream) to verify end-to-end bare-metal video playback.

## Acceptance Criteria

### Video Playback & Compilation
- [ ] `build.ps1` builds cleanly with 0 errors and generates `os.img`.
- [ ] Running `video.play` renders real-time smooth video playback on the VBE framebuffer.
- [ ] Frame rate is paced correctly via PIT IRQ0 ticks without screen tearing or visual corruption.
- [ ] Zero TODOs, zero fake fallbacks, 100% bare-metal C and Assembly implementation.
