#include "pc.h"
#include "../../arch/mips32/intr.h"

#include <driver/vga.h>
#include <zjunix/syscall.h>
#include <zjunix/utils.h>

struct list_head wait;                          // 等待状态
struct list_head exited;                        // 结束状态
struct list_head tasks;                         // 所有进程
unsigned char ready_bitmap[PRIORITY_LEVELS];                 // 就绪位图，表明该优先级的就绪队列里是否有东西
struct ready_queue_element ready_queue[PRIORITY_LEVELS];     // 就绪队列

// 复制上下文
static void copy_context(context* src, context* dest) {
    dest->epc = src->epc;
    dest->at = src->at;
    dest->v0 = src->v0;
    dest->v1 = src->v1;
    dest->a0 = src->a0;
    dest->a1 = src->a1;
    dest->a2 = src->a2;
    dest->a3 = src->a3;
    dest->t0 = src->t0;
    dest->t1 = src->t1;
    dest->t2 = src->t2;
    dest->t3 = src->t3;
    dest->t4 = src->t4;
    dest->t5 = src->t5;
    dest->t6 = src->t6;
    dest->t7 = src->t7;
    dest->s0 = src->s0;
    dest->s1 = src->s1;
    dest->s2 = src->s2;
    dest->s3 = src->s3;
    dest->s4 = src->s4;
    dest->s5 = src->s5;
    dest->s6 = src->s6;
    dest->s7 = src->s7;
    dest->t8 = src->t8;
    dest->t9 = src->t9;
    dest->hi = src->hi;
    dest->lo = src->lo;
    dest->gp = src->gp;
    dest->sp = src->sp;
    dest->fp = src->fp;
    dest->ra = src->ra;
}

// 初始化进程管理的相关全局链表和数组
void init_pc_list() {
    INIT_LIST_HEAD(&wait);
    INIT_LIST_HEAD(&exited);
    INIT_LIST_HEAD(&tasks);

    for (int i=0; i<PRIORITY_LEVELS; ++i) {
        /* 初始化就绪队列 */
        INIT_LIST_HEAD(&(ready_queue[i].queue_head));
        ready_queue[i].number = 0;
        ready_bitmap[i] = 0; // 初始化就绪位图
    }

}

void init_pc() {
    struct task_struct *idle;
    init_pc_list();

//    idle = (struct task_struct*)(kernel_sp - KERNEL_STACK_SIZE);
    idle[0].ASID = 0;
    idle[0].time_counter = PROC_DEFAULT_TIMESLOTS;
    kernel_strcpy(idle[0].name, "init");
    register_syscall(10, pc_kill_syscall);
    register_interrupt_handler(7, pc_schedule);

    asm volatile(
        "li $v0, 1000000\n\t"   // 1000000->v0
        "mtc0 $v0, $11\n\t"     // $11 compare
        "mtc0 $zero, $9");      // $9 count
}

void pc_schedule(unsigned int status, unsigned int cause, context* pt_context) {
    // Save context
    copy_context(pt_context, &(pcb[curr_proc].context));
    int i;
    for (i = 0; i < 8; i++) {
        curr_proc = (curr_proc + 1) & 7;
        if (pcb[curr_proc].ASID >= 0)
            break;
    }
    if (i == 8) {
        kernel_puts("Error: PCB[0] is invalid!\n", 0xfff, 0);
        while (1);
    }
    // Load context
    copy_context(&(pcb[curr_proc].context), pt_context);
    asm volatile("mtc0 $zero, $9\n\t");
}

int pc_peek() {
    int i = 0;
    for (i = 0; i < 8; i++)
        if (pcb[i].ASID < 0)
            break;
    if (i == 8)
        return -1;
    return i;
}

void pc_create(int asid, void (*func)(), unsigned int init_sp, unsigned int init_gp, char* name) {
    pcb[asid].context.epc = (unsigned int)func;
    pcb[asid].context.sp = init_sp;
    pcb[asid].context.gp = init_gp;
    kernel_strcpy(pcb[asid].name, name);
    pcb[asid].ASID = asid;
}

void pc_kill_syscall(unsigned int status, unsigned int cause, context* pt_context) {
    if (curr_proc != 0) {
        pcb[curr_proc].ASID = -1;
        pc_schedule(status, cause, pt_context);
    }
}

int pc_kill(int proc) {
    proc &= 7;
    if (proc != 0 && pcb[proc].ASID >= 0) {
        pcb[proc].ASID = -1;
        return 0;
    } else if (proc == 0)
        return 1;
    else
        return 2;
}

struct task_struct* get_curr_pcb() {
    return &pcb[curr_proc];
}

int print_proc() {
    int i;
    kernel_puts("PID name\n", 0xfff, 0);
    for (i = 0; i < 8; i++) {
        if (pcb[i].ASID >= 0)
            kernel_printf(" %x  %s\n", pcb[i].ASID, pcb[i].name);
    }
    return 0;
}
