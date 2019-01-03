#include <zjunix/vfs/vfs.h>

u32 vfs_read(struct file *file, char *buf, u32 count, u32 *pos) {
	u32 ret;

	if (!(file->f_mode & FMODE_READ))
		return -EBADF;
	if (!file->f_op || (!file->f_op->read))
        return -EINVAL;
        			
	ret = file->f_op->read(file, buf, count, pos);

	return ret;
}

// 虚拟文件系统写接口
u32 vfs_write(struct file *file, char *buf, u32 count, u32 *pos) {
	u32 ret;

	if (!(file->f_mode & FMODE_WRITE))
		return -EBADF;
	if (!file->f_op || (!file->f_op->write))
        return -EINVAL;
   	
	ret = file->f_op->write(file, buf, count, pos);	
    return ret;
}
//count是文件长度，pos是起始地址
// 通用读文件方法fat32 ext3均使用此方法
u32 generic_file_read(struct file * file, u8 * buf u32 count, u32 * pos)
{

	struct inode* file_inode;
	u32 startPageNo;
	u32 startPageCur;
    	u32 endPageNo;
	file_inode = file->f_dentry->d_inode
}
u32 generic_file_write(struct file * file, u8 * buf u32 count, u32 * pos)
{

}
u32 generic_file_flush(struct file *)
{

}