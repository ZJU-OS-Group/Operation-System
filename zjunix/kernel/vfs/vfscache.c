//
// Created by Desmond.
//
#include <zjunix/type.h>
#include <zjunix/list.h>
#include <zjunix/vfs/vfscache.h>
#include <zjunix/slab.h>
// 公用缓存
struct cache * dcache;
struct cache * pcache;
struct cache * icache;

//dcache缓存操作函数
struct cache_operations dentry_cache_operations = {
    .look_up    = dcache_look_up,
    .add        = dcache_add,
    .is_full    = cache_is_full,
};
//icache缓存操作函数
struct cache_operations inode_cache_operations = {
    .is_full    = cache_is_full,
};
//pcache缓存操作函数
struct cache_operations page_cache_operations = {
    .look_up    = pcache_look_up,
    .add        = pcache_add,
    .is_full    = cache_is_full,
    .write_back = pcache_write_back,
};

unsigned long init_cache()
{
    u32 err, i;
    err = -ENOMEM;
    //初始化dcache
    dcache = (struct cache*) kmalloc(sizeof(struct cache));
    if(dcache == 0)
    {
        kfree(dcache);
        return err;
    }
    dcache->cache_size = 0;
    dcache->cache_capacity = DCACHE_CAPACITY;
    dcache->cache_tablesize = DCACHE_HASHTABLE_SIZE;
    INIT_LIST_HEAD(dcache->c_lru);
    dcache->c_hashtable = (struct list_head*) kmalloc ( DCACHE_HASHTABLE_SIZE * sizeof(struct list_head) );
    for ( i = 0; i < DCACHE_HASHTABLE_SIZE; i++ )
        INIT_LIST_HEAD(dcache->c_hashtable + i);
    dcache->c_op = (&(dentry_cache_operations));
    //初始化icache
    icache = (struct cache*) kmalloc(sizeof(struct cache));
     if(icache == 0)
    {
        kfree(icache);
        return err;
    }
    icache->cache_size = 0;
    icache->cache_capacity = ICACHE_CAPACITY;
    icache->cache_tablesize = ICACHE_HASHTABLE_SIZE;
    INIT_LIST_HEAD(dcache->c_lru);
    icache->c_hashtable = (struct list_head*) kmalloc ( ICACHE_HASHTABLE_SIZE * sizeof(struct list_head) );
    for ( i = 0; i < ICACHE_HASHTABLE_SIZE; i++ )
        INIT_LIST_HEAD(icache->c_hashtable + i);
    icache->c_op = (&(inode_cache_operations));
    //初始化pcache
    pcache = (struct cache*) kmalloc(sizeof(struct cache));
     if(pcache == 0)
    {
        kfree(pcache);
        return err;
    }
    pcache->cache_size = 0;
    pcache->cache_capacity = PCACHE_CAPACITY;
    pcache->cache_tablesize = PCACHE_HASHTABLE_SIZE;
    INIT_LIST_HEAD(pcache->c_lru);
    pcache->c_hashtable = (struct list_head*) kmalloc ( PCACHE_HASHTABLE_SIZE * sizeof(struct list_head) );
    for ( i = 0; i < PCACHE_HASHTABLE_SIZE; i++ )
        INIT_LIST_HEAD(pcache->c_hashtable + i);
    pcache->c_op = (&(page_cache_operations));
    return 0;
}

u8 cache_is_full(struct cache * this)
{
    if(this->cache_size == this->cache_capacity)
        return 1;
    else
        return 0;
}

void release_dentry(struct dentry *dentry){
    kfree(dentry);
}

void release_inode(struct inode *inode){

    kfree(inode);
}

void release_page(struct vfs_page* page){
    kfree(page->page_data);
    kfree(page);
}