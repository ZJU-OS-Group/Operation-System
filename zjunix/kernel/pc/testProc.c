#include <zjunix/pc.h>
#include <zjunix/debug/debug.h>
#include <intr.h>

// for test ps_kill
void system_loop_proc() {
    debug_info("system_loop_proc: I'm in!!!\n");
    while (1);
}

// for test ps_kill_syscall
void system_suicide_proc() {
    int i;
    debug_start("suicide: I'm in!\n");
    for (i = 0; i < 100; i++);
    debug_info("SSSSSSSSSSSSSSSSSSSSSSSSSSSSSS\n");
    for (i = 0; i < 100; i++);
    debug_info("I love OS with no bug.\n");
    for (i = 0; i < 100; i++);
    debug_info("God bless me.\n");
    for (i = 0; i < 100; i++);
    debug_end("suicide: I will call kill system call soon~ get ready baby!\n");
    for (i = 0; i < 100; i++);
    pc_exit();
}

void system_father() {
    int i;
    debug_info("I'm father proc, I'm creating the child and waiting for it.\n");
    disable_interrupts();
    pid_t target_pid = pc_create("system_child",(void*)system_child, 0, 0, 0, 0, HIGH_PRIORITY_CLASS);
    enable_interrupts();
    join(target_pid);
    debug_info("Father proc finishes.\n");
    pc_exit();
}

void system_child() {
    debug_info("I'm child proc. I'm gonna exit.\n");
    int i;
    kernel_printf("...");
    kernel_printf("");
    for (i = 0; i < 100; i++) kernel_printf(".....\n");
    kernel_printf("...");
    debug_info("Child proc finishes. \n");
    pc_exit();
}