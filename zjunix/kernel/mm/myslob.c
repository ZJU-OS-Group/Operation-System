//
// Created by m on 2019/1/6.
//
#include <zjunix/slob.h>
static struct list_head* free_slob_small;
static struct list_head* free_slob_medium;
static struct list_head* free_slob_large;

void init_slob()
{
    INIT_LIST_HEAD(free_slob_small);
    INIT_LIST_HEAD(free_slob_medium);
    INIT_LIST_HEAD(free_slob_large);
}

void* slob_alloc(int size, int align)
{
    u32 err;
    struct slob_page *tempPage;
    struct list_head *prev;
    struct list_head *slob_list;
    if (size < SLOB_BREAK1)
        slob_list = free_slob_small;
    else if (size < SLOB_BREAK2)
        slob_list = free_slob_medium;
    else
        slob_list = free_slob_large;

    list_for_each_entry(tempPage, slob_list, SLOB_PAGE.info.page_list)
    {//遍历slob链表
        if(tempPage->SLOB_PAGE.info.units < SLOB_UINTS(size))
        {
            continue;
        }

        err = slob_page_alloc(tempPage, size, align);
    }

}

void *slob_page_alloc(struct slob_page *tempPage, int size, int align)
{

}