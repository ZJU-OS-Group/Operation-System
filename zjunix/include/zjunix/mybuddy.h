//
// Created by m on 2019/1/5.
//

#ifndef OPERATION_SYSTEM_MYBUDDY_H
#define OPERATION_SYSTEM_MYBUDDY_H
#include <zjunix/list.h>
#include <zjunix/lock.h>

// 1K per page
#define PAGE_SHIFT 10
#define PAGE_FREE 0x00
#define PAGE_USED 0xff

#define PAGE_ALIGN (~((1 << PAGE_SHIFT) - 1))
#define MAX_ORDER 4

#define PAGE_ADDR(x, base)  ((x - base) <<PAGE_SHIFT)
struct page
{
    int order;
    //各page项间的双向链表
    struct list_head page_list;
};
struct free_area {
    //内存区空闲内存块数
    unsigned int nr_free;
    //连接每种类型迁移内存块的链表
    struct list_head* free_head;
};
struct _free_area_
{
    struct free_area free_areas[MAX_ORDER + 1];
};
struct buddy_mem
{
    struct _free_area_* free_zone;

};
void mem_init();
void free_pages();
void* get_free_pages(int);
void* _get_free_pages(int);
#endif //OPERATION_SYSTEM_MYBUDDY_H
