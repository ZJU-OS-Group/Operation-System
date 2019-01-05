//
// Created by m on 2019/1/5.
//

#ifndef OPERATION_SYSTEM_MYSLAB_H
#define OPERATION_SYSTEM_MYSLAB_H
#include <zjunix/list.h>
#include <zjunix/mybuddy.h>
struct kmem_cache{
    unsigned int size;              // Total size of a slab
    unsigned int obj_size;           // Object size
    struct kmem_cache_node node;    // Partial and full lists
    struct kmem_cache_cpu cpu;      // Current page
};
struct kmem_list3{
    struct list_head slabs_full;
    struct list_head slabs_partial;
    struct list_head slabs_free;
};
struct array_cache{

    int avail;
    int touched;
    long limit;
    long batchcount;
    struct list_head entry;
};
void kmem_cache_init();
void kmem_list3_init(struct kmem_list3*);
void kmem_free_pages();
void kmem_getpages();
void kmem_cache_mgmt();
void kmem_cache_init_objs();
void kmem_slab_link_end();
void kmem_slab_unlink();
void kmem_cache_alloc();
void kmem_cache_alloc_refill();
void* _cache_alloc();
void* __cache_alloc();
struct array_cache* cpu_cache_get(struct kmem_cache*);
int cache_grow(struct kmem_cache *, gfp_t , int ,void*);
extern void* kmalloc(unsigned int size);
extern void kfree(void* obj);
#endif //OPERATION_SYSTEM_MYSLAB_H
