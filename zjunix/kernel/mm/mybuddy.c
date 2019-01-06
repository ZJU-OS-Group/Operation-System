//
// Created by m on 2019/1/5.
//
#include <zjunix/mybuddy.h>
#include <zjunix/utils.h>
extern struct buddy_mem* buddyMem;
void* get_free_pages(int nr_pages)
{
    int order_needed = 0;
    struct page* free_page;

    if(nr_pages == 0) return 0;
    while(nr_pages > (1 << order_needed))
    {
        order_needed++;
    }
    if(order_needed > MAX_ORDER)
    {
        return 0;
    }

    free_page = _get_free_pages(order_needed);
    if(!free_page) return 0;

    return (void*)
}
void* _get_free_pages(int order)
{
    int temp_order;
    int found = 0;
    struct page* free_page;
    struct page* free_page_buddy;

    for(temp_order = order;temp_order <= MAX_ORDER;temp_order++)
    {
        if(buddyMem->free_zone->free_areas[temp_order].nr_free != 0)
        {
            found = 1;
            break;
        }
    }
    if(found)
    {
        free_page = container_of(buddyMem->free_zone->free_areas[temp_order].free_head->next, struct page, page_list);
        buddyMem->free_zone->free_areas[temp_order].nr_free--;

    }
    else
        return 0;
}
