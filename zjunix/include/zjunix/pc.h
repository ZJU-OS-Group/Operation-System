#ifndef _ZJUNIX_PC_H
#define _ZJUNIX_PC_H
#include <zjunix/pid.h>
#include <zjunix/vfs/vfs.h>
#include "list.h"

#define TASK_NAME_LEN           32
#define PRIORITY_LEVELS         32          /* 优先级等级数 */
#define KERNEL_STACK_SIZE       4096     /* 内核栈大小 */
#define PROC_DEFAULT_TIMESLOTS  6    /* 默认时间配额 */
#define PRIORITY_CLASS_NUM      7
#define PRIORITY_LEVEL_NUM      7
#define KERNEL_NUM              4

/**************************************** 优先权类 *************************************/
enum PRIORITY_CLASS{
    REALTIME_PRIORITY_CLASS,
    HIGH_PRIORITY_CLASS,
    ABOVE_NORMAL_PRIORITY_CLASS,
    NORMAL_PRIORITY_CLASS,
    BELOW_NORMAL_PRIORITY_CLASS,
    IDLE_PRIORITY_CLASS,
    ZERO_PRIORITY_CLASS
};

enum PRIORITY_LEVEL{
    TIME_CRITICAL,
    HIGHEST,
    ABOVE_NORMAL,
    NORMAL,
    BELOW_NORMAL,
    LOWEST,
    IDLE
};

/**************************************** 进程状态 *************************************/
#define S_INIT 0
#define S_READY 1
#define S_RUNNING 2
#define S_STANDBY 3
#define S_WAIT 4
#define S_TRANSITION 5
#define S_TERMINATE 6

typedef struct {
    unsigned int epc;
    unsigned int at;
    unsigned int v0, v1;
    unsigned int a0, a1, a2, a3;
    unsigned int t0, t1, t2, t3, t4, t5, t6, t7;
    unsigned int s0, s1, s2, s3, s4, s5, s6, s7;
    unsigned int t8, t9;
    unsigned int hi, lo;
    unsigned int gp;
    unsigned int sp;
    unsigned int fp;
    unsigned int ra;
} context;

// 进程就绪队列
struct ready_queue_element{
    int number;
    struct list_head queue_head;
};

struct task_struct{
    context context;                    /* 进程上下文信息 */
    int ASID;                           /* 进程地址空间ID号 */
    char name[TASK_NAME_LEN];           /* 进程名 */
    pid_t pid;                          /* 当前进程PID号 */
    pid_t parent;                       /* 父进程PID号 */
    int state;                          /* 当前进程状态 */
    unsigned int time_counter;          /* 时间片 */
    struct file* task_files;            /* 进程打开的文件指针 */
    unsigned int priority_class;        /* 优先级类序号 */
    unsigned int priority_level;        /* 优先级内部级别 */
    struct list_head schedule_list;     /* 用于进程调度 */
    struct list_head task_node;         /* 用于添加进进程列表 */
    struct list_head wait_queue;        /* 正在等待该进程的进程列表 */
    struct list_head wait_node;         /* 当该进程在等待别的进程时，此node会连入另一进程的wait_queue */
};

typedef union {
    struct task_struct task;
    unsigned char kernel_stack[KERNEL_STACK_SIZE];
} task_union;  //进程控制块


void init_pc();
void pc_schedule(unsigned int status, unsigned int cause, context* pt_context);
int pc_create(char *task_name, void(*entry)(unsigned int argc, void *args),
              unsigned int argc, void *args, pid_t *retpid, int is_user, unsigned int priority_class);
void pc_kill_syscall(unsigned int status, unsigned int cause, context* pt_context);
int pc_kill(pid_t pid); // 杀死pid对应的进程
struct task_struct* find_in_tasks(pid_t pid); // 在tasks列表中找到pid对应的进程并返回其控制块
struct task_struct* get_curr_pcb();
int print_proc();   // 打印出就绪队列中的进程信息

void join(pid_t);
void wake(pid_t);
struct task_struct* get_preemptive_task();                                  // 找到可以抢占当前task的进程
struct task_struct* find_next_task();                                       // 找到下一个要被运行的task
void task_files_release(struct task_struct* task);                          // 释放进程的文件
int is_realtime(struct task_struct* task);                                  // 根据进程优先级判断是否是实时任务
void pc_schedule_core(unsigned int status, unsigned int cause, context* pt_context); // 从pc_schedule中抽出来的core code
void pc_exchange(struct task_struct* next, context* pt_context, int flag);  // 将当前进程换成next

void add_wait(struct task_struct *task);                    // 将进程添加进等待列表
void add_exit(struct task_struct *task);                    // 将进程添加至结束列表
void add_task(struct task_struct *task);                    // 将进程添加至所有进程列表
void add_ready(struct task_struct *task);                   // 将进程添加进就绪队列
void remove_wait(struct task_struct *task);                 // 从等待列表中删除进程
void remove_exit(struct task_struct *task);                 // 从退出列表中删除进程
void remove_task(struct task_struct *task);                 // 从进程列表中删除进程
void remove_ready(struct task_struct *task);                // 从就绪队列中删除task
void change_priority(struct task_struct *task, int delta);  // 修改进程的优先级

int min(int a, int b);
unsigned max(unsigned int a,unsigned int b);

#endif
