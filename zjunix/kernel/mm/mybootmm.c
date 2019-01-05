//
// Created by m on 2019/1/5.
//
#include <zjunix/mybootmm.h>
#include <zjunix/myslab.h>
#include <zjunix/mybuddy.h>
void init_bootmm()
{
    //bootmem迁移至伙伴系统
    mem_init();
    //构建好了kmem_cache实例cache_cache，且构建好了kmem_cache的slab
    //分配器,并由init_kmem_list3[0]组织, 相应的array为init_array_cache
    kmem_cache_init();
}