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
    u32 addr;
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
        if(tempPage->SLOB_PAGE.info.units < (SLOB_UNITS(size)))
        {
            continue;
        }
        //尝试分配对象
        err = slob_page_alloc(tempPage, size, align);
/*这里将slob_list链表头移动到prev->next前面，以便下次遍历时能够从prev->next开始遍历*/
//First Fit策略
        if (prev != slob_list->prev && slob_list->next != prev->next)
        {
            list_move_tail(slob_list, prev->next);
            break;
        }

    }

    if (! err) {//没有分配到对象，也就是说slob_list中没有可以满足分配要求的slob了
        err = slob_new_pages(gfp & ~__GFP_ZERO, 0);//创建新的slob
        if (!err)
        {
            return 0;
        }
        addr = err;
        tempPage = slob_page(addr);//获取slob的地址
        set_slob_page(tempPage);

        tempPage->SLOB_PAGE.info.units = SLOB_UNITS(SLOB_BREAK3);//计算单元数
        tempPage->SLOB_PAGE.info.free = addr;	//设置首个空闲块的地址
        INIT_LIST_HEAD(&(tempPage->SLOB_PAGE.info.page_list));
        set_slob(addr, SLOB_UNITS(SLOB_BREAK3), addr + SLOB_UNITS(SLOB_BREAK3));
        set_slob_page_free(tempPage, slob_list);    //将现在的页链入slob_list
        addr = slob_page_alloc(tempPage, size, align);//从新的slob中分配块
    }
    return err;
}

void *kmem_cache_alloc_node(struct kmem_cache *c, gfp_t flags, int node)
{
    void *b;

    if (c->size < (1<< PAGE_SHIFT)) {//对象小于PAGE_SIZE，由Slob分配器进行分配
        b = slob_alloc(c->size, flags, c->align, node);
        trace_kmem_cache_alloc_node(_RET_IP_, b, c->size,
                                    SLOB_UNITS(c->size) * SLOB_UNIT,
                                    flags, node);
    }
    else{//否则通过伙伴系统分配
        b = slob_new_pages(flags, get_order(c->size), node);
        trace_kmem_cache_alloc_node(_RET_IP_, b, c->size,
                                    (1<< PAGE_SHIFT) << get_order(c->size),
                                    flags, node);
    }

    if (c->ctor)//如果定义了构造函数则调用构造函数
        c->ctor(b);

    kmemleak_alloc_recursive(b, c->size, 1, c->flags, flags);
    return b;
}

void set_slob_page(struct slob_page* tempPage)
{

}
void *slob_page_alloc(struct slob_page *tempPage, int size, int align)
{

}