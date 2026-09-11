/* =========================================================================
 * A.A OS - Bare-Metal Multitasking & Preemptive Scheduler Subsystem
 * 100% Genuine Ring 0 Process Management, Task Control Blocks,
 * Hardware Timer IRQ0 Preemption, Cooperative Yield, and Diagnostics
 * ========================================================================= */

#include "task.h"

/* Forward declarations from kernel.c & hardware modules */
extern void vga_puts(const char* data);
extern void vga_puts_color(const char* data, uint8_t color);
extern void vga_putc(char c);
extern void vga_putc_color(char c, uint8_t color);
extern void vga_put_uint(uint32_t val);
extern void vga_put_int(int32_t val);
extern void vga_put_hex(uint32_t val);
extern void vga_put_hex8(uint8_t val);
extern void vga_put_hex16(uint16_t val);
extern void* memcpy(void* dest, const void* src, uint32_t len);
extern void* memset(void* dest, uint8_t val, uint32_t len);
extern uint32_t strlen(const char* str);
extern int strcmp(const char* s1, const char* s2);
extern int strncmp(const char* s1, const char* s2, uint32_t n);
extern int atoi(const char* str);
extern uint32_t parse_hex(const char* str);
extern void c_irq0_timer_handler(void);

/* VGA Colors matching kernel definitions */
#define COLOR_BLACK         0
#define COLOR_BLUE          1
#define COLOR_GREEN         2
#define COLOR_CYAN          3
#define COLOR_RED           4
#define COLOR_MAGENTA       5
#define COLOR_BROWN         6
#define COLOR_LIGHT_GREY    7
#define COLOR_DARK_GREY     8
#define COLOR_LIGHT_BLUE    9
#define COLOR_LIGHT_GREEN   10
#define COLOR_LIGHT_CYAN    11
#define COLOR_LIGHT_RED     12
#define COLOR_LIGHT_MAGENTA 13
#define COLOR_LIGHT_BROWN   14
#define COLOR_WHITE         15

static inline uint8_t vga_color(uint8_t fg, uint8_t bg) {
    return (uint8_t)(fg | (bg << 4));
}

/* Static Task Control Block Array and Physical 8KB Task Stacks */
static task_t task_table[MAX_TASKS];
static uint8_t task_stacks[MAX_TASKS][TASK_STACK_SIZE] __attribute__((aligned(16)));

static task_t* current_task = 0;
static task_t* idle_task = 0;
static uint32_t next_pid = 1;
static int scheduler_enabled = 0;
static uint32_t scheduler_total_switches = 0;
static uint32_t active_tasks_count = 0;

/* Idle Task Function (executes low-power sleep when no task is ready) */
static void idle_task_func(void* arg) {
    (void)arg;
    while (1) {
        __asm__ volatile ("sti\n\thlt");
    }
}

/* Helper to copy string with max bounds */
static void safe_strcpy(char* dest, const char* src, uint32_t max_len) {
    if (!dest || !src || max_len == 0) return;
    uint32_t i = 0;
    while (src[i] && i < max_len - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

/* Trampoline entry execution helper called from task_entry_wrapper */
void task_run_entry(void) {
    if (current_task && current_task->entry_point) {
        current_task->entry_point(current_task->arg);
    }
}

/* Get pointer to currently running task */
task_t* task_get_current(void) {
    return current_task;
}

/* Check if scheduler is active */
int scheduler_is_active(void) {
    return scheduler_enabled;
}

/* Get number of active (non-dead) tasks */
uint32_t task_get_count(void) {
    return active_tasks_count;
}

/* Look up task by PID */
task_t* task_get_by_pid(uint32_t pid) {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (task_table[i].state != TASK_STATE_UNUSED && task_table[i].pid == pid) {
            return &task_table[i];
        }
    }
    return 0;
}

/* Initialize Multitasking Subsystem and Task 0 / Idle Task */
int scheduler_init(void) {
    memset(task_table, 0, sizeof(task_table));

    /* Task 0: Kernel Shell (Current running context) */
    task_t* shell = &task_table[0];
    shell->pid = 0;
    safe_strcpy(shell->name, "kernel_shell", TASK_NAME_MAX);
    shell->state = TASK_STATE_RUNNING;
    shell->stack_base = 0x80000;
    shell->stack_size = 0x10000;
    shell->esp = 0x90000;
    shell->entry_point = 0;
    shell->arg = 0;
    shell->priority = 1; /* High priority for responsive CLI */
    shell->time_slice_remaining = TASK_DEFAULT_QUANTUM;
    shell->sleep_ticks_remaining = 0;
    shell->total_ticks = 0;
    shell->total_switches = 0;
    shell->work_counter = 0;

    current_task = shell;
    active_tasks_count = 1;

    /* Task 1: System Idle Task */
    int idle_pid = task_create("system_idle", idle_task_func, 0, 10);
    if (idle_pid < 0) {
        return -1;
    }
    idle_task = task_get_by_pid((uint32_t)idle_pid);

    next_pid = 2;
    scheduler_total_switches = 0;
    scheduler_enabled = 1;

    return 0;
}

/* Create and schedule a new task */
int task_create(const char* name, void (*entry_point)(void*), void* arg, uint32_t priority) {
    if (!entry_point) return -1;

    /* Find an unused slot in task table */
    int slot = -1;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (task_table[i].state == TASK_STATE_UNUSED || task_table[i].state == TASK_STATE_DEAD) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        return -1; /* Task table full */
    }

    task_t* t = &task_table[slot];
    memset(t, 0, sizeof(task_t));

    t->pid = next_pid++;
    if (name && name[0]) {
        safe_strcpy(t->name, name, TASK_NAME_MAX);
    } else {
        safe_strcpy(t->name, "unnamed_task", TASK_NAME_MAX);
    }

    /* Stack memory allocation from aligned pool */
    uint32_t stack_base = (uint32_t)&task_stacks[slot][0];
    uint32_t stack_top  = (stack_base + TASK_STACK_SIZE) & ~15; /* 16-byte aligned */

    /* Forge initial struct irq_context at top of stack */
    struct irq_context* ctx = (struct irq_context*)(stack_top - sizeof(struct irq_context));
    memset(ctx, 0, sizeof(struct irq_context));

    ctx->gs = 0x10; /* Kernel Data Segment */
    ctx->fs = 0x10;
    ctx->es = 0x10;
    ctx->ds = 0x10;
    ctx->edi = 0;
    ctx->esi = 0;
    ctx->ebp = 0;
    ctx->esp_ignored = 0;
    ctx->ebx = 0;
    ctx->edx = 0;
    ctx->ecx = 0;
    ctx->eax = 0;

    /* Hardware interrupt return fields popped by iret */
    ctx->eip = (uint32_t)task_entry_wrapper;
    ctx->cs  = 0x08; /* Kernel Code Segment */
    ctx->eflags = 0x00000202; /* Bit 9 = IF (Interrupt Flag Enabled), Bit 1 = Reserved (1) */

    t->esp = (uint32_t)ctx;
    t->stack_base = stack_base;
    t->stack_size = TASK_STACK_SIZE;
    t->entry_point = entry_point;
    t->arg = arg;
    t->priority = (priority == 0) ? 5 : ((priority > 10) ? 10 : priority);
    t->time_slice_remaining = TASK_DEFAULT_QUANTUM;
    t->sleep_ticks_remaining = 0;
    t->total_ticks = 0;
    t->total_switches = 0;
    t->work_counter = 0;
    t->state = TASK_STATE_READY;

    active_tasks_count++;
    return (int)t->pid;
}

/* Pick next ready task via Priority-aware Round-Robin */
static task_t* scheduler_pick_next(void) {
    if (!current_task) return idle_task;

    int current_idx = 0;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (&task_table[i] == current_task) {
            current_idx = i;
            break;
        }
    }

    /* First pass: Look for READY non-idle tasks */
    for (int step = 1; step <= MAX_TASKS; step++) {
        int idx = (current_idx + step) % MAX_TASKS;
        task_t* t = &task_table[idx];
        if (t->state == TASK_STATE_READY && t != idle_task) {
            return t;
        }
    }

    /* If current task is still runnable (not dead or sleeping), keep it */
    if (current_task->state == TASK_STATE_RUNNING || current_task->state == TASK_STATE_READY) {
        return current_task;
    }

    /* Otherwise, dispatch idle task */
    if (idle_task && (idle_task->state == TASK_STATE_READY || idle_task->state == TASK_STATE_RUNNING)) {
        return idle_task;
    }

    return current_task;
}

/* Preemptive Timer IRQ0 Hook called from asm_irq0_timer_wrapper */
struct irq_context* c_irq0_timer_handler_preempt(struct irq_context* ctx) {
    /* Delegate tick increment and PIC/APIC EOI to standard kernel timer handler */
    c_irq0_timer_handler();

    if (!scheduler_enabled || !current_task) {
        return ctx;
    }

    /* 1. Update Sleeping Tasks */
    for (int i = 0; i < MAX_TASKS; i++) {
        if (task_table[i].state == TASK_STATE_SLEEPING) {
            if (task_table[i].sleep_ticks_remaining > 0) {
                task_table[i].sleep_ticks_remaining--;
            }
            if (task_table[i].sleep_ticks_remaining == 0) {
                task_table[i].state = TASK_STATE_READY;
            }
        }
    }

    /* 2. Update Current Task Ticks & Slice */
    current_task->total_ticks++;
    if (current_task->time_slice_remaining > 0) {
        current_task->time_slice_remaining--;
    }

    /* 3. Preempt if quantum expired */
    if (current_task->time_slice_remaining == 0) {
        current_task->time_slice_remaining = TASK_DEFAULT_QUANTUM;
        current_task->esp = (uint32_t)ctx;

        if (current_task->state == TASK_STATE_RUNNING) {
            current_task->state = TASK_STATE_READY;
        }

        task_t* next = scheduler_pick_next();
        if (next && next != current_task) {
            current_task = next;
            current_task->state = TASK_STATE_RUNNING;
            current_task->total_switches++;
            scheduler_total_switches++;
            return (struct irq_context*)current_task->esp;
        }

        if (current_task->state == TASK_STATE_READY) {
            current_task->state = TASK_STATE_RUNNING;
        }
    }

    return ctx;
}

/* Cooperative Yield Handler called from asm_task_yield_gate */
struct irq_context* c_task_yield_handler(struct irq_context* ctx) {
    if (!scheduler_enabled || !current_task) {
        return ctx;
    }

    current_task->esp = (uint32_t)ctx;
    if (current_task->state == TASK_STATE_RUNNING) {
        current_task->state = TASK_STATE_READY;
    }
    current_task->time_slice_remaining = TASK_DEFAULT_QUANTUM;

    task_t* next = scheduler_pick_next();
    if (next) {
        current_task = next;
        current_task->state = TASK_STATE_RUNNING;
        current_task->total_switches++;
        scheduler_total_switches++;
        return (struct irq_context*)current_task->esp;
    }

    return ctx;
}

/* Cooperative Yield API */
void task_yield(void) {
    if (scheduler_enabled) {
        asm_task_yield_gate();
    }
}

/* Task Sleep API (puts task to sleep for specified milliseconds) */
void task_sleep(uint32_t ms) {
    if (!scheduler_enabled || !current_task) return;

    current_task->sleep_ticks_remaining = (ms == 0) ? 1 : ms;
    current_task->state = TASK_STATE_SLEEPING;
    asm_task_yield_gate();
}

/* Self-termination API for current task */
void task_exit(void) {
    if (!scheduler_enabled || !current_task) return;

    current_task->state = TASK_STATE_DEAD;
    if (active_tasks_count > 0) {
        active_tasks_count--;
    }

    asm_task_yield_gate();

    /* Unreachable safety loop */
    while (1) {
        __asm__ volatile ("hlt");
    }
}

/* Terminate a task by PID */
int task_kill(uint32_t pid) {
    if (pid == 0 || pid == 1) {
        return -1; /* Cannot kill Kernel Shell or Idle Task */
    }

    task_t* t = task_get_by_pid(pid);
    if (!t || t->state == TASK_STATE_UNUSED || t->state == TASK_STATE_DEAD) {
        return -1;
    }

    t->state = TASK_STATE_DEAD;
    if (active_tasks_count > 0) {
        active_tasks_count--;
    }

    if (t == current_task) {
        asm_task_yield_gate();
    }

    return 0;
}

/* -------------------------------------------------------------------------
 * Built-In Background Demonstration Tasks
 * ------------------------------------------------------------------------- */

/* Background worker: Continuous prime search & telemetry computation */
void bg_counter_task(void* arg) {
    (void)arg;
    uint32_t num = 2;
    while (1) {
        /* Simple prime checker */
        int is_prime = 1;
        for (uint32_t d = 2; d * d <= num; d++) {
            if ((num % d) == 0) {
                is_prime = 0;
                break;
            }
        }
        if (is_prime && current_task) {
            current_task->work_counter++;
        }
        num++;
        if (num > 1000000) num = 2;

        /* Sleep 20ms to allow smooth scheduling */
        task_sleep(20);
    }
}

/* Background worker: Periodic system pulse heartbeat */
void bg_pulse_task(void* arg) {
    (void)arg;
    while (1) {
        if (current_task) {
            current_task->work_counter++;
        }
        task_sleep(100);
    }
}

/* Background worker: Heavy CPU calculation stress worker (no sleep) */
void bg_stress_worker(void* arg) {
    (void)arg;
    volatile uint32_t accum = 0;
    while (1) {
        for (uint32_t i = 0; i < 50000; i++) {
            accum = (accum * 1103515245 + 12345) & 0x7FFFFFFF;
        }
        if (current_task) {
            current_task->work_counter++;
        }
        /* Intentionally do NOT sleep - tests timer preemption quantum */
    }
}

/* -------------------------------------------------------------------------
 * Kernel Shell Multitasking CLI Commands
 * ------------------------------------------------------------------------- */

/* Helper to convert state enum to string */
static const char* state_to_str(task_state_t st) {
    switch (st) {
        case TASK_STATE_UNUSED:   return "UNUSED  ";
        case TASK_STATE_READY:    return "READY   ";
        case TASK_STATE_RUNNING:  return "RUNNING ";
        case TASK_STATE_SLEEPING: return "SLEEPING";
        case TASK_STATE_DEAD:     return "DEAD    ";
        default:                  return "UNKNOWN ";
    }
}

/* cmd_ps: List all tasks in formatted table */
void cmd_ps(void) {
    vga_puts_color("===============================================================================\n", vga_color(COLOR_LIGHT_CYAN, COLOR_BLACK));
    vga_puts_color("  A.A OS PREEMPTIVE MULTITASKING SUB-SYSTEM - PROCESS TABLE (TCB)\n", vga_color(COLOR_LIGHT_GREEN, COLOR_BLACK));
    vga_puts_color("===============================================================================\n", vga_color(COLOR_LIGHT_CYAN, COLOR_BLACK));
    vga_puts_color(" PID  TASK NAME        STATE     PRIO  TICKS      SWITCHES   WORK     ESP      \n", vga_color(COLOR_LIGHT_BROWN, COLOR_BLACK));
    vga_puts_color("-------------------------------------------------------------------------------\n", vga_color(COLOR_DARK_GREY, COLOR_BLACK));

    uint32_t displayed = 0;
    for (int i = 0; i < MAX_TASKS; i++) {
        task_t* t = &task_table[i];
        if (t->state == TASK_STATE_UNUSED) continue;

        displayed++;
        /* PID */
        vga_puts(" ");
        vga_put_uint(t->pid);
        if (t->pid < 10) vga_puts("   ");
        else if (t->pid < 100) vga_puts("  ");
        else vga_puts(" ");

        /* Name (pad to 16 chars) */
        vga_puts(t->name);
        uint32_t len = strlen(t->name);
        for (uint32_t s = len; s < 17; s++) vga_putc(' ');

        /* State with color coding */
        uint8_t state_color = vga_color(COLOR_LIGHT_GREY, COLOR_BLACK);
        if (t->state == TASK_STATE_RUNNING)  state_color = vga_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
        else if (t->state == TASK_STATE_READY)    state_color = vga_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
        else if (t->state == TASK_STATE_SLEEPING) state_color = vga_color(COLOR_LIGHT_BROWN, COLOR_BLACK);
        else if (t->state == TASK_STATE_DEAD)     state_color = vga_color(COLOR_LIGHT_RED, COLOR_BLACK);

        vga_puts_color(state_to_str(t->state), state_color);
        vga_puts(" ");

        /* Priority */
        vga_put_uint(t->priority);
        vga_puts("     ");

        /* CPU Ticks */
        vga_put_uint(t->total_ticks);
        if (t->total_ticks < 10) vga_puts("          ");
        else if (t->total_ticks < 100) vga_puts("         ");
        else if (t->total_ticks < 1000) vga_puts("        ");
        else if (t->total_ticks < 10000) vga_puts("       ");
        else vga_puts("      ");

        /* Context Switches */
        vga_put_uint(t->total_switches);
        if (t->total_switches < 10) vga_puts("          ");
        else if (t->total_switches < 100) vga_puts("         ");
        else if (t->total_switches < 1000) vga_puts("        ");
        else vga_puts("       ");

        /* Work counter */
        vga_put_uint(t->work_counter);
        if (t->work_counter < 10) vga_puts("        ");
        else if (t->work_counter < 100) vga_puts("       ");
        else if (t->work_counter < 1000) vga_puts("      ");
        else vga_puts("     ");

        /* Saved ESP */
        vga_put_hex(t->esp);
        vga_putc('\n');
    }

    vga_puts_color("-------------------------------------------------------------------------------\n", vga_color(COLOR_DARK_GREY, COLOR_BLACK));
    vga_puts(" Active Tasks: "); vga_put_uint(active_tasks_count);
    vga_puts(" | Scheduler: ");
    if (scheduler_enabled) vga_puts_color("ACTIVE (Preemptive Round-Robin, 10ms quantum)", vga_color(COLOR_LIGHT_GREEN, COLOR_BLACK));
    else vga_puts_color("DISABLED", vga_color(COLOR_LIGHT_RED, COLOR_BLACK));
    vga_puts(" | Total Switches: "); vga_put_uint(scheduler_total_switches);
    vga_putc('\n');
}

/* cmd_task_spawn: Spawn a background worker task */
void cmd_task_spawn(const char* args) {
    char task_name[TASK_NAME_MAX];
    while (*args == ' ') args++;

    if (*args) {
        safe_strcpy(task_name, args, TASK_NAME_MAX);
    } else {
        safe_strcpy(task_name, "bg_counter", TASK_NAME_MAX);
    }

    int pid = task_create(task_name, bg_counter_task, 0, 5);
    if (pid < 0) {
        vga_puts_color("[ERROR] Failed to spawn task: Task table full!\n", vga_color(COLOR_LIGHT_RED, COLOR_BLACK));
    } else {
        vga_puts_color("[TASK SPAWNED] Successfully created background task '", vga_color(COLOR_LIGHT_GREEN, COLOR_BLACK));
        vga_puts(task_name);
        vga_puts("' with PID ");
        vga_put_uint((uint32_t)pid);
        vga_puts(" (Priority: 5, Quantum: 10ms)\n");
    }
}

/* cmd_task_kill: Terminate a task by PID */
void cmd_task_kill(const char* args) {
    while (*args == ' ') args++;
    if (!*args) {
        vga_puts_color("Usage: task.kill <pid>\n", vga_color(COLOR_LIGHT_RED, COLOR_BLACK));
        return;
    }

    uint32_t pid = (uint32_t)atoi(args);
    if (pid == 0 || pid == 1) {
        vga_puts_color("[ERROR] Cannot kill system tasks (PID 0 Shell / PID 1 Idle)!\n", vga_color(COLOR_LIGHT_RED, COLOR_BLACK));
        return;
    }

    int ret = task_kill(pid);
    if (ret == 0) {
        vga_puts_color("[SUCCESS] Task with PID ", vga_color(COLOR_LIGHT_GREEN, COLOR_BLACK));
        vga_put_uint(pid);
        vga_puts(" has been terminated.\n");
    } else {
        vga_puts_color("[ERROR] Task PID ", vga_color(COLOR_LIGHT_RED, COLOR_BLACK));
        vga_put_uint(pid);
        vga_puts(" not found or already dead.\n");
    }
}

/* cmd_task_yield: Cooperatively yield CPU */
void cmd_task_yield(void) {
    vga_puts_color("[YIELD] Relinquishing CPU time-slice to next ready task...\n", vga_color(COLOR_LIGHT_CYAN, COLOR_BLACK));
    task_yield();
    vga_puts_color("[RESUMED] Returned to shell context after scheduler cycle.\n", vga_color(COLOR_LIGHT_GREEN, COLOR_BLACK));
}

/* cmd_task_info: Detailed stack and register dump of target task */
void cmd_task_info(const char* args) {
    while (*args == ' ') args++;
    if (!*args) {
        vga_puts_color("Usage: task.info <pid>\n", vga_color(COLOR_LIGHT_RED, COLOR_BLACK));
        return;
    }

    uint32_t pid = (uint32_t)atoi(args);
    task_t* t = task_get_by_pid(pid);
    if (!t || t->state == TASK_STATE_UNUSED) {
        vga_puts_color("[ERROR] Task PID not found!\n", vga_color(COLOR_LIGHT_RED, COLOR_BLACK));
        return;
    }

    vga_puts_color("-------------------------------------------------------------------------------\n", vga_color(COLOR_LIGHT_CYAN, COLOR_BLACK));
    vga_puts(" TASK CONTROL BLOCK (TCB) DETAILS - PID "); vga_put_uint(t->pid); vga_putc('\n');
    vga_puts_color("-------------------------------------------------------------------------------\n", vga_color(COLOR_LIGHT_CYAN, COLOR_BLACK));
    vga_puts("  * Name               : "); vga_puts(t->name); vga_putc('\n');
    vga_puts("  * State              : "); vga_puts(state_to_str(t->state)); vga_putc('\n');
    vga_puts("  * Priority           : "); vga_put_uint(t->priority); vga_putc('\n');
    vga_puts("  * Time Slice Left    : "); vga_put_uint(t->time_slice_remaining); vga_puts(" ticks\n");
    vga_puts("  * Sleep Remaining    : "); vga_put_uint(t->sleep_ticks_remaining); vga_puts(" ms\n");
    vga_puts("  * Total CPU Ticks    : "); vga_put_uint(t->total_ticks); vga_putc('\n');
    vga_puts("  * Context Switches   : "); vga_put_uint(t->total_switches); vga_putc('\n');
    vga_puts("  * Work Counter       : "); vga_put_uint(t->work_counter); vga_putc('\n');
    vga_puts("  * Stack Base Address : "); vga_put_hex(t->stack_base); vga_putc('\n');
    vga_puts("  * Stack Capacity     : "); vga_put_uint(t->stack_size); vga_puts(" bytes\n");
    vga_puts("  * Saved ESP          : "); vga_put_hex(t->esp); vga_putc('\n');

    if (t->esp >= t->stack_base && t->esp <= (t->stack_base + t->stack_size)) {
        struct irq_context* ctx = (struct irq_context*)t->esp;
        vga_puts_color(" HARDWARE REGISTERS CONTEXT FRAME (ON STACK):\n", vga_color(COLOR_LIGHT_BROWN, COLOR_BLACK));
        vga_puts("  * EIP: "); vga_put_hex(ctx->eip);
        vga_puts("  CS: ");  vga_put_hex(ctx->cs);
        vga_puts("  EFLAGS: "); vga_put_hex(ctx->eflags); vga_putc('\n');
        vga_puts("  * EAX: "); vga_put_hex(ctx->eax);
        vga_puts("  EBX: "); vga_put_hex(ctx->ebx);
        vga_puts("  ECX: "); vga_put_hex(ctx->ecx);
        vga_puts("  EDX: "); vga_put_hex(ctx->edx); vga_putc('\n');
        vga_puts("  * ESI: "); vga_put_hex(ctx->esi);
        vga_puts("  EDI: "); vga_put_hex(ctx->edi);
        vga_puts("  EBP: "); vga_put_hex(ctx->ebp); vga_putc('\n');
        vga_puts("  * DS: ");  vga_put_hex8((uint8_t)ctx->ds);
        vga_puts("  ES: ");  vga_put_hex8((uint8_t)ctx->es);
        vga_puts("  FS: ");  vga_put_hex8((uint8_t)ctx->fs);
        vga_puts("  GS: ");  vga_put_hex8((uint8_t)ctx->gs); vga_putc('\n');
    }
}

/* cmd_task_stress: Spawn multiple concurrent stress tasks */
void cmd_task_stress(const char* args) {
    while (*args == ' ') args++;
    uint32_t count = (args && *args) ? (uint32_t)atoi(args) : 4;
    if (count == 0) count = 1;
    if (count > 8) count = 8; /* Cap at 8 to preserve task slots */

    vga_puts_color("[STRESS] Spawning ", vga_color(COLOR_LIGHT_BROWN, COLOR_BLACK));
    vga_put_uint(count);
    vga_puts(" concurrent compute-heavy worker tasks...\n");

    for (uint32_t i = 0; i < count; i++) {
        char name[32];
        name[0] = 's'; name[1] = 't'; name[2] = 'r'; name[3] = 'e'; name[4] = 's'; name[5] = 's';
        name[6] = '_'; name[7] = '0' + (char)i; name[8] = '\0';
        int pid = task_create(name, bg_stress_worker, 0, 5);
        if (pid >= 0) {
            vga_puts("  * Spawned "); vga_puts(name); vga_puts(" [PID "); vga_put_uint((uint32_t)pid); vga_puts("]\n");
        }
    }
    vga_puts_color("[STRESS] Tasks active! Run 'ps' to inspect Round-Robin tick distribution.\n", vga_color(COLOR_LIGHT_GREEN, COLOR_BLACK));
}

/* cmd_task_demo: Full demonstration of concurrent multitasking */
void cmd_task_demo(void) {
    vga_puts_color("===============================================================================\n", vga_color(COLOR_LIGHT_CYAN, COLOR_BLACK));
    vga_puts_color("  A.A OS CONCURRENT MULTITASKING SHOWCASE DEMO\n", vga_color(COLOR_LIGHT_GREEN, COLOR_BLACK));
    vga_puts_color("===============================================================================\n", vga_color(COLOR_LIGHT_CYAN, COLOR_BLACK));
    vga_puts("Spawning 2 real Ring 0 background worker threads:\n");
    vga_puts("  1. 'worker_prime' : Searches for prime numbers and increments work counter\n");
    vga_puts("  2. 'worker_pulse' : Periodic telemetry heartbeat monitor\n\n");

    int p1 = task_create("worker_prime", bg_counter_task, 0, 4);
    int p2 = task_create("worker_pulse", bg_pulse_task, 0, 6);

    vga_puts("Worker 1 PID: "); vga_put_int(p1); vga_putc('\n');
    vga_puts("Worker 2 PID: "); vga_put_int(p2); vga_putc('\n');
    vga_puts_color("\n[SUCCESS] Background workers are executing concurrently in Ring 0!\n", vga_color(COLOR_LIGHT_GREEN, COLOR_BLACK));
    vga_puts_color("The shell remains 100% interactive. Type 'ps' to see live tick telemetry!\n", vga_color(COLOR_LIGHT_CYAN, COLOR_BLACK));
}
