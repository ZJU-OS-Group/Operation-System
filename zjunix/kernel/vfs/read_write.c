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
//返回读了多少字节
u32 generic_file_read(struct file * file, u8 * buf, u32 count, u32 * pos)
{

	struct inode* file_inode;
	u32 startPageNo;
	u32 startPageCur;
    u32 endPageNo;
	u32 endPageCur;
	u32 tempPageNo;
	u32 tempPageNoCur;
	u32 tempCur = 0;
	u32 realPageNo;
	u32 readCount = 0;
	file_inode = file->f_dentry->d_inode;
	//计算起始页号
	startPageNo = (*pos) / file_inode->i_block_size;
	startPageCur = (*pos) % file_inode->i_block_size;
	//看end页是否包含在内
	if((*pos) + count < file_inode->i_size)
	{
		endPageNo = ( (*pos) + count ) / file_inode->i_block_size;
        	endPageCur = ( (*pos) + count ) % file_inode->i_block_size;
	}
	else
	{
		endPageNo = file_inode->i_size / file_inode->i_block_size;
        	endPageCur = file_inode->i_size % file_inode->i_block_size;
	}
	//读取每一页
	for(tempPageNo = startPageNo;tempPageNo <= endPageNo;tempPageNo++)
	{
		u32 tempPage;
		realPageNo = file_inode->i_data.a_op->bmap(file_inode, tempPageNo);
		//在缓存pcache中查找，放入tempPage

		if(tempPage == 0) //不在pcache中
		{
			tempPage = (struct* )
		}
		u32 dest, src;
		if(tempPageNo == startPageNo)
		{
			dest = buf;
			src = tempPage->p_data + startPageCur;
			if(startPageNo == endPageNo)
			{
				readCount = endPageCur - startPageCur;	
			}
			else
			{
				readCount = file_inode->i_block_size - startPageCur;
			}
		}
		else if(tempPageNo == endPageNo)
		{
			readCount = endPageCur;
			dest = buf + tempCur;
			src = tempPage->p_data;		
		}
		else
		{
			readCount = file_inode->i_block_size;
			dest = buf + tempCur;
			src = tempPage->p_data;
		}
		kernel_memcpy(dest, src, readCount);
		tempCur += readCount;
		(*pos) += readCount;
	}

	file->f_pos = (*pos);
	return tempCur;
}
u32 generic_file_write(struct file * file, u8 * buf, u32 count, u32 * pos)
{
	struct inode* file_inode;
	u32 init_pos = (*pos);
	u32 startPageNo;
	u32 startPageCur;
    u32 endPageNo;
	u32 endPageCur;
	u32 tempPageNo;
	u32 tempPageNoCur;
	u32 tempCur = 0;
	u32 realPageNo;
	u32 writeCount = 0;
	file_inode = file->f_dentry->d_inode;
	//计算起始页号
	startPageNo = (*pos) / file_inode->i_block_size;
	startPageCur = (*pos) % file_inode->i_block_size;
	endPageNo = ((*pos) + count) / file_inode->i_block_size;
	endPageCur = ((*pos) + count) % file_inode->i_block_size;

	for(tempPageNo = startPageNo;tempPageNo <= endPageNo;tempPageNo++)
	{
		u32 tempPage;
		realPageNo = file_inode->i_data.a_op->bmap(file_inode, tempPageNo);
		//在缓存pcache中查找，放入tempPage
		if(tempPage == 0) //不在pcache中
		{
			
		}
		u32 dest, src;
		if(tempPageNo == startPageNo)
		{
			src = buf;
			dest = tempPage->p_data + startPageCur;
			if(startPageNo == endPageNo)
			{
				writeCount = endPageCur - startPageCur;	
			}
			else
			{
				writeCount = file_inode->i_block_size - startPageCur;
			}
		}
		else if(tempPageNo == endPageNo)
		{
			writeCount = endPageCur;
			src = buf + tempCur;
			dest = tempPage->p_data;		
		}
		else
		{
			writeCount = file_inode->i_block_size;
			src = buf + tempCur;
			dest = tempPage->p_data;
		}
		kernel_memcpy(dest, src, writeCount);
		tempCur += writeCount;
		(*pos) += writeCount;
	}

	//改变文件大小
	if(init_pos + count > file_inode->i_fsize)
	{
		file_inode->i_fsize = init_pos + count;
		file_inode->i_sb->s_op->write_inode(file_inode, file->dentry->d_parent);
	}

	file->f_pos = (*pos);
	return tempCur;
}

u32 generic_file_flush(struct file * file)
{

}