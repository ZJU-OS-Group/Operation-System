#include <zjunix/vfs/vfs.h>
#define     ext3_MAX_NAME_LEN      255

struct ext3_super_block {
    u32 inodes_count;           //索引节点总数
    u32 blocks_count;           //块为单位的文件系统大小
    u32 free_inodes_count;      //空闲索引节点数量
    u32 free_blocks_count;      //空闲块数量
    u32 first_block;            //第一个使用的块号
    u32 log_block_size;         //块大小对2取对数的结果
    u16 inode_size;             //索引节点结构的大小
};

static struct super_operations ext3_super_ops = {

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
        .readdir = ext3_read_dir,
        //.write   = write
        //.flush =
        //.read =
};

struct dentry_operations ext3_dentry_operations = {

};

struct inode_operations ext3_inode_operations = {
        .create = ext3_create,
        .lookup = ext3_lookup,
        .link   = ext3_link
};




struct ext3_super_block_info {
    u8 first_inode;
    u8 inode_size;

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
    char   file_name[ext3_MAX_NAME_LEN];  //文件名
};

struct file_system_type ext3_fs_type = {
        .name = "ext3",
        .get_sb = ext3_get_super_block,
        .kill_sb = ext3_kill_super_block
};

struct ext3_inode {
    u16                 i_mode;  //文件类型和访问权限
    u16                 i_uid;   //拥有者标识符
    u32	                i_size;                             // 文件大小（字节数）
    u32	                i_blocks;                           // 关联的块数
    u32	                i_flags;                            // 打开的标记
    u32	                i_block[ext3_N_BLOCKS];             // 存放所有相关的块地址
};

struct ext3_group_description {
    u32	                block_bitmap;                       // 块位图所在块
    u32	                inode_bitmap;                       // inode位图所在块
    u32	                inode_table;                        // inode列表所在块
    u16	                free_blocks_count;                  // 空闲块数
    u16	                free_inodes_count;                  // 空闲节点数
    u16	                dirs_count;                         // 目录数
    u16	                pad;
    u32	                reserved[3];                        // 保留
};

struct ext3_base_information {
    u32                 base;                            // 启动块的基地址（绝对扇区地址，下同）
    u32                 first_sb_sect;                   // 第一个super_block的基地址
    u32                 first_gd_sect;                  // 第一个组描述符表的基地址
    union {
        u8                        *data;
        struct ext3_super_block   *attr;
    } super_block;                                                   // 超级块数据
};


struct ext3_super_block* ext3_init_super() {
    struct ext3_super_block ans = (struct ext3_super_block *)kmalloc(sizeof(struct ext3_super_block));
    if (ans == 0) return null;

    return 0;
}

struct ext3_base_information* ext3_init_base_information(u32 base){
    struct ext3_base_information* ans = (struct ext3_base_information *)kmalloc(sizeof(struct ext3_base_information));
    if (ans == 0) return null;
    ans->base = base;
    ans->first_sb_sect = base + EX3_BOOT_SECTOR_SIZE;
    ans->super_block =
    return ans;
}

void init_ext3(u32 base){

}

u32 ext3_create(){

}

struct dentry * ext3_lookup(){

}

u32 ext3_link(){

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

u32 ext3_statfs (struct dentry *, struct kstatfs *){

}
/* VFS调用该函数获取文件系统状态 */
u32 ext3_remount_fs (struct super_block *, u32 *, char *){

}
/* 指定新的安装选项重新安装文件系统时，VFS会调用该函数 */
