#include "../../arch/mips32/intr.h"
#include "../../arch/mips32/arch.h"

#include <driver/vga.h>
#include <zjunix/syscall.h>
#include <zjunix/utils.h>
#include <zjunix/pc.h>
#include <zjunix/pid.h>
#include <zjunix/slab.h>
#include <zjunix/vfs/vfs.h>
#include <zjunix/vfs/errno.h>
#include <zjunix/debug/debug.h>

const unsigned int PRIORITY[PRIORITY_CLASS_NUM][PRIORITY_LEVEL_NUM] = {
        {31,26,25,24,23,22,16},
        {15,15,14,13,12,11,1},
        {15,12,11,10,9,8,1},
        {15,10,9,8,7,6,1},
        {15,8,7,6,5,4,1},
        {15,6,5,4,3,2,1},
        {0,0,0,0,0,0,0}
};

struct list_head wait;                          // 等待列表
struct list_head exited;                        // 结束列表
struct list_head tasks;                         // 所有进程列表
unsigned char ready_bitmap[PRIORITY_LEVELS];                 // 就绪位图，表明该优先级的就绪队列里是否有东西
//todo:就绪位图修改成u32
struct ready_queue_element ready_queue[PRIORITY_LEVELS];     // 就绪队列
struct task_struct *current = 0;                // 当前进程
volatile int semaphore = 0;   //信号量，避免两个中断产生临界区问题

int min(int a, int b) {
    if (a > b) return b; return a;
}

unsigned int max(unsigned int a,unsigned int b) {
    if (a < b) return b; return a;
}
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
    debug_start("[pc.c: init_pc_list:75]\n");
    INIT_LIST_HEAD(&wait);
    INIT_LIST_HEAD(&exited);
    INIT_LIST_HEAD(&tasks);
    for (int i=0; i<PRIORITY_LEVELS; ++i) {
        /* 初始化就绪队列 */
        INIT_LIST_HEAD(&(ready_queue[i].queue_head));
        ready_queue[i].number = 0;
        ready_bitmap[i] = 0; // 初始化就绪位图
    }
    debug_end("[pc.c: init_pc_list:86]\n");

}

// current等待pid对应的进程
void join(pid_t target_pid){
    struct task_struct* target;
    struct task_struct* record = current; // 用来存一下current

    // 判断pid是否存在，如果不存在返回没有找到对应进程的错误提示
    if (!pid_exist(target_pid)) {
        kernel_puts("Process not found.\n", VGA_RED, VGA_BLACK);
        return ;
    }

    target = find_in_tasks(target_pid);

    // 向target的等待队列里面添加上current
    list_add_tail(&(current->wait_node),&(target->wait_queue));

    // todo：系统调用，触发schedule，然后将原来的current从ready加入wait中

    record->state = S_WAIT;
    remove_ready(record);
    add_wait(record);
}

// 唤醒pid对应的进程
void wake(pid_t target_pid){
    struct task_struct* target;

    // 判断pid是否存在，如果不存在返回没有找到对应进程的错误提示
    if (!pid_exist(target_pid)) {
        kernel_puts("Process not found.\n", VGA_RED, VGA_BLACK);
        return ;
    }

    target = find_in_tasks(target_pid);
    if (target->state != S_WAIT) // 不在wait状态的进程不能被唤醒
        kernel_puts("The process is not in waiting status.\n", VGA_RED, VGA_BLACK);
    else {
        target->state = S_READY; // 将状态换成ready
        remove_wait(target); // 从wait列表中移除
        add_ready(target); // 添加进ready列表中
    }
}

void init_pc() {
    debug_start("[pc.c: init_pc:134]\n");
    struct task_struct *idle;
    init_pc_list();

    idle = (struct task_struct*)(kernel_sp - KERNEL_STACK_SIZE);
    idle->ASID = 0;
    idle->time_counter = PROC_DEFAULT_TIMESLOTS;
    kernel_strcpy(idle->name, "init");

    idle->priority_class = ZERO_PRIORITY_CLASS;
    idle->priority_level = NORMAL;

    current = idle;
    add_task(current);
    register_syscall(10, pc_kill_syscall);
    register_interrupt_handler(7, pc_schedule);

    asm volatile(
    "li $v0, 1000000\n\t"   // 1000000->v0
    "mtc0 $v0, $11\n\t"     // $11 compare
    "mtc0 $zero, $9");      // $9 count
    debug_end("[pc.c: init_pc:149]\n");
}

void init_context(context * dest){
    dest->epc = 0;
    dest->at = 0;
    dest->v0 = 0;
    dest->v1 = 0;
    dest->a0 = 0;
    dest->a1 = 0;
    dest->a2 = 0;
    dest->a3 = 0;
    dest->t0 = 0;
    dest->t1 = 0;
    dest->t2 = 0;
    dest->t3 = 0;
    dest->t4 = 0;
    dest->t5 = 0;
    dest->t6 = 0;
    dest->t7 = 0;
    dest->s0 = 0;
    dest->s1 = 0;
    dest->s2 = 0;
    dest->s3 = 0;
    dest->s4 = 0;
    dest->s5 = 0;
    dest->s6 = 0;
    dest->s7 = 0;
    dest->t8 = 0;
    dest->t9 = 0;
    dest->hi = 0;
    dest->lo = 0;
    dest->gp = 0;
    dest->sp = 0;
    dest->fp = 0;
    dest->ra = 0;
}


int pc_create(char *task_name, void(*entry)(unsigned int argc, void *args),
              unsigned int argc, void *args, pid_t *retpid, int is_user, unsigned int priority_class) {
    debug_start("[pc.c: pc_create:158]\n");
    //这里暂时不考虑is_user的情况
    if (is_user){

    }
    else {

    }
    unsigned int init_gp;
    task_union* new_task;
    pid_t pid_num = -1;
    if (!pid_alloc(&pid_num)) {
        goto err_handler;
    }
    new_task = (task_union *) kmalloc(sizeof(unsigned char) * KERNEL_STACK_SIZE);
    if (new_task == 0) {
        goto err_handler;
    }
    //debug_warning("PC: NEXT\n");
    kernel_strcpy(new_task->task.name,task_name);
    //debug_warning("PC: NEXT\n");
    new_task->task.parent = current->pid;  //todo: 这里保存的是父进程的pid号
    //debug_warning("PC: NEXT\n");
    new_task->task.time_counter = PROC_DEFAULT_TIMESLOTS; //分配一整个默认时间配额
    //debug_warning("PC: NEXT\n");
    new_task->task.priority_class = priority_class; //暂时设置成正常的priority
    //debug_warning("PC: NEXT\n");
    new_task->task.priority_level = NORMAL;
    //debug_warning("PC: NEXT\n");
    context* new_context = &(new_task->task.context);
    //debug_warning("PC: NEXT\n");
//  kernel_printf("%d\n",&(new_task->task.context));
    init_context(new_context); //初始化新进程的上下文信息
    //debug_warning("PC: NEXT\n");
//    kernel_memset(new_task->kernel_stack,0,KERNEL_STACK_SIZE);
    new_task->task.ASID = new_task->task.pid = pid_num;
    debug_warning("newPid is : ");
    kernel_printf("%d\n",pid_num);
    new_task->task.state = S_INIT;
    INIT_LIST_HEAD(&(new_task->task.schedule_list));
    INIT_LIST_HEAD(&(new_task->task.task_node));
    INIT_LIST_HEAD(&(new_task->task.wait_queue));
    INIT_LIST_HEAD(&(new_task->task.wait_node));
    //debug_warning("PC: NEXT\n");
    new_task->task.task_files = 0;
    new_context->epc = (unsigned int) entry;  //初始化pc地址
//    debug_warning("newEPC:");
//    kernel_printf("%d\n",new_context->epc);
    new_context->sp = (unsigned int) (new_context + KERNEL_STACK_SIZE); //初始化栈顶指针
    //debug_warning("PC: NEXT 99\n");
    asm volatile("la %0, _gp\n\t" : "=r"(init_gp));
    new_context->gp = init_gp;  //初始化全局指针
    new_context->a0 = argc;     //初始化参数寄存器a0 a1
    new_context->a1 = (unsigned int) args;
//    debug_warning("PC: NEXT 00\n");
    if (retpid != 0) *retpid = pid_num;  //提供返回pid
    add_task(&(new_task->task));
    //debug_warning("PC: NEXT 11\n");
    new_task->task.state = S_READY;     //标记当前进程状态为READY
    //debug_warning("PC: NEXT 22\n");
    add_ready(&(new_task->task));
    debug_end("[pc.c: pc_create:200]\n");
    return 1;
    err_handler:
    {
        if (pid_num != -1) pid_free(pid_num);
        debug_err("[pc.c: pc_create:205]\n");
        return 0;
    }
}


/*************************************************************************************************/
// 每次调度的时候清空退出列表
void clear_exit() {
    struct task_struct* next;

    int count=0;
    while (exited.next!=&exited) { // 循环直到exit只剩下一个dummy head node
        next = container_of(exited.next, struct task_struct, schedule_list);
        if (next->state!=S_TERMINATE) {
            kernel_puts("There's a task whose state is not terminate.\n", VGA_RED, VGA_BLACK);
            continue;
        }
        remove_exit(next);
        remove_task(next);
        kernel_printf("clear exited: count = %d\n", ++count);
        kernel_printf("");
    }
}

// 进程正常结束，结束当前进程
void pc_kill_syscall(unsigned int status, unsigned int cause, context* pt_context) {
    while (semaphore);
    semaphore = 1;
    debug_start("[pc.c: pc_kill_syscall:xxx]\n");
    debug_warning("kill-syscall!");
    kernel_printf("%s\n",current->name);
    // 保留当前进程的pid
    pid_t kill_pid = current->pid;
    // 调度到下一个进程（将当前进程归到就绪队列中去）
    if (current->ASID != 0) {
        pc_schedule_core(status, cause, pt_context);
        pc_kill(kill_pid);
    }
    // 根据之前存的pid将对应的进程kill掉
    debug_end("[pc.c: pc_kill_syscall:xxx]\n");
    semaphore = 0;
}

// 杀死进程，返回值0正常，返回值-1出错
int pc_kill(pid_t pid) {
    debug_start("[pc.c: pc_kill:244]\n");
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
//    debug_warning("TARGET KILLEE:");
//    kernel_printf("%d\n",pid);
    // 判断pid是否存在，如果不存在返回没有找到对应进程的错误提示
    if (!pid_exist(pid)) {
        kernel_puts("Process not found.\n", VGA_RED, VGA_BLACK);
        enable_interrupts();
        return -1;
    }

//    debug_warning("STEP1\n");
    // 从tasks列表中找到pid对应的进程
    target = find_in_tasks(pid);
//    debug_warning("STEP2\n");
    if (target->state == S_READY) {
//        debug_warning("yes1\n");
        remove_ready(target); // 从就绪队列中删除
    } else if(target->state == S_WAIT) {
//        debug_warning("yes2\n");
        remove_wait(target);
    } else if(target->state == S_RUNNING) {
//        debug_warning("yes3\n");
        // 不可能出现还在running的，已经在pid==current->pid这一步过滤掉了
    }
    target->state = S_TERMINATE; // 状态标记为终止
//    debug_warning("STEP3\n");
    add_exit(target);
    // 关闭进程文件，释放进程文件空间
//    debug_warning("STEP4\n");
    if (target->task_files)
        task_files_release(target);
//    debug_warning("STEP5\n");
    // 释放pid
    pid_free(pid);
//    while(1);
    enable_interrupts();
    debug_end("[pc.c: pc_kill:286]\n");
    return 0;
}

// 根据进程优先级判断是否是实时任务
int is_realtime(struct task_struct* task) {
//    debug_start("[pc.c:is_realtime:366]\n");
    return PRIORITY[task->priority_class][task->priority_level] > 15;
}

void pc_schedule_core(unsigned int status, unsigned int cause, context* pt_context){
    struct task_struct* next;
    /* 判断异常类型 */
    if (cause==0) {
        // TODO: interruption
    }
    else if (cause==8) {
        // TODO: system call
    }

    // 清理结束链表
    clear_exit();
    /* 时间配额减少，每次触发时钟中断，从时间配额里面减少3 */
    current->time_counter -= 3;
    change_priority(current,1);
//    kernel_printf("");
    /* 时间配额没有用完，判断需不需要被抢占 */
    if (current->time_counter > 0) {
//        kernel_printf("Test1: what's the current process? %s\n",current->name);
        next = get_preemptive_task();
//        kernel_printf("Test1-2: what's the next process? %s\n",next->name);
        if (next!=current) { // 会被抢占
//            kernel_printf("Test1-3: will preemptive");
            if (is_realtime(current)) // 如果是实时task，重置时间片
                pc_exchange(next, pt_context, 1);
            else
                pc_exchange(next, pt_context, 0);
        }
    } else {
//        kernel_printf("Test2: what's the current process? %s\n",current->name);
//        /* 找到将要调度的下一个进程 */
        next = find_next_task();
        if (next!=current) {
            pc_exchange(next, pt_context, 1);
        }
    }
//    kernel_printf("");
    //debug_end("[pc.c: pc_schedule:328]\n");
    // 复位count，结束时钟中断
    asm volatile("mtc0 $zero, $9\n\t");
}

void pc_schedule(unsigned int status, unsigned int cause, context* pt_context) {
    while (semaphore);
    semaphore = 1;
    pc_schedule_core(status,cause,pt_context);
    //debug_start("[pc.c: pc_schedule:296]\n");
//    debug_warning("[pc_schedule] start name = ");
//    kernel_printf("%s\n",current->name);
    semaphore = 0;
}

// 将当前进程换成next
// pt_context：当前上下文
// flag：是否需要重置时间片
void pc_exchange(struct task_struct* next, context* pt_context, int flag) {
//    debug_start("[pc.c:pc_exchange:426]\n");
    if (flag) // 重置时间片
        current->time_counter = PROC_DEFAULT_TIMESLOTS;
    /* 保存上下文 */
    copy_context(pt_context, &(current->context));
    /* 将next从ready列表中拿出来 */
    next->state = S_RUNNING;
    remove_ready(next);
    /* 将current添加进ready列表中 */
    current->state = S_READY;
    add_ready(current);
    /* 更换当前进程 */
    current = next;
    /* 将新的上下文保存到pt_context中 */
    copy_context(&(current->context), pt_context);
//    debug_end("[pc.c:pc_exchange:441]\n");
}

// 寻找有没有可以抢占当前进程的进程
struct task_struct* get_preemptive_task() {
//    debug_start("[pc.c:get_preemptive_task:441]\n");
    struct task_struct* next;
    u32 current_priority = PRIORITY[current->priority_class][current->priority_level];
    next = (struct task_struct*)kmalloc(sizeof(struct task_struct));
//    next = &(current->context);
    next = current;

    // 从高到低寻找比current优先级高的
//    kernel_printf("Test1-3: current priority: %d\n",current_priority);
//    kernel_printf("Test1-3: prioirty_levels: %d\n",PRIORITY_LEVELS);

    for (int i = PRIORITY_LEVELS-1; i > current_priority; --i) {
        if (ready_bitmap[i]) {
            next = container_of(ready_queue[i].queue_head.next, struct task_struct, schedule_list);
            break;
        }
    }
    return next;
}

// 打印在就绪队列中的所有进程信息
int print_proc() {
    kernel_puts("PID\tname\tpriority\n", 0xfff, 0);
    kernel_printf(" %x  %s  %d\n", current->ASID, current->name, PRIORITY[current->priority_class][current->priority_level]);
    for (int i = 0; i < PRIORITY_LEVELS; ++i) {
        if (ready_bitmap[i]) {
            int number = ready_queue[i].number;
            struct list_head *this = ready_queue[i].queue_head.next; // 从第一个task开始
            struct task_struct *pcb = container_of(this, struct task_struct, schedule_list); // 找到对应的pcb
            while(number) { // 循环完为止
                kernel_printf(" %x  %s  %d\n", pcb->ASID, pcb->name, PRIORITY[pcb->priority_class][pcb->priority_level]);
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
        debug_err("continue!\n");
        result = container_of(this, struct task_struct, task_node); // 获取对应的pcb
        if (result->pid == pid) { // 判断pid是否一致，一致则返回结果
            return result;
        }
        this = this->next;
    }
    return (struct task_struct*)0; // 遍历到最后也没有找到，返回0
}

// 在就绪队列中寻找下一个要运行的进程并返回
struct task_struct* find_next_task() {
    //debug_start("[pc.c: find_next_task:410]\n");
    struct task_struct* next;
    u32 current_priority = PRIORITY[current->priority_class][current->priority_level];

    /* 先从优先级高于current的列表里面寻找，若找到直接返回 */
    next = get_preemptive_task();
    if (next != current)
        return next;

    // 如果没有找到，那么找同级的
    if (ready_bitmap[current_priority]) {
        next = container_of(ready_queue[current_priority].queue_head.next, struct task_struct, schedule_list);
    }
    //debug_end("[pc.c: find_next_task:423] equal priority\n");
    return next;
}

// 释放task的内容
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
    u32 priority = PRIORITY[task->priority_class][task->priority_level];
    list_add_tail(&(task->schedule_list), &(ready_queue[priority].queue_head));
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
    u32 priority = PRIORITY[task->priority_class][task->priority_level];
    list_del(&(task->schedule_list));
    INIT_LIST_HEAD(&(task->schedule_list));
    ready_queue[priority].number--;
    if (ready_queue[priority].number == 0) // 更新就绪位图对应位状态
        ready_bitmap[priority] = 0;
}

// 修改进程的优先级
void change_priority(struct task_struct *task, int delta) {
//    task->priority += delta;
    task->priority_level = max(min(TIME_CRITICAL,task->priority_level + delta),IDLE_PRIORITY_CLASS);
}
