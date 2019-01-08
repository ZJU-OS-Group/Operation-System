#include <zjunix/vfs/vfs.h>

void pcache_write_back(void* obj)
{
        struct vfs_page* tempPage = (struct vfs_page*) obj;
        tempPage->p_address_space->a_op->writepage(tempPage);
}

void pcache_LRU(struct cache *this) {
        struct  list_head* replaced_page;
        struct vfs_page*  _replaced_page;
        replaced_page = this->c_lru.prev;
        _replaced_page = container_of(replaced_page, struct vfs_page, p_lru);
        if(_replaced_page->page_state & P_DIRTY)
        {
                this->c_op->write_back(_replaced_page);
        }
        //三个链表的地址管理
        list_del(&(_replaced_page->p_lru));
        list_del(&(_replaced_page->page_hashtable));
        list_del(&(_replaced_page->page_list));
        //释放页
        release_page(_replaced_page);
}

void pcache_add(struct cache* this, void* obj) {
        u32 hash_value;
        if(cache_is_full(this))//缓存已满
        {
                pcache_LRU(this); //释放LRU一个页
        }
        struct vfs_page* tempPage = (struct vfs_page*)obj;
        hash_value = getIntHash(tempPage->page_address, this->cache_tablesize);
        //哈希链表头部插入
        list_add(&(tempPage->page_hashtable), &(this->c_hashtable[hash_value]));
        //LRU头部插入
        list_add(&(this->c_lru), &(tempPage->p_lru));
        this->cache_size++;
}

void* pcache_look_up(struct cache* this, struct condition* conditions) {
        u8 found = 0;
        u32 page_address = *((u32*)(conditions->cond1));
        u32 hash_value;
        struct vfs_page* result;
        struct list_head* temp_list_node;
        struct list_head* start_list;
        hash_value = getIntHash(page_address, this->cache_tablesize);
        start_list = temp_list_node = &(this->c_hashtable[hash_value]);
        while(temp_list_node->next!= start_list)
        {
                temp_list_node = temp_list_node->next;
                struct vfs_page* temp = container_of(temp_list_node, struct vfs_page, page_hashtable);
                if(temp->page_address == page_address && conditions->cond2 == temp->p_address_space->a_host)
                {
                        result = temp;
                        found = 1;
                        break;
                }
        }
        if(found)
        {
                list_del(&(result->page_hashtable));
                list_del(&(result->p_lru));
                list_add(&(result->page_hashtable), start_list);
                list_add(&(result->p_lru), &(this->c_lru));
                return (void*)result;
        }

        return 0;
}


