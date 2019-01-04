#include <zjunix/vfs/vfs.h>
#include <zjunix/vfs/ext3.h>
#include <zjunix/utils.h>
#include "../../../usr/myvi.h"

struct address_space_operations ext3_address_space_operations = {  //地址空间操作
        .writepage  = ext3_writepage,
        .readpage   = ext3_readpage,
        .bmap       = ext3_bmap,
};

struct file_system_type ext3_fs_type = {
        .name = "ext3",
};

struct super_operations ext3_super_ops = {

};
// struct inode *(*alloc_inode)(struct super_block *sb);       /* 创建和初始化一个索引节点对象 */
//void (*destroy_inode)(struct inode *);                      /* 释放给定的索引节点 */
//
//void (*dirty_inode) (struct inode *);                       /* VFS在索引节点被修改时会调用这个函数 */
//u32 (*write_inode) (struct inode *, u32);                   /* 将索引节点写入磁盘，wait表示写操作是否需要同步 */
//void (*drop_inode) (struct inode *);                        /* 最后一个指向索引节点的引用被删除后，VFS会调用这个函数 */
//void (*delete_inode) (struct inode *);                      /* 从磁盘上删除指定的索引节点 */
//void (*put_super) (struct super_block *);                   /* 卸载文件系统时由VFS调用，用来释放超级块 */
//void (*write_super) (struct super_block *);                 /* 用给定的超级块更新磁盘上的超级块 */
//u32 (*sync_fs)(struct super_block *sb, u32 wait);           /* 使文件系统中的数据与磁盘上的数据同步 */
//u32 (*statfs) (struct dentry *, struct kstatfs *);          /* VFS调用该函数获取文件系统状态 */
//u32 (*remount_fs) (struct super_block *, u32 *, char *);    /* 指定新的安装选项重新安装文件系统时，VFS会调用该函数 */
//void (*clear_inode) (struct inode *);                       /* VFS调用该函数释放索引节点，并清空包含相关数据的所有页面 */
//void (*umount_begin) (struct super_block *);

struct file_operations ext3_file_operations = {
        .write   = ext3_write,
        .read = ext3_read,
        .readdir = ext3_readdir,
        .open = ext3_open
};

/*long (*read) (struct file *, char* , u32,  long long *);
*//* 由系统调用write()调用它 *//*
long (*write) (struct file *, const char* , u32, long long *);
*//* 返回目录列表中的下一个目录，由系统调用readdir()调用它 *//*
u32 (*readdir) (struct file *, struct getdent *);
*//* 创建一个新的文件对象,并将它和相应的索引节点对象关联起来 *//*
int (*open) (struct inode *, struct file *);*/

struct dentry_operations ext3_dentry_operations = {

};


//通过相对文件页号计算相对物理页号， inode是该页所在的inode
u32 ext3_bmap(struct inode* inode, u32 target_page){
    // 0-11：数据块
    // 12：一级索引
    // 13：二级索引
    // 14：三级索引
    u32 *pageTable = inode->i_data.a_page;
    u32 entry_num = inode->i_block_size >> EXT3_BLOCK_ADDR_SHIFT;
    if (target_page < EXT3_FIRST_MAP_INDEX) {
        return pageTable[target_page];  //因为初始化的时候就已经把所有能直接访问到的数据块都添加到页缓存里了
    }
    if (target_page < EXT3_FIRST_MAP_INDEX + entry_num) {
        u8 *index_block = (u8 *)kmalloc(inode->i_block_size * sizeof(u8));
        if (index_block == 0) return -ENOMEM;
        u32 err = read_block(index_block,pageTable[EXT3_FIRST_MAP_INDEX],inode->i_block_size >> SECTOR_LOG_SIZE);
        if (err) return -EIO;
        //index_block块里的都是地址
        u32 index = (target_page - EXT3_FIRST_MAP_INDEX) << EXT3_BLOCK_ADDR_SHIFT;
        u32 actual_addr = get_u32(index_block + index);
        kfree(index_block);
        return actual_addr;
    }
    if (target_page < EXT3_FIRST_MAP_INDEX + (entry_num + 1) * entry_num) {
        u8 *index_block = (u8 *)kmalloc(inode->i_block_size * sizeof(u8));
        if (index_block == 0) return -ENOMEM;
        u32 err = read_block(index_block,pageTable[EXT3_SECOND_MAP_INDEX],inode->i_block_size >> SECTOR_LOG_SIZE);
        //读取这个二级索引块里的所有地址
        if (err) return -EIO;
        //下面需要寻找一级索引块地址
        u32 pre_index = (target_page - EXT3_FIRST_MAP_INDEX - entry_num);
        u32 index = (pre_index / entry_num) << EXT3_BLOCK_ADDR_SHIFT;
        //寻找到一级索引块地址以后读取对应的块
        u32 index1_addr = get_u32(index_block + index);
        err = read_block(index_block,index1_addr,inode->i_block_size >> SECTOR_LOG_SIZE);
        index = (pre_index % entry_num) << EXT3_BLOCK_ADDR_SHIFT;
        u32 actual_addr = get_u32(index_block + index);
        kfree(index_block);
        return actual_addr;
    }
    if (target_page < EXT3_FIRST_MAP_INDEX + entry_num * (entry_num * (entry_num + 1) + 1)) {
        u8 *index_block = (u8 *)kmalloc(inode->i_block_size * sizeof(u8));
        if (index_block == 0) return -ENOMEM;
        u32 err = read_block(index_block,pageTable[EXT3_THIRD_MAP_INDEX],inode->i_block_size >> SECTOR_LOG_SIZE);
        //读取这个三级索引块里的所有地址
        if (err) return -EIO;
        //下面需要寻找二级索引块地址
        u32 pre_index = (target_page - EXT3_FIRST_MAP_INDEX - entry_num * (entry_num + 1));
        u32 index = (pre_index / (entry_num * entry_num)) << EXT3_BLOCK_ADDR_SHIFT;
        //寻找到二级索引块地址以后读取对应的块
        u32 index2_addr = get_u32(index_block + index);
        err = read_block(index_block,index2_addr,inode->i_block_size >> SECTOR_LOG_SIZE);
        if (err) return -EIO;
        index = (pre_index % (entry_num * entry_num) / entry_num) << EXT3_BLOCK_ADDR_SHIFT;   //Attention: 助教原版的代码这里有bug
        u32 index1_addr = get_u32(index_block + index);
        err = read_block(index_block,index1_addr,inode->i_block_size >> SECTOR_LOG_SIZE);
        if (err) return -EIO;
        index = (pre_index % entry_num) << EXT3_BLOCK_ADDR_SHIFT;
        u32 actual_addr = get_u32(index_block + index);
        kfree(index_block);
        return actual_addr;
    }
    return -EFAULT;
}

u32 ext3_writepage(struct vfs_page * page) {
    struct inode * target_inode = page->p_address_space->a_host;  //获得对应的inode
    u32 sector_num = target_inode->i_block_size >> SECTOR_BYTE_SIZE; //由于一块大小和一页大小相等，所以需要写出的扇区是这么大
    u32 base_addr = ((struct ext3_information *) target_inode->i_sb->s_fs_info)->base;  //计算文件系统基地址
    u32 target_addr = base_addr + page->page_address * (target_inode->i_block_size >> SECTOR_BYTE_SIZE); //加上页地址
    u32 err = write_block(page->page_data,target_addr,sector_num);      //向目标地址写目标数量个扇区
    if (err) return -EIO;
    return 0;
}

u32 ext3_readpage(struct vfs_page * page){
    struct inode * source_inode = page->p_address_space->a_host;
    u32 sector_num = source_inode -> i_block_size >> SECTOR_BYTE_SIZE;
    u32 base_addr = ((struct ext3_information *) source_inode->i_sb->s_fs_info)->base;  //计算文件系统基地址
    u32 source_addr = base_addr + page->page_address * (source_inode->i_block_size >> SECTOR_BYTE_SIZE); //加上页地址
    page->page_data = (u8*)kmalloc(sizeof(u8) * source_inode->i_block_size);
    if (page->page_data == 0) return -ENOMEM;
    kernel_memset(page->page_data,0,sizeof(u8) * source_inode->i_block_size);
    u32 err = read_block(page->page_data,source_addr,sector_num);
    if (err) return -EIO;
    return 0;
}




u32 ext3_create(struct inode *pInode, struct dentry *pDentry, int i, struct nameidata *pNameidata){

}

struct dentry * ext3_lookup(struct inode *pInode, struct dentry *pDentry, struct nameidata *pNameidata){

}

u32 ext3_link(struct dentry *pDentry, struct inode *pInode, struct dentry *pDentry1){

}

struct inode_operations ext3_inode_operations = {
        .create = ext3_create,
        .lookup = ext3_lookup,
        .link   = ext3_link
};


u32 ext3_init_super(struct ext3_base_information* information) {
    struct super_block* ans = (struct super_block *)kmalloc(sizeof(struct super_block));
    if (ans == 0) return -ENOMEM;
    ans->s_dirt = S_CLEAR;  //标记当前超级块是否被写脏
    ans->s_root = 0;  //留待下一步构造根目录
    ans->s_op = (&ext3_super_ops);
    ans->s_block_size = EXT3_BLOCK_SIZE_BASE << information->super_block.content->block_size;
    ans->s_fs_info = (void*) information;
    ans->s_type = (&ext3_fs_type);
    return (u32) ans;
}

u32 ext3_init_base_information(u32 base){
    struct ext3_base_information* ans = (struct ext3_base_information *)kmalloc(sizeof(struct ext3_base_information));
    if (ans == 0) return -ENOMEM;
    ans->base = base;
    ans->first_sb_sect = base + EXT3_BOOT_SECTOR_SIZE;  //跨过引导区数据
    ans->super_block.fill = (u8*) kmalloc(sizeof(u8) * EXT3_SUPER_SECTOR_SIZE * SECTOR_BYTE_SIZE);  //初始化super_block区域
    if (ans->super_block.fill == 0) return -ENOMEM;
    u32 err = read_block(ans->super_block.fill,ans->first_sb_sect,EXT3_SUPER_SECTOR_SIZE);  //从指定位置开始读取super_block
    if (err) return -EIO;
    //SECTOR是物理的， BASE_BLOCK_SIZE是逻辑的
    u32 ratio = EXT3_BLOCK_SIZE_BASE << ans->super_block.content->block_size >> SECTOR_LOG_SIZE;   //一个block里放多少个sector
    if (ratio <= 2) ans->first_gdt_sect = ans->first_sb_sect + EXT3_SUPER_SECTOR_SIZE;  //如果只能放2个以内的sector，那么gb和sb将紧密排列
    else ans->first_gdt_sect = base + ratio;  //否则直接跳过第一个块
    return (u32) ans;
}


u32 ext3_init_dir_entry(struct super_block* super_block) {
    struct dentry* ans = (struct dentry*)kmalloc(sizeof(struct dir_entry));
    if (ans == 0) return -ENOMEM;
    ans->d_name.name = "/";
    ans->d_mounted = 0;
    ans->d_name.len = 1;
    ans->d_count = 1;
    ans->d_parent = 0;
    ans->d_op = (&ext3_dentry_operations);
    ans->d_inode = 0;
    ans->d_sb = super_block;
    ans->d_parent = 0;
    INIT_LIST_HEAD(&(ans->d_alias));
    INIT_LIST_HEAD(&(ans->d_lru));
    INIT_LIST_HEAD(&(ans->d_subdirs));
    return (u32) ans;
}

u32 ext3_init_inode(struct super_block* super_block) {
    struct inode* ans = (struct inode*) kmalloc(sizeof(struct inode));
    if (ans == 0) return -ENOMEM;
    ans->i_op = &ext3_inode_operations;
    ans->i_ino = EXT3_ROOT_INO;
    ans->i_fop = &ext3_file_operations;
    ans->i_count = 1;
    ans->i_sb = super_block;
    ans->i_block_size = super_block->s_block_size;
    INIT_LIST_HEAD(&(ans->i_list)); //初始化索引节点链表
    INIT_LIST_HEAD(&(ans->i_dentry));  //初始化目录项链表
    INIT_LIST_HEAD(&(ans->i_hash));  //初始化散列表
    //todo
    switch (ans->i_block_size){
        case 1024: ans->i_block_size_bit = 10; break;
        case 2048: ans->i_block_size_bit = 11; break;
        case 4096: ans->i_block_size_bit = 12; break;
        case 8192: ans->i_block_size_bit = 13; break;
        default: return -EFAULT;
    }
    ans->i_data.a_host = ans;
    ans->i_data.a_pagesize = super_block->s_block_size;
    ans->i_data.a_op = (&ext3_address_space_operations);
    INIT_LIST_HEAD(&(ans->i_data.a_cache));
    return (u32) ans;
}

u32 get_inode_table_sect(struct inode* inode) {  //通过inode寻找inode_table的地址
    u8 target_buffer[SECTOR_BYTE_SIZE];  //存储目标组描述符内容
    struct ext3_base_information* base_information = (struct ext3_base_information*) inode->i_sb->s_fs_info;
    //获取当前inode内的文件系统信息
    u32 ext3_base = base_information->base;
    //获得ext3的基址地址
    u32 block_size = inode->i_block_size;
    //获得ext3的块大小
    u32 inodes_per_group = base_information->super_block.content->inodes_per_group;
    //获得ext3的每组内的inode数目
    if (inode->i_ino > base_information->super_block.content->inode_num) return -EFAULT;
    //如果inode编号超出总数量则抛出异常
    u32 group_num = (u32) ((inode->i_ino - 1) / inodes_per_group);
    //获取根据当前节点的节点号获取节点的组号
    //下一步目标：找到这一个inode对应的组描述符表
    u32 sect = base_information->first_gdt_sect + group_num / (SECTOR_BYTE_SIZE /  EXT3_GROUP_DESC_BYTE);
    //后面部分的算式求一个扇区有多少个组，用组号除以该数据得到inode所在组的组描述符的扇区位置
    u32 offset = group_num % (SECTOR_BYTE_SIZE /  EXT3_GROUP_DESC_BYTE);
    //计算当前扇区里第几个组是inode所在的组
    u32 err = read_block(target_buffer,sect,1);  //组标识符的全部信息都保存在target_buffer里
    if (err) return 0;
    u32 group_block_num = get_u32(target_buffer + offset * EXT3_GROUP_DESC_BYTE); //获取组标识符，读取块位图所在块编号
    u32 group_sector_base = base_information->base + group_block_num * (inode->i_block_size >> SECTOR_BYTE_SIZE);
    //定位到块位图所在块的起始扇区位置
    u32 group_inode_table_base = group_sector_base + 2 * (inode->i_block_size >> SECTOR_BYTE_SIZE);
    //往后面再移动两个块，直接定位到inode_table上
    return group_inode_table_base;
}

u32 ext3_fill_inode(struct inode *inode) {  //从硬件获得真实的inode信息并填充到vfs块内
    u32 i;  //For loop
    u8 target_buffer[SECTOR_BYTE_SIZE];
    struct ext3_base_information *ext3_base_information = inode->i_sb->s_fs_info;
    u32 inode_size = ext3_base_information->super_block.content->inode_size;
    u32 inode_table_base = get_inode_table_sect(inode);
    u32 inner_index = (u32) ((inode->i_ino - 1) % ext3_base_information->super_block.content->inodes_per_group);
    //求该inode在组内的序号
    u32 offset_sect = inner_index /  (SECTOR_BYTE_SIZE / inode_size);
    //求组内扇区偏移量：计算方式：下标*大小/扇区大小，之所以用两个除法是为了能够避免每个SECTOR里不能刚好容纳若干inode的情况
    u32 inode_sect = inode_table_base + offset_sect;
    u32 err = read_block(target_buffer,inode_sect,1);
    if (err) return -EIO;
    u32 inode_sect_offset = inner_index % (SECTOR_BYTE_SIZE / inode_size));
    // 求inode在扇区内的偏移量
    struct ext3_inode * target_inode = (struct ext3_inode*) (target_buffer + inode_sect_offset * inode_size);
    //计算该inode在指定扇区内的地址
    inode->i_blocks = target_inode->i_blocks;
    inode->i_size = target_inode->i_size;
    inode->i_atime = target_inode->i_atime;
    inode->i_ctime = target_inode->i_ctime;
    inode->i_dtime = target_inode->i_dtime;
    inode->i_mtime = target_inode->i_mtime;
    inode->i_uid = target_inode->i_uid;
    inode->i_gid = target_inode->i_gid;
    //填充文件页到逻辑页的映射表
    inode->i_data.a_page = (u32 *) kmalloc(EXT3_N_BLOCKS*sizeof(u32));
    if (inode->i_data.a_page == 0) return -ENOMEM;
    for (i = 0; i < EXT3_N_BLOCKS; i++)
        inode->i_data.a_page[i] = target_inode->i_block[i];
    //拷贝数据块
    return 0;
}

void init_ext3(u32 base){
    u32 base_information_pointer = ext3_init_base_information(base);  //读取ext3基本信息
    if (base_information_pointer < 0) goto err;

    u32 super_block_pointer = ext3_init_super(
            (struct ext3_base_information *) base_information_pointer);         //初始化超级块
    if (super_block_pointer < 0) goto err;
    struct super_block* super_block = (struct super_block *) super_block_pointer;

    u32 root_dentry_pointer = ext3_init_dir_entry(super_block);  //初始化目录项
    if (root_dentry_pointer < 0) goto err;
    super_block->s_root = (struct dentry *) root_dentry_pointer;

    u32 root_inode_pointer = ext3_init_inode(super_block);      //初始化索引节点
    if (root_inode_pointer < 0) goto err;
    struct inode* root_inode = (struct inode *) root_inode_pointer;

    u32 result = ext3_fill_inode(root_inode);                   //填充索引节点
    if (result < 0) goto err;

    u32 i; //Loop
    u32 target_location;
    struct vfs_page* current_page;
    //获取根目录数据
    for (int i = 0; i < root_inode->i_blocks; ++i) {
        target_location = root_inode->i_data.a_op->bmap(root_inode, i);
        //计算第i个块的物理地址
        if (target_location < 0) goto err;
        current_page = (struct vfs_page *)kmalloc(sizeof(struct vfs_page));
        if (current_page == 0) goto err;
        current_page->page_state = P_CLEAR;
        current_page->page_address = target_location;
        current_page->p_address_space = &(root_inode->i_data);
        INIT_LIST_HEAD(current_page->p_lru);
        INIT_LIST_HEAD(current_page->page_hashtable);
        INIT_LIST_HEAD(current_page->page_list);
        u32 err = root_inode->i_data.a_op->readpage(current_page);
        //读取当前页
        if (err < 0) {
            release_page(current_page);
            goto err;
        }


    }
    err: {} //pass

}

void ext3_unlink(){

}

void ext3_mkdir(){

}

void ext3_rmdir(){

}

void ext3_rename(){

}

struct inode ext3_read_inode(){

}

void ext3_read_dir(){

}

void ext3_write_inode(){

}

void ext3_put_inode(){

}

void ext3_delete_inode(){

}

void ext3_put_super(){

}

void ext3_write_super(){

}

u32 ext3_statfs (struct dentry *a, struct kstatfs *b){

}
/* VFS调用该函数获取文件系统状态 */
u32 ext3_remount_fs (struct super_block* a, u32 * b, char *c){

}


/* 指定新的安装选项重新安装文件系统时，VFS会调用该函数 */
