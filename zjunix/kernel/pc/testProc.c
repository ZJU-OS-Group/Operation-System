#include <zjunix/pc.h>
#include <zjunix/debug/debug.h>

// for test ps_kill
void system_loop_proc() {
    debug_info("system_loop_proc: I'm in!!!\n");
    while (1);
}

// for test ps_kill_syscall
void system_suicide_proc() {
    debug_start("suicide: I'm in!\n");
    debug_info("SSSSSSSSSSSSSSSSSSSSSSSSSSSSSS\n");
    debug_info("I love OS with no bug.\n");
    debug_info("God bless me.\n");
    debug_end("suicide: I will call kill system call soon~ get ready baby!\n");
}
