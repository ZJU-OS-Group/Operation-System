
#include <zjunix/pid.h>
#include <zjunix/debug/debug.h>

unsigned char pidmap[PID_BYTES];    //pid 位图
pid_t nextPid;      //当前的最大pid
unsigned short pidPool[PID_POOL_SIZE+2]; //存放弃用的pid
char stackTop;
pid_t minInPool;

inline pid_t min_pid(pid_t a, pid_t b) {
    if (a > b) return b; return a;
}
//以下操作都是成功:返回1，不成功：返回0
void init_pid() {
    debug_start("[pc.c: init_pid]\n");
    int i;
    for (i = 0; i < PID_BYTES; i++) pidmap[i] = 0;
    pidmap[0] = PIDMAP_INIT;
    nextPid = PID_MIN;
    stackTop = 0;
    minInPool = PID_MAX_LIMIT;
    debug_end("[pc.c: init_pid]\n");
}

int pid_alloc(pid_t *ret){
    if (stackTop > 0) {
        pid_t ans = pidPool[--stackTop];
        pidmap[ans >> 3] |= (1 << (ans & 7)); //pidmap置位
        *ret = ans;   //交付pid
        return 1;
    }
    pid_t i;
    for (i = nextPid; i < PID_MAX_LIMIT; i++) {
        if (pid_exist(i)) continue;
        pidmap[i >> 3] |= (1 << (i & 7)); //pidmap置位
        nextPid = (pid_t) (i + 1);      //更新nextPid
        *ret = i;
        return 1;  //交付pid
    }
    if (i >= PID_MAX_LIMIT) return 0;
}

int pid_free(pid_t pid){
    int ans = pid_exist(pid);
    if (ans) {
        pidmap[pid >> 3] &= ~(1 << (pid & 7));
        pidPool[stackTop++] = pid;
        minInPool = min_pid(pid,minInPool);  //更新池内最小pid
        if (stackTop > PID_POOL_SIZE) {
            nextPid = min_pid(minInPool,nextPid);
            stackTop = 0;
            minInPool = PID_MAX_LIMIT;
        }  //如果被弃用的PID过多的话，将栈清空，并且让next_pid回到最小值处开始后移
    }
    return ans;
}

int pid_exist(pid_t pid){
    return ((pid <= PID_MAX_LIMIT) && ((pidmap[pid >> 3] & (1 << (pid & 7)))));
}