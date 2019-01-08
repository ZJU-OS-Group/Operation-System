#include <zjunix/vfs/vfs.h>

extern struct cache * pcache;
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
	u32 err;
	u32 startPageNo, startPageCur;
    u32 endPageNo, endPageCur;
	u32 tempPageNo, tempPageNoCur, tempCur = 0;
	u32 realPageNo;
	u32 readCount = 0;
	struct inode* file_inode;
	struct vfs_page* tempPage;
	struct condition conditions;

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
		realPageNo = file_inode->i_data.a_op->bmap(file_inode, tempPageNo);
		//在缓存pcache中查找，放入tempPage
		conditions.cond1 = &realPageNo;
		conditions.cond2 = file_inode;

		tempPage = pcache->c_op->look_up(pcache, &conditions);
		if(tempPage == 0) //不在pcache中
		{
			//新调页
			tempPage = (struct vfs_page*) kmalloc(sizeof(struct vfs_page));
			tempPage->page_state = P_CLEAR;
			tempPage->page_address = realPageNo;
			tempPage->p_address_space = &(file_inode->i_data);
			INIT_LIST_HEAD(&(tempPage->page_hashtable));
			INIT_LIST_HEAD(&(tempPage->page_list));
			INIT_LIST_HEAD(&(tempPage->p_lru));

			err = file_inode->i_data.a_op->readpage(tempPage);
			if(err)
			{
				err = -EIO;
				return err;
			}

			pcache->c_op->add(pcache, tempPage);
			list_add(&(tempPage->page_list), &(file_inode->i_data.a_cache));
		}
		u8* dest;
		u8* src;
		if(tempPageNo == startPageNo)
		{
			dest = buf;
			src = tempPage->page_data + startPageCur;
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
			src = tempPage->page_data;
		}
		else
		{
			readCount = file_inode->i_block_size;
			dest = buf + tempCur;
			src = tempPage->page_data;
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
	u32 init_pos;
	u32 err;
	u32 startPageNo, startPageCur;
	u32 endPageNo, endPageCur;
	u32 tempPageNo, tempPageNoCur, tempCur = 0;
	u32 realPageNo;
	struct inode* file_inode;
	struct vfs_page* tempPage;
	struct condition conditions;
	u32 writeCount = 0;

	init_pos = (*pos);
	file_inode = file->f_dentry->d_inode;
	//计算起始页号
	startPageNo = (*pos) / file_inode->i_block_size;
	startPageCur = (*pos) % file_inode->i_block_size;
	endPageNo = ((*pos) + count) / file_inode->i_block_size;
	endPageCur = ((*pos) + count) % file_inode->i_block_size;

	for(tempPageNo = startPageNo;tempPageNo <= endPageNo;tempPageNo++)
	{
		realPageNo = file_inode->i_data.a_op->bmap(file_inode, tempPageNo);
		//在缓存pcache中查找，放入tempPage
		conditions.cond1 = &realPageNo;
		conditions.cond2 = file_inode;

		tempPage = pcache->c_op->look_up(pcache, &conditions);
		if(tempPage == 0) //不在pcache中
		{
			//新调页
			tempPage = (struct vfs_page*) kmalloc(sizeof(struct vfs_page));
			tempPage->page_state = P_CLEAR;
			tempPage->page_address = realPageNo;
			tempPage->p_address_space = &(file_inode->i_data);
			INIT_LIST_HEAD(&(tempPage->page_hashtable));
			INIT_LIST_HEAD(&(tempPage->page_list));
			INIT_LIST_HEAD(&(tempPage->p_lru));

			err = file_inode->i_data.a_op->readpage(tempPage);
			if(err)
			{
				err = -EIO;
				return err;
			}

			pcache->c_op->add(pcache, tempPage);
			list_add(&(tempPage->page_list), &(file_inode->i_data.a_cache));
		}

		u8* dest;
		u8* src;
		if(tempPageNo == startPageNo)
		{
			src = buf;
			dest = tempPage->page_data + startPageCur;
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
			dest = tempPage->page_data;
		}
		else
		{
			writeCount = file_inode->i_block_size;
			src = buf + tempCur;
			dest = tempPage->page_data;
		}
		kernel_memcpy(dest, src, writeCount);
		tempCur += writeCount;
		(*pos) += writeCount;
	}

	//改变文件大小
	if(init_pos + count != file_inode->i_size)
	{
		file_inode->i_size = init_pos + count;
		file_inode->i_sb->s_op->write_inode(file_inode, file->f_dentry->d_parent);
	}

	file->f_pos = (*pos);
	return tempCur;
}

u32 generic_file_flush(struct file * file)
{
	struct vfs_page *page;
	struct inode *inode;
	struct list_head *a, *begin;
	struct address_space *mapping;

	inode = file->f_dentry->d_inode;  //文件inode节点
	mapping = &(inode->i_data);
	begin = &(mapping->a_cache);
	a = begin->next;

	// 把文件关联的已缓冲页逐页写回
	while ( a != begin ){
		page = container_of(a, struct vfs_page, page_list);
		if ( page->page_state & P_DIRTY ){
			mapping->a_op->writepage(page);
		}
		a = a->next;
	}

	return 0;
}