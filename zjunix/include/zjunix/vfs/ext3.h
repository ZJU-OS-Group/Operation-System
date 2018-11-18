#ifndef _ZJUNIX_VFS_EXT_3_H
#define _ZJUNIX_VFS_EXT_3_H
#include <zjunix/vfs/vfs.h>

#define EXT3_NAME_LEN 255
#define EXT3_N_BLOCKS 15

struct ext3_base_info
{
    u32 ex_base;//启动块绝对扇区地址
    u32 ex_first_sb_sect;//第一个super block基地址
    u32 ex_first_gdt_sect;//第一个组描述符表基地址
    union{
        u8     *data;
        struct ext3_super *attr;
    }super_block;       //super block数据
}
//EXT3 文件系统的超级块
struct ext3_super{
    u32 inode_num;                    //文件系统中包含的i节点总数。
    u32 block_num;                    //文件系统中包含的总块数。
    u32 res_block_num;                    //保留块数，文件系统给自身保留的块数量。
    u32 free_block_num;                   //空闲块数，既当前文件系统的可用块数量。
    u32 free_inode_num;                    //空闲i节点数，当前文件系统的可用i节点数量。
    u32 first_data_block_no;                    //第一个数据块，既0号块组的起始起始块号。
    u32 block_size;                    //块大小描述值，块大小=2的N次方*1024字节，N为改参数。
    u32 slice_size;                    //段大小描述值，与块大小描述值相同。
    u32 blocks_per_group;                     //块组大小描述值，既每块组中的块数量。
    u32 slices_per_group;                    //每块组中包含的段数，与块组大小描述符相同。
    u32 inodes_per_group;                   //每块中包含i节点数。
}
//EXT3 文件系统的目录项
struct ext3_dir_entry{
    u32	                ino;                  // 文件的inode号
	u16                 rec_len;             // 目录项长度（字节）
    u8	                name_len;           // 名字长度（字节）
    u8                  file_type;         // 文件类型 0x01是文件 0x02是目录 0x07是符号链接
	char	            name[EXT3_NAME_LEN];   //名字，不知如何实现变长
}

// EXT3组描述符
struct ext3_group_desc {
	u32	block_bitmap; // 块位图所在块起始块号
	u32	inode_bitmap;// inode位图所在块起始块号
	u32	inode_table;   // inode列表所在块起始块号
	u16	free_blocks_count;  // 空闲块数
	u16	free_inodes_count;  // 空闲节点数
	u16	used_dirs_count;   // 目录数
	u16	pad;      // 填充
	u32	reserved[3];//未用
};

struct ext3_inode {
	u16 i_mode;  // 文件模式
	u16 i_uid_low;   // userid的低16位
	u32 i_size_low;  // 文件大小（字节数低32位）
	u32 i_atime; // 最近访问时间
	u32 i_ctime; // i节点变化时间（创建时间）
	u32 i_mtime; // 最后修改时间
	u32 i_dtime; // 删除时间
	u16 i_gid_low;   // groupid的低16位
	u16 i_links_count;// 链接计数
	u32 i_blocks; // 扇区数 关联的块数
	u32 i_flags; // 打开的标记
	u32 osd1;    // 与操作系统相关1(未用)
	u32 i_block[EXT3_N_BLOCKS]; // 存放所有相关的块地址
	u32 i_generation;  // (NFS用)文件的版本
	u32 i_file_acl; // 文件的ACL扩展属性块
	u32 i_dir_acl;  // 目录的ACL
    u32 i_size_high; //文件大小(字节数高32位)
    u32 i_faddr;   // 碎片地址(段地址)
    u8 i_fno;     //段号
    u8 i_fsize;     //段大小
    u32 i_uid_high;// userid的高16位
    u16 i_gid_high;   // groupid的高16位
    u32 osd2; // 与操作系统相关2 未用
};

//函数prototype







#endif