#include <driver/ps2.h>
#include <driver/vga.h>
#include <zjunix/bootmm.h>
#include <zjunix/buddy.h>
#include <zjunix/fs/fat.h>
#include <zjunix/log.h>
#include <zjunix/pc.h>
#include <zjunix/slab.h>
#include <zjunix/syscall.h>
#include <zjunix/time.h>
#include "../usr/ps.h"
#include <intr.h>
#include <exc.h>
#include <page.h>
#include <arch.h>
#include <zjunix/vfs/vfs.h>

void machine_info() {
    int row;
    int col;
    kernel_printf("\n%s\n", "ZJUNIX V?.?");
    row = cursor_row;
    col = cursor_col;
    cursor_row = 29;
    kernel_printf("%s", "Created by Oops! Group, Zhejiang University.");
    cursor_row = row;
    cursor_col = col;
    kernel_set_cursor();
}

#pragma GCC push_options
#pragma GCC optimize("O0")
void create_startup_process() {
    unsigned int init_gp;
    asm volatile("la %0, _gp\n\t" : "=r"(init_gp));
    /**int pc_create(char *task_name, void(*entry)(unsigned int argc, void *args),
               unsigned int argc, void *args, pid_t *retpid, int is_user, unsigned int priority_class);*/
    //pc_create(1, ps, (unsigned int)kmalloc(4096) + 4096, init_gp, "powershell");
    pc_create("powershell", (void*)ps, 0, 0, 0, 0, HIGH_PRIORITY_CLASS);
    log(LOG_OK, "Shell init");
    pc_create("time", (void*)system_time_proc, 0, 0, 0, 0, HIGH_PRIORITY_CLASS);
//    pc_create(2, system_time_proc, (unsigned int)kmalloc(4096) + 4096, init_gp, "time");
    log(LOG_OK, "Timer init");
//    unsigned int init_gp;
//    asm volatile("la %0, _gp\n\t" : "=r"(init_gp));
//    pc_create(1, ps, (unsigned int)kmalloc(4096) + 4096, init_gp, "powershell");
//    log(LOG_OK, "Shell init");
//    pc_create(2, system_time_proc, (unsigned int)kmalloc(4096) + 4096, init_gp, "time");
//    log(LOG_OK, "Timer init");
}
#pragma GCC pop_options

void init_kernel() {
    kernel_clear_screen(31);
    // Exception
    init_exception();
    // Page table
    init_pgtable();
    // Drivers
    init_vga();
    init_ps2();
    // Memory management
    log(LOG_START, "Memory Modules.");
    init_bootmm();
    log(LOG_OK, "Bootmem.");
    init_buddy();
    log(LOG_OK, "Buddy.");
    init_slab();
    log(LOG_OK, "Slab.");
    log(LOG_END, "Memory Modules.");
    // File system
    log(LOG_START, "Virtual File System.");
//    init_fs();
    init_vfs();
    log(LOG_END, "Virtual File System.");
    // System call
    log(LOG_START, "System Calls.");
    init_syscall();
    log(LOG_END, "System Calls.");
    // Process control
    log(LOG_START, "Process Control Module.");
    init_pid();
    init_pc();
    create_startup_process();
    log(LOG_END, "Process Control Module.");
    // Interrupts
    log(LOG_START, "Enable Interrupts.");
    init_interrupts();
    log(LOG_END, "Enable Interrupts.");
    // Init finished
    machine_info();
    *GPIO_SEG = 0x11223344;
    // Enter shell
    while (1);
}

