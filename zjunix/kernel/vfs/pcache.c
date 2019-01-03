#include <zjunix/vfs/vfs.h>
#include <zjunix/vfs/vfscache.h>

void pcache_write_back(void* obj)
{
        struct vfs_page* tempPage = (struct vfs_page*) obj;
        tempPage->p_address_space->a_op->writepage(tempPage);
}
unsigned long pcache_add(struct cache* this, void* obj)
{

}
unsigned long pcache_look_up(struct cache* this, struct condition* conditions)
{

}