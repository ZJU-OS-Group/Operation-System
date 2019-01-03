
#include <zjunix/vfs/vfs.h>

void pcache_write_back(void* obj)
{
        struct vfs_page* tempPage = (struct vfs_page*) obj;
        tempPage->p_address_space->a_op->writepage(tempPage);
}
void pcache_LRU(struct cache *this) {
        struct  list_head* replaced_page;
        struct vfs_page*  _replaced_page;
        replaced_page = this->c_lru->prev;
        _replaced_page = container_of(replaced_page, struct vfs_page, p_lru);
        if(_replaced_page->page_state & P_DIRTY)
        {

        }

}
struct vfs_page* pcache_add(struct cache* this, void* obj)
{
        if(cache_is_full(this))//缓存已满
                pcache_LRU(this); //释放一个页面
}
void* pcache_look_up(struct cache* this, struct condition* conditions)
{

}


