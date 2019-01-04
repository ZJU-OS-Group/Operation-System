#ifndef _ZJUNIX_VFS_EXT_3_H
#define _ZJUNIX_VFS_EXT_3_H
#include <zjunix/vfs/vfs.h>

#define 				EXT3_N_BLOCKS 						15
#define                 EXT3_GROUP_DESC_BYTE                32				//组描述符的字节数
#define                 EXT3_MAX_NAME_LEN                   255
#define                 EXT3_BOOT_SECTOR_SIZE               2
#define                 EXT3_SUPER_SECTOR_SIZE              2
#define                 EXT3_BLOCK_SIZE_BASE                1024
#define                 EXT3_ROOT_INO                       2
#define                 EXT3_FIRST_MAP_INDEX                12
#define                 EXT3_SECOND_MAP_INDEX               13
#define                 EXT3_THIRD_MAP_INDEX                14
#define                 EXT3_BLOCK_ADDR_SHIFT               2  //每个数据块两个字节

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


struct ext3_inode {
    u16	                i_mode;                             // 文件模式
    u16	                i_uid;                              // UID的低16位
    u32	                i_size;                             // 文件大小（字节数）
    u32	                i_atime;                            // 最近访问时间
    u32	                i_ctime;                            // 创建时间
    u32	                i_mtime;                            // 修改时间
    u32	                i_dtime;                            // 删除时间
    u16	                i_gid;                              // GID的低16位
    u16	                i_links_count;                      // 链接计数
    u32	                i_blocks;                           // 关联的块数
    u32	                i_flags;                            // 打开的标记
    u32                 osd1;                               // 与操作系统相关1
    u32	                i_block[EXT3_N_BLOCKS];             // 存放所有相关的块地址
    u32	                i_generation;                       // （NFS用）文件的版本
    u32	                i_file_acl;                         // 文件的ACL
    u32	                i_dir_acl;                          // 目录的ACL
    u32	                i_faddr;                            // 碎片地址
    u32                 osd2[3];                            // 与操作系统相关2
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

u32 ext3_writepage(struct vfs_page * page) ;

u32 ext3_readpage(struct vfs_page * page);

//通过相对文件页号计算相对物理页号
u32 ext3_bmap(struct inode* inode, u32 target_page);

u32 ext3_read (struct file *, char* , u32,  long long *);
/* 由系统调用write()调用它 */
u32 ext3_write (struct file *, const char* , u32, long long *);
/* 返回目录列表中的下一个目录，调由系统调用readdir()用它 */
u32 ext3_readdir (struct file *, struct getdent *);
/* 创建一个新的文件对象,并将它和相应的索引节点对象关联起来 */
u32 ext3_open (struct inode *, struct file *);
/* 当已打开文件的引用计数减少时,VFS调用该函数，将修改后的内容写回磁盘 */
u32 ext3_flush (struct file *);
#endif