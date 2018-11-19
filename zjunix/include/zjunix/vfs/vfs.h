#ifndef _ZJUNIX_VFS_VFS_H
#define _ZJUNIX_VFS_VFS_H
#include <zjunix/type.h>
#include <zjunix/list.h>






// 函数声明

// for user command
u32 vfs_cat(const u8 *);
u32 vfs_mkdir(const u8 *);
u32 vfs_rm(const u8 *);
u32 vfs_ls(const u8 *);
u32 vfs_cd(const u8 *);
u32 vfs_mv(const u8 *);

#endif