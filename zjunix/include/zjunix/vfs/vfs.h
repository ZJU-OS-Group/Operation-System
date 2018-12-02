#ifndef _ZJUNIX_VFS_VFS_H
#define _ZJUNIX_VFS_VFS_H
#include <zjunix/type.h>
#include <zjunix/list.h>

// 文件打开方式，即open函数的入参flags。打开文件时用，vfs_open第二个参数
#define O_RDONLY	                            0x0000                   // readonly 只读
#define O_WRONLY	                            0x0001                   // writeonly 只写
#define O_RDWR		                            0x0002                   // read&write 可读可写

// 数据结构定义
// 打开的文件
struct file {
    u32                                 f_pos;                  // 文件当前的读写位置
	struct list_head	                f_list;                 // 用于通用文件对象链表的指针
	struct dentry		                *f_dentry;              // 与文件相关的目录项对象
	struct vfsmount                     *f_vfsmnt;              // 含有该文件的已安装文件系统
	struct file_operations	            *f_op;                  // 指向文件操作表的指针
	u32 		                        f_flags;                // 当打开文件时所指定的标志
	u32			                        f_mode;                 // 进程的访问方式
	struct address_space	            *f_mapping;             // 指向文件地址空间对象的指针
};


// 函数声明

// for user command
u32 vfs_cat(const u8 *);
u32 vfs_mkdir(const u8 *);
u32 vfs_rm(const u8 *);
u32 vfs_ls(const u8 *);
u32 vfs_cd(const u8 *);
u32 vfs_mv(const u8 *);

#endif