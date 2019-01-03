//
// Created by zc on 2019-01-03.
//

#ifndef OPERATION_SYSTEM_VFSCACHE_H
#define OPERATION_SYSTEM_VFSCACHE_H

#include <zjunix/type.h>

struct cache {
    u8 cache_size;
    u8 cache_capacity;
    u32 cache_tablesize;
    struct list_head *content_list;
    struct list_head *c_hashtable;
    struct cache_operations *c_op;
};

struct cache_operations {
    /* 往缓冲区加入一个元素。如果发生缓冲区满，执行使用LRU算法的替换操作 */
    void (*add)(struct cache*, void*);

};

#endif //OPERATION_SYSTEM_VFSCACHE_H
