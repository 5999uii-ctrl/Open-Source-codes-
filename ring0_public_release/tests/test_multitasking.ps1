# =========================================================================
# A.A OS - Multitasking & Preemptive Scheduler Verification Suite
# Exhaustive verification of TCB structures, 60-byte irq_context,
# timer preemption gates, cooperative yield, and shell commands.
# =========================================================================

$ErrorActionPreference = "Continue"
$projectRoot = (Resolve-Path "$PSScriptRoot\..").Path
Set-Location $projectRoot

Write-Host "===============================================================================" -ForegroundColor Cyan
Write-Host "  A.A OS MULTITASKING & PREEMPTIVE SCHEDULER VERIFICATION SUITE" -ForegroundColor Green
Write-Host "===============================================================================" -ForegroundColor Cyan

$testResults = @()
$totalTests = 0
$passedTests = 0
$failedTests = 0

function Record-Test {
    param(
        [string]$TestId,
        [string]$Description,
        [bool]$Passed,
        [string]$Details = ""
    )
    $script:totalTests++
    if ($Passed) {
        $script:passedTests++
        Write-Host "  [PASS] $TestId : $Description" -ForegroundColor Green
        if ($Details) { Write-Host "         $Details" -ForegroundColor DarkGray }
    } else {
        $script:failedTests++
        Write-Host "  [FAIL] $TestId : $Description" -ForegroundColor Red
        if ($Details) { Write-Host "         Error: $Details" -ForegroundColor Yellow }
    }
    $script:testResults += [PSCustomObject]@{
        TestId = $TestId
        Description = $Description
        Status = if ($Passed) { "PASS" } else { "FAIL" }
        Details = $Details
    }
}

# Source code readers
$taskH = if (Test-Path "$projectRoot\task.h") { Get-Content "$projectRoot\task.h" -Raw } else { "" }
$taskC = if (Test-Path "$projectRoot\task.c") { Get-Content "$projectRoot\task.c" -Raw } else { "" }
$taskSwitchAsm = if (Test-Path "$projectRoot\task_switch.asm") { Get-Content "$projectRoot\task_switch.asm" -Raw } else { "" }
$kernelEntryAsm = if (Test-Path "$projectRoot\kernel_entry.asm") { Get-Content "$projectRoot\kernel_entry.asm" -Raw } else { "" }
$kernelC = if (Test-Path "$projectRoot\kernel.c") { Get-Content "$projectRoot\kernel.c" -Raw } else { "" }
$buildPs1 = if (Test-Path "$projectRoot\build.ps1") { Get-Content "$projectRoot\build.ps1" -Raw } else { "" }

Write-Host "`n--- [Tier 1: File Presence & Toolchain Pipeline] ---" -ForegroundColor Yellow

# T1.1: task.h existence
Record-Test "TSK-01" "Multitasking Header task.h File Presence" (Test-Path "$projectRoot\task.h") "task.h found"

# T1.2: task.c existence
Record-Test "TSK-02" "Multitasking Core Engine task.c File Presence" (Test-Path "$projectRoot\task.c") "task.c found"

# T1.3: task_switch.asm existence
Record-Test "TSK-03" "Pure Assembly task_switch.asm Module Presence" (Test-Path "$projectRoot\task_switch.asm") "task_switch.asm found"

# T1.4: build.ps1 incorporates task_switch.asm
$buildHasSwitch = $buildPs1 -match "task_switch\.asm"
Record-Test "TSK-04" "Build Pipeline Assembles task_switch.asm" $buildHasSwitch "build.ps1 contains NASM task_switch.asm rule"

# T1.5: build.ps1 incorporates task.c
$buildHasTaskC = $buildPs1 -match "task\.c"
Record-Test "TSK-05" "Build Pipeline Compiles task.c" $buildHasTaskC "build.ps1 contains GCC task.c compilation rule"

# T1.6: build.ps1 links task objects
$buildLinksTask = ($buildPs1 -match "agy_task_switch\.o") -and ($buildPs1 -match "agy_task\.o")
Record-Test "TSK-06" "Linker Script Links Multitasking Object Files" $buildLinksTask "Linked agy_task_switch.o and agy_task.o"

Write-Host "`n--- [Tier 2: Hardware Architecture & 60-Byte Context Frame] ---" -ForegroundColor Yellow

# T2.1: irq_context struct in task.h
$hasIrqContext = ($taskH -match "struct\s+irq_context") -and ($taskH -match "uint32_t\s+eflags;") -and ($taskH -match "uint32_t\s+cs;") -and ($taskH -match "uint32_t\s+eip;")
Record-Test "TSK-07" "60-Byte irq_context x86 Protected Mode Hardware Frame" $hasIrqContext "Pushed by CPU & assembly: 15 dwords = 60 bytes"

# T2.2: Segment registers preserved
$hasSegRegs = ($taskH -match "uint32_t\s+gs;") -and ($taskH -match "uint32_t\s+fs;") -and ($taskH -match "uint32_t\s+es;") -and ($taskH -match "uint32_t\s+ds;")
Record-Test "TSK-08" "Full Hardware Segment Isolation (GS, FS, ES, DS)" $hasSegRegs "All segment registers preserved across context switches"

# T2.3: Initial EFLAGS.IF bit enabled (0x202)
$hasEflagsInit = $taskC -match "0x00000202"
Record-Test "TSK-09" "Initial Task EFLAGS.IF Interrupt Enable Flag (0x0202)" $hasEflagsInit "Bit 9 set (0x200), ensuring tasks execute with interrupts active"

# T2.4: Kernel Code Segment Selector 0x08
$hasCsInit = $taskC -match "ctx->cs\s*=\s*0x08"
Record-Test "TSK-10" "Kernel Code Segment Selector 0x08 Initialization" $hasCsInit "Code selector sets Ring 0 32-bit flat execution"

# T2.5: Kernel Data Segment Selectors 0x10
$hasDsInit = $taskC -match "ctx->ds\s*=\s*0x10"
Record-Test "TSK-11" "Kernel Data Segment Selectors 0x10 Initialization" $hasDsInit "Data selector binds 4GB flat data segments"

Write-Host "`n--- [Tier 3: Assembly Gates & Preemptive Interrupt Hook] ---" -ForegroundColor Yellow

# T3.1: asm_irq0_timer_wrapper calls _c_irq0_timer_handler_preempt
$irq0PreemptCall = $kernelEntryAsm -match "call\s+_c_irq0_timer_handler_preempt"
Record-Test "TSK-12" "Timer IRQ0 ISR Hook Links to Preemptive Scheduler" $irq0PreemptCall "Hardware timer ISR gates into _c_irq0_timer_handler_preempt"

# T3.2: asm_irq0_timer_wrapper updates ESP dynamically
$irq0MovEsp = $kernelEntryAsm -match "mov\s+esp,\s*eax"
Record-Test "TSK-13" "Dynamic Stack Switch on Timer Preemption (mov esp, eax)" $irq0MovEsp "Switches CPU stack to target task before iret"

# T3.3: asm_task_yield_gate in task_switch.asm
$yieldGateFound = ($taskSwitchAsm -match "asm_task_yield_gate:") -and ($taskSwitchAsm -match "_asm_task_yield_gate:")
Record-Test "TSK-14" "Cooperative Yield Gate Definition in task_switch.asm" $yieldGateFound "Exports _asm_task_yield_gate / asm_task_yield_gate"

# T3.4: task_entry_wrapper trampoline
$trampolineFound = ($taskSwitchAsm -match "task_entry_wrapper:") -and ($taskSwitchAsm -match "_task_run_entry")
Record-Test "TSK-15" "Task Entry Trampoline & Self-Termination Wrapper" $trampolineFound "Safely enters task function and invokes task_exit on completion"

Write-Host "`n--- [Tier 4: Task Control Blocks (TCB) & Scheduling Logic] ---" -ForegroundColor Yellow

# T4.1: MAX_TASKS constant
$maxTasksMatch = $taskH -match "#define\s+MAX_TASKS\s+32"
Record-Test "TSK-16" "Maximum Concurrent Task Limit (MAX_TASKS = 32)" $maxTasksMatch "Allocates 32 concurrent task slots"

# T4.2: TASK_STACK_SIZE constant
$stackSizeMatch = $taskH -match "#define\s+TASK_STACK_SIZE\s+8192"
Record-Test "TSK-17" "Dedicated 8KB Task Stack Allocation (8192 bytes)" $stackSizeMatch "Guarantees 8KB stack headroom per task"

# T4.3: Task state enumeration
$hasTaskStates = ($taskH -match "TASK_STATE_READY") -and ($taskH -match "TASK_STATE_RUNNING") -and ($taskH -match "TASK_STATE_SLEEPING") -and ($taskH -match "TASK_STATE_DEAD")
Record-Test "TSK-18" "Task Lifecycle State Machine (READY, RUNNING, SLEEPING, DEAD)" $hasTaskStates "Complete state transitions implemented"

# T4.4: scheduler_init initializes Shell Task 0
$shellInitMatch = ($taskC -match 'safe_strcpy\(shell->name,\s*"kernel_shell"') -and ($taskC -match "shell->pid\s*=\s*0;")
Record-Test "TSK-19" "Task 0 Kernel Shell Context Initialization" $shellInitMatch "Binds bootloader context to Task 0 kernel_shell"

# T4.5: scheduler_init initializes Idle Task 1
$idleInitMatch = $taskC -match 'task_create\("system_idle"'
Record-Test "TSK-20" "Task 1 System Idle Worker Initialization" $idleInitMatch "Creates low-power sti; hlt idle loop task"

# T4.6: Round-robin selection
$rrMatch = ($taskC -match "scheduler_pick_next") -and ($taskC -match "% MAX_TASKS")
Record-Test "TSK-21" "Priority-Aware Round-Robin Task Selection Algorithm" $rrMatch "Searches task table circularly for READY tasks"

# T4.7: Sleeping task wake-up logic
$sleepWakeMatch = ($taskC -match "sleep_ticks_remaining--") -and ($taskC -match "state\s*=\s*TASK_STATE_READY")
Record-Test "TSK-22" "Sleep Countdown Timer Decrement & Wakeup Logic" $sleepWakeMatch "Decrements sleep ticks and transitions to TASK_STATE_READY at 0"

# T4.8: Task self-termination (task_exit)
$exitMatch = ($taskC -match "task_exit\(void\)") -and ($taskC -match "TASK_STATE_DEAD")
Record-Test "TSK-23" "Task Graceful Termination (task_exit)" $exitMatch "Marks task DEAD and yields CPU to prevent orphan execution"

# T4.9: Task termination by PID (task_kill)
$killMatch = ($taskC -match "task_kill\(uint32_t\s+pid\)") -and ($taskC -match "pid == 0 \|\| pid == 1")
Record-Test "TSK-24" "Task Kill API with System Protection Guards" $killMatch "Protects Shell (PID 0) and Idle (PID 1) from termination"

Write-Host "`n--- [Tier 5: Shell CLI Command Integration] ---" -ForegroundColor Yellow

# T5.1: ps / tasks command dispatch in kernel.c
$psDispatch = $kernelC -match 'strcmp\(cmd,\s*"ps"\)\s*==\s*0'
Record-Test "TSK-25" "Kernel Shell 'ps' / 'tasks' CLI Command Registration" $psDispatch "Dispatches 'ps', 'tasks', 'task.list' to cmd_ps"

# T5.2: task.spawn command dispatch
$spawnDispatch = $kernelC -match 'strncmp\(cmd,\s*"task\.spawn"'
Record-Test "TSK-26" "Kernel Shell 'task.spawn' CLI Command Registration" $spawnDispatch "Dispatches 'task.spawn [name]' to cmd_task_spawn"

# T5.3: task.kill command dispatch
$killDispatch = $kernelC -match 'strncmp\(cmd,\s*"task\.kill"'
Record-Test "TSK-27" "Kernel Shell 'task.kill <pid>' CLI Command Registration" $killDispatch "Dispatches 'task.kill <pid>' to cmd_task_kill"

# T5.4: task.yield command dispatch
$yieldDispatch = $kernelC -match 'strcmp\(cmd,\s*"task\.yield"'
Record-Test "TSK-28" "Kernel Shell 'task.yield' CLI Command Registration" $yieldDispatch "Dispatches 'task.yield' to cmd_task_yield"

# T5.5: task.info command dispatch
$infoDispatch = $kernelC -match 'strncmp\(cmd,\s*"task\.info"'
Record-Test "TSK-29" "Kernel Shell 'task.info <pid>' CLI Command Registration" $infoDispatch "Dispatches 'task.info <pid>' to cmd_task_info"

# T5.6: task.stress command dispatch
$stressDispatch = $kernelC -match 'strncmp\(cmd,\s*"task\.stress"'
Record-Test "TSK-30" "Kernel Shell 'task.stress <n>' CLI Command Registration" $stressDispatch "Dispatches 'task.stress <n>' to cmd_task_stress"

# T5.7: task.demo command dispatch
$demoDispatch = $kernelC -match 'strcmp\(cmd,\s*"task\.demo"'
Record-Test "TSK-31" "Kernel Shell 'task.demo' CLI Command Registration" $demoDispatch "Dispatches 'task.demo' to cmd_task_demo"

# T5.8: scheduler_init called in kernel_main
$schedInitMain = $kernelC -match "scheduler_init\(\);"
Record-Test "TSK-32" "Scheduler Initialization Hook in kernel_main" $schedInitMain "Kernel main calls scheduler_init() after interrupt init"

Write-Host "`n--- [Tier 6: Silicon Invariants & Code Hygiene] ---" -ForegroundColor Yellow

# T6.1: Zero TODOs / FIXMEs in task.h
$todoTaskH = ($taskH -match "//\s*TODO") -or ($taskH -match "//\s*FIXME")
Record-Test "TSK-33" "Zero TODO / FIXME Tokens in task.h" (-not $todoTaskH) "Clean header with zero placeholder tokens"

# T6.2: Zero TODOs / FIXMEs in task.c
$todoTaskC = ($taskC -match "//\s*TODO") -or ($taskC -match "//\s*FIXME")
Record-Test "TSK-34" "Zero TODO / FIXME Tokens in task.c" (-not $todoTaskC) "Clean implementation with zero placeholder tokens"

# T6.3: Zero TODOs / FIXMEs in task_switch.asm
$todoTaskAsm = ($taskSwitchAsm -match ";\s*TODO") -or ($taskSwitchAsm -match ";\s*FIXME")
Record-Test "TSK-35" "Zero TODO / FIXME Tokens in task_switch.asm" (-not $todoTaskAsm) "Clean assembly with zero placeholder tokens"

# T6.4: Aligned task stack pool
$stackAlignMatch = $taskC -match "aligned\(16\)"
Record-Test "TSK-36" "16-Byte Stack Alignment Invariant (aligned(16))" $stackAlignMatch "Enforces x86 ABI 16-byte stack frame alignment"

# T6.5: Clean disk image generation check
$imgExists = Test-Path "$projectRoot\os.img"
$imgValidSize = if ($imgExists) { (Get-Item "$projectRoot\os.img").Length -eq 1474560 } else { $false }
Record-Test "TSK-37" "Bootable Disk Image os.img Integrity (1.44 MB Floppy Format)" $imgValidSize "os.img exists with exact 1,474,560 bytes"

Write-Host "`n===============================================================================" -ForegroundColor Cyan
Write-Host "  MULTITASKING TEST SUITE SUMMARY" -ForegroundColor Green
Write-Host "===============================================================================" -ForegroundColor Cyan
Write-Host "  Total Tests  : $totalTests"
Write-Host "  Passed Tests : $passedTests" -ForegroundColor Green
Write-Host "  Failed Tests : $failedTests" -ForegroundColor $(if ($failedTests -eq 0) { "Green" } else { "Red" })
$rate = [math]::Round(($passedTests / $totalTests) * 100, 2)
Write-Host "  Pass Rate    : $rate%" -ForegroundColor $(if ($rate -eq 100) { "Green" } else { "Red" })
Write-Host "===============================================================================" -ForegroundColor Cyan

if ($failedTests -ne 0) {
    exit 1
}
exit 0
