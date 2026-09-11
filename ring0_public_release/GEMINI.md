# A.A OS - Core Engineering Rules

## 1. Absolute Honesty & Zero False Claims (Strict Invariant)
- **100% Truth & Transparency**: Never lie, fabricate claims, or exaggerate capabilities. Always state the exact, genuine hardware reality.
- **Absolute Ban on Fake Claims Under Tough Situations**: Never make fake claims, misleading classifications, or false architectural statements under ANY circumstances, no matter how tough, conflicting, or restrictive the constraints are.
- **Mandatory Upfront Technical Reality Presentation**: If a requested feature or constraint conflicts with physical silicon reality (e.g., executing non-x86 bytes on bare-metal x86 boot without an x86 loader), ALWAYS state the exact technical reality upfront with 100% honesty instead of generating hidden opcodes or claiming false compliance.
- **Zero Simulation / Zero Mocking**: Under no circumstances should any command, driver, or hardware readout use fake static text, mocked return values, or simulated stubs.
- **Absolute Ban on Shortcuts & Fake Fallback Paths**: Never create artificial fallback addresses (e.g. `0xFEA00000`), hardcoded MAC addresses, or fake branches under the excuse of "making commands run in test VMs". If physical hardware is absent on the PCI bus, immediately and honestly report zero physical hardware with zero fabricated paths.
- **100% Genuine Bare-Metal Implementation**: Whatever feature or command is requested must be implemented directly on real hardware silicon, physical memory, or hardware I/O ports.
- **Zero Incomplete Work & Zero TODOs**: Never leave unfinished placeholders, stubbed functions, or TODO comments. All features must be fully completed and functional.
- **Zero Mock Data in Tables & Sniffers**: Network and wireless scan tables, frame sniffers, and packet telemetry must poll and decode live physical DMA descriptor rings. When zero external packets are detected, honestly report zero frames.
- **Hardware-Facing vs Software Subsystems Separation**: Hardware-facing features must interact directly with physical hardware through correct architectural mechanisms. Software subsystems must be real kernel functionality and never represented as hardware access when they are not.

## 2. No Rushing & Deep Rigorous Engineering (Jald Baazi Se Parhaiz)
- **Never Cut Corners**: Never rush implementations or make superficial changes. Every line of assembly and C code must be carefully designed, reviewed, and cleanly compiled.
- **Strict Ban on Lazy Workarounds**: Taking shortcuts, mocking absent hardware, or bypassing physical reality is strictly forbidden.
- **Double Verification (2-Pass Check)**: Always verify all changes twice before reporting completion.
- **Mandatory Post-Update Bug-Fixer Pass**: After every feature or code update, perform a mandatory bug audit and verification pass to guarantee zero regressions.
- **Maximum Advanced Silicon Feature Mastery**: Always engineer the most advanced, capable silicon features (e.g. DMA ring queues, hardware microcode containers, cryptographic engines, hardware virtualization) rather than basic legacy stubs.

## 3. Direct Silicon & Port I/O Hardware Drivers
- **ATA/IDE Hard Disk Controller**: Direct PIO port reads/writes (0x1F0-0x1F7, 0x170-0x177) with real commands (IDENTIFY 0xEC, READ SECTORS 0x20, WRITE SECTORS 0x30, CACHE FLUSH 0xE7).
- **PC Motherboard Speaker & PIT**: Direct Intel 8254 PIT (0x40-0x43) and Port 0x61 frequency modulation.
- **CPU & Registers**: Live values via inline assembly (cpuid, rdtsc, rdmsr, cr0, cr2, cr3, cr4, eflags, esp, ebp, cs, ds, ss, es, fs, gs, wbinvd, clflush).
- **Direct Port I/O Instructions**: Real inb, inw, inl, outb, outw, outl execution for any I/O port (0x0000 - 0xFFFF).
- **Physical Memory & Ring 0 Mastery**: Probes live BIOS E820 SMAP tables, BDA (0x0400), MBR (0x7C00), and dynamic kernel heap (0x00200000+). Unrestricted physical memory read/write/fill/xor/diff/bench via memory.control.self().
- **Motherboard RTC & CMOS**: Live CMOS registers (0x70, 0x71) and NVRAM battery status.
- **PCI Bus**: Live PCI Configuration Space Mechanism #1 (0xCF8, 0xCFC).
- **PS/2 Mouse & Scroll Wheel**: Intel 8042 Aux device (0x60, 0x64), IntelliMouse magic knock sequence (200->100->80) for Z-axis scroll wheel packet decoding.
- **Parallel Port (LPT1 / IEEE 1284)**: BDA 0x0408 base I/O port, live pin status registers (0x379), and direct pin voltage output (0x378).
- **Serial Port (COM1 UART 16550)**: Ports 0x3F8-0x3FE with internal loopback diagnostics and baud rate divisor programming.
- **PCIe NVMe 1.0–1.4+ Master Driver**: 64-bit BAR0 MMIO, 64-byte SQE / 16-byte CQE DMA queues, Admin/IO queue pairs, physical NAND flash LBA read/write/dump/flush.
- **Intel 8254x Gigabit Ethernet (e1000)**: 32-bit BAR0 MMIO, 32-entry TX/RX circular DMA descriptor rings, raw IEEE 802.3 frame transmit/sniffing, hardware MAC reprogramming (RAL0/RAH0).
- **Intel Wireless (iwlwifi) & IEEE 802.11 Wi-Fi Subsystem**: 256-entry 64-bit DMA rings, Intel uCode firmware container, freestanding WPA2-PSK crypto engine (PBKDF2-4096, PRF-512, AES-128 S-Box, CRC32 FCS), 802.11 FSM (Scan, Auth, Assoc, 4-Way Handshake), and 802.11 QoS Data framing with LLC/SNAP encapsulation.
- **Intel VMX & Extended Page Tables (EPT)**: Full 4-level PML4 -> PDPT -> PD -> PT guest-to-host physical address isolation with VMCS EPT_POINTER configuration.

## 4. Real-Mode Bootloader & Memory Loading Invariants
- **16-Bit Segment-Advancing Multi-Sector Reader**:
  - In 16-bit x86 Real Mode, segment offsets wrap at 64 KB (0xFFFF).
  - Multi-sector INT 13h kernel loaders must NEVER load large kernels (>32 KB) into a fixed segment.
  - Always read in safe chunks (e.g. 32 sectors = 16 KB) with Offset fixed at 0x0000, advancing the 16-bit Segment register (ES: 0x0800 -> 0x0C00 -> 0x1000...) to prevent corrupting the Real-Mode IVT and stack (0x0000:0x0000) and preventing infinite PC reboot loops.
  - Maintain a valid MBR partition table at offset 446 (0x1BE) and boot signature 0xAA55 at offset 510.

## 5. Real-Time OS (RTOS) & Direct Silicon Transistor Pointer Aliasing
- **Interrupt-Driven Real-Time Architecture**:
  - Shift from busy-polling loops to true hardware interrupt-driven execution using PIC 8259 + IDT handlers for IRQ0 (PIT / System Timer) and IRQ1 (PS/2 Keyboard).
  - Idle CPU states must use `hlt` instructions to sleep until physical hardware interrupts arrive.
- **Direct Silicon Pointer Aliasing**:
  - Access memory-mapped hardware registers directly using raw volatile pointer dereferencing on physical addresses (e.g., Local APIC at `0xFEE00000+`, BIOS Data Area at `0x00000400`) without intermediate abstraction layers.
- **Pure Assembly ISR Gate Wrappers**:
  - High-speed interrupt gates and context switches must use dedicated assembly wrappers (`pushal`, `popal`, `iret`) in `kernel_entry.asm`.

## 6. Evidence-Based Completion & Status Taxonomy (Strict Invariant)
- **Strict Criteria for COMPLETE**:
  - A feature may ONLY be marked `COMPLETE` when its implementation is present, its critical code path is connected, its required hardware structures/registers/commands are correctly handled, and available evidence proves the claim.
- **What is NOT Proof of Implementation**:
  - Code comments, function names, printed console messages, hardcoded return values, or variable names are NOT proof of implementation.
- **PARTIAL Classification**:
  - If only part of a driver or subsystem exists, it must be marked `PARTIAL`.
- **UNVERIFIED Classification**:
  - If hardware-specific correctness cannot be verified from available physical hardware or authoritative documentation, it must be marked `UNVERIFIED`.
- **Prohibition on Superficial Inference**:
  - Never infer "complete driver" merely from detection, initialization, register structs, or a successful-looking console message.

## 7. Strict Real-Code Rules & Final Verification Checklist (User Command)
- **Universal Guardrails (Jab bhi mere liye code likho, review karo, ya modify karo, in rules ko strictly follow karo)**:
  1. **No fake code.**
  2. **No mocks** unless explicitly requested.
  3. **No simulations** unless explicitly requested.
  4. **No emulations** unless explicitly requested.
  5. **No TODOs, placeholders, stubs, or unfinished implementations.**
  6. **No hard-coded values pretending to be real hardware/system data.**
  7. **No fabricated hardware addresses, devices, registers, APIs, or capabilities.**
  8. **No fake success messages.** If operation is not actually successful, do not claim success.
  9. **No false claims about hardware access.**
  10. **Every function must have a real implementation** appropriate to the target platform.
  11. **Code must target the exact environment specified** (e.g. Linux x86-64, Windows, bare metal x86, etc.).
  12. **Use real APIs, instructions, system calls, registers, and protocols** appropriate to the target environment.
  13. **Do not silently replace a difficult real implementation with a fake/simple alternative.**
  14. **Do not hide limitations.** Clearly state requirements (e.g. CPU features, privilege level, OS facility, driver, bootloader, hardware).
  15. **Do not claim "100% bug-free" unless exhaustively verified.** Identify what remains unverified.
  16. If implementation is genuinely not possible under constraints, **say so instead of producing fake code.**
  17. **Strictly distinguish**:
      - `IMPLEMENTED` — genuinely implemented.
      - `PARTIAL` — some functionality exists but complete requirement is not met.
      - `UNVERIFIED` — implementation exists but could not be tested in target environment.
      - `NOT POSSIBLE UNDER THESE CONSTRAINTS` — cannot genuinely be done.
  18. **Pre-delivery verification checks**:
      - compilation/assembly/linker errors
      - ABI/calling-convention errors
      - invalid instructions/memory access/privilege level issues
      - incorrect hardware/API assumptions, architecture mismatches, undefined behavior, misleading comments.
  19. **Never make code look more powerful than it is.**
  20. **Truth is more important than completing the request.**
- **Mandatory Final Verification Output Form**:
  Before presenting code, you must explicitly state:
  - **Genuinely Implemented**: [List of features/functions fully implemented]
  - **Real Hardware/Software Interface**: [Specific APIs, registers, ports, etc.]
  - **Required Environment**: [Target OS, CPU mode, privilege ring, etc.]
  - **Verified**: [What has been run/tested successfully]
  - **Unverified**: [What hasn't been tested or requires external hardware]

