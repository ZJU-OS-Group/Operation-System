#ifndef  _ZJUNIX_PID_H
#define  _ZJUNIX_PID_H

#define PID_MAX_DEFAULT 2048
#define PID_MAX_LIMIT (sizeof(long) > 4 ? 1024 : PID_MAX_DEFAULT)
#define PID_POOL_SIZE 129
#define  PID_BYTES 257
//避免数组大小是2的整数次幂的时候出现的存储器访问问题

#define  IDLE_PID  0            //idle 进程
#define  INIT_PID  1            //for kernel shell
#define  PIDMAP_INIT  0x01
#define  PID_MIN   1

typedef unsigned short pid_t;
pid_t min_pid(pid_t a, pid_t b);

void init_pid();
int pid_alloc(pid_t *ret);
int pid_free(pid_t num);
int pid_exist(pid_t pid);

#endif