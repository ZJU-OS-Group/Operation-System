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
//unsigned char ready_bitmap[PRIORITY_LEVELS];                 // 就绪位图，表明该优先级的就绪队列里是否有东西
unsigned long ready_bitmap;
unsigned long free_bitmap;          //空闲位图，标记哪个处理器现在是空闲的
struct ready_queue_element ready_queue[PRIORITY_LEVELS];             // 就绪队列
struct task_struct * current[KERNEL_NUM];                // 当前进程
int curKernel = 0;  //标记当前所用的核是0号核
volatile int semaphore = 0;   //信号量，避免两个中断产生临界区问题
int proc_priority[KERNEL_NUM];  //每个处理器上的进程优先级
volatile int counter_num = 0;

// 寻找当前运行的进程优先级最低的处理器
int findLowest() {
    int i = 0;
    int pos = 0;
    for (i = 1; i < KERNEL_NUM; i++)
        if (proc_priority[i] < proc_priority[pos]) pos = i;
    return pos;
}

int min(int a, int b) {
    if (a > b) return b; return a;
}

unsigned int max(unsigned int a,unsigned int b) {
    if (a < b) return b; return a;
}

unsigned long get_ready_bit(int bitPos) {
    return (ready_bitmap >> (bitPos)) & 1;
}

unsigned long get_free_bit(int bitPos) {
    return (free_bitmap >> (bitPos)) & 1;
}

void set_ready_bit(int bitPos){
    ready_bitmap |= (1 << bitPos);
}

void reset_ready_bit(int bitPos) {
    ready_bitmap &= ~(1 << bitPos); //1左移若干位，然后取反，这样的话只有这一位是空，和原来的数字取一下与就可以
}

void set_free_bit(int bitPos) {
    free_bitmap |= (1 << bitPos);
}

void reset_free_bit(int bitPos) {
    free_bitmap &= ~(1 << bitPos);
}

// 找到下一个工作中的处理器
int find_next_working_bit(int bitPos) {
    int i;
    if (free_bitmap == 0x00000000) return -1;  //如果所有的机器都未在运行，那么就返回-1
    for (i = bitPos+1; i < KERNEL_NUM; i++)
        if (get_free_bit(i) == 1 && current[i]->priority_class != ZERO_PRIORITY_CLASS) return i;   //这里要求从bitPos的下一个开始扫描，选取第一个非空类
    for (i = 0; i < bitPos; i++)
        if (get_free_bit(i) == 1 && current[i]->priority_class != ZERO_PRIORITY_CLASS) return i;   //否则返回正在使用的下一个bit，同上
    return -1;
}

// 找到下一个空闲中的处理器
int find_next_free_bit(){  //寻找空闲处理器
    int i;
    if (free_bitmap == 0xFFFFFFFF) return -1;  //如果所有位图都是满的，那么就返回-1
    for (i = 0; i < 32; i++)
        if (get_free_bit(i) == 0) return i;   //否则返回空的bit
    return -1;
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
    }
    ready_bitmap = 0x00000000;  //初始化就绪位图，标记所有优先级都没有进程
    free_bitmap = 0x00000000;   //初始化空闲位图，标记所有的处理器都已经空闲
    debug_end("[pc.c: init_pc_list:86]\n");
}

// current等待pid对应的进程
void join(pid_t target_pid){
    struct task_struct* target;
    struct task_struct* record = current[curKernel]; // 用来存一下current

    // 判断pid是否存在，如果不存在返回没有找到对应进程的错误提示
    if (!pid_exist(target_pid)) {
        kernel_puts("Process not found.\n", VGA_RED, VGA_BLACK);
        return ;
    }

    target = find_in_tasks(target_pid);

    // 向target的等待队列里面添加上current
    list_add_tail(&(current[curKernel]->wait_node),&(target->wait_queue));


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

// idle 进程入口
void system_idle_proc() {
//    debug_info("system_loop_proc: I'm in!!!\n");
    while (1);
}

void init_pc() {
    debug_start("[pc.c: init_pc:134]\n");
//    struct task_struct *idle[KERNEL_NUM];
    init_pc_list();

//    idle = (struct task_struct**)(kernel_sp - KERNEL_STACK_SIZE); //这里可以分配KERNEL_NUM个idle
//    idle = (struct task_struct**)(KERNEL_NUM * sizeof(struct task_struct));
    int i;
    for (i = 0; i < KERNEL_NUM; i++) {
        int pc_pid = pc_create("idle", (void*)system_idle_proc, 0, 0, 0, 0, ZERO_PRIORITY_CLASS);
        current[i] = find_in_tasks((pid_t)pc_pid);
//        idle[i] = (struct task_struct*)kmalloc(sizeof(struct task_struct));
//        idle[i]->ASID = 0;
//        idle[i]->time_counter = PROC_DEFAULT_TIMESLOTS;
//        kernel_strcpy(idle[i]->name, "init");
//        idle[i]->priority_class = ZERO_PRIORITY_CLASS;
//        idle[i]->priority_level = NORMAL;
//        current[i] = idle[i];
//        add_task(current[i]);
    }

    register_syscall(10, pc_kill_syscall);
    register_syscall(13, pc_schedule_wait);

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

// 创建新进程，返回进程pid
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
    new_task->task.parent = current[curKernel]->pid;
    //debug_warning("PC: NEXT\n");
    new_task->task.time_counter = PROC_DEFAULT_TIMESLOTS; //分配一整个默认时间配额
    //debug_warning("PC: NEXT\n");
    new_task->task.priority_class = priority_class; //暂时设置成正常的priority
    //debug_warning("PC: NEXT\n");
    new_task->task.priority_level = NORMAL;
    //debug_warning("PC: NEXT\n");
    context* new_context = &(new_task->task.context);
    //debug_warning("PC: NEXT\n");
//    kernel_printf("%d\n",&(new_task->task.context));
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
    return new_task->task.pid;
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
    }
}

// 进程正常结束，结束当前进程
void pc_kill_syscall(unsigned int status, unsigned int cause, context* pt_context) {
    while (semaphore);
    semaphore = 1;
    debug_start("[pc.c: pc_kill_syscall:xxx]\n");
    debug_warning("kill-syscall!");
    kernel_printf("%s\n",current[curKernel]->name);
    // 保留当前进程的pid
    pid_t kill_pid = current[curKernel]->pid;
    // 调度到下一个进程（将当前进程归到就绪队列中去）
    if (current[curKernel]->ASID != 0) {
        // 先将优先级降下来，以便调度到idle
        current[curKernel]->priority_class = ZERO_PRIORITY_CLASS;
        current[curKernel]->priority_level = IDLE;
        //直接降到0
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
    if (pid==current[curKernel]->pid) {
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

    // 从tasks列表中找到pid对应的进程
    target = find_in_tasks(pid);
    if (target->state == S_READY) {
        remove_ready(target); // 从就绪队列中删除
    } else if(target->state == S_WAIT) {
        remove_wait(target);
    } else if(target->state == S_RUNNING) {
        // 不可能出现还在running的，已经在pid==current[curKernel]->pid这一步过滤掉了
    }
    target->state = S_TERMINATE; // 状态标记为终止
    reset_free_bit(curKernel);  // 标记当前处理器空闲
    add_exit(target);
    // 关闭进程文件，释放进程文件空间
    if (target->task_files)
        task_files_release(target);
    // 释放pid
    pid_free(pid);
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
    counter_num++;
    kernel_printf("");
    if (counter_num == 1000) {
        kernel_printf("current procname: %d %d\n",curKernel,free_bitmap);
        int i;
        for (i = 0; i < 32; i++)
            kernel_printf("%s, ",current[i]->name);
        counter_num = 1;
    }

    struct task_struct* next,last,preemBefore,preemAfter;
/* 判断异常类型 */

    if (cause==0) {
    }
    else if (cause==8) {
    }
    // 清理结束链表
    context *before,*after;
    before = &(current[curKernel]->context);
    clear_exit();
/* 时间配额减少，每次触发时钟中断，从时间配额里面减少3 */

    current[curKernel]->time_counter -= 3;
    change_priority(current[curKernel],1);
    if (current[curKernel]->time_counter == 0) {
        //这里要把当前核心的当前进程移送到ready队列
        proc_priority[curKernel] = PRIORITY[current[curKernel]->priority_class][current[curKernel]->priority_level];
        reset_free_bit(curKernel);
        current[curKernel]->time_counter = PROC_DEFAULT_TIMESLOTS;  //分配一个完整的时间配额
        current[curKernel]->state = S_READY;
        //如果时间片用完了，那么要把当前的进程移交到就绪队列，并且设置当前处理器无负载
        add_ready(current[curKernel]);
    }
    next = find_next_task();
    //首先不需要等待时间片用完，只要时钟中断触发就可以安排新的task，这里要找到下一个要运行的进程
    if (next != 0) {  //一定会有的，因为这里有32个空闲进程
        //如果就绪队列还有内容，那么尝试将其调度到一个处理器上
        int allocKernel = find_next_free_bit();
        if (allocKernel != -1) {             //说明现在还有一个空处理器
            current[allocKernel] = next;     //把新拿到的进程分配给新的处理器
            next->state = S_RUNNING;
            set_free_bit(allocKernel);       //标记当前目标分配的处理器接收到了进程
            remove_ready(next);              //next已经可以运行，不属于就绪状态
            proc_priority[allocKernel] = PRIORITY[next->priority_class][next->priority_level];
        } else {
            // 说明现在全满了
            // 第一步：找到目前具有最低优先级的处理器
            // 第二步：判断是否抢占并更新优先级
            allocKernel = findLowest();
            int nextPriority = PRIORITY[next->priority_class][next->priority_level];
            int currentPriority = PRIORITY[current[curKernel]->priority_class][current[curKernel]->priority_level];
            if (nextPriority > currentPriority) { //这里直接抢占
                if (allocKernel == curKernel) {
                    copy_context(pt_context, &(current[curKernel]->context));
/* 将新的上下文保存到pt_context中 */

                    copy_context(&(next->context), pt_context);
                }
                add_ready(current[allocKernel]);  //目标分配核的进程标记为ready
                current[allocKernel]->state = S_READY;
                remove_ready(next);
                next->state = S_RUNNING;
                current[allocKernel] = next;
                proc_priority[allocKernel] = PRIORITY[next->priority_class][next->priority_level];
            }
        }
    }
    int nextKernel = find_next_working_bit(curKernel);  //寻找下一个正在使用的处理器并更新到新的处理器
    // 现在还需要检查是否切换了kernel
    if (nextKernel != -1)    //如果能找到下一个正在运行的处理器，那么移交控制权
    {
        after = &current[nextKernel]->context; //求出下一个要调度的目标上下文
        copy_context(pt_context, &(current[curKernel]->context));
/* 将新的上下文保存到pt_context中 */
        curKernel = nextKernel;
        copy_context(&(current[nextKernel]->context), pt_context);
    } //如果等于-1，那么就依然运行当前的kernel，这意味着要么其他没在跑，要么都是idle
//    kernel_printf("current epc : %d\n",pt_context->epc);
    asm volatile("mtc0 $zero, $9\n\t");
}

void pc_schedule(unsigned int status, unsigned int cause, context* pt_context) {
//    debug_start("[pc.c: pc_schedule:417]\n");
    while (semaphore);
    semaphore = 1;
//    debug_warning("current procname:\n");
//    kernel_printf("%s\n",current[curKernel]->name);
    pc_schedule_core(status,cause,pt_context);
//    debug_end("[pc.c: pc_schedule:419]\n");
    semaphore = 0;
}


// 打印在就绪队列中的所有进程信息
int print_proc() {
    kernel_puts("ASID\tPID\tname\tpriority\n", 0xfff, 0);
    kernel_printf(" %x\t%d\t%s\t%d\n", current[curKernel]->ASID, current[curKernel]->pid, current[curKernel]->name, PRIORITY[current[curKernel]->priority_class][current[curKernel]->priority_level]);
    for (int i = 0; i < PRIORITY_LEVELS; ++i) {
        if (get_ready_bit(i) == 1) {
            int number = ready_queue[i].number;
            struct list_head *this = ready_queue[i].queue_head.next; // 从第一个task开始
            struct task_struct *pcb = container_of(this, struct task_struct, schedule_list); // 找到对应的pcb
            while(number) { // 循环完为止
                kernel_printf(" %x\t%d\t%s\t%d\n", pcb->ASID, pcb->pid, pcb->name, PRIORITY[pcb->priority_class][pcb->priority_level]);
                this = this->next;
                pcb = container_of(this, struct task_struct, schedule_list);
                number--;
            }
        }
    }
    return 0;
}

struct task_struct* get_curr_pcb() {
    return current[curKernel];
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
    struct task_struct* next = 0;
    for (int i = PRIORITY_LEVELS-1; i >= 0; --i) {
        if (get_ready_bit(i)) {  //如果该优先级就绪了，那么获取这个next
            next = container_of(ready_queue[i].queue_head.next, struct task_struct, schedule_list);
            break;
        }
    }
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

/*  if (ready_bitmap[priority]==0) // 修改就绪位图对应位状态
         ready_bitmap[priority] = 1;*/

//    if (get_ready_bit(priority) == 0)  //这句话是不需要的
    set_ready_bit(priority); // 只要有相应的优先级进入，就直接把这一位置位
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
        reset_ready_bit(priority);
//        ready_bitmap[priority] = 0;

}

// 修改进程的优先级
void change_priority(struct task_struct *task, int delta) {
//    task->priority += delta;
    task->priority_level = max(min(TIME_CRITICAL,task->priority_level + delta),IDLE);
}
