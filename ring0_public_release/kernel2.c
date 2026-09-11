/* =========================================================================
 * A.A OS - Kernel 2: Advanced Bare-Metal Silicon Mastery Engine
 * Hardware Debugger (DR0-DR7), ACPI/SMBIOS Hunter, MSR Controller,
 * Raw Packet Crafter & Frame Injector, Disk Forensics & HPA Master
 * ========================================================================= */

typedef unsigned char      uint8_t;
typedef signed char        int8_t;
typedef unsigned short     uint16_t;
typedef signed short       int16_t;
typedef unsigned int       uint32_t;
typedef int                int32_t;
typedef unsigned long long uint64_t;
typedef long long          int64_t;

/* Memory Management Pure Assembly Hooks (memorymanagement.asm) */
extern void asm_mem_copy_32(void* dest, const void* src, uint32_t count);
extern void asm_mem_fill_32(void* dest, uint8_t byte_val, uint32_t count);
extern void asm_mem_zero_32(void* dest, uint32_t count);
extern void asm_mem_xor_mask(void* addr, uint32_t mask, uint32_t count_bytes);
extern void asm_mem_reverse_endian_32(void* addr, uint32_t count_dwords);
extern uint32_t asm_mem_search_byte(uint32_t start_addr, uint32_t end_addr, uint8_t target);
extern void asm_dram_5pass_wash(uint32_t start_addr, uint32_t length_bytes);
extern void asm_mem_benchmark_latency(uint32_t addr, uint32_t* out_read_cycles, uint32_t* out_write_cycles);
extern void asm_mem_flush_range(uint32_t start_addr, uint32_t length_bytes);

enum vga_color {
    COLOR_BLACK = 0,
    COLOR_BLUE = 1,
    COLOR_GREEN = 2,
    COLOR_CYAN = 3,
    COLOR_RED = 4,
    COLOR_MAGENTA = 5,
    COLOR_BROWN = 6,
    COLOR_LIGHT_GREY = 7,
    COLOR_DARK_GREY = 8,
    COLOR_LIGHT_BLUE = 9,
    COLOR_LIGHT_GREEN = 10,
    COLOR_LIGHT_CYAN = 11,
    COLOR_LIGHT_RED = 12,
    COLOR_LIGHT_MAGENTA = 13,
    COLOR_LIGHT_BROWN = 14,
    COLOR_WHITE = 15,
};

/* Forward declarations from kernel.c */
extern void vga_puts(const char* data);
extern void vga_puts_color(const char* data, uint8_t color);
extern void vga_putc(char c);
extern void vga_putc_color(char c, uint8_t color);
extern void vga_put_uint(uint32_t val);
extern void vga_put_hex(uint32_t val);
extern void vga_put_hex8(uint8_t val);
extern void vga_put_hex16(uint16_t val);
extern void vga_clear_screen(void);
extern void dump_memory_hex(uint32_t start_addr, uint32_t length);
extern uint32_t parse_hex(const char* str);
extern int atoi(const char* str);
extern uint32_t strlen(const char* str);
extern int strcmp(const char* s1, const char* s2);
extern int strncmp(const char* s1, const char* s2, uint32_t n);
extern void* memcpy(void* dest, const void* src, uint32_t len);
extern void* memset(void* dest, uint8_t val, uint32_t len);
extern int ata_read_sector(uint8_t channel, uint8_t drive, uint32_t lba, uint8_t* buffer);
extern int ata_write_sector(uint8_t channel, uint8_t drive, uint32_t lba, const uint8_t* buffer);
extern int wifi_send_80211_frame(const void* frame_data, uint16_t len);
extern int e1000_send_packet(const void* data, uint16_t len);
extern int e1000_receive_packet(void* dest_buf, uint16_t max_len);
extern void readline(char* buffer, uint32_t max_len);

static inline uint8_t vga_color_entry(enum vga_color fg, enum vga_color bg) {
    return (uint8_t)(fg | (bg << 4));
}

static inline void rdmsr(uint32_t msr, uint32_t* lo, uint32_t* hi) {
    __asm__ volatile ("rdmsr" : "=a"(*lo), "=d"(*hi) : "c"(msr));
}

static inline void wrmsr(uint32_t msr, uint32_t lo, uint32_t hi) {
    __asm__ volatile ("wrmsr" : : "a"(lo), "d"(hi), "c"(msr));
}

static inline uint32_t parse_num(const char* str) {
    if (!str) return 0;
    while (*str == ' ') str++;
    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        return parse_hex(str);
    }
    const char* p = str;
    int is_hex = 0;
    while (*p && *p != ' ') {
        if ((*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F')) {
            is_hex = 1;
            break;
        }
        p++;
    }
    if (is_hex) return parse_hex(str);
    return (uint32_t)atoi(str);
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}


/* Pure Assembly Hardware Debugger Hooks (hardware_debug.asm) */
extern uint32_t asm_read_dr0(void);
extern uint32_t asm_read_dr1(void);
extern uint32_t asm_read_dr2(void);
extern uint32_t asm_read_dr3(void);
extern uint32_t asm_read_dr6(void);
extern uint32_t asm_read_dr7(void);
extern void asm_write_dr0(uint32_t val);
extern void asm_write_dr1(uint32_t val);
extern void asm_write_dr2(uint32_t val);
extern void asm_write_dr3(uint32_t val);
extern void asm_write_dr6(uint32_t val);
extern void asm_write_dr7(uint32_t val);
extern void asm_arm_hardware_breakpoint(uint32_t bp_idx, uint32_t addr, uint32_t condition, uint32_t length_code);
extern void asm_disarm_hardware_breakpoint(uint32_t bp_idx);

/* =========================================================================
 * 1. Hardware Silicon Debugger & Breakpoint Engine (DR0 - DR7)
 * ========================================================================= */

static inline uint32_t read_dr0(void) { return asm_read_dr0(); }
static inline uint32_t read_dr1(void) { return asm_read_dr1(); }
static inline uint32_t read_dr2(void) { return asm_read_dr2(); }
static inline uint32_t read_dr3(void) { return asm_read_dr3(); }
static inline uint32_t read_dr6(void) { return asm_read_dr6(); }
static inline uint32_t read_dr7(void) { return asm_read_dr7(); }

static inline void write_dr0(uint32_t val) { asm_write_dr0(val); }
static inline void write_dr1(uint32_t val) { asm_write_dr1(val); }
static inline void write_dr2(uint32_t val) { asm_write_dr2(val); }
static inline void write_dr3(uint32_t val) { asm_write_dr3(val); }
static inline void write_dr6(uint32_t val) { asm_write_dr6(val); }
static inline void write_dr7(uint32_t val) { asm_write_dr7(val); }

void cmd_debug_registers(void) {
    vga_puts_color("===============================================================================\n", vga_color_entry(COLOR_LIGHT_CYAN, COLOR_BLACK));
    vga_puts_color("   CPU SILICON HARDWARE DEBUG REGISTERS STATUS (DR0 - DR7)                     \n", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
    vga_puts_color("===============================================================================\n", vga_color_entry(COLOR_LIGHT_CYAN, COLOR_BLACK));

    uint32_t dr0 = read_dr0();
    uint32_t dr1 = read_dr1();
    uint32_t dr2 = read_dr2();
    uint32_t dr3 = read_dr3();
    uint32_t dr6 = read_dr6();
    uint32_t dr7 = read_dr7();

    vga_puts("  * DR0 (Breakpoint 0 Addr) : "); vga_put_hex(dr0); vga_puts("\n");
    vga_puts("  * DR1 (Breakpoint 1 Addr) : "); vga_put_hex(dr1); vga_puts("\n");
    vga_puts("  * DR2 (Breakpoint 2 Addr) : "); vga_put_hex(dr2); vga_puts("\n");
    vga_puts("  * DR3 (Breakpoint 3 Addr) : "); vga_put_hex(dr3); vga_puts("\n");
    vga_puts("  * DR6 (Debug Status Reg)  : "); vga_put_hex(dr6);
    vga_puts(" [B0: "); vga_put_uint(dr6 & 1);
    vga_puts(" | B1: "); vga_put_uint((dr6 >> 1) & 1);
    vga_puts(" | B2: "); vga_put_uint((dr6 >> 2) & 1);
    vga_puts(" | B3: "); vga_put_uint((dr6 >> 3) & 1);
    vga_puts(" | SingleStep(BD): "); vga_put_uint((dr6 >> 14) & 1);
    vga_puts("]\n");

    vga_puts("  * DR7 (Debug Control Reg) : "); vga_put_hex(dr7);
    vga_puts(" [L0: "); vga_put_uint(dr7 & 1);
    vga_puts(" | G0: "); vga_put_uint((dr7 >> 1) & 1);
    vga_puts(" | L1: "); vga_put_uint((dr7 >> 2) & 1);
    vga_puts(" | L2: "); vga_put_uint((dr7 >> 4) & 1);
    vga_puts(" | L3: "); vga_put_uint((dr7 >> 6) & 1);
    vga_puts("]\n");
    vga_puts_color("-------------------------------------------------------------------------------\n", vga_color_entry(COLOR_LIGHT_CYAN, COLOR_BLACK));
    vga_puts(" Commands: 'dbg.bp <0-3> <addr> <r|w|x> <1|2|4>' | 'dbg.clear <0-3>'\n");
}

void cmd_debug_set_breakpoint(const char* args) {
    while (*args == ' ') args++;
    if (!*args) {
        vga_puts("Usage: dbg.bp <0-3> <addr_hex> <r|w|x> <1|2|4>\n");
        return;
    }
    int bp_idx = *args - '0';
    if (bp_idx < 0 || bp_idx > 3) {
        vga_puts_color("[ERROR] Breakpoint index must be 0, 1, 2, or 3!\n", vga_color_entry(COLOR_LIGHT_RED, COLOR_BLACK));
        return;
    }
    while (*args && *args != ' ') args++;
    while (*args == ' ') args++;

    uint32_t addr = parse_hex(args);
    while (*args && *args != ' ') args++;
    while (*args == ' ') args++;

    char type = (*args) ? *args : 'x';
    while (*args && *args != ' ') args++;
    while (*args == ' ') args++;

    int len = (*args) ? (*args - '0') : 1;

    uint32_t cond = 0; // 00 = execute
    if (type == 'w' || type == 'W') cond = 1; // 01 = write only
    else if (type == 'r' || type == 'R') cond = 3; // 11 = read/write

    uint32_t len_bits = 0; // 00 = 1 byte
    if (len == 2) len_bits = 1; // 01 = 2 bytes
    else if (len == 4) len_bits = 3; // 11 = 4 bytes

    if (bp_idx == 0) write_dr0(addr);
    else if (bp_idx == 1) write_dr1(addr);
    else if (bp_idx == 2) write_dr2(addr);
    else if (bp_idx == 3) write_dr3(addr);

    uint32_t dr7 = read_dr7();
    dr7 |= (1 << (bp_idx * 2));
    dr7 &= ~(0x0F << (16 + (bp_idx * 4)));
    dr7 |= ((cond | (len_bits << 2)) << (16 + (bp_idx * 4)));
    write_dr7(dr7);

    vga_puts_color("[SUCCESS] Silicon Hardware Breakpoint ", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
    vga_put_uint(bp_idx); vga_puts(" armed at address "); vga_put_hex(addr);
    vga_puts(" (Type: "); vga_putc(type); vga_puts(", Length: "); vga_put_uint(len); vga_puts(" bytes)!\n");
}

void cmd_debug_clear_breakpoint(const char* args) {
    while (*args == ' ') args++;
    int bp_idx = *args - '0';
    if (bp_idx < 0 || bp_idx > 3) {
        vga_puts("Usage: dbg.clear <0-3>\n");
        return;
    }
    uint32_t dr7 = read_dr7();
    dr7 &= ~(3 << (bp_idx * 2));
    write_dr7(dr7);

    if (bp_idx == 0) write_dr0(0);
    else if (bp_idx == 1) write_dr1(0);
    else if (bp_idx == 2) write_dr2(0);
    else if (bp_idx == 3) write_dr3(0);

    vga_puts_color("[CLEARED] Hardware Breakpoint ", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
    vga_put_uint(bp_idx); vga_puts(" disabled.\n");
}

/* =========================================================================
 * 2. ACPI & SMBIOS Physical Firmware Table Hunter
 * ========================================================================= */

struct acpi_rsdp {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t ext_checksum;
    uint8_t reserved[3];
} __attribute__((packed));

struct acpi_sdt_header {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed));


static int acpi_checksum_valid(const void* ptr, uint32_t len) {
    const uint8_t* p = (const uint8_t*)ptr;
    uint8_t sum = 0;
    for (uint32_t i = 0; i < len; i++) sum += p[i];
    return (sum == 0);
}

void cmd_acpi_scan_tables(void) {
    vga_clear_screen();
    vga_puts_color("===============================================================================\n", vga_color_entry(COLOR_LIGHT_CYAN, COLOR_BLACK));
    vga_puts_color("   REAL MOTHERBOARD ACPI PHYSICAL TABLE HUNTER (EBDA & BIOS ROM SCAN)          \n", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
    vga_puts_color("===============================================================================\n", vga_color_entry(COLOR_LIGHT_CYAN, COLOR_BLACK));

    struct acpi_rsdp* found_rsdp = 0;
    uint32_t rsdp_addr = 0;

    for (uint32_t addr = 0x000E0000; addr < 0x00100000; addr += 16) {
        if (strncmp((const char*)addr, "RSD PTR ", 8) == 0) {
            if (acpi_checksum_valid((const void*)addr, 20)) {
                found_rsdp = (struct acpi_rsdp*)addr;
                rsdp_addr = addr;
                break;
            }
        }
    }

    if (!found_rsdp) {
        for (uint32_t addr = 0x0009FC00; addr < 0x000A0000; addr += 16) {
            if (strncmp((const char*)addr, "RSD PTR ", 8) == 0) {
                if (acpi_checksum_valid((const void*)addr, 20)) {
                    found_rsdp = (struct acpi_rsdp*)addr;
                    rsdp_addr = addr;
                    break;
                }
            }
        }
    }

    if (!found_rsdp) {
        vga_puts_color("[NOT FOUND] ACPI RSDP Pointer not detected in standard physical ROM space.\n", vga_color_entry(COLOR_LIGHT_RED, COLOR_BLACK));
        return;
    }

    vga_puts("  * ACPI RSDP Physical Location : "); vga_put_hex(rsdp_addr); vga_puts("\n");
    vga_puts("  * OEM Identifier              : ");
    for (int i = 0; i < 6; i++) vga_putc(found_rsdp->oem_id[i]);
    vga_puts("\n  * ACPI Specification Revision : "); vga_put_uint(found_rsdp->revision);
    vga_puts((found_rsdp->revision == 0) ? " (ACPI 1.0 RSDT 32-bit)\n" : " (ACPI 2.0+ XSDT 64-bit)\n");
    vga_puts("  * RSDT Physical Address Space : "); vga_put_hex(found_rsdp->rsdt_address); vga_puts("\n");

    if (found_rsdp->rsdt_address >= 0x00010000 && found_rsdp->rsdt_address < 0xB0000000) {
        struct acpi_sdt_header* rsdt = (struct acpi_sdt_header*)found_rsdp->rsdt_address;
        vga_puts_color("-------------------------------------------------------------------------------\n", vga_color_entry(COLOR_LIGHT_CYAN, COLOR_BLACK));
        vga_puts_color("DISCOVERED PHYSICAL ACPI SYSTEM TABLES:\n", vga_color_entry(COLOR_LIGHT_BROWN, COLOR_BLACK));

        uint32_t entries = (rsdt->length - sizeof(struct acpi_sdt_header)) / 4;
        uint32_t* table_ptrs = (uint32_t*)(found_rsdp->rsdt_address + sizeof(struct acpi_sdt_header));

        for (uint32_t i = 0; i < entries && i < 16; i++) {
            uint32_t tbl_addr = table_ptrs[i];
            if (tbl_addr >= 0x00010000 && tbl_addr < 0xB0000000) {
                struct acpi_sdt_header* sdt = (struct acpi_sdt_header*)tbl_addr;
                vga_puts("  ["); vga_put_uint(i); vga_puts("] Signature: '");
                for (int k = 0; k < 4; k++) vga_putc(sdt->signature[k]);
                vga_puts("' | Physical Addr: "); vga_put_hex(tbl_addr);
                vga_puts(" | Length: "); vga_put_uint(sdt->length); vga_puts(" bytes\n");
            }
        }
    }
}

/* =========================================================================
 * 3. Genuine Wi-Fi / Ethernet DMA Packet Injection Engine
 * ========================================================================= */

static int parse_mac_bytes(const char* str, uint8_t out[6]) {
    while (*str == ' ') str++;
    for (int i = 0; i < 6; i++) {
        if (!*str) return 0;
        char hex_buf[3];
        hex_buf[0] = *str++;
        hex_buf[1] = (*str && *str != ':' && *str != '-') ? *str++ : '0';
        hex_buf[2] = 0;
        out[i] = (uint8_t)parse_hex(hex_buf);
        if (*str == ':' || *str == '-') str++;
    }
    return 1;
}

void cmd_wifi_inject_deauth(const char* args) {
    while (*args == ' ') args++;
    if (!*args) {
        vga_puts("Usage: packet.inject.deauth <target_mac> <bssid> <reason_code>\n");
        vga_puts("Example: packet.inject.deauth FF:FF:FF:FF:FF:FF 00:11:22:33:44:55 7\n");
        return;
    }

    uint8_t target_mac[6];
    uint8_t bssid[6];
    uint16_t reason = 7;

    if (!parse_mac_bytes(args, target_mac)) {
        vga_puts_color("[ERROR] Invalid target MAC address format! Use XX:XX:XX:XX:XX:XX\n", vga_color_entry(COLOR_LIGHT_RED, COLOR_BLACK));
        return;
    }
    while (*args && *args != ' ') args++;
    while (*args == ' ') args++;

    if (!parse_mac_bytes(args, bssid)) {
        vga_puts_color("[ERROR] Invalid BSSID MAC address format! Use XX:XX:XX:XX:XX:XX\n", vga_color_entry(COLOR_LIGHT_RED, COLOR_BLACK));
        return;
    }
    while (*args && *args != ' ') args++;
    while (*args == ' ') args++;
    if (*args) reason = (uint16_t)atoi(args);

    uint8_t frame[128];
    memset(frame, 0, sizeof(frame));

    frame[0] = 0xC0; // Type: Mgmt, Subtype: Deauth
    frame[1] = 0x00;
    frame[2] = 0x3A;
    frame[3] = 0x01;

    memcpy(&frame[4], target_mac, 6);
    memcpy(&frame[10], bssid, 6);
    memcpy(&frame[16], bssid, 6);

    frame[22] = 0x00; frame[23] = 0x00;
    frame[24] = (uint8_t)(reason & 0xFF);
    frame[25] = (uint8_t)(reason >> 8);

    int res = wifi_send_80211_frame(frame, 26);
    if (res) {
        vga_puts_color("[AIR INJECTION SUCCESS] Transmitted 26-Byte Raw 802.11 Deauth Frame via DMA Ring!\n", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
    } else {
        vga_puts_color("[HARDWARE STATUS] Intel Wi-Fi physical controller not detected on PCI bus.\n", vga_color_entry(COLOR_LIGHT_RED, COLOR_BLACK));
        vga_puts("  * Genuine Reality: Bare-metal transmission requires physical Intel Wireless PCIe silicon.\n");
    }
}

/* =========================================================================
 * 4. MSR Silicon Performance & Thermal Overclocking Controller
 * ========================================================================= */

void cmd_msr_read(const char* args) {
    while (*args == ' ') args++;
    if (!*args) {
        vga_puts("Usage: msr.read <msr_hex> (e.g. msr.read 0x198 for IA32_PERF_STATUS)\n");
        return;
    }
    uint32_t msr = parse_hex(args);
    uint32_t lo = 0, hi = 0;
    rdmsr(msr, &lo, &hi);

    vga_puts_color("[MSR READOUT] MSR Index: ", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
    vga_put_hex(msr); vga_puts(":\n");
    vga_puts("  * High 32-bit (EDX) : "); vga_put_hex(hi); vga_puts("\n");
    vga_puts("  * Low  32-bit (EAX) : "); vga_put_hex(lo); vga_puts("\n");
    vga_puts("  * Full 64-bit Hex   : "); vga_put_hex(hi); vga_puts(" "); vga_put_hex(lo); vga_puts("\n");
}

void cmd_cpu_thermal_stress(const char* args) {
    while (*args == ' ') args++;
    uint32_t iters = 1000000;
    if (*args) iters = (uint32_t)atoi(args) * 500000;

    vga_puts_color("[CPU SILICON THERMAL MONITOR & LOAD STRESS]\n", vga_color_entry(COLOR_LIGHT_CYAN, COLOR_BLACK));

    // Check CPUID leaf 0 for Intel vendor
    uint32_t eax0 = 0, ebx0 = 0, ecx0 = 0, edx0 = 0;
    __asm__ volatile ("cpuid" : "=a"(eax0), "=b"(ebx0), "=c"(ecx0), "=d"(edx0) : "a"(0));
    int is_intel = (ebx0 == 0x756E6547 && edx0 == 0x49656E69 && ecx0 == 0x6C65746E);

    // Check CPUID leaf 6 for Digital Thermal Sensor (EAX bit 0)
    uint32_t eax6 = 0, ebx6 = 0, ecx6 = 0, edx6 = 0;
    if (eax0 >= 6) {
        __asm__ volatile ("cpuid" : "=a"(eax6), "=b"(ebx6), "=c"(ecx6), "=d"(edx6) : "a"(6));
    }
    int dts_supported = (is_intel && (eax6 & 1));

    uint32_t therm_before_lo = 0, therm_before_hi = 0;
    uint32_t target_lo = 0, target_hi = 0;
    uint32_t tj_max = 100;

    if (dts_supported) {
        rdmsr(0x19C, &therm_before_lo, &therm_before_hi); // IA32_THERM_STATUS
        rdmsr(0x1A2, &target_lo, &target_hi);             // MSR_TEMPERATURE_TARGET
        uint32_t tm = (target_lo >> 16) & 0xFF;
        if (tm >= 60 && tm <= 125) tj_max = tm;

        uint32_t dts_before = (therm_before_lo >> 16) & 0x7F;
        vga_puts("  * Pre-Stress DTS Readout (MSR 0x19C) : "); vga_put_hex(therm_before_lo);
        if (therm_before_lo & (1 << 31)) {
            vga_puts(" (Temp: ~"); vga_put_uint(tj_max - dts_before); vga_puts(" C)\n");
        } else {
            vga_puts(" [DTS Reading Valid Bit not set]\n");
        }
    } else {
        vga_puts("  * Digital Thermal Sensor (MSR 0x19C): Not supported on this CPU model (DTS=0)\n");
    }

    uint32_t s_lo, s_hi, e_lo, e_hi;
    __asm__ volatile ("rdtsc" : "=a"(s_lo), "=d"(s_hi));

    volatile uint32_t a = 0x12345678, b = 0x9ABCDEF0;
    for (uint32_t i = 0; i < iters; i++) {
        a = (a ^ b) + 0x01010101;
        b = (b << 3) | (b >> 29);
        a ^= (b + i);
    }

    __asm__ volatile ("rdtsc" : "=a"(e_lo), "=d"(e_hi));
    uint32_t total_cycles = e_lo - s_lo;

    vga_puts_color("[COMPLETE] Executed ", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
    vga_put_uint(iters); vga_puts(" intensive silicon crunch iterations!\n");
    vga_puts("  * Elapsed Core Cycles (RDTSC) : ~"); vga_put_uint(total_cycles); vga_puts(" cycles\n");

    if (dts_supported) {
        uint32_t therm_after_lo = 0, therm_after_hi = 0;
        rdmsr(0x19C, &therm_after_lo, &therm_after_hi);
        uint32_t dts_after = (therm_after_lo >> 16) & 0x7F;
        vga_puts("  * Post-Stress DTS (MSR 0x19C) : "); vga_put_hex(therm_after_lo);
        if (therm_after_lo & (1 << 31)) {
            vga_puts(" (Temp: ~"); vga_put_uint(tj_max - dts_after); vga_puts(" C)\n");
        }
    }
}

/* =========================================================================
 * 5. Secret ATA Hard Disk Forensics & HPA Capacity Master
 * ========================================================================= */

void cmd_disk_hex_dump(const char* args) {
    while (*args == ' ') args++;
    uint32_t lba = 0;
    if (*args) lba = parse_num(args);

    uint8_t sector[512];
    memset(sector, 0, 512);

    if (!ata_read_sector(0, 0, lba, sector)) {
        vga_puts_color("[FAIL] Could not read LBA ", vga_color_entry(COLOR_LIGHT_RED, COLOR_BLACK));
        vga_put_uint(lba); vga_puts(" from ATA Primary Master!\n");
        return;
    }

    vga_puts_color("===============================================================================\n", vga_color_entry(COLOR_LIGHT_CYAN, COLOR_BLACK));
    vga_puts_color("  REAL DISK SILICON FORENSIC HEX DUMP - LBA ", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
    vga_put_uint(lba); vga_puts_color(" (512 BYTES PHYSICAL READ)\n", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
    vga_puts_color("===============================================================================\n", vga_color_entry(COLOR_LIGHT_CYAN, COLOR_BLACK));

    for (int r = 0; r < 512; r += 16) {
        vga_puts("[+0x"); vga_put_hex16((uint16_t)r); vga_puts("] ");
        for (int c = 0; c < 16; c++) {
            vga_put_hex8(sector[r + c]);
            vga_putc(' ');
        }
        vga_puts("| ");
        for (int c = 0; c < 16; c++) {
            uint8_t b = sector[r + c];
            vga_putc((b >= 32 && b <= 126) ? (char)b : '.');
        }
        vga_putc('\n');
    }
}

/* =========================================================================
 * 6. Pure Assembly Memory Control Suite (Powered by memorymanagement.asm)
 * ========================================================================= */

void cmd_mem_wash_5pass(const char* args) {
    while (*args == ' ') args++;
    uint32_t addr = 0x00200000;
    uint32_t len  = 1024 * 1024; // 1 MB default
    if (*args) {
        addr = parse_hex(args);
        while (*args && *args != ' ') args++;
        while (*args == ' ') args++;
        if (*args) len = parse_hex(args);
    }

    vga_puts_color("[DRAM 5-PASS WASH] Executing pure assembly military DRAM wash on physical RAM ", vga_color_entry(COLOR_LIGHT_CYAN, COLOR_BLACK));
    vga_put_hex(addr); vga_puts(" (Length: "); vga_put_uint(len); vga_puts(" bytes)...\n");
    vga_puts("  * Pass 1: 0x00000000 (All Zeros) + wbinvd\n");
    vga_puts("  * Pass 2: 0xFFFFFFFF (All Ones)  + wbinvd\n");
    vga_puts("  * Pass 3: 0xAAAAAAAA (Checker)   + wbinvd\n");
    vga_puts("  * Pass 4: 0x55555555 (Inverted)  + wbinvd\n");
    vga_puts("  * Pass 5: 0xAA55AA55 (Magic Cal) + wbinvd\n");

    asm_dram_5pass_wash(addr, len);

    vga_puts_color("[COMPLETE] Pure assembly DRAM wash completed with 100% hardware cache flush!\n", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
}

void cmd_mem_search(const char* args) {
    while (*args == ' ') args++;
    if (!*args) {
        vga_puts("Usage: mem.search <start_hex> <end_hex> <byte_hex>\n");
        return;
    }
    uint32_t start = parse_hex(args);
    while (*args && *args != ' ') args++;
    while (*args == ' ') args++;
    uint32_t end = parse_hex(args);
    while (*args && *args != ' ') args++;
    while (*args == ' ') args++;
    uint8_t target = (uint8_t)parse_hex(args);

    vga_puts_color("[MEM SEARCH] Scanning RAM ", vga_color_entry(COLOR_LIGHT_CYAN, COLOR_BLACK));
    vga_put_hex(start); vga_puts(" -> "); vga_put_hex(end);
    vga_puts(" for byte 0x"); vga_put_hex8(target); vga_puts(" using 'repne scasb'...\n");

    uint32_t match = asm_mem_search_byte(start, end, target);
    if (match) {
        vga_puts_color("[FOUND] Hardware match at physical address: 0x", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
        vga_put_hex(match); vga_puts("\n");
        dump_memory_hex(match, 32);
    } else {
        vga_puts_color("[NOT FOUND] Zero matching bytes in specified address space.\n", vga_color_entry(COLOR_LIGHT_RED, COLOR_BLACK));
    }
}

void cmd_mem_endian_flip(const char* args) {
    while (*args == ' ') args++;
    if (!*args) {
        vga_puts("Usage: mem.bswap <addr_hex> <dwords_count>\n");
        return;
    }
    uint32_t addr = parse_hex(args);
    while (*args && *args != ' ') args++;
    while (*args == ' ') args++;
    uint32_t count = (*args) ? parse_hex(args) : 8;

    vga_puts_color("[MEM BSWAP] Reversing endianness across ", vga_color_entry(COLOR_LIGHT_CYAN, COLOR_BLACK));
    vga_put_uint(count); vga_puts(" dwords at physical RAM "); vga_put_hex(addr); vga_puts(" using x86 'bswap'...\n");

    vga_puts("Before:\n");
    dump_memory_hex(addr, count * 4);

    asm_mem_reverse_endian_32((void*)addr, count);

    vga_puts("After bswap:\n");
    dump_memory_hex(addr, count * 4);
}

/* =========================================================================
 * 7. Dispatcher for All Kernel 2 Subsystems
 * ========================================================================= */

extern void asm_chip_pic_set_state(uint32_t enable);
extern uint32_t asm_chip_apic_set_state(uint32_t enable);
extern void asm_chip_ps2_set_state(uint32_t device, uint32_t enable);
extern void asm_chip_cache_set_state(uint32_t enable);
extern void asm_chip_speaker_set_state(uint32_t enable);
extern uint32_t asm_chip_pci_set_power_state(uint8_t bus, uint8_t slot, uint8_t func, uint8_t state);
extern void asm_silicon_atomic_swap_mem(uint32_t addr1, uint32_t addr2, uint32_t dword_count);
extern void asm_silicon_pit_set_rate(uint32_t freq_hz);
extern uint32_t asm_silicon_get_cr0(void);
extern uint32_t asm_silicon_get_cr4(void);
extern void asm_silicon_set_wp(uint32_t enable);
extern void asm_silicon_wrmsr_raw(uint32_t msr, uint32_t lo, uint32_t hi);
extern void asm_silicon_cli(void);
extern void asm_silicon_sti(void);
extern void asm_silicon_write_phys_dword(uint32_t addr, uint32_t val);
extern uint32_t asm_silicon_read_phys_dword(uint32_t addr);
extern void asm_silicon_triple_fault(void);

// Pure Assembly Storage & Memory Editor Declarations (storage_editor.asm)
extern uint32_t asm_storage_edit_sector_byte(uint32_t lba, uint32_t offset, uint8_t new_val, void* temp_buf);
extern uint32_t asm_storage_fill_sectors_raw(uint32_t start_lba, uint32_t count, uint8_t byte_val, void* temp_buf);
extern uint32_t asm_storage_copy_sector_raw(uint32_t src_lba, uint32_t dst_lba, void* temp_buf);
extern void asm_mem_edit_byte_raw(uint32_t addr, uint8_t val);
extern void asm_mem_fill_raw(uint32_t addr, uint32_t count, uint8_t val);
extern void asm_mem_erase_raw(uint32_t addr, uint32_t count);

// Silicon Engine Declarations (silicon_engine.asm)
typedef struct {
    uint32_t eax, ebx, ecx, edx, esi, edi, ebp, esp;
    uint32_t cs, ds, ss, es, fs, gs;
    uint32_t cr0, cr2, cr3, cr4;
    uint32_t eflags;
} __attribute__((packed)) cpu_snapshot_t;

extern void asm_capture_cpu_snapshot(void* snapshot_struct_76_bytes);
extern void asm_measure_instruction_latencies(uint32_t* results_array_8_elements);
extern void asm_flush_tlb_page(uint32_t virtual_addr);
extern void asm_full_pipeline_flush(void);
extern int asm_pure_memcmp(const void* s1, const void* s2, uint32_t n);
extern void asm_pure_memcpy_fast(void* dest, const void* src, uint32_t count_bytes);
extern void asm_pure_memset_dword(void* dest, uint32_t dword_val, uint32_t dword_count);

void cmd_asm_snapshot(const char* args) {
    (void)args;
    cpu_snapshot_t snap;
    memset(&snap, 0, sizeof(snap));
    asm_capture_cpu_snapshot(&snap);

    vga_clear_screen();
    vga_puts_color("===============================================================================\n", vga_color_entry(COLOR_LIGHT_CYAN, COLOR_BLACK));
    vga_puts_color("   [ATOMIC CPU SILICON SNAPSHOT] DIRECT PURE ASSEMBLY REGISTER CAPTURE         \n", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
    vga_puts_color("===============================================================================\n", vga_color_entry(COLOR_LIGHT_CYAN, COLOR_BLACK));

    vga_puts_color("1. 32-BIT GENERAL PURPOSE REGISTERS (GPRs):\n", vga_color_entry(COLOR_LIGHT_BROWN, COLOR_BLACK));
    vga_puts("  * EAX: "); vga_put_hex(snap.eax); vga_puts("   EBX: "); vga_put_hex(snap.ebx);
    vga_puts("   ECX: "); vga_put_hex(snap.ecx); vga_puts("   EDX: "); vga_put_hex(snap.edx); vga_putc('\n');
    vga_puts("  * ESI: "); vga_put_hex(snap.esi); vga_puts("   EDI: "); vga_put_hex(snap.edi);
    vga_puts("   EBP: "); vga_put_hex(snap.ebp); vga_puts("   ESP: "); vga_put_hex(snap.esp); vga_puts("\n\n");

    vga_puts_color("2. 16-BIT HARDWARE SEGMENT SELECTORS:\n", vga_color_entry(COLOR_LIGHT_BROWN, COLOR_BLACK));
    vga_puts("  * CS (Code): 0x"); vga_put_hex16((uint16_t)snap.cs);
    vga_puts("   DS (Data): 0x"); vga_put_hex16((uint16_t)snap.ds);
    vga_puts("   SS (Stack): 0x"); vga_put_hex16((uint16_t)snap.ss); vga_putc('\n');
    vga_puts("  * ES: 0x"); vga_put_hex16((uint16_t)snap.es);
    vga_puts("        FS: 0x"); vga_put_hex16((uint16_t)snap.fs);
    vga_puts("        GS: 0x"); vga_put_hex16((uint16_t)snap.gs); vga_puts("\n\n");

    vga_puts_color("3. CPU CONTROL REGISTERS (CR0 - CR4):\n", vga_color_entry(COLOR_LIGHT_BROWN, COLOR_BLACK));
    vga_puts("  * CR0: "); vga_put_hex(snap.cr0); vga_puts(" (PE="); vga_put_uint(snap.cr0 & 1);
    vga_puts(", WP="); vga_put_uint((snap.cr0 >> 16) & 1); vga_puts(", CD="); vga_put_uint((snap.cr0 >> 30) & 1); vga_puts(")\n");
    vga_puts("  * CR2: "); vga_put_hex(snap.cr2); vga_puts(" (Page Fault Linear Address)\n");
    vga_puts("  * CR3: "); vga_put_hex(snap.cr3); vga_puts(" (Page Directory Base)\n");
    vga_puts("  * CR4: "); vga_put_hex(snap.cr4); vga_puts(" (PSE="); vga_put_uint((snap.cr4 >> 4) & 1);
    vga_puts(", PAE="); vga_put_uint((snap.cr4 >> 5) & 1); vga_puts(", PGE="); vga_put_uint((snap.cr4 >> 7) & 1); vga_puts(")\n\n");

    vga_puts_color("4. EFLAGS STATUS & PRIVILEGE:\n", vga_color_entry(COLOR_LIGHT_BROWN, COLOR_BLACK));
    vga_puts("  * EFLAGS: "); vga_put_hex(snap.eflags);
    vga_puts(" [ IF="); vga_put_uint((snap.eflags >> 9) & 1);
    vga_puts(" CF="); vga_put_uint(snap.eflags & 1);
    vga_puts(" ZF="); vga_put_uint((snap.eflags >> 6) & 1);
    vga_puts(" SF="); vga_put_uint((snap.eflags >> 7) & 1);
    vga_puts(" IOPL="); vga_put_uint((snap.eflags >> 12) & 3);
    vga_puts(" ]\n");
    vga_puts_color("===============================================================================\n", vga_color_entry(COLOR_LIGHT_CYAN, COLOR_BLACK));
}

void cmd_asm_bench(const char* args) {
    (void)args;
    uint32_t lats[8];
    memset(lats, 0, sizeof(lats));

    vga_puts_color("[SILICON BENCHMARK] Measuring exact instruction latencies via serialized RDTSC...\n", vga_color_entry(COLOR_LIGHT_CYAN, COLOR_BLACK));
    asm_measure_instruction_latencies(lats);

    vga_puts_color("-------------------------------------------------------------------------------\n", vga_color_entry(COLOR_LIGHT_CYAN, COLOR_BLACK));
    vga_puts("  * NOP Latency                     : "); vga_put_uint(lats[0]); vga_puts(" CPU cycles\n");
    vga_puts("  * ADD (reg, reg) Latency          : "); vga_put_uint(lats[1]); vga_puts(" CPU cycles\n");
    vga_puts("  * IMUL (reg, reg) Latency         : "); vga_put_uint(lats[2]); vga_puts(" CPU cycles\n");
    vga_puts("  * IDIV (reg) Latency              : "); vga_put_uint(lats[3]); vga_puts(" CPU cycles\n");
    vga_puts("  * MOV [Physical RAM], reg Latency : "); vga_put_uint(lats[4]); vga_puts(" CPU cycles\n");
    vga_puts("  * IN AL, 0x80 (Port I/O) Latency  : "); vga_put_uint(lats[5]); vga_puts(" CPU cycles\n");
    vga_puts("  * CPUID (Leaf 0) Latency          : "); vga_put_uint(lats[6]); vga_puts(" CPU cycles\n");
    vga_puts("  * WBINVD (Cache Flush) Latency    : "); vga_put_uint(lats[7]); vga_puts(" CPU cycles\n");
    vga_puts_color("-------------------------------------------------------------------------------\n", vga_color_entry(COLOR_LIGHT_CYAN, COLOR_BLACK));
}

void cmd_asm_tlb(const char* args) {
    while (*args == ' ') args++;
    uint32_t addr = parse_num(args);
    asm_flush_tlb_page(addr);
    vga_puts_color("[TLB FLUSH] Executed 'invlpg' on address: ", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
    vga_put_hex(addr); vga_putc('\n');
}

void cmd_asm_pipeline_flush(const char* args) {
    (void)args;
    asm_full_pipeline_flush();
}

static uint8_t s_editor_buf[512] __attribute__((aligned(4)));

void cmd_disk_edit(const char* args) {
    while (*args == ' ') args++;
    if (!*args) {
        vga_puts("Usage: disk.edit <lba> <offset_0_511> <hex_byte>\n");
        return;
    }
    uint32_t lba = parse_num(args);
    while (*args && *args != ' ') args++;
    while (*args == ' ') args++;
    uint32_t offset = parse_num(args);
    while (*args && *args != ' ') args++;
    while (*args == ' ') args++;
    uint8_t val = (uint8_t)parse_num(args);

    if (offset >= 512) {
        vga_puts_color("[ERROR] Offset must be between 0 and 511.\n", vga_color_entry(COLOR_LIGHT_RED, COLOR_BLACK));
        return;
    }

    vga_puts_color("[RAW SILICON DISK EDIT] Modifying LBA ", vga_color_entry(COLOR_LIGHT_CYAN, COLOR_BLACK));
    vga_put_uint(lba); vga_puts(", Offset "); vga_put_uint(offset);
    vga_puts(" with byte 0x"); vga_put_hex8(val); vga_puts(" via pure assembly ATA PIO...\n");

    uint32_t ok = asm_storage_edit_sector_byte(lba, offset, val, s_editor_buf);
    if (ok) {
        vga_puts_color("[SUCCESS] Physical sector byte modified and written to disk with cache flush!\n", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
    } else {
        vga_puts_color("[FAIL] Hardware ATA PIO write failed on drive.\n", vga_color_entry(COLOR_LIGHT_RED, COLOR_BLACK));
    }
}

void cmd_disk_fill(const char* args) {
    while (*args == ' ') args++;
    if (!*args) {
        vga_puts("Usage: disk.fill <start_lba> <count> <hex_byte>\n");
        return;
    }
    uint32_t lba = parse_num(args);
    while (*args && *args != ' ') args++;
    while (*args == ' ') args++;
    uint32_t count = parse_num(args);
    while (*args && *args != ' ') args++;
    while (*args == ' ') args++;
    uint8_t val = (uint8_t)parse_num(args);

    if (count == 0) count = 1;

    vga_puts_color("[RAW DISK FILL] Writing pattern 0x", vga_color_entry(COLOR_LIGHT_CYAN, COLOR_BLACK));
    vga_put_hex8(val); vga_puts(" across "); vga_put_uint(count);
    vga_puts(" sectors starting at LBA "); vga_put_uint(lba); vga_puts("...\n");

    uint32_t done = asm_storage_fill_sectors_raw(lba, count, val, s_editor_buf);
    vga_puts_color("[COMPLETE] Successfully wrote ", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
    vga_put_uint(done); vga_puts(" physical sectors to storage.\n");
}

void cmd_disk_erase(const char* args) {
    while (*args == ' ') args++;
    if (!*args) {
        vga_puts("Usage: disk.erase <start_lba> [count]\n");
        return;
    }
    uint32_t lba = parse_num(args);
    while (*args && *args != ' ') args++;
    while (*args == ' ') args++;
    uint32_t count = (*args) ? parse_num(args) : 1;
    if (count == 0) count = 1;

    vga_puts_color("[RAW DISK ERASE] Zeroing out ", vga_color_entry(COLOR_LIGHT_RED, COLOR_BLACK));
    vga_put_uint(count); vga_puts(" sectors starting at LBA "); vga_put_uint(lba); vga_puts(" via pure assembly...\n");

    uint32_t done = asm_storage_fill_sectors_raw(lba, count, 0x00, s_editor_buf);
    vga_puts_color("[SUCCESS] Erased ", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
    vga_put_uint(done); vga_puts(" physical sectors with zeroes!\n");
}

void cmd_disk_copy(const char* args) {
    while (*args == ' ') args++;
    if (!*args) {
        vga_puts("Usage: disk.copy <src_lba> <dst_lba>\n");
        return;
    }
    uint32_t src = parse_num(args);
    while (*args && *args != ' ') args++;
    while (*args == ' ') args++;
    uint32_t dst = parse_num(args);

    vga_puts_color("[RAW DISK CLONE] Cloning physical LBA ", vga_color_entry(COLOR_LIGHT_CYAN, COLOR_BLACK));
    vga_put_uint(src); vga_puts(" -> LBA "); vga_put_uint(dst); vga_puts("...\n");

    uint32_t ok = asm_storage_copy_sector_raw(src, dst, s_editor_buf);
    if (ok) {
        vga_puts_color("[SUCCESS] Physical sector cloned successfully!\n", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
    } else {
        vga_puts_color("[FAIL] Sector clone failed on storage controller.\n", vga_color_entry(COLOR_LIGHT_RED, COLOR_BLACK));
    }
}

void cmd_mem_edit(const char* args) {
    while (*args == ' ') args++;
    if (!*args) {
        vga_puts("Usage: mem.edit <addr_hex> <byte_hex>\n");
        return;
    }
    uint32_t addr = parse_hex(args);
    while (*args && *args != ' ') args++;
    while (*args == ' ') args++;
    uint8_t val = (uint8_t)parse_hex(args);

    asm_silicon_set_wp(0);
    asm_mem_edit_byte_raw(addr, val);
    vga_puts_color("[MEM EDIT] Written byte 0x", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
    vga_put_hex8(val); vga_puts(" to physical address "); vga_put_hex(addr); vga_puts(" with cache flush.\n");
}

void cmd_mem_fill(const char* args) {
    while (*args == ' ') args++;
    if (!*args) {
        vga_puts("Usage: mem.fill <addr_hex> <count_hex> <byte_hex>\n");
        return;
    }
    uint32_t addr = parse_hex(args);
    while (*args && *args != ' ') args++;
    while (*args == ' ') args++;
    uint32_t count = parse_hex(args);
    while (*args && *args != ' ') args++;
    while (*args == ' ') args++;
    uint8_t val = (uint8_t)parse_hex(args);

    asm_silicon_set_wp(0);
    asm_mem_fill_raw(addr, count, val);
    vga_puts_color("[MEM FILL] Filled ", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
    vga_put_uint(count); vga_puts(" bytes at "); vga_put_hex(addr); vga_puts(" with pattern 0x"); vga_put_hex8(val); vga_puts("\n");
}

void cmd_mem_erase(const char* args) {
    while (*args == ' ') args++;
    if (!*args) {
        vga_puts("Usage: mem.erase <addr_hex> <count_hex>\n");
        return;
    }
    uint32_t addr = parse_hex(args);
    while (*args && *args != ' ') args++;
    while (*args == ' ') args++;
    uint32_t count = parse_hex(args);

    asm_silicon_set_wp(0);
    asm_mem_erase_raw(addr, count);
    vga_puts_color("[MEM ERASE] Zeroed out ", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
    vga_put_uint(count); vga_puts(" bytes at physical address "); vga_put_hex(addr); vga_puts(".\n");
}

void cmd_mem_view(const char* args) {
    while (*args == ' ') args++;
    if (!*args) {
        vga_puts("Usage: mem.view <addr_hex> [length_hex]\n");
        return;
    }
    uint32_t addr = parse_hex(args);
    while (*args && *args != ' ') args++;
    while (*args == ' ') args++;
    uint32_t len = (*args) ? parse_hex(args) : 64;
    if (len > 512) len = 512;

    vga_puts_color("===============================================================================\n", vga_color_entry(COLOR_LIGHT_CYAN, COLOR_BLACK));
    vga_puts_color("  PHYSICAL RAM RAW MEMORY DUMP: ", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
    vga_put_hex(addr); vga_puts_color(" (", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
    vga_put_uint(len); vga_puts_color(" BYTES)\n", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
    vga_puts_color("===============================================================================\n", vga_color_entry(COLOR_LIGHT_CYAN, COLOR_BLACK));

    uint8_t* p = (uint8_t*)addr;
    for (uint32_t r = 0; r < len; r += 16) {
        vga_puts("["); vga_put_hex(addr + r); vga_puts("] ");
        for (uint32_t c = 0; c < 16; c++) {
            if (r + c < len) {
                vga_put_hex8(p[r + c]);
                vga_putc(' ');
            } else {
                vga_puts("   ");
            }
        }
        vga_puts("| ");
        for (uint32_t c = 0; c < 16; c++) {
            if (r + c < len) {
                uint8_t b = p[r + c];
                vga_putc((b >= 32 && b <= 126) ? (char)b : '.');
            }
        }
        vga_putc('\n');
    }
}

void cmd_chip_control(const char* args) {
    while (*args == ' ') args++;
    if (*args == '\0' || strcmp(args, "status") == 0 || strcmp(args, "list") == 0) {
        vga_clear_screen();
        vga_puts_color("===============================================================================\n", vga_color_entry(COLOR_LIGHT_CYAN, COLOR_BLACK));
        vga_puts_color("   [CHIP POWER CONTROLLER] MOTHERBOARD SILICON HARDWARE ON/OFF DASHBOARD       \n", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
        vga_puts_color("===============================================================================\n", vga_color_entry(COLOR_LIGHT_CYAN, COLOR_BLACK));
        
        uint32_t cr0 = asm_silicon_get_cr0();
        int cache_disabled = (cr0 & (1 << 30)) ? 1 : 0;
        int wp_active = (cr0 & (1 << 16)) ? 1 : 0;
        
        vga_puts_color("1. CPU Core SRAM Caching Hierarchy (L1/L2/L3):\n", vga_color_entry(COLOR_LIGHT_BROWN, COLOR_BLACK));
        vga_puts("  * Status      : ");
        if (!cache_disabled) vga_puts_color("[POWERED ON / ACTIVE]\n", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
        else vga_puts_color("[POWERED OFF / DISABLED]\n", vga_color_entry(COLOR_LIGHT_RED, COLOR_BLACK));
        vga_puts("  * Control     : chip cache <on|off>\n\n");

        vga_puts_color("2. Supervisor Write Protection (CR0.WP - Ring 0 Memory Lock):\n", vga_color_entry(COLOR_LIGHT_BROWN, COLOR_BLACK));
        vga_puts("  * Status      : ");
        if (wp_active) vga_puts_color("[LOCKED / CR0.WP=1]\n", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
        else vga_puts_color("[UNLOCKED / ZERO SECURITY CR0.WP=0]\n", vga_color_entry(COLOR_LIGHT_RED, COLOR_BLACK));
        vga_puts("  * Control     : chip wp <on|off>\n\n");

        vga_puts_color("3. Intel 8259 Dual PIC (Programmable Interrupt Controllers):\n", vga_color_entry(COLOR_LIGHT_BROWN, COLOR_BLACK));
        uint8_t m_mask = inb(0x21), s_mask = inb(0xA1);
        vga_puts("  * Status      : ");
        if (m_mask == 0xFF && s_mask == 0xFF) vga_puts_color("[POWERED OFF / ALL IRQs MASKED]\n", vga_color_entry(COLOR_LIGHT_RED, COLOR_BLACK));
        else vga_puts_color("[POWERED ON / IRQs ACTIVE]\n", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
        vga_puts("  * Control     : chip pic <on|off>\n\n");

        vga_puts_color("4. On-Die Local APIC Interrupt Core:\n", vga_color_entry(COLOR_LIGHT_BROWN, COLOR_BLACK));
        uint32_t apic_lo = 0, apic_hi = 0;
        rdmsr(0x1B, &apic_lo, &apic_hi);
        vga_puts("  * Status      : ");
        if (apic_lo & (1 << 11)) vga_puts_color("[POWERED ON / HARDWARE ENABLED]\n", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
        else vga_puts_color("[POWERED OFF / HARDWARE DISABLED]\n", vga_color_entry(COLOR_LIGHT_RED, COLOR_BLACK));
        vga_puts("  * Control     : chip apic <on|off>\n\n");

        vga_puts_color("5. Intel 8042 PS/2 Keyboard & Mouse Controller Interface:\n", vga_color_entry(COLOR_LIGHT_BROWN, COLOR_BLACK));
        vga_puts("  * Control     : chip kbd <on|off> | chip mouse <on|off>\n\n");

        vga_puts_color("6. Motherboard Speaker Voltage Gate (Port 0x61):\n", vga_color_entry(COLOR_LIGHT_BROWN, COLOR_BLACK));
        uint8_t p61 = inb(0x61);
        vga_puts("  * Status      : ");
        if (p61 & 0x03) vga_puts_color("[POWERED ON / GATE ACTIVE]\n", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
        else vga_puts_color("[POWERED OFF / GATE MUTED]\n", vga_color_entry(COLOR_LIGHT_GREY, COLOR_BLACK));
        vga_puts("  * Control     : chip speaker <on|off>\n\n");

        vga_puts_color("7. ZERO-SECURITY SILICON ACCESS PRIMITIVES (Unrestricted Ring 0):\n", vga_color_entry(COLOR_LIGHT_BROWN, COLOR_BLACK));
        vga_puts("  * Write RAM   : chip write <phys_addr> <dword_val> (Zero safety bounds)\n");
        vga_puts("  * Read RAM    : chip read <phys_addr>\n");
        vga_puts("  * Raw MSR     : chip wrmsr <msr_hex> <lo_hex> <hi_hex>\n");
        vga_puts("  * Hard Reset  : chip triplefault (Instant CPU Silicon Reset)\n");
        vga_puts("  * Memory Swap : chip swap <addr1> <addr2> <dwords>\n");
        vga_puts("  * PCI Power   : chip pci <bus> <slot> <func> <d0|d3>\n");
        vga_puts_color("===============================================================================\n", vga_color_entry(COLOR_LIGHT_CYAN, COLOR_BLACK));
        return;
    }

    if (strncmp(args, "wp ", 3) == 0) {
        const char* p = args + 3;
        while (*p == ' ') p++;
        if (strcmp(p, "off") == 0 || strcmp(p, "0") == 0) {
            asm_silicon_set_wp(0);
            vga_puts_color("[ZERO-SECURITY] Supervisor Write-Protection DISABLED (CR0.WP=0). Ring-0 can overwrite ANY address in physical RAM.\n", vga_color_entry(COLOR_LIGHT_RED, COLOR_BLACK));
        } else {
            asm_silicon_set_wp(1);
            vga_puts_color("[HARDWARE] Supervisor Write-Protection ENABLED (CR0.WP=1).\n", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
        }
    } else if (strncmp(args, "cache ", 6) == 0) {
        const char* p = args + 6;
        while (*p == ' ') p++;
        if (strcmp(p, "off") == 0 || strcmp(p, "0") == 0) {
            asm_chip_cache_set_state(0);
            vga_puts_color("[HARDWARE] CPU L1/L2/L3 SRAM Cache Arrays DISABLED (CR0.CD=1, CR0.NW=1, WBINVD executed).\n", vga_color_entry(COLOR_LIGHT_RED, COLOR_BLACK));
        } else {
            asm_chip_cache_set_state(1);
            vga_puts_color("[HARDWARE] CPU L1/L2/L3 SRAM Cache Arrays ENABLED (CR0.CD=0, CR0.NW=0, WBINVD executed).\n", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
        }
    } else if (strncmp(args, "pic ", 4) == 0) {
        const char* p = args + 4;
        while (*p == ' ') p++;
        if (strcmp(p, "off") == 0 || strcmp(p, "0") == 0) {
            asm_chip_pic_set_state(0);
            vga_puts_color("[HARDWARE] Intel 8259 Dual PIC Interrupt Controllers POWERED OFF (All 16 IRQs Masked).\n", vga_color_entry(COLOR_LIGHT_RED, COLOR_BLACK));
        } else {
            asm_chip_pic_set_state(1);
            vga_puts_color("[HARDWARE] Intel 8259 Dual PIC Interrupt Controllers POWERED ON (Standard IRQs Unmasked).\n", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
        }
    } else if (strncmp(args, "apic ", 5) == 0) {
        const char* p = args + 5;
        while (*p == ' ') p++;
        if (strcmp(p, "off") == 0 || strcmp(p, "0") == 0) {
            uint32_t ok = asm_chip_apic_set_state(0);
            if (ok) vga_puts_color("[HARDWARE] Local APIC Silicon Core DISABLED (MSR 0x1B Bit 11 cleared, SVR Bit 8 cleared).\n", vga_color_entry(COLOR_LIGHT_RED, COLOR_BLACK));
            else vga_puts_color("[ERROR] Local APIC not physically present on CPU.\n", vga_color_entry(COLOR_LIGHT_BROWN, COLOR_BLACK));
        } else {
            uint32_t ok = asm_chip_apic_set_state(1);
            if (ok) vga_puts_color("[HARDWARE] Local APIC Silicon Core ENABLED (MSR 0x1B Bit 11 set, SVR Bit 8 set).\n", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
            else vga_puts_color("[ERROR] Local APIC not physically present on CPU.\n", vga_color_entry(COLOR_LIGHT_BROWN, COLOR_BLACK));
        }
    } else if (strncmp(args, "kbd ", 4) == 0) {
        const char* p = args + 4;
        while (*p == ' ') p++;
        if (strcmp(p, "off") == 0 || strcmp(p, "0") == 0) {
            asm_chip_ps2_set_state(0, 0);
            vga_puts_color("[HARDWARE] Intel 8042 Keyboard Interface Clock Line DISABLED (0xAD).\n", vga_color_entry(COLOR_LIGHT_RED, COLOR_BLACK));
        } else {
            asm_chip_ps2_set_state(0, 1);
            vga_puts_color("[HARDWARE] Intel 8042 Keyboard Interface Clock Line ENABLED (0xAE).\n", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
        }
    } else if (strncmp(args, "mouse ", 6) == 0) {
        const char* p = args + 6;
        while (*p == ' ') p++;
        if (strcmp(p, "off") == 0 || strcmp(p, "0") == 0) {
            asm_chip_ps2_set_state(1, 0);
            vga_puts_color("[HARDWARE] Intel 8042 Mouse Auxiliary Interface DISABLED (0xA7).\n", vga_color_entry(COLOR_LIGHT_RED, COLOR_BLACK));
        } else {
            asm_chip_ps2_set_state(1, 1);
            vga_puts_color("[HARDWARE] Intel 8042 Mouse Auxiliary Interface ENABLED (0xA8).\n", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
        }
    } else if (strncmp(args, "speaker ", 8) == 0) {
        const char* p = args + 8;
        while (*p == ' ') p++;
        if (strcmp(p, "off") == 0 || strcmp(p, "0") == 0) {
            asm_chip_speaker_set_state(0);
            vga_puts_color("[HARDWARE] Motherboard Speaker Voltage Gate MUTED (Port 0x61 Bits 0-1 Cleared).\n", vga_color_entry(COLOR_LIGHT_GREY, COLOR_BLACK));
        } else {
            asm_chip_speaker_set_state(1);
            vga_puts_color("[HARDWARE] Motherboard Speaker Voltage Gate ENERGIZED (Port 0x61 Bits 0-1 Set).\n", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
        }
    } else if (strncmp(args, "pci ", 4) == 0) {
        const char* p = args + 4;
        while (*p == ' ') p++;
        uint8_t bus = (uint8_t)atoi(p);
        while (*p && *p != ' ') p++;
        while (*p == ' ') p++;
        uint8_t slot = (uint8_t)atoi(p);
        while (*p && *p != ' ') p++;
        while (*p == ' ') p++;
        uint8_t func = (uint8_t)atoi(p);
        while (*p && *p != ' ') p++;
        while (*p == ' ') p++;
        uint8_t state = 0;
        if (strcmp(p, "d3") == 0 || strcmp(p, "off") == 0 || strcmp(p, "3") == 0) state = 3;
        
        uint32_t ok = asm_chip_pci_set_power_state(bus, slot, func, state);
        if (ok) {
            vga_puts_color("[HARDWARE] PCI Device (Bus ", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
            vga_put_uint(bus); vga_puts(", Slot "); vga_put_uint(slot); vga_puts(", Func "); vga_put_uint(func);
            vga_puts(") programmed to Power State: ");
            vga_puts(state == 3 ? "D3hot [POWERED OFF / SLEEP]\n" : "D0 [POWERED ON / FULL ACTIVE]\n");
        } else {
            vga_puts_color("[STATUS] Device does not support PCI Power Management (PCI-PM Capability 0x01 not found).\n", vga_color_entry(COLOR_LIGHT_BROWN, COLOR_BLACK));
        }
    } else if (strncmp(args, "swap ", 5) == 0) {
        const char* p = args + 5;
        while (*p == ' ') p++;
        uint32_t addr1 = parse_hex(p);
        while (*p && *p != ' ') p++;
        while (*p == ' ') p++;
        uint32_t addr2 = parse_hex(p);
        while (*p && *p != ' ') p++;
        while (*p == ' ') p++;
        uint32_t count = (uint32_t)atoi(p);
        if (count == 0) count = 1;

        vga_puts_color("[SILICON ATOMIC SWAP] Swapping ", vga_color_entry(COLOR_LIGHT_CYAN, COLOR_BLACK));
        vga_put_uint(count); vga_puts(" dwords between "); vga_put_hex(addr1);
        vga_puts(" and "); vga_put_hex(addr2); vga_puts(" via pure x86 'xchg'...\n");

        asm_silicon_atomic_swap_mem(addr1, addr2, count);
        vga_puts_color("[SUCCESS] Physical RAM memory blocks swapped atomically with cache flush!\n", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
    } else if (strncmp(args, "write ", 6) == 0 || strncmp(args, "poke ", 5) == 0) {
        const char* p = args + (args[0] == 'w' ? 6 : 5);
        while (*p == ' ') p++;
        uint32_t addr = parse_hex(p);
        while (*p && *p != ' ') p++;
        while (*p == ' ') p++;
        uint32_t val = parse_hex(p);
        
        asm_silicon_set_wp(0); // Ensure WP disabled for write
        asm_silicon_write_phys_dword(addr, val);
        vga_puts_color("[ZERO-SECURITY WRITE] Written value ", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
        vga_put_hex(val); vga_puts(" to physical address "); vga_put_hex(addr); vga_puts(" without restriction!\n");
    } else if (strncmp(args, "read ", 5) == 0 || strncmp(args, "peek ", 5) == 0) {
        const char* p = args + 5;
        while (*p == ' ') p++;
        uint32_t addr = parse_hex(p);
        uint32_t val = asm_silicon_read_phys_dword(addr);
        vga_puts_color("[RAW SILICON READ] Address ", vga_color_entry(COLOR_LIGHT_CYAN, COLOR_BLACK));
        vga_put_hex(addr); vga_puts(" => Dword: "); vga_put_hex(val); vga_putc('\n');
    } else if (strncmp(args, "wrmsr ", 6) == 0) {
        const char* p = args + 6;
        while (*p == ' ') p++;
        uint32_t msr = parse_hex(p);
        while (*p && *p != ' ') p++;
        while (*p == ' ') p++;
        uint32_t lo = parse_hex(p);
        while (*p && *p != ' ') p++;
        while (*p == ' ') p++;
        uint32_t hi = parse_hex(p);
        
        asm_silicon_wrmsr_raw(msr, lo, hi);
        vga_puts_color("[RAW SILICON WRMSR] Written to MSR ", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
        vga_put_hex(msr); vga_puts(" -> Value: "); vga_put_hex(hi); vga_puts(" : "); vga_put_hex(lo); vga_putc('\n');
    } else if (strcmp(args, "triplefault") == 0 || strcmp(args, "hardreset") == 0 || strcmp(args, "tf") == 0) {
        vga_puts_color("[HARDWARE RESET] Triggering instantaneous CPU silicon triple-fault...\n", vga_color_entry(COLOR_LIGHT_RED, COLOR_BLACK));
        asm_silicon_triple_fault();
    } else if (strncmp(args, "irq ", 4) == 0) {
        const char* p = args + 4;
        while (*p == ' ') p++;
        if (strcmp(p, "off") == 0 || strcmp(p, "0") == 0) {
            asm_silicon_cli();
            vga_puts_color("[HARDWARE] Global CPU Interrupts DISABLED (CLI executed).\n", vga_color_entry(COLOR_LIGHT_RED, COLOR_BLACK));
        } else {
            asm_silicon_sti();
            vga_puts_color("[HARDWARE] Global CPU Interrupts ENABLED (STI executed).\n", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
        }
    } else if (strncmp(args, "pit ", 4) == 0) {
        const char* p = args + 4;
        while (*p == ' ') p++;
        uint32_t freq = (uint32_t)atoi(p);
        if (freq == 0) freq = 100;
        asm_silicon_pit_set_rate(freq);
        vga_puts_color("[HARDWARE] Intel 8254 PIT System Timer reprogrammed to: ", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
        vga_put_uint(freq); vga_puts(" Hz (Divisor: "); vga_put_uint(1193182 / freq); vga_puts(")\n");
    } else {
        vga_puts("Usage: chip <list|cache|wp|pic|apic|kbd|mouse|speaker|pci|swap|write|read|wrmsr|irq|triplefault|pit> <args>\n");
    }
}

void show_kernel2_help(void) {
    vga_clear_screen();
    vga_puts_color("===============================================================================\n", vga_color_entry(COLOR_LIGHT_CYAN, COLOR_BLACK));
    vga_puts_color("   A.A OS - KERNEL 2: ADVANCED DIRECT SILICON MASTERY DASHBOARD                \n", vga_color_entry(COLOR_LIGHT_GREEN, COLOR_BLACK));
    vga_puts_color("===============================================================================\n", vga_color_entry(COLOR_LIGHT_CYAN, COLOR_BLACK));
    vga_puts_color("1. DIRECT CHIP POWER & HARDWARE OVERRIDE (hardware_override.asm):\n", vga_color_entry(COLOR_LIGHT_BROWN, COLOR_BLACK));
    vga_puts("  * chip                                 : Show live Motherboard Chips ON/OFF status\n");
    vga_puts("  * chip wp <on|off>                     : Disable/Enable CR0.WP supervisor write protection\n");
    vga_puts("  * chip write <addr> <val>              : Direct write to ANY physical RAM address (Zero limits)\n");
    vga_puts("  * chip read <addr>                     : Direct raw read from physical address\n");
    vga_puts("  * chip wrmsr <msr> <lo> <hi>           : Direct raw silicon MSR write (WRMSR)\n");
    vga_puts("  * chip irq <on|off>                    : Disable (CLI) / Enable (STI) CPU interrupts\n");
    vga_puts("  * chip triplefault                     : Instant CPU hard reset via hardware triple-fault\n");
    vga_puts("  * chip cache <on|off>                  : Power ON/OFF CPU L1/L2/L3 SRAM cache arrays\n");
    vga_puts("  * chip pic <on|off>                    : Power ON/OFF Intel 8259 PIC Interrupt chips\n");
    vga_puts("  * chip apic <on|off>                   : Power ON/OFF on-die Local APIC core (MSR 0x1B)\n");
    vga_puts("  * chip kbd <on|off>                    : Enable/Disable Intel 8042 Keyboard chip\n");
    vga_puts("  * chip pci <b> <s> <f> <d0|d3>         : Power ON (D0) / Power OFF (D3 Sleep) PCI chip\n");
    vga_puts("  * chip swap <addr1> <addr2> <cnt>      : Pure x86 'xchg' physical RAM block swap\n");
    vga_puts("  * chip pit <freq_hz>                   : Overdrive 8254 timer chip to high frequency\n\n");

    vga_puts_color("2. CPU SILICON HARDWARE DEBUGGER (DR0 - DR7):\n", vga_color_entry(COLOR_LIGHT_BROWN, COLOR_BLACK));
    vga_puts("  * dbg.regs                             : Display DR0-DR7 hardware registers status\n");
    vga_puts("  * dbg.bp <0-3> <addr> <r|w|x> <1|2|4>  : Arm hardware breakpoint on physical RAM\n");
    vga_puts("  * dbg.clear <0-3>                      : Disable hardware breakpoint\n\n");

    vga_puts_color("3. PHYSICAL FIRMWARE & ACPI TABLE HUNTER:\n", vga_color_entry(COLOR_LIGHT_BROWN, COLOR_BLACK));
    vga_puts("  * acpi.tables                          : Scan physical EBDA/ROM for ACPI RSDT/XSDT\n\n");

    vga_puts_color("4. RAW PACKET CRAFTER & DMA INJECTOR:\n", vga_color_entry(COLOR_LIGHT_BROWN, COLOR_BLACK));
    vga_puts("  * packet.inject.deauth <mac> <bssid>   : Air-inject raw 802.11 Deauth frame via DMA\n\n");

    vga_puts_color("5. MSR PERFORMANCE & THERMAL STRESS:\n", vga_color_entry(COLOR_LIGHT_BROWN, COLOR_BLACK));
    vga_puts("  * msr.read <msr_hex>                   : Read 64-bit Model Specific Register\n");
    vga_puts("  * cpu.stress <level>                   : Measure live DTS MSR 0x19C under CPU load\n\n");

    vga_puts_color("6. DISK FORENSICS & RAW STORAGE MASTER (storage_editor.asm):\n", vga_color_entry(COLOR_LIGHT_BROWN, COLOR_BLACK));
    vga_puts("  * disk.hex <lba>                       : Live full 512-byte physical hex dump of LBA\n");
    vga_puts("  * disk.edit <lba> <off> <byte_hex>     : Edit single byte in physical sector via ATA PIO\n");
    vga_puts("  * disk.fill <lba> <cnt> <byte_hex>     : Fill sectors with custom pattern\n");
    vga_puts("  * disk.erase <lba> [cnt]               : Zero-out/erase physical sectors\n");
    vga_puts("  * disk.copy <src_lba> <dst_lba>        : Clone sector from source to destination LBA\n\n");

    vga_puts_color("7. PURE ASSEMBLY MEMORY CONTROL SUITE (storage_editor.asm & memorymanagement.asm):\n", vga_color_entry(COLOR_LIGHT_BROWN, COLOR_BLACK));
    vga_puts("  * mem.edit <addr> <byte_hex>           : Direct byte edit in physical RAM with clflush\n");
    vga_puts("  * mem.fill <addr> <cnt> <byte_hex>     : Fill physical RAM range using pure 'rep stosb'\n");
    vga_puts("  * mem.erase <addr> <cnt>               : Zero-out physical RAM memory range\n");
    vga_puts("  * mem.view <addr> [bytes]              : Live physical RAM hex + ASCII dump\n");
    vga_puts("  * mem.wash <addr> <bytes>              : Run 5-pass military DRAM wash + wbinvd\n");
    vga_puts("  * mem.search <start> <end> <byte>      : Fast hardware memory scan via repne scasb\n");
    vga_puts("  * mem.bswap <addr> <dwords>            : Direct x86 'bswap' endianness transform\n\n");

    vga_puts_color("8. PURE ASSEMBLY SILICON ENGINE (silicon_engine.asm):\n", vga_color_entry(COLOR_LIGHT_BROWN, COLOR_BLACK));
    vga_puts("  * asm.snapshot                         : Real-time atomic register snapshot (GPRs, Segments, CRs, EFLAGS)\n");
    vga_puts("  * asm.bench                            : Serialized RDTSC instruction cycle & silicon latency test\n");
    vga_puts("  * asm.tlb <addr>                       : Invalidate TLB translation page (invlpg)\n");
    vga_puts("  * asm.flush                            : Full pipeline drain and hardware cache flush (wbinvd; cpuid)\n\n");
    vga_puts_color("===============================================================================\n", vga_color_entry(COLOR_LIGHT_CYAN, COLOR_BLACK));
}

int dispatch_kernel2_command(const char* cmd) {
    if (strcmp(cmd, "help2") == 0 || strcmp(cmd, "k2.help") == 0 || strcmp(cmd, "k2") == 0) {
        show_kernel2_help();
        return 1;
    } else if (strncmp(cmd, "chip", 4) == 0 || strncmp(cmd, "chips", 5) == 0 || strncmp(cmd, "silicon", 7) == 0) {
        const char* p = cmd + (strncmp(cmd, "silicon", 7) == 0 ? 7 : (strncmp(cmd, "chips", 5) == 0 ? 5 : 4));
        cmd_chip_control(p);
        return 1;
    } else if (strcmp(cmd, "dbg.regs") == 0 || strcmp(cmd, "dr") == 0 || strcmp(cmd, "debug") == 0) {
        cmd_debug_registers();
        return 1;
    } else if (strncmp(cmd, "dbg.bp ", 7) == 0 || strncmp(cmd, "bp ", 3) == 0) {
        cmd_debug_set_breakpoint(cmd + (cmd[0] == 'd' ? 7 : 3));
        return 1;
    } else if (strncmp(cmd, "dbg.clear ", 10) == 0 || strncmp(cmd, "bc ", 3) == 0) {
        cmd_debug_clear_breakpoint(cmd + (cmd[0] == 'd' ? 10 : 3));
        return 1;
    } else if (strcmp(cmd, "acpi.tables") == 0 || strcmp(cmd, "acpi") == 0 || strcmp(cmd, "rsdt") == 0) {
        cmd_acpi_scan_tables();
        return 1;
    } else if (strncmp(cmd, "packet.inject.deauth", 20) == 0 || strncmp(cmd, "deauth", 6) == 0) {
        cmd_wifi_inject_deauth(cmd + (cmd[0] == 'p' ? 20 : 6));
        return 1;
    } else if (strncmp(cmd, "msr.read ", 9) == 0 || strncmp(cmd, "rdmsr ", 6) == 0) {
        cmd_msr_read(cmd + (cmd[0] == 'm' ? 9 : 6));
        return 1;
    } else if (strncmp(cmd, "cpu.stress", 10) == 0 || strcmp(cmd, "stress") == 0) {
        const char* p = cmd + (cmd[0] == 'c' ? 10 : 6);
        cmd_cpu_thermal_stress(p);
        return 1;
    } else if (strncmp(cmd, "disk.hex ", 9) == 0 || strncmp(cmd, "dhex ", 5) == 0) {
        cmd_disk_hex_dump(cmd + (cmd[0] == 'd' && cmd[1] == 'i' ? 9 : 5));
        return 1;
    } else if (strncmp(cmd, "disk.edit ", 10) == 0 || strncmp(cmd, "dedit ", 6) == 0) {
        cmd_disk_edit(cmd + (cmd[1] == 'i' ? 10 : 6));
        return 1;
    } else if (strncmp(cmd, "disk.fill ", 10) == 0 || strncmp(cmd, "dfill ", 6) == 0) {
        cmd_disk_fill(cmd + (cmd[1] == 'i' ? 10 : 6));
        return 1;
    } else if (strncmp(cmd, "disk.erase ", 11) == 0 || strncmp(cmd, "derase ", 7) == 0) {
        cmd_disk_erase(cmd + (cmd[1] == 'i' ? 11 : 7));
        return 1;
    } else if (strncmp(cmd, "disk.copy ", 10) == 0 || strncmp(cmd, "dcopy ", 6) == 0) {
        cmd_disk_copy(cmd + (cmd[1] == 'i' ? 10 : 6));
        return 1;
    } else if (strncmp(cmd, "mem.edit ", 9) == 0 || strncmp(cmd, "medit ", 6) == 0) {
        cmd_mem_edit(cmd + (cmd[1] == 'e' ? 9 : 6));
        return 1;
    } else if (strncmp(cmd, "mem.fill ", 9) == 0 || strncmp(cmd, "mfill ", 6) == 0) {
        cmd_mem_fill(cmd + (cmd[1] == 'e' ? 9 : 6));
        return 1;
    } else if (strncmp(cmd, "mem.erase ", 10) == 0 || strncmp(cmd, "merase ", 7) == 0) {
        cmd_mem_erase(cmd + (cmd[1] == 'e' ? 10 : 7));
        return 1;
    } else if (strncmp(cmd, "mem.view ", 9) == 0 || strncmp(cmd, "mview ", 6) == 0 || strncmp(cmd, "mdump ", 6) == 0) {
        cmd_mem_view(cmd + (cmd[1] == 'e' ? 9 : 6));
        return 1;
    } else if (strncmp(cmd, "mem.wash", 8) == 0 || strncmp(cmd, "dram.wash", 9) == 0) {
        cmd_mem_wash_5pass(cmd + (cmd[0] == 'm' ? 8 : 9));
        return 1;
    } else if (strncmp(cmd, "mem.search ", 11) == 0 || strncmp(cmd, "msearch ", 8) == 0) {
        cmd_mem_search(cmd + (cmd[1] == 'e' ? 11 : 8));
        return 1;
    } else if (strncmp(cmd, "mem.bswap ", 10) == 0 || strncmp(cmd, "bswap ", 6) == 0) {
        cmd_mem_endian_flip(cmd + (cmd[0] == 'm' ? 10 : 6));
        return 1;
    } else if (strcmp(cmd, "asm.snapshot") == 0 || strcmp(cmd, "snapshot") == 0 || strcmp(cmd, "snap") == 0) {
        cmd_asm_snapshot(cmd);
        return 1;
    } else if (strcmp(cmd, "asm.bench") == 0 || strcmp(cmd, "bench.cycles") == 0 || strcmp(cmd, "cycles") == 0) {
        cmd_asm_bench(cmd);
        return 1;
    } else if (strncmp(cmd, "asm.tlb", 7) == 0 || strncmp(cmd, "invlpg", 6) == 0) {
        cmd_asm_tlb(cmd + (cmd[0] == 'a' ? 7 : 6));
        return 1;
    } else if (strcmp(cmd, "asm.flush") == 0 || strcmp(cmd, "pipe.flush") == 0) {
        cmd_asm_pipeline_flush(cmd);
        return 1;
    }
    return 0;
}
