# E2E Test Infra: A.A OS Bare-Metal Video Playback Engine

## Test Philosophy
- Opaque-box, requirement-driven verification of Ring 0 Video Playback Engine in A.A OS.
- 100% genuine validation against physical silicon invariants, assembly structures, PIT IRQ0 timing, double-buffer blitting, sound sync, and compilation with `build.ps1`.
- Methodology: Category-Partition + Boundary Value Analysis (BVA) + Pairwise Interaction Testing + Real-World Stress Testing.

## Feature Inventory & Test Mapping
| # | Feature | Requirement Source | Tier 1 (Feature) | Tier 2 (Boundary) | Tier 3 (Cross) | Tier 4 (Scenario) |
|---|---------|-------------------|:----------------:|:-----------------:|:--------------:|:-----------------:|
| 1 | BGA LFB Mode Setting & BAR0 Probing | ORIGINAL_REQUEST §R1 | 5 tests | 5 tests | ✓ | ✓ |
| 2 | DRAM Double-Buffering & Fast Blitter | ORIGINAL_REQUEST §R1 | 5 tests | 5 tests | ✓ | ✓ |
| 3 | PIT IRQ0 Frame Pacing (30/60 FPS) | ORIGINAL_REQUEST §R2 | 5 tests | 5 tests | ✓ | ✓ |
| 4 | PC Speaker / AC'97 Sound Sync | ORIGINAL_REQUEST §R2 | 5 tests | 5 tests | ✓ | ✓ |
| 5 | Procedural 3D Raycaster Stream | ORIGINAL_REQUEST §R3 | 5 tests | 5 tests | ✓ | ✓ |
| 6 | Multi-Wave Plasma & Matrix Streams | ORIGINAL_REQUEST §R3 | 5 tests | 5 tests | ✓ | ✓ |
| 7 | Kernel CLI Commands (`video.*`) | ORIGINAL_REQUEST §R3 | 5 tests | 5 tests | ✓ | ✓ |

## Test Architecture
- **Automated Verification Harness**: PowerShell / batch scripts invoking build checks, structural static analyzers, symbol checkers, and QEMU execution validation.
- **Pass/Fail Semantics**: Exit code 0 for all passing suites; non-zero exit code on failure.
- **Directory Layout**:
  - `tests/`: Test runners and scripts.
  - `tests/test_build.ps1`: Clean compilation & image generation test.
  - `tests/test_symbols.ps1`: Verification of required assembly labels, exported C symbols, and memory bounds.
  - `tests/test_video_e2e.ps1`: End-to-end multi-tier test harness.

## Test Tiers Breakdown
### Tier 1 — Feature Coverage (35 test cases)
- T1.1: BGA port discovery & register read/write.
- T1.2: PCI Display Controller class 0x03 BAR0 parsing.
- T1.3: Mode setting for 640x480, 800x600, 1024x768 @ 32bpp.
- T1.4: Offscreen DRAM buffer allocation at physical 0x00800000 / heap.
- T1.5: Assembly fast blit (`_asm_video_blit_lfb` / `rep movsd`) transfer.
- T1.6: PIT Channel 0 reprogramming to 1000 Hz.
- T1.7: Target tick calculator for 30 FPS / 60 FPS.
- T1.8: Low-power frame pacing wait loop with `sti; hlt`.
- T1.9: PC Speaker tone activation on video event.
- T1.10: Speaker tone deactivation on frame finish.
- T1.11: 3D Raycaster sphere geometry projection.
- T1.12: Q16.16 fixed-point arithmetic & integer square root.
- T1.13: Multi-wave plasma sinusoidal synthesis using 256-entry sine LUT.
- T1.14: Cyber Matrix rain glyph stream & green phosphor fading.
- T1.15: `video.play` default stream execution.
- T1.16: `video.info` telemetry printout.
- T1.17: `video.stop` execution and 80x25 text mode restoration.
- T1.18 - T1.35: Specific unit feature checks for buffer offsets, pitch strides, command aliases, and color bitmask conversions.

### Tier 2 — Boundary & Corner Cases (35 test cases)
- T2.1: BGA controller absent handling (graceful error, no hang).
- T2.2: Zero target FPS input (`video.play sphere 0` -> auto-clamped to 60 FPS).
- T2.3: Excessive target FPS input (`video.play sphere 1000` -> clamped to 120 FPS).
- T2.4: Invalid stream name input (`video.play invalid_stream` -> help message).
- T2.5: Immediate user abort (`ESC` / `'q'` pressed on frame 0).
- T2.6: Rapid stream switching without memory leak or double allocation.
- T2.7: Double buffer memory boundaries (prevent write past end of buffer).
- T2.8: Fixed-point overflow protection in raycaster normal normalization.
- T2.9: Sine table index wrapping `[0..255]`.
- T2.10 - T2.35: Memory aliasing checks, interrupt re-entrancy, timer wrap-around, and edge pixel clipping.

### Tier 3 — Cross-Feature Combinations (7 test cases)
- T3.1: 3D Raycaster + 60 FPS Pacing + PC Speaker Acoustic Bounce Sync.
- T3.2: Multi-Wave Plasma + 30 FPS Pacing + AC'97 Harmonic Arpeggio Sync.
- T3.3: Cyber Matrix Rain + 60 FPS Pacing + Terminal Sound Effect Sync.
- T3.4: Dynamic Mode Switching (Sphere -> Plasma -> Matrix) during active playback.
- T3.5: `video.info` executed while playback is active vs stopped.
- T3.6: Fast start-stop stress cycling (10 iterations of `video.play` and `video.stop`).
- T3.7: Video Playback -> Text Mode -> Normal Shell Commands -> Video Playback.

### Tier 4 — Real-World Application Scenarios (5 test cases)
- T4.1: Full Multimedia Showcase Demo (`video.play demo` running complete rotating sequence).
- T4.2: High-Resolution 1024x768 60 FPS stress test (maintaining frame rate under continuous rendering).
- T4.3: Real-Time Interactive Controls (Space to pause, 1/2/3 to switch streams, ESC to exit).
- T4.4: Zero Memory Leak / DRAM Stability Run across 1000 frames.
- T4.5: Clean recovery and text mode prompt usability after video termination.

## Minimum Coverage Thresholds
- Tier 1: ≥35 test cases (5 per feature)
- Tier 2: ≥35 test cases (5 per feature)
- Tier 3: ≥7 pairwise cross-feature tests
- Tier 4: ≥5 realistic application scenarios
- Tier 5: Adversarial coverage hardening & zero-tolerance forensic audit
