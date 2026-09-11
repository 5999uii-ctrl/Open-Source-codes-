/* =========================================================================
 * A.A OS - Bare-Metal Multitasking Subsystem Header
 * Task Control Blocks, Hardware Registers Trap Frame, Preemptive Scheduling
 * ========================================================================= */

#ifndef _AAS_TASK_H_
#define _AAS_TASK_H_

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

#define MAX_TASKS               32
#define TASK_STACK_SIZE         8192    /* 8 KB per task stack */
#define TASK_DEFAULT_QUANTUM    10      /* 10 timer ticks (10ms at 1000 Hz) */
#define TASK_NAME_MAX           32

/* Task Lifecycle States */
typedef enum {
    TASK_STATE_UNUSED = 0,
    TASK_STATE_READY,
    TASK_STATE_RUNNING,
    TASK_STATE_SLEEPING,
    TASK_STATE_DEAD
} task_state_t;

/* Exact 60-byte x86 Protected Mode Hardware & Software Register Frame */
struct irq_context {
    /* Pushed by assembly wrapper */
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp_ignored;   /* Pushed by pusha, ignored on popa */
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;

    /* Pushed by CPU hardware on interrupt / or by yield gate */
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
};

/* Task Control Block (TCB) */
typedef struct task {
    uint32_t        pid;
    char            name[TASK_NAME_MAX];
    task_state_t    state;
    uint32_t        esp;                    /* Saved kernel stack pointer */
    uint32_t        stack_base;             /* Bottom of stack in RAM */
    uint32_t        stack_size;             /* Total stack capacity */
    void            (*entry_point)(void*);  /* Task entry function */
    void*           arg;                    /* Argument passed to entry function */
    uint32_t        priority;               /* 1 (highest) to 10 (lowest) */
    uint32_t        time_slice_remaining;   /* Quantum ticks remaining in current slice */
    uint32_t        sleep_ticks_remaining;  /* Sleep countdown timer */
    uint32_t        total_ticks;            /* Total CPU ticks consumed */
    uint32_t        total_switches;         /* Number of times scheduled */
    uint32_t        work_counter;           /* General purpose task telemetry counter */
} task_t;

/* Assembly Context Switching Gates */
extern void asm_task_yield_gate(void);
extern void task_entry_wrapper(void);

/* Core Multitasking API */
int      scheduler_init(void);
int      task_create(const char* name, void (*entry_point)(void*), void* arg, uint32_t priority);
void     task_yield(void);
void     task_sleep(uint32_t ms);
void     task_exit(void);
int      task_kill(uint32_t pid);
task_t*  task_get_current(void);
task_t*  task_get_by_pid(uint32_t pid);
uint32_t task_get_count(void);
int      scheduler_is_active(void);

/* Preemptive Scheduler Hook & Yield Gate Handlers */
struct irq_context* c_irq0_timer_handler_preempt(struct irq_context* ctx);
struct irq_context* c_task_yield_handler(struct irq_context* ctx);

/* Built-in Background Tasks */
void bg_counter_task(void* arg);
void bg_pulse_task(void* arg);
void bg_stress_worker(void* arg);

/* Shell CLI Command Handlers */
void cmd_ps(void);
void cmd_task_spawn(const char* args);
void cmd_task_kill(const char* args);
void cmd_task_yield(void);
void cmd_task_info(const char* args);
void cmd_task_stress(const char* args);
void cmd_task_demo(void);

#endif /* _AAS_TASK_H_ */
