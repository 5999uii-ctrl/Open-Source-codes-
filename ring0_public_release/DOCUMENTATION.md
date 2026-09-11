# A.A OS - Mukammal Roman Urdu Documentation & User Guide
### 32-bit x86 Bare-Metal Operating System (Asaan Alfaaz Mein)

---

## 📖 Fehrist (Table of Contents)
1. [A.A OS Kiya Hai? (Asaan Lafzon Mein Ta'aruf)](#1-aa-os-kiya-hai-asaan-lafzon-mein-taaruf)
2. [Computer Ke Andar Ka Structure (Hardware & Memory Layout)](#2-computer-ke-andar-ka-structure-hardware--memory-layout)
3. [Assembly Ki 8 Khas Files (Pure Assembly Modules)](#3-assembly-ki-8-khas-files-pure-assembly-modules)
4. [Do Kernels Ka Kaam (`kernel.c` aur `kernel2.c`)](#4-do-kernels-ka-kaam-kernelc-aur-kernel2c)
5. [A.A OS Ko Build (Tayyar) Kaise Karein?](#5-aa-os-ko-build-tayyar-kaise-karein)
6. [QEMU Emulator Mein Kaise Chalayein?](#6-qemu-emulator-mein-kaise-chalayein)
7. [USB Flash Drive Mein Kaise Dalein (Flashing Guide)](#7-usb-flash-drive-mein-kaise-dalein-flashing-guide)
8. [Asli Computer / Laptop Par Boot Kaise Karein?](#8-asli-computer--laptop-par-boot-kaise-karein)
9. [Screen aur Menu Ko Chalane Ka Tareeqa (Mouse & Keyboard)](#9-screen-aur-menu-ko-chalane-ka-tareeqa-mouse--keyboard)
10. [Mukammal Commands Guide (Asaan Matlab Ke Sath)](#10-mukammal-commands-guide-asaan-matlab-ke-sath)
    - [Category 1: Chips Ko ON/OFF Karna & Zero-Security Controls (`chip`)](#category-1-chips-ko-onoff-karna--zero-security-controls-chip)
    - [Category 2: CPU, Processor Ki Garmi & Debugging (`cpu`, `cpu.stress`)](#category-2-cpu-processor-ki-garmi--debugging-cpu-cpustress)
    - [Category 3: RAM Memory Ka Asli Control (`mem`, `memory`)](#category-3-ram-memory-ka-asli-control-mem-memory)
    - [Category 4: Hard Disk & USB Storage (`disk.hex`, `disk`)](#category-4-hard-disk--usb-storage-diskhex-disk)
    - [Category 5: Motherboard Speaker, Ghari & Buttons (`beep`, `time`, `leds`)](#category-5-motherboard-speaker-ghari--buttons-beep-time-leds)
    - [Category 6: Computer Band Karna (`pc.shutdown()`, `poweroff`)](#category-6-computer-band-karna-pcshutdown-poweroff)
    - [Category 7: Screen Ke Rang & Utilities (`color`, `calc`, `clear`)](#category-7-screen-ke-rang--utilities-color-calc-clear)
    - [Category 8: Preemptive Multitasking & Concurrent Tasks (`ps`, `task.*`)](#category-8-preemptive-multitasking--concurrent-tasks-ps-task)
11. [Quick Command List (Aik Nazar Mein)](#11-quick-command-list-aik-nazar-mein)
12. [Zaroori Baatein & Troubleshooting](#12-zaroori-baatein--troubleshooting)

---

## 1. A.A OS Kiya Hai? (Asaan Lafzon Mein Ta'aruf)

**A.A OS** aik aisa asli operating system hai jo kisi Windows ya Linux ke upar nahi chalta, balke **computer ke hardware, chips aur processor par seedha (direct bare-metal)** chalta hai!

### 🌟 Is OS Ke 3 Sakht Qawaneen:
1. **100% Asli Reality (No Fake Code / No Mocks)**: Is OS mein koi jhoota text ya fake return value nahi hai. Jo command bhi aap chalate hain, woh computer ke physical circuit se live data mangwati hai.
2. **Ring 0 (Badshah Level Access)**: Is OS ke paas computer ka sab se aala darja (Ring 0) hai, yaani computer ki koi bhi memory ya chip is se chupi hui nahi hai.
3. **Asli PC Par Bootable**: Yeh virtual machine (QEMU) mein bhi chalta hai aur aapke real laptop ya desktop PC par USB laga kar bhi chalta hai!

---

## 2. Computer Ke Andar Ka Structure (Hardware & Memory Layout)

Jab computer on hota hai to woh A.A OS ko is tarah chalata hai:
1. **Bootloader (`boot.asm`)**: Computer start hote hi USB ke sab se pehle hissay (Sector 0 / `0x7C00`) ko parhta hai.
2. **Kernel Loading**: Bootloader poore OS ko thora thora karke RAM memory ke `0x8000` address par load karta hai.
3. **32-Bit Switch**: Processor ko 32-bit Protected Mode mein switch karta hai taake computer poori 4GB RAM ko use kar sake.
4. **Kernel Start (`kernel_entry.asm`)**: OS ka main control room start ho jata hai aur screen par prompt aa jata hai: `A.A-OS>`.

### 🗺️ RAM Memory Ka Asaan Naqsha:
| Memory Address | Asaan Naam | Yeh Kiya Karta Hai? |
|---|---|---|
| `0x00000000 - 0x000003FF` | **IVT (Interrupt Table)** | Hardware ke signals ka pata |
| `0x00000400 - 0x000004FF` | **BDA (Equipment List)** | Motherboard par kitne ports aur devices hain |
| `0x00007C00 - 0x00007DFF` | **MBR Bootloader** | Computer ko start karne wala 512 bytes ka code |
| `0x00008000 - 0x0008FFFF` | **A.A OS Kernel** | **A.A OS ka apna asli dimagh aur drivers** |
| `0x00090000 - 0x0009FFFF` | **Kernel Stack** | Temporary calculations ki jagah |
| `0x000B8000 - 0x000BFFFF` | **VGA Screen Memory** | **Screen par jo text aur rang nazar aate hain** |
| `0x00200000+` | **Free RAM Heap** | Extra RAM jahan naya data save hota hai |

---

## 3. Assembly Ki 10 Khas Files (Pure Assembly Modules)

Hardware par 100% direct control hasil karne ke liye **10 alag alag pure x86 assembly modules** banaye gaye hain:

1. **[`boot.asm`](file:///c:/Users/ALLAH/OneDrive/Desktop/my_os/boot.asm)**:
   - Computer ko start karta hai aur BIOS se poori RAM ka size maloom karta hai.
2. **[`kernel_entry.asm`](file:///c:/Users/ALLAH/OneDrive/Desktop/my_os/kernel_entry.asm)**:
   - CPU ke 32 darwaze (Exceptions jaise divide by zero, page fault) set karta hai taake system crash na ho.
3. **[`silicon_engine.asm`](file:///c:/Users/ALLAH/OneDrive/Desktop/my_os/silicon_engine.asm)**:
   - **[NEW] Real-time Atomic CPU Register Snapshotter & Serialized RDTSC Benchmark.** Tamam 8 registers, 6 segments, control registers (`CR0`-`CR4`) aur TLB flushes (`invlpg`) ko direct assembly se operate karta hai (`asm.snapshot`, `asm.bench`, `asm.tlb`, `asm.flush`).
4. **[`storage_editor.asm`](file:///c:/Users/ALLAH/OneDrive/Desktop/my_os/storage_editor.asm)**:
   - **Hard disk aur RAM ka direct editor.** Kisi bhi sector ka byte badalna (`disk.edit`), sector zero/erase karna (`disk.erase`), copy karna (`disk.copy`), aur RAM mein direct likhna/erase karna (`mem.edit`, `mem.fill`, `mem.erase`).
5. **[`hardware_override.asm`](file:///c:/Users/ALLAH/OneDrive/Desktop/my_os/hardware_override.asm)**:
   - **Motherboard ki chips ko ON/OFF karne wala switch.** Keyboard, cache, PIC, speaker aur power states ko direct control karta hai.
6. **[`memorymanagement.asm`](file:///c:/Users/ALLAH/OneDrive/Desktop/my_os/memorymanagement.asm)**:
   - RAM memory ki safai (5-Pass Military Wash), `bswap` se number ultana, aur memory allocation karta hai.
7. **[`hardware_io.asm`](file:///c:/Users/ALLAH/OneDrive/Desktop/my_os/hardware_io.asm)**:
   - Hard disk se sectors parhna/likhna (`rep insw`), motherboard ghari (RTC), aur speaker frequency chalata hai.
8. **[`hardware_debug.asm`](file:///c:/Users/ALLAH/OneDrive/Desktop/my_os/hardware_debug.asm)**:
   - CPU ke andar hardware spy registers (`DR0` se `DR7`) set karta hai.
9. **[`ps2_driver.asm`](file:///c:/Users/ALLAH/OneDrive/Desktop/my_os/ps2_driver.asm)**:
   - Keyboard aur Mouse ke signals leta hai, aur keyboard ki 3 battiyan (LEDs) jalata hai.
10. **[`serial_driver.asm`](file:///c:/Users/ALLAH/OneDrive/Desktop/my_os/serial_driver.asm)**:
   - COM Serial wire se communication karta hai.
11. **[`vga_driver.asm`](file:///c:/Users/ALLAH/OneDrive/Desktop/my_os/vga_driver.asm)**:
   - Screen par super-fast text likhta hai aur cursor ko move karta hai.

---

## 4. Do Kernels Ka Kaam (`kernel.c` aur `kernel2.c`)

* **[`kernel.c`](file:///c:/Users/ALLAH/OneDrive/Desktop/my_os/kernel.c) (Main Kernel)**:
  - Isme 100+ commands ka parser, Intel Gigabit Internet Driver (e1000), Wi-Fi driver (iwlwifi), NVMe SSD driver, aur ACPI shutdown mojood hai.
* **[`kernel2.c`](file:///c:/Users/ALLAH/OneDrive/Desktop/my_os/kernel2.c) (Advanced Silicon Subsystems)**:
  - Isme `chip` control dashboard, ACPI table hunter, Hard disk hex viewer, aur processor thermal stress sensor mojood hai.

---

## 5. A.A OS Ko Build (Tayyar) Kaise Karein?

Agar aapne code mein koi tabdeeli ki hai to usko compile karne ke liye PowerShell mein yeh run karein:

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1
```
*(Ya folder mein **`build.bat`** par double click karein).*

Yeh script:
1. Tamam 8 assembly files ko assemble karega.
2. `kernel.c` aur `kernel2.c` ko compile karega.
3. Linker se jor kar **`os.img`** (1.41 MB bootable image) bana dega!

---

## 6. QEMU Emulator Mein Kaise Chalayein?

Bina computer restart kiye test karne ke liye:

```powershell
powershell -ExecutionPolicy Bypass -File .\run.ps1
```
*(Ya **`run.bat`** par double click karein).*

> [!TIP]
> Agar QEMU window ke andar mouse phans jaye to keyboard par **`Ctrl + Alt + G`** dabayein taake mouse wapas Windows par aa jaye!

---

## 7. USB Flash Drive Mein Kaise Dalein (Flashing Guide)

### Tareeqa 1: 1-Click Automated PowerShell (Sab Se Asaan)
Apne computer mein **PowerShell (Run as Administrator)** open karein aur yeh run karein:
```powershell
& ".\auto_flash.ps1"
```
Yeh khud aapki USB ko pehchan kar Sector 0 par `os.img` likh dega!

### Tareeqa 2: 1-Click File Explorer Batch
Folder mein **[`FLASH_USB_ADMIN.bat`](file:///c:/Users/ALLAH/OneDrive/Desktop/my_os/FLASH_USB_ADMIN.bat)** par **Right Click $\rightarrow$ Run as administrator** karein.

### Tareeqa 3: Rufus Ke Zariye
1. Folder mein mojood **[`rufus.exe`](file:///c:/Users/ALLAH/OneDrive/Desktop/my_os/rufus.exe)** open karein.
2. Device mein apni **USB Drive** select karein.
3. **SELECT** dabayein $\rightarrow$ `os.img` choose karein (*All files filter ke sath*).
4. **START** dabayein $\rightarrow$ **"Write in DD Image mode"** choose karein.

---

## 8. Asli Computer / Laptop Par Boot Kaise Karein?

1. USB ko computer mein lagayein aur PC ko **Restart** karein.
2. Computer on hote hi screen aane se pehle **Boot Menu key** ko bar bar dabayein:
   - **Dell / Lenovo**: `F12`
   - **HP**: `F9`
   - **Asus / MSI / Gigabyte**: `F8` ya `F11`
   - **Acer / Toshiba**: `F12`
3. **BIOS Setup** mein yeh 2 cheezein check karein:
   - **Secure Boot**: **Disabled (Off)** hona chahiye.
   - **Boot Mode**: **Legacy Mode** ya **UEFI with CSM Enabled** hona chahiye.
4. Boot Menu se apni **USB Drive** select karke **Enter** dabayein $\rightarrow$ A.A OS direct hardware par chal padega!

---

## 9. Screen aur Menu Ko Chalane Ka Tareeqa (Mouse & Keyboard)

* **Help Menu (`help`)**:
  - **Mouse Scroll Wheel**: Scroll wheel ko neechay ya ooper ghuma kar pages aagay peechay kar sakte hain.
  - **Keyboard Buttons**:
    - **`1`**, **`2`**, **`3`**, **`4`**: Direct kisi page par jump karne ke liye.
    - **`Space`** ya **`Down Arrow`**: Agla page dekhne ke liye.
    - **`Up Arrow`**: Pichla page dekhne ke liye.
    - **`Q`** ya **`Esc`**: Wapas command prompt par aane ke liye.
* **Kernel 2 Help**: Type karein **`help2`**.

---

## 10. Mukammal Commands Guide (Asaan Matlab Ke Sath)

---

### Category 1: Chips Ko ON/OFF Karna & Zero-Security Controls (`chip`)

*Aapke computer ke motherboard par lagi microchips aur RAM ke tamam locks ko khatam karke direct control:*

| Command | Asaan Matlab (Yeh Kiya Karta Hai?) |
|---|---|
| **`chip`** ya **`chip list`** | Screen par poora chart dikhata hai ke computer ki kaun si chip **ON** hai aur kaun si **OFF**. |
| **`chip wp off`** | **RAM ka Protection Lock tod deta hai (`CR0.WP=0`).** Iske baad aap RAM ke kisi bhi read-only hissay, code ya page table par apna data likh sakte hain! |
| **`chip wp on`** | RAM ka Protection Lock dobara laga deta hai (`CR0.WP=1`). |
| **`chip write <addr> <val>`** | Physical RAM ke kisi bhi address par direct 32-bit number likh deta hai (Zero limit, koi rokaawat nahi).<br>*Misaal: `chip write 0x00200000 0xDEADBEEF`* |
| **`chip read <addr>`** | Physical RAM ke kisi bhi address par mojood number parh kar batata hai.<br>*Misaal: `chip read 0x00200000`* |
| **`chip wrmsr <msr> <lo> <hi>`** | Processor ke Model Specific Register par direct value likhta hai. |
| **`chip irq off`** | Direct **`cli`** instruction chala kar CPU ke tamam interrupts ko foran band kar deta hai. |
| **`chip irq on`** | Direct **`sti`** instruction chala kar interrupts wapas on kar deta hai. |
| **`chip triplefault`** | Computer ke processor ko foran **Hard Reset** (restart) kar deta hai. |
| **`chip swap <addr1> <addr2> <count>`** | RAM ke do hisson ke data ko aik second mein aapas mein **adla badli (exchange)** kar deta hai! |
| **`chip cache off`** | CPU ke andar ki super-fast memory (L1/L2/L3) band kar deta hai (PC slow ho jayega). |
| **`chip cache on`** | CPU cache ko dobara on karke full speed kar deta hai. |
| **`chip pic off`** | Traffic police chip (PIC 8259) ko sula deta hai (16 IRQs band). |
| **`chip pic on`** | Traffic police chip ko wapas jagata hai. |
| **`chip apic off` / `on`** | Modern processor ke internal Local APIC controller ko band/chalu karta hai. |
| **`chip kbd off`** | Keyboard ki taar (clock line) kaat deta hai (buttons dabane par koi reaction nahi hoga). |
| **`chip kbd on`** | Keyboard ki taar wapas jor deta hai. |
| **`chip speaker off` / `on`** | Motherboard speaker ke bijli switch ko band ya chalu karta hai. |
| **`chip pci <b> <s> <f> d3`** | Kisi bhi card (Wi-Fi, LAN, Graphic Card) ki bijli band karke usko Deep Sleep mein daal deta hai. |
| **`chip pci <b> <s> <f> d0`** | Us card ko wapas Full Power ON kar deta hai. |
| **`chip pit <freq>`** | Motherboard ke timer ko bohot tez frequency (e.g. `1000` Hz) par chalata hai. |

---

### Category 2: CPU, Processor Ki Garmi & Debugging (`cpu`, `cpu.stress`)

| Command | Asaan Matlab |
|---|---|
| **`cpu`** ya **`cpuid`** | Processor ka asli naam (Intel/AMD), model, aur 40+ hardware features dikhata hai. |
| **`cpu.stress 1`** | Processor par bhaari calculation ka load daalta hai aur uske sensor se **live temperature (°C)** napta hai! |
| **`dbg.regs`** ya **`dr`** | CPU ke andar bani hui spy/debug registers (`DR0` se `DR7`) ka live status dikhata hai. |
| **`dbg.bp 0 0x8000 x 1`** | Address `0x8000` par hardware trap/breakpoint laga deta hai. |
| **`dbg.clear 0`** | Breakpoint ko khatam karta hai. |
| **`cr`** | CPU ke Control Registers (`CR0`, `CR2`, `CR3`, `CR4`) parhta hai. |
| **`flags`** | CPU ke indicator jhande (`EFLAGS`) parhta hai. |
| **`msr <hex>`** | Processor ka internal MSR register parhta hai (jaise `msr 19c`). |

---

### Category 3: RAM Memory Ka Asli Control (`mem.edit`, `mem.fill`, `mem.erase`, `mem.view`)

| Command | Asaan Matlab |
|---|---|
| **`memory`** | Computer mein lagi asli RAM ka size (Total MB aur Kaam ki MB) dikhata hai. |
| **`mem.edit <addr> <byte_hex>`** | Physical RAM ke kisi bhi address par **single byte modify/edit** karta hai (`clflush` ke sath).<br>*Misaal: `mem.edit 0x200000 0x77`* |
| **`mem.fill <addr> <cnt> <byte>`**| Pure assembly `rep stosb` se RAM ke kisi bhi hissay ko custom pattern se **fill** karta hai.<br>*Misaal: `mem.fill 0x200000 64 0xAA`* |
| **`mem.erase <addr> <cnt>`** | RAM ke kisi bhi hissay ko **zeroes (`0x00`) se erase/clear** karta hai.<br>*Misaal: `mem.erase 0x200000 64`* |
| **`mem.view <addr> [bytes]`** | Physical RAM ka **16-column Live Hex + ASCII View** screen par dikhata hai.<br>*Misaal: `mem.view 0x200000 32`* |
| **`mem.wash <addr> <bytes>`** | RAM ke capacitors ko bilkul saaf karne ke liye **5-Pass Military DRAM Wash** aur cache flush chalata hai. |
| **`mem.bswap <addr> <dwords>`** | Pure x86 `bswap` instruction se RAM ke numbers ko ulta (endianness reverse) karta hai. |
| **`mem.search <start> <end> <byte>`**| Super-fast tareeqe se RAM ke andar koi number ya harf dhoondta hai. |

---

### Category 4: Hard Disk & USB Storage Editor (`disk.edit`, `disk.fill`, `disk.erase`, `disk.copy`)

| Command | Asaan Matlab |
|---|---|
| **`disk`** | Primary Hard Disk ya USB controller ka model aur size batata hai. |
| **`disk.hex <lba>`** | Hard disk ke kisi bhi sector ka poora **512-byte raw binary forensic data** screen par dikhata hai.<br>*Misaal: `disk.hex 0` ya `disk.hex 600`* |
| **`disk.edit <lba> <off> <byte>`**| Physical sector ke **kisi bhi single byte (Offset 0–511)** ko direct modify/edit karta hai aur cache flush karta hai!<br>*Misaal: `disk.edit 600 0 0x42`* |
| **`disk.fill <lba> <cnt> <byte>`**| Pure assembly ATA PIO se kisi bhi sector ko custom pattern se **fill / write** karta hai.<br>*Misaal: `disk.fill 600 5 0xAA`* |
| **`disk.erase <lba> [cnt]`** | Kisi bhi sector ko **zeroes (`0x00`) se erase/delete/clear** kar deta hai!<br>*Misaal: `disk.erase 600 1`* |
| **`disk.copy <src_lba> <dst_lba>`**| Aik physical sector ko doosre sector par **clone/copy/backup** kar deta hai.<br>*Misaal: `disk.copy 600 601`* |
| **`nvme`** | Fast NVMe Solid-State Drive (SSD) ko probe karta hai. |

---

### Category 5: Motherboard Speaker, Ghari & Buttons (`beep`, `time`, `leds`)

| Command | Asaan Matlab |
|---|---|
| **`beep`** | Motherboard ke internal buzzer se **"BEEP / Teeet"** ki sound bajata hai bina kisi sound driver ke! |
| **`beep 440`** | Low-pitch musical note bajata hai (`beep 1500`, `beep 2500`). |
| **`time`** | Motherboard ke RTC chip se live date aur time parh kar batata hai. |
| **`leds 7`** | Keyboard ki teeno physical battiyan (Caps Lock, Num Lock, Scroll Lock) aik sath jala deta hai! (`leds 0` se off). |
| **`acpi.tables`** | Motherboard ke ACPI tables (RSDP, RSDT, MADT, FACP) scan karke dhoondta hai. |
| **`pci`** | Computer mein lage Graphic card, Wi-Fi, Ethernet, Audio chips scan karke list banata hai. |
| **`inb <port>` / `outb <port> <val>`** | Motherboard ke kisi bhi I/O port par direct data bhejne ya lene ke liye. |

---

### Category 6: Computer Band Karna (`pc.shutdown()`, `poweroff`)

| Command | Asaan Matlab |
|---|---|
| **`pc.shutdown()`** ya **`poweroff`** | Motherboard ke ACPI power switch ko direct signal bhej kar **computer ko bilkul band (Power Off)** kar deta hai! |
| **`reboot`** | Keyboard chip ko pulse bhej kar computer restart karta hai. |

---

### Category 7: Screen Ke Rang & Utilities (`color`, `calc`, `clear`)

| Command | Asaan Matlab |
|---|---|
| **`color matrix`** | Screen ko Hacker Green Matrix color mein tabdeel karta hai (`color cyber`, `color white`, `color bsd`). |
| **`calc 1024 * 16`** | Bare-metal math calculator (jama, tafreeq, zarb, taqseem). |
| **`clear`** ya **`cls`** | Screen saaf karke cursor ko shuru mein le aata hai. |

---

### Category 8: Preemptive Multitasking & Concurrent Tasks (`ps`, `task.*`)

> **A.A OS Preemptive Multitasking Subsystem — 100% Asli Ring 0 Context Switching!**
> Hardware PIT IRQ0 (1000 Hz) interrupt timer har 10ms baad processor par running task ko preempt (switch) karta hai. Shell prompt (`A.A-OS>`) ke sath background mein multiple tasks aik sath chalte hain!

| Command | Asaan Matlab (Yeh Kiya Karta Hai?) |
|---|---|
| **`ps`** ya **`tasks`** | Tamam active processes ka live table dikhata hai (PID, Name, State: RUNNING/READY/SLEEPING, Priority, CPU Ticks, Switches, Saved ESP). |
| **`task.spawn [naam]`** | Ring 0 mein naya background worker process start karta hai jo background mein math calculations aur prime search karta hai. |
| **`task.kill <pid>`** | Kisi bhi background task ko PID number se terminate/band kar deta hai (System Shell aur Idle task protected hain). |
| **`task.yield`** | Running process apni CPU baari voluntary tor par agle ready task ko de deta hai. |
| **`task.info <pid>`** | Task Control Block (TCB) aur stack par mojood asli hardware registers (`EIP`, `CS`, `EFLAGS`, `EAX`-`EDI`, `DS`-`GS`) ka dump dikhata hai. |
| **`task.stress <n>`** | Aik sath N concurrent heavy worker tasks spawn karke scheduler ki fair round-robin distribution test karta hai. |
| **`task.demo`** | 2 background worker threads start karke multitasking ka live demonstration dikhata hai jabke shell 100% interactive rehta hai! |

---

## 11. Quick Command List (Aik Nazar Mein)

| Kaam | Command |
|---|---|
| Motherboard chips ka dashboard dekhna | `chip` |
| RAM ka write-lock todna (Zero Security) | `chip wp off` |
| RAM par direct likhna | `chip write 0x200000 0x12345678` |
| RAM se number parhna | `chip read 0x200000` |
| Computer ko hard reset karna | `chip triplefault` |
| Motherboard speaker bajana | `beep` |
| CPU ka live temperature napna | `cpu.stress 1` |
| Hard disk ka binary data dekhna | `disk.hex 0` |
| Keyboard ki battiyan jalana | `leds 7` |
| Motherboard ghari dekhna | `time` |
| Screen ka rang change karna | `color matrix` |
| Computer band karna | `pc.shutdown()` |
| Multitasking Process List dekhna | `ps` ya `tasks` |
| Multitasking Showcase Demo chalana | `task.demo` |
| Help manual kholna | `help` ya `help2` |

---

## 12. Zaroori Baatein & Troubleshooting

1. **PC Par Boot Na Hone Ki Wajah**:
   - Agar USB boot na ho, to BIOS mein **Secure Boot = Disabled** aur **Legacy / CSM = Enabled** zaroor karein.
2. **`power` vs `poweroff`**:
   - `power`: Processor ka energy meter dikhata hai.
   - `poweroff` ya `pc.shutdown()`: Computer ko physically band karta hai.
3. **Zero Security Ka Faida**:
   - `chip wp off` chalane ke baad aap computer ki kisi bhi memory par bina kisi rok tok ke experiment kar sakte hain!

---

### 100% Asli Bare-Metal Engineering. Zero Jhoot. Direct Silicon Control.
