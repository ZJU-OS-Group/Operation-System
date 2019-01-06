//
// Created by m on 2019/1/6.
//

#ifndef OPERATION_SYSTEM_SLOB_H
#define OPERATION_SYSTEM_SLOB_H
#include <zjunix/list.h>
#include <zjunix/buddy.h>
#include <zjunix/utils.h>
#include <zjunix/type.h>

#define SIZE_INT 4
#define SLOB_AVAILABLE 0x0
#define SLOB_USED 0xff

#define SLOB_BREAK1 256
#define SLOB_BREAK2 1024
#define SLOB_BREAK3 1<<PAGE_SHIFT
#define SLOB_UNIT_SIZE sizeof(struct slob_unit)
#define SLOB_UNITS(x)  x/SLOB_UNIT_SIZE


struct slob_unit{

};

struct kmem_cache {
    unsigned int size, align; //对象大小，对齐值
    unsigned long flags;      //属性标识
    const char *name;         //缓存名
    void (*ctor)(void *);     //分配对象时的构造函数
};

struct slob_page {
    union {
        struct {
            unsigned long flags;/* 填充用，为了不覆盖page->flags */
            atomic_t _count;      /* 填充用，为了不覆盖page->_count */
            int units;      /* slob的空闲单元数 */
            unsigned long pad[2];
            slob_t *free;        /* 指向第一个空闲块 */
            struct list_head page_list;    /* 用于链入slob全局链表 */
        }info;
        struct page page;
    }SLOB_PAGE;
};
struct slob_block
{
    int units;  //这个slob块中包含的units数量
};

//slob.c中实现的各个函数
void init_slob();
void* slob_alloc(int size, int align);
void *slob_page_alloc(struct slob_page *tempPage, int size, int align);
void set_slob_page(struct slob_page*);
#endif //OPERATION_SYSTEM_SLOB_H
