#include <zjunix/pc.h>
#include <zjunix/debug/debug.h>

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
