#ifndef _ZJUNIX_PC_H
#define _ZJUNIX_PC_H
#include <zjunix/pid.h>
#include <zjunix/fs/fat.h>
#include "list.h"

#define TASK_NAME_LEN 32
#define PRIORITY_LEVELS 32          /* 优先级等级数 */
#define KERNEL_STACK_SIZE  4096     /* 内核栈大小 */
#define PROC_DEFAULT_TIMESLOTS 6    /* 默认时间配额 */

/**************************************** 优先权类 *************************************/
#define IDLE_PRIORITY_CLASS 4
#define BELOW_NORMAL_PRIORITY_CLASS 6
#define NORMAL_PRIORITY_CLASS 8
#define ABOVE _NORMAL_PRIORITY_CLASS 10
#define HIGH_PRIORITY_CLASS 13
#define REALTIME_PRIORITY_CLASS 24


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
    unsigned int priority;              /* 优先级 */
    FILE * task_files;                  /* 进程打开的文件指针 */
    struct task_struct *prev,*succ;     /* 链表中的前继和后续 */
    struct list_head schedule_list;     /* 用于进程调度 */
    struct list_head task_node;         /* 用于添加进进程列表 */
};

typedef union {
    struct task_struct task;
    unsigned char kernel_stack[KERNEL_STACK_SIZE];
} task_union;  //进程控制块


void init_pc();
void pc_schedule(unsigned int status, unsigned int cause, context* pt_context);
void pc_create(char *task_name, void(*entry)(unsigned int argc, void *args),
               unsigned int argc, void *args, pid_t *retpid, int is_user);
void pc_kill_syscall(unsigned int status, unsigned int cause, context* pt_context);
int pc_kill(int proc);
struct task_struct* get_curr_pcb();
int print_proc();
struct task_struct* find_next_task();

#endif  // !_ZJUNIX_PC_H