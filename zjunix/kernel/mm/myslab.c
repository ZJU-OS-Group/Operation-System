//
// Created by m on 2019/1/5.
//
#include <zjunix/slab.h>
#include <zjunix/myslab.h>

void kmem_list3_init(struct kmem_list3* temp_kmem_list3)
{
    INIT_LIST_HEAD(&(temp_kmem_list3->slabs_full));
    INIT_LIST_HEAD(&(temp_kmem_list3->slabs_partial));
    INIT_LIST_HEAD(&(temp_kmem_list3->slabs_free));
    /*
    temp_kmem_list3->shared = NULL;
    temp_kmem_list3->alien = NULL;
    temp_kmem_list3->colour_next = 0;
    temp_kmem_list3parent->free_objects = 0;
    temp_kmem_list3parent->free_touched = 0;
     */
}
void *cache_alloc_refill(struct kmem_cache *temp_cache, gfp_t flags)
{

}
int cache_grow(struct kmem_cache *temp_cache, gfp_t flags, int node_id, void *obj_pointer)
{

}
void* kmalloc(unsigned int size)
{

}
void kfree(void* obj)
{

}