//
// Created by zc on 2019-01-03.
//

#ifndef OPERATION_SYSTEM_VFSCACHE_H
#define OPERATION_SYSTEM_VFSCACHE_H
#include <zjunix/vfs/err.h>
#include <zjunix/type.h>


#define DCACHE_CAPACITY         64
#define DCACHE_HASHTABLE_SIZE   16
#define ICACHE_CAPACITY         64
#define ICACHE_HASHTABLE_SIZE   16
#define PCACHE_CAPACITY         64
#define PCACHE_HASHTABLE_SIZE   16

#define         P_CLEAR                         0
#define         P_DIRTY                         1

#include <zjunix/vfs/vfs.h>
struct condition;
struct qstr;
struct dentry;
struct vfs_page;
struct cache {
    u8 cache_size;
    u8 cache_capacity;
    u32 cache_tablesize;
    struct list_head *c_lru;
    struct list_head *c_hashtable;
    struct cache_operations *c_op;
};

struct cache_operations {
    /* 往缓冲区加入一个元素。如果发生缓冲区满，执行使用LRU算法的替换操作 */
    void (*add)(struct cache*, void*);
    /* 判断缓冲区是否满 */
    u32 (*is_full)(struct cache*);
    /* 当一个缓冲页被替换出去时，写回外存以保持数据协调 */
    void (*write_back)(void*);
    /* 根据某一条件，用哈希表在缓冲区中查找对应元素是否存在 */
    void* (*look_up)(struct cache*, struct condition*);
};


// dcache.c for dentry cache
void dget(struct dentry *);
void dput(struct dentry *);
void * dcache_look_up(struct cache *, struct condition *);
struct dentry * dcache_add(struct dentry *, struct qstr *);
struct dentry * d_alloc(struct dentry *, const struct qstr *);
// pcache.c for page cache
void* pcache_look_up(struct cache*, struct condition*);
void pcache_add(struct cache*, void*);
void pcache_write_back(void*);
//
int getIntHash(long , long);
void release_dentry(struct dentry *dentry);
void release_inode(struct inode *inode);
void release_page(struct vfs_page* page);
u8 cache_is_full(struct cache *);
#endif //OPERATION_SYSTEM_VFSCACHE_H
