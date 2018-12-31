#include <zjunix/vfs/vfs.h>


// 打开操作中的主要函数，通过路径名得到相应的nameidata结构
// 通过path_work()轮流调用real_lookup()函数，再调用各文件系统自己的inode_op->lookup
// 得到给定路径名对应的dentry和vfsmount结构
u32 open_namei(const u8 *pathname, u32 flag, u32 mode, struct nameidata *nd)