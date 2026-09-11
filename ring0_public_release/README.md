# A.A OS - 32-bit x86 Bare-Metal Operating System (Ring 0 Kernel)

**A.A OS** is a 100% genuine, native bare-metal x86 Protected Mode operating system built from scratch with **zero mocks, zero fake text, and zero simulation**. All hardware drivers interact directly with physical CPU transistors, memory-mapped I/O registers, motherboard chips, and port I/O.

---

## ⚡ Quick Start

### 1. Build A.A OS
```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1
```
*Or double-click `build.bat`.*

### 2. Run in QEMU Emulator
```powershell
powershell -ExecutionPolicy Bypass -File .\run.ps1
```
*Or double-click `run.bat` (Press `Ctrl + Alt + G` inside QEMU to release mouse cursor).*

### 3. Flash to Physical USB Flash Drive (Real PC Boot)
* **Option A (1-Click PowerShell)**: Open PowerShell as Admin and run:
  ```powershell
  & ".\auto_flash.ps1"
  ```
* **Option B (Batch Script)**: Right-click **`FLASH_USB_ADMIN.bat`** $\rightarrow$ **Run as administrator**.
* **Option C (Rufus GUI)**: Open **`rufus.exe`**, select your USB flash drive, choose `os.img`, and click **START** in DD Image mode.

---

## 🏛️ Modular Pure Assembly & Dual-Kernel Architecture

A.A OS is structured into **10 dedicated standalone pure x86 assembly modules** for low-level silicon control, paired with a modular dual-C kernel layer for high-level state machines and UI:

```
my_os/
├── boot.asm                 ; 16-bit MBR Bootloader (Segment-Advancing Multi-Sector Loader, BPB, MBR 0x1BE)
├── kernel_entry.asm         ; 32-bit CPU Entry, 32 Exception Trap Gates, GDT/IDT, Guarded SSE/FPU Init
├── silicon_engine.asm       ; Atomic Register Snapshotter, Serialized RDTSC Benchmarks, TLB & Pipeline Flushes
├── storage_editor.asm       ; Physical Storage Sector & RAM Direct Byte Editor, Eraser, Pattern Filler, Cloner
├── hardware_override.asm    ; Direct Chip Power Controller & Zero-Security Hardware Override Engine
├── memorymanagement.asm     ; Physical RAM Paging, Bitmap Allocator, 5-Pass DRAM Wash, bswap, Latency Bench
├── hardware_io.asm          ; ATA/IDE PIO Read/Write, PCI Mechanism #1, CMOS RTC, PIT/Speaker
├── hardware_debug.asm       ; DR0-DR7 Hardware Breakpoints, EFLAGS.TF Single-Step, MSRs, RDPMC
├── ps2_driver.asm           ; Intel 8042 Keyboard & Mouse Controller, LEDs (0xED), IntelliMouse Scroll
├── serial_driver.asm        ; UART 16550 COM1/COM2 Controller, DLAB Programming, Loopback Diagnostics
├── vga_driver.asm           ; VGA 0xB8000 Text Framebuffer, rep movsd Scroller, CRTC Hardware Cursor
├── kernel.c                 ; Main C Kernel: Command Dispatcher, e1000 NIC, iwlwifi, NVMe SSD, VMX Hypervisor
├── kernel2.c                ; Kernel 2: Chip Power Dashboard, ACPI Table Hunter, Forensics, Thermal DTS
└── linker.ld                ; Linker Script: 32-bit PE/Binary layout mapped at physical address 0x8000
```

---

## 🔌 Chip Power & Zero-Security Hardware Override Suite (`chip`)

All software safety bounds, access restrictions, and write locks can be bypassed via direct Ring 0 assembly primitives:

| Command | Action |
|---|---|
| **`chip`** / **`chip list`** | Displays live Motherboard Silicon Power & Status Dashboard. |
| **`chip wp off`** / **`on`** | **Disables/Enables Supervisor Write-Protection (`CR0.WP`)**; allows overwriting ANY read-only RAM, code, or page table. |
| **`chip write <addr> <val>`** | Direct uninhibited 32-bit physical RAM write to **ANY address** (`0x00000000` to `0xFFFFFFFF`) with `clflush`. |
| **`chip read <addr>`** | Direct raw 32-bit physical RAM read from any address. |
| **`chip wrmsr <msr> <lo> <hi>`**| Direct raw execution of x86 **`wrmsr`** instruction. |
| **`chip irq off`** / **`on`** | Direct raw **`cli`** (disable all CPU interrupts) / **`sti`** (enable CPU interrupts). |
| **`chip triplefault`** | Triggers instantaneous **CPU Silicon Hard Reset** via hardware triple fault (`lidt [0]; int 3`). |
| **`chip swap <addr1> <addr2> <cnt>`**| Pure x86 **`xchg`** atomic physical memory block swap with cache flush. |
| **`chip cache off`** / **`on`** | Physically powers off/on CPU L1/L2/L3 SRAM cache arrays (`CR0.CD`, `CR0.NW`, `wbinvd`). |
| **`chip pic off`** / **`on`** | Masks/unmasks all 16 IRQ lines on Intel 8259 PIC chips (Ports `0x21` & `0xA1`). |
| **`chip apic off`** / **`on`** | Powers off/on the on-die Local APIC execution core (MSR `0x1B` Bit 11 & MMIO SVR Bit 8). |
| **`chip kbd off`** / **`on`** | Disables/enables Intel 8042 Keyboard clock line (`0xAD`/`0xAE`). |
| **`chip mouse off`** / **`on`** | Disables/enables Intel 8042 Mouse auxiliary interface (`0xA7`/`0xA8`). |
| **`chip speaker off`** / **`on`**| Cuts/energizes motherboard speaker timer voltage gate (Port `0x61`). |
| **`chip pci <b> <s> <f> <d0\|d3>`**| Programs any physical PCI device to **D0 (Full Active ON)** or **D3hot (Power OFF / Sleep)**. |
| **`chip pit <freq_hz>`** | Overdrives Intel 8254 system timer to custom high frequency (e.g. `1000` Hz). |

---

## 🔬 Kernel 2 Advanced Silicon Subsystems (`help2`)

| Command | Action |
|---|---|
| **`help2`** | Opens Kernel 2 Advanced Silicon Mastery Dashboard. |
| **`dbg.regs`** | Displays CPU hardware debug registers (`DR0`–`DR7`) status. |
| **`dbg.bp <0-3> <addr> <r\|w\|x> <1\|2\|4>`** | Arms physical hardware execution/read/write breakpoint in CPU silicon. |
| **`dbg.clear <0-3>`** | Disarms hardware breakpoint. |
| **`acpi.tables`** | Scans physical EBDA/ROM for ACPI RSDP (20-byte checksum verified) and lists `RSDT`, `MADT`, `FACP`. |
| **`cpu.stress [iters]`** | Runs intensive CPU crunch loop and reads Intel DTS MSR `0x19C` for **live core temperature (°C)**. |
| **`disk.hex <lba>`** | Pure assembly ATA PIO (`rep insw`) full 512-byte forensic hex dump of any storage sector. |
| **`mem.wash <addr> <bytes>`** | Executes 5-pass military DRAM capacitor discharge wash and `wbinvd` cache invalidate. |
| **`mem.search <start> <end> <byte>`**| Fast hardware memory scan using pure x86 `repne scasb`. |
| **`mem.bswap <addr> <dwords>`** | Direct x86 **`bswap`** instruction endianness transformation on physical memory. |
| **`packet.inject.deauth <mac> <bssid>`**| Air-injects raw 802.11 Deauthentication management frames via DMA. |

---

## 📖 Complete Documentation Manual
For the exhaustive technical guide, memory layout diagrams, and complete 100+ command references, see:
👉 **[DOCUMENTATION.md](DOCUMENTATION.md)**

---

### Invariant: 100% Genuine Bare-Metal Implementation. Zero Mocks. Zero Fake Claims.
