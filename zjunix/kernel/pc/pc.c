#include "../../arch/mips32/intr.h"
#include "../../arch/mips32/arch.h"

#include <driver/vga.h>
#include <zjunix/syscall.h>
#include <zjunix/utils.h>
#include <zjunix/pc.h>
#include <zjunix/slab.h>
#include <zjunix/vfs/vfs.h>

struct list_head wait;                          // 等待列表
struct list_head exited;                        // 结束列表
struct list_head tasks;                         // 所有进程列表
unsigned char ready_bitmap[PRIORITY_LEVELS];                 // 就绪位图，表明该优先级的就绪队列里是否有东西
struct ready_queue_element ready_queue[PRIORITY_LEVELS];     // 就绪队列
struct task_struct *current = 0;                // 当前进程

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

    idle = (struct task_struct*)(kernel_sp - KERNEL_STACK_SIZE);
    idle->ASID = 0;
    idle->time_counter = PROC_DEFAULT_TIMESLOTS;
    kernel_strcpy(idle->name, "init");
    register_syscall(10, pc_kill_syscall);
    register_interrupt_handler(7, pc_schedule);

    asm volatile(
        "li $v0, 1000000\n\t"   // 1000000->v0
        "mtc0 $v0, $11\n\t"     // $11 compare
        "mtc0 $zero, $9");      // $9 count
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

/*************************************************************************************************/

void pc_kill_syscall(unsigned int status, unsigned int cause, context* pt_context) {
    if (curr_proc != 0) {
        pcb[curr_proc].ASID = -1;
        pc_schedule(status, cause, pt_context);
    }
}

// 杀死进程，返回值0正常，返回值-1出错
int pc_kill(pid_t pid) {
    int res;
    struct task_struct* target;
    /* 三种不能kill的特殊情况进行特判 */
    if (pid==IDLE_PID) {
        kernel_puts("Can't kill the idle process.\n", VGA_RED, VGA_BLACK);
        return -1;
    }
    if (pid==INIT_PID) {
        kernel_puts("Can't kill the init process.\n", VGA_RED, VGA_BLACK);
        return -1;
    }
    if (pid==current->pid) {
        kernel_puts("Can't kill self.\n", VGA_RED, VGA_BLACK);
        return -1;
    }

    disable_interrupts();
    // 判断pid是否存在，如果不存在返回没有找到对应进程的错误提示
    res = pid_exist(pid);
    if (!res) {
        kernel_puts("Process not found.\n", VGA_RED, VGA_BLACK);
    }

    // 从tasks列表中找到pid对应的进程
    target = find_in_tasks(pid);

    if (target->state == S_READY) {
        remove_ready(target); // 从就绪队列中删除
    } else if(target->state == S_WAIT) {
        remove_wait(target);
    } else if(target->state == S_RUNNING) {
        // TODO: when running, how?
    }
    target->state = S_TERMINATE; // 状态标记为终止
    add_exit(target);
    // 关闭进程文件，释放进程文件空间
    if (target->task_files)
        task_files_release(target);

    // 释放pid
    pid_free(pid);
    enable_interrupts();
    return 0;
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

// 打印在就绪队列中的所有进程信息
int print_proc() {
    kernel_puts("PID name\n", 0xfff, 0);
    for (int i = 0; i < PRIORITY_LEVELS; ++i) {
        if (ready_bitmap[i]) {
            int number = ready_queue[i].number;
            struct list_head *this = ready_queue[i].queue_head.next; // 从第一个task开始
            struct task_struct *pcb = container_of(this, struct task_struct, schedule_list); // 找到对应的pcb
            while(number) { // 循环完为止
                kernel_printf(" %x  %s\n", pcb->ASID, pcb->name);
                this = this->next;
                pcb = container_of(this, struct task_struct, schedule_list);
                number--;
            }
        }
    }
    return 0;
}

struct task_struct* get_curr_pcb() {
    return current;
}

// 在所有进程列表中找到pid对应的进程，返回控制块
struct task_struct* find_in_tasks(pid_t pid) {
    struct list_head* start = &tasks;
    struct list_head* this;
    struct task_struct* result;

    this = start->next;
    while (this != start) { // 循环遍历进程列表
        result = container_of(this, struct task_struct, task_node); // 获取对应的pcb
        if (result->pid == pid) { // 判断pid是否一致，一致则返回结果
            return result;
        }
    }
    return (struct task_struct*)0; // 遍历到最后也没有找到，返回0
}

// 在就绪队列中寻找下一个要运行的进程并返回
struct task_struct* find_next_task() {
    struct task_struct* next;
    u32 current_priority = current->priority-1; // priority从1～32，但是就绪队列数组从0～31

    next = current;
    // 从高到低寻找比current优先级高的
    for (int i = PRIORITY_LEVELS; i > current_priority; --i) {
        if (ready_bitmap[i]) {
            next = container_of(ready_queue[i].queue_head.next, struct task_struct, schedule_list);
            return next;
        }
    }
    // 如果没有找到，那么找同级的
    if (ready_bitmap[current_priority]) {
        next = container_of(ready_queue[current_priority].queue_head.next, struct task_struct, schedule_list);
    }
    return next;
}

void task_files_release(struct task_struct* task) {
    vfs_close(task->task_files);
    kfree(&(task->task_files));
}

/**************************************** 一些对队列的功能性操作 ********************************************/

// 将进程添加进等待列表
void add_wait(struct task_struct *task) {
    list_add_tail(&(task->schedule_list), &wait);
}

// 将进程添加至结束列表
void add_exit(struct task_struct *task) {
    list_add_tail(&(task->schedule_list), &exited);
}

// 将进程添加至所有进程列表
void add_task(struct task_struct *task) {
    list_add_tail(&(task->task_node), &tasks);
}

// 将进程添加进就绪队列
void add_ready(struct task_struct *task) {
    u32 priority = task->priority-1;
    list_add_tail(&(task->schedule_list), &ready_queue[priority].queue_head);
    ready_queue[priority].number++; // 该优先级的就绪队列长度加一
    if (ready_bitmap[priority]==0) // 修改就绪位图对应位状态
        ready_bitmap[priority] = 1;
}

// 从等待列表中删除进程
void remove_wait(struct task_struct *task) {
    list_del(&(task->schedule_list));
    INIT_LIST_HEAD(&(task->schedule_list));
}

// 从退出列表中删除进程
void remove_exit(struct task_struct *task) {
    list_del(&(task->schedule_list));
    INIT_LIST_HEAD(&(task->schedule_list));
}

// 从进程列表中删除进程
void remove_task(struct task_struct *task) {
    list_del(&(task->task_node));
    INIT_LIST_HEAD(&(task->task_node));
}

// 从就绪队列中删除进程
void remove_ready(struct task_struct *task) {
    u32 priority = task->priority-1;
    list_del(&(task->schedule_list));
    INIT_LIST_HEAD(&(task->schedule_list));
    ready_queue[priority].number--;
    if (ready_queue[priority].number == 0) // 更新就绪位图对应位状态
        ready_bitmap[priority] = 0;
}

// 修改进程的优先级
void change_priority(struct task_struct *task, int delta) {
    task->priority += delta;
}
