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

//dcache缓存操作函数
struct cache_operations dentry_cache_operations = {
    .look_up    = dcache_look_up,
    .add        = dcache_add,
    .is_full    = cache_is_full,
};
//pcache缓存操作函数
struct cache_operations page_cache_operations = {
    .look_up    = pcache_look_up,
    .add        = pcache_add,
    .is_full    = cache_is_full,
    .write_back = pcache_write_back,
};

u32 init_cache()
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
    INIT_LIST_HEAD(&(dcache->c_lru));
    dcache->c_hashtable = (struct list_head*) kmalloc ( DCACHE_HASHTABLE_SIZE * sizeof(struct list_head) );
    for ( i = 0; i < DCACHE_HASHTABLE_SIZE; i++ )
        INIT_LIST_HEAD(dcache->c_hashtable + i);
    dcache->c_op = (&(dentry_cache_operations));
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
    INIT_LIST_HEAD(&(pcache->c_lru));
    pcache->c_hashtable = (struct list_head*) kmalloc ( PCACHE_HASHTABLE_SIZE * sizeof(struct list_head) );
    for ( i = 0; i < PCACHE_HASHTABLE_SIZE; i++ )
        INIT_LIST_HEAD(pcache->c_hashtable + i);
    pcache->c_op = (&(page_cache_operations));
    return 0;
}

u32 cache_is_full(struct cache * this)
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

u32 getIntHash(long key, long size)
{
    return (u32) (key & (size + 1));
}