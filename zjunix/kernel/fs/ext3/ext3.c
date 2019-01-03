#include <zjunix/vfs/vfs.h>
#include <zjunix/vfs/ext3.h>
#include "../../../usr/myvi.h"

#define     EXT3_MAX_NAME_LEN               255
#define     EXT3_BOOT_SECTOR_SIZE           2
#define     EXT3_SUPER_SECTOR_SIZE          2
#define     EXT3_BLOCK_SIZE_BASE            1024
#define     EXT3_N_BLOCKS                   15
#define     EXT3_ROOT_INO                   2

struct ext3_super_block {
    u32                 inode_num;                          // inode数
    u32                 block_num;                          // 块数
    u32                 res_block_num;                      // 保留块数
    u32                 free_block_num;                     // 空闲块数
    u32                 free_inode_num;                     // 空闲inode数
    u32                 first_data_block_no;                // 第一个数据块号
    u32                 block_size;                         // 块长度（从1K开始的移位数）
    u32                 slice_size;                         // 片长度（从1K开始的移位数）
    u32                 blocks_per_group;                   // 每组块数
    u32                 slices_per_group;                   // 每组片数
    u32                 inodes_per_group;                   // 每组indoes数
    u32                 install_time;                       // 安装时间
    u32                 last_write_in;                      // 最后写入时间
    u16                 install_count;                      // 安装计数
    u16                 max_install_count;                  // 最大安装数
    u16                 magic;                              // 魔数
    u16                 state;                              // 状态
    u16                 err_action;                         // 出错动作
    u16                 edition_change_mark;                // 改版标志
    u32                 last_check;                         // 最后检测时间
    u32                 max_check_interval;                 // 最大检测间隔
    u32                 operating_system;                   // 操作系统
    u32                 edition_mark;                       // 版本标志
    u16                 uid;                                // uid
    u16                 gid;                                // pid
    u32                 first_inode;                        // 第一个非保留的inode
    u16                 inode_size;                         // inode的大小
    u16                 block_group;                        // 超级块的组号
    u32                 feature_compat;                     // 能够兼容的特性
    u32                 feature_incompat;                   // 不能够兼容的特性
    u32                 feature_rocompat;                   // 只读的兼容特性
    u8                  uuid[16];                           //
    char                volume_name[16];                    //
    char                last_mounted[64];                   //最后挂载的目录
    u32                 usage_bitmap;                       //用于压缩
    u8                  prealloc_blocks;                    //
    u8                  prealloc_dir_blocks;                //
    u16                 reserved_gdt_blocks;                //
    u8                  journal_uuid[16];                   //日志超级块的uuid
    u32                 journal_inum;                       //日志文件的inode number
    u32                 journal_dev;                        //日志文件的设备号
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
        //.write   = write
        //.flush =
        //.read =
};

struct dentry_operations ext3_dentry_operations = {

};

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




struct ext3_super_block_info {

};

enum {
    ext3_UNKNOWN = 0,
    ext3_NORMAL = 1,
    ext3_DIR = 2,
    ext3_LINK = 7
};

struct ext3_dir_entry{
    u32     inode_num;          //索引节点号
    u16     entry_len;          //目录项长度
    u8      file_name_len;      //文件名长度
    u8      file_type;          //文件类型
    char    file_name[EXT3_BOOT_SECTOR_SIZE];  //文件名
};

struct file_system_type ext3_fs_type = {
        .name = "ext3",
};

struct ext3_inode {
    u16                 i_mode;  //文件类型和访问权限
    u16                 i_uid;   //拥有者标识符
    u32	                i_size;                             // 文件大小（字节数）
    u32	                i_blocks;                           // 关联的块数
    u32	                i_flags;                            // 打开的标记
    u32	                i_block[EXT3_N_BLOCKS];             // 存放所有相关的块地址
};

struct ext3_group_description {
    u32	                block_bitmap;                       // 块位图所在块
    u32	                inode_bitmap;                       // inode位图所在块
    u32	                inode_table;                        // inode列表所在块
    u16	                free_blocks_count;                  // 空闲块数
    u16	                free_inodes_count;                  // 空闲节点数
    u16	                used_dirs_count;                    // 目录数
    u16	                pad;                                // 以下均为保留
    u32	                reserved[3];
};

struct ext3_base_information {
    u32                 base;                            // 启动块的基地址（绝对扇区地址，下同）
    u32                 first_sb_sect;                   // 第一个super_block的基地址
    u32                 first_gdt_sect;                  // 第一个组描述符表的基地址
    union {
        u8                        *fill;
        struct ext3_super_block   *content;
    } super_block;                                                   // 超级块数据
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
    //ans->i_data.a_op = todo
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

u32 ext3_fill_inode(struct inode *inode) {
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
