#include <zjunix/vfs/vfs.h>
#include <zjunix/vfs/vfscache.h>
#include "fat32.h"

#include <zjunix/log.h>
#include <zjunix/slab.h>
#include <driver/vga.h>
#include <zjunix/utils.h>
extern struct dentry                    * root_dentry;              // vfs.c
extern struct dentry                    * pwd_dentry;
extern struct vfsmount                  * root_mount;
extern struct vfsmount                  * pwd_mnt;

extern struct cache                     * dcache;                   // vfscache.c
extern struct cache                     * pcache;

// 设置VFS的接口函数
struct super_operations fat32_super_operations = {
    .delete_inode   = fat32_delete_inode,
    .write_inode    = fat32_write_inode,
};

struct inode_operations fat32_inode_operations[2] = {
    {
        .lookup = fat32_inode_lookup,
        .create = fat32_create,
        .mkdir = fat32_mkdir,
    },
    {
        .create = fat32_create,
    }
};

struct dentry_operations fat32_dentry_operations = {
    .d_compare    = generic_compare_filename,
};

struct file_operations fat32_file_operations = {
    .read		= generic_file_read,
    .write      = generic_file_write,
    .flush      = generic_file_flush,
    .readdir    = fat32_readdir,
};

struct address_space_operations fat32_address_space_operations = {
    .writepage  = fat32_writepage,
    .readpage   = fat32_readpage,
    .bmap       = fat32_bitmap,
};

u32 init_fat32(u32 base)
{
    u32 i, j, k;
    u32 err;
    u32 cluNo;
    struct fat32_basic_information* fat32_BI;
    struct file_system_type* fat32_fs_type;
    struct super_block* fat32_sb;
    struct vfs_page* tempPage;
    struct inode* root_inode;

    // 构建 fat32_basic_information 结构
    fat32_BI = (struct fat32_basic_information *) kmalloc ( sizeof(struct fat32_basic_information) );
    if (fat32_BI == 0)
    {
        err = -ENOMEM;
        return err;
    }

    // 构建 fat32_dos_boot_record 结构
    fat32_BI->fat32_DBR = (struct fat32_dos_boot_record *) kmalloc ( sizeof(struct fat32_dos_boot_record) );
    if (fat32_BI->fat32_DBR == 0)
    {
        err = -ENOMEM;
        return err;
    }
    fat32_BI->fat32_DBR->base = base;
    kernel_memset(fat32_BI->fat32_DBR->data, 0, sizeof(fat32_BI->fat32_DBR->data));
    err = read_block(fat32_BI->fat32_DBR->data, fat32_BI->fat32_DBR->base, 1);        // DBR在基地址所在的一个扇区
    if (err)
    {
        err = -EIO;
        return err;
    }
    fat32_BI->fat32_DBR->system_sign_and_version = get_u32(fat32_BI->fat32_DBR->data + 0x03);
    fat32_BI->fat32_DBR->sec_per_clu   = *(fat32_BI->fat32_DBR->data + 0x0D);
    fat32_BI->fat32_DBR->reserved      = get_u16 (fat32_BI->fat32_DBR->data + 0x0E);
    fat32_BI->fat32_DBR->fat_num       = *(fat32_BI->fat32_DBR->data + 0x10);
    fat32_BI->fat32_DBR->sec_num       = get_u16 (fat32_BI->fat32_DBR->data + 0x20);
    fat32_BI->fat32_DBR->fat_size      = get_u32 (fat32_BI->fat32_DBR->data + 0x24);
    fat32_BI->fat32_DBR->fat32_version = get_u16 (fat32_BI->fat32_DBR->data + 0x2A);
    fat32_BI->fat32_DBR->root_clu      = get_u32 (fat32_BI->fat32_DBR->data + 0x2C);
    fat32_BI->fat32_DBR->system_format_ASCII[0] = (char) (fat32_BI->fat32_DBR->data + 0x52);
    fat32_BI->fat32_DBR->system_format_ASCII[1] = (char) (fat32_BI->fat32_DBR->data + 0x53);
    fat32_BI->fat32_DBR->system_format_ASCII[2] = (char) (fat32_BI->fat32_DBR->data + 0x54);
    fat32_BI->fat32_DBR->system_format_ASCII[3] = (char) (fat32_BI->fat32_DBR->data + 0x55);
    fat32_BI->fat32_DBR->system_format_ASCII[4] = (char) (fat32_BI->fat32_DBR->data + 0x56);
    fat32_BI->fat32_DBR->system_format_ASCII[5] = (char) (fat32_BI->fat32_DBR->data + 0x57);
    fat32_BI->fat32_DBR->system_format_ASCII[6] = (char) (fat32_BI->fat32_DBR->data + 0x58);
    fat32_BI->fat32_DBR->system_format_ASCII[7] = (char) (fat32_BI->fat32_DBR->data + 0x59);

    // 构建 fat32_file_system_information 结构
    fat32_BI->fat32_FSINFO = (struct fat32_file_system_information *) kmalloc \
        ( sizeof(struct fat32_file_system_information) );
    if (fat32_BI->fat32_FSINFO == 0)
    {
        err = -ENOMEM;
        return err;
    }
    fat32_BI->fat32_FSINFO->base = fat32_BI->fat32_DBR->base + 1;                     // FSINFO在基地址后一个扇区
    kernel_memset(fat32_BI->fat32_FSINFO->data, 0, sizeof(fat32_BI->fat32_FSINFO->data));
    err = read_block(fat32_BI->fat32_FSINFO->data, fat32_BI->fat32_FSINFO->base, 1);
    if (err)
    {
        err = -EIO;
        return err;
    }
    kernel_printf("load fat32 basic information ok!");
        // 构建 fat32_file_allocation_table 结构
    fat32_BI->fat32_FAT1 = (struct fat32_file_allocation_table *) kmalloc ( sizeof(struct fat32_file_allocation_table) );
    if (fat32_BI->fat32_FAT1 == 0)
        return -ENOMEM;
    fat32_BI->fat32_FAT1->base = base + fat32_BI->fat32_DBR->reserved;                 // FAT起始于非保留扇区开始的扇区

    fat32_BI->fat32_FAT1->data_sec = fat32_BI->fat32_FAT1->base + fat32_BI->fat32_DBR->fat_num * fat32_BI->fat32_DBR->fat_size;
    fat32_BI->fat32_FAT1->root_sec = fat32_BI->fat32_FAT1->data_sec + ( fat32_BI->fat32_DBR->root_clu - 2 ) * fat32_BI->fat32_DBR->sec_per_clu;
    for(i = 0;i < FAT32_CLUSTER_NUM;i++)
        fat32_BI->fat32_FAT1->clu_situ[i] = 0x00000000;

    kernel_printf("load fat32 file allocation table ok!");
     // 构建 file_system_type 结构
    fat32_fs_type = (struct file_system_type *) kmalloc ( sizeof(struct file_system_type) );
    if (fat32_fs_type == 0)
    {
        err = -ENOMEM;
        return err;
    }
    fat32_fs_type->name = "fat32";

    // 构建根目录关联的 dentry 结构
    root_dentry = (struct dentry *) kmalloc ( sizeof(struct dentry) );
    if (fat32_fs_type == 0)
    {
        err = -ENOMEM;
        return err;
    }
    INIT_LIST_HEAD(&(root_dentry->d_hash));
    INIT_LIST_HEAD(&(root_dentry->d_lru));
    INIT_LIST_HEAD(&(root_dentry->d_subdirs));
    INIT_LIST_HEAD(&(root_dentry->d_u.d_child));
    INIT_LIST_HEAD(&(root_dentry->d_alias));
    dcache->c_op->add(dcache, root_dentry);
    //相关联的信息初始化
    pwd_dentry = root_dentry;

    //构建super_block 结构
    fat32_sb = (struct super_block*)kmalloc(sizeof(struct super_block));
    if (fat32_fs_type == 0)
    {
        err = -ENOMEM;
        return err;
    }

    fat32_sb->s_type = fat32_fs_type;
    fat32_sb->s_op = &(fat32_super_operations);
    fat32_sb->s_dirt = S_CLEAR;
    fat32_sb->s_fs_info = (void*)(fat32_BI);
    fat32_sb->s_block_size = fat32_BI->fat32_DBR->sec_per_clu << SECTOR_LOG_SIZE;
    fat32_sb->s_count = 0;
    fat32_sb->s_root = root_dentry;
    INIT_LIST_HEAD(&(fat32_sb->s_inodes));
    INIT_LIST_HEAD(&(fat32_sb->s_list));

    // 构建根目录关联的 inode 结构

    root_inode = (struct inode *) kmalloc ( sizeof(struct inode) );
    if (root_inode == 0)
        return -ENOMEM;
    root_inode->i_count             = 1;
    root_inode->i_ino               = fat32_BI->fat32_DBR->root_clu;
    root_inode->i_op                = &(fat32_inode_operations[0]);
    root_inode->i_fop    = &(fat32_file_operations);
    root_inode->i_sb     = fat32_sb;
    root_inode->i_blocks = 0;
    INIT_LIST_HEAD(&(root_inode->i_dentry));
    INIT_LIST_HEAD(&(root_inode->i_hash));
    INIT_LIST_HEAD(&(root_inode->i_sb_list));
    //root_inode->i_size              = fat32_BI->fat32_FAT1->fat.table[fat32_BI->fat32_DBR->root_clu].size;
    // TODO 得到i_size
    root_inode->i_block_size = fat32_sb->s_block_size;
    if(fat32_sb->s_block_size == 1024)
    {
        root_inode->i_block_size_bit = 10;
    }
    else if(fat32_sb->s_block_size == 2048)
    {
        root_inode->i_block_size_bit = 11;
    }
    else if(fat32_sb->s_block_size == 4096)
    {
        root_inode->i_block_size_bit = 12;
    }
    else if(fat32_sb->s_block_size == 8192)
    {
        root_inode->i_block_size_bit = 13;
    }
    else
    {
        err = -EFAULT;
        return err;
    }
    //相关联项赋值
    root_dentry->d_inode = root_inode;

    // 构建根目录inode结构中的address_space结构
    root_inode->i_data.a_host       = root_inode;
    root_inode->i_data.a_pagesize   = fat32_sb->s_block_size;
    root_inode->i_data.a_op         = &(fat32_address_space_operations);
    INIT_LIST_HEAD(&(root_inode->i_data.a_cache));

    //开始从初始簇遍历
    cluNo = fat32_BI->fat32_DBR->root_clu;
    //32位，虚拟地址范围不能超过0x0FFFFFFF
    while ( 0x0FFFFFFF != cluNo ){
        root_inode->i_blocks++;
        cluNo = read_fat(root_inode, cluNo);          // 读FAT32表
    }
    root_inode->i_data.a_page = (u32 *)kmalloc(sizeof(u32) * root_inode->i_blocks);
    if (root_inode->i_data.a_page == 0)
    {
        err =  -ENOMEM;
        return err;
    }
    kernel_memset(root_inode->i_data.a_page, 0, root_inode->i_blocks);

    cluNo = fat32_BI->fat32_DBR->root_clu;
    for(i = 0;i < root_inode->i_blocks;i++)
    {
        root_inode->i_data.a_page[i] = cluNo;
        cluNo = read_fat(root_inode, cluNo);
    }
    kernel_printf("load fat32 root inode ok!");
    //读取根目录文件夹数据
    for(i = 0;i < root_inode->i_blocks;i++)
    {
        tempPage = (struct vfs_page*) kmalloc(sizeof(struct vfs_page));
        if(tempPage == 0)
        {
            err = -ENOMEM;
            return err;
        }
        tempPage->page_state = P_CLEAR;
        tempPage->page_address = root_inode->i_data.a_page[i];
        tempPage->p_address_space = root_inode->i_data;
        INIT_LIST_HEAD(tempPage->page_list);
        INIT_LIST_HEAD(tempPage->page_hashtable);
        INIT_LIST_HEAD(tempPage->p_lru);

        err = tempPage->p_address_space->a_op->readpage(tempPage);
        if(IS_ERR_VALUE(err))
        {
            release_page(tempPage);
            err = -EIO;
            return err;
        }
        pcache_add(pcache, tempPage);
        list_add(tempPage->page_list, &(tempPage->p_address_space->a_cache));
    }
    kernel_printf("load fat32 root dir page ok!");

    // 构建本文件系统关联的 vfsmount挂载
    root_mount = (struct vfsmount*)kmalloc(sizeof(struct vfsmount));
    if(root_mount == 0)
    {
        err = -ENOMEM;
        return err;
    }
    root_mount->mnt_root = root_dentry;
    root_mount->mnt_parent = root_mount;
    root_mount->mnt_sb = fat32_sb;
    root_mount->mnt_mountpoint = root_dentry;
    root_mount->mnt_flags = 1;
    INIT_LIST_HEAD(&root_mount->mnt_child);
    INIT_LIST_HEAD(&root_mount->mnt_expire);
    INIT_LIST_HEAD(&root_mount->mnt_hash);
    INIT_LIST_HEAD(&root_mount->mnt_share);
    INIT_LIST_HEAD(&root_mount->mnt_slave);
    INIT_LIST_HEAD(&root_mount->mnt_slave_list);
    INIT_LIST_HEAD(&root_mount->mnt_mounts);
    INIT_LIST_HEAD(&root_mount->mnt_list);

    pwd_mnt = root_mount;
    return 0;
}

//长短文件名转换
void fat32_convert_filename(struct qstr* dest, const struct qstr* src, u8 mode, u32 direction){
    u8* name;
    int i;
    u32 j;
    u32 err;
    int dot;
    int end, null_len, dot_pos;

    dest->name = 0;
    dest->len = 0;
    //从一般文件名到8-3文件名    
    if ( direction == FAT32_FILE_NAME_NORMAL_TO_LONG ){
        name = (u8 *) kmalloc ( MAX_FAT32_SHORT_FILE_NAME_LEN * sizeof(u8) );

        // 找到作为拓展名的“.”
        dot = 0;
        dot_pos = 0;
        for ( dot_pos = src->len; dot_pos >= 0; dot_pos--)
        {
            if ( src->name[dot_pos] == '.' )
            {
                dot = 1;
                break;
            }
        }

        // 先转换“.”前面的部分
        if ( dot_pos > MAX_FAT32_SHORT_FILE_NAME_BASE_LEN )
            end = MAX_FAT32_SHORT_FILE_NAME_BASE_LEN - 1;
        else
            end = dot_pos - 1;

        for ( i = 0; i < MAX_FAT32_SHORT_FILE_NAME_BASE_LEN; i++ )
        {
            if ( i > end )
                name[i] = '\0';
            else 
            {
                if ( src->name[i] <= 'z' && src->name[i] >= 'a' )
                    name[i] = src->name[i] - 'a' + 'A';
                else
                    name[i] = src->name[i];
            }
        }

        // 再转换“.”后面的部分
        for ( i = MAX_FAT32_SHORT_FILE_NAME_BASE_LEN, j = dot_pos + 1; i < MAX_FAT32_SHORT_FILE_NAME_LEN; i++, j++ )
        {
            if ( j >= src->len )
                name[i] == '\0';
            else
            {
                if ( src->name[j] <= 'z' && src->name[j] >= 'a' )
                    name[i] = src->name[j] - 'a' + 'A';
                else
                    name[i] = src->name[j];
            }
        }
        
        dest->name = name;
        dest->len = MAX_FAT32_SHORT_FILE_NAME_LEN;
    }

    // 从8-3到一般文件名
    else if ( direction == FAT32_FILE_NAME_LONG_TO_NORMAL ) 
    {
        // 默认src的长度必为 MAX_FAT32_SHORT_FILE_NAME_LEN
        // 首先找出新字符串的长度，同时找出“.”的位置
        null_len = 0;
        dot_pos = MAX_FAT32_SHORT_FILE_NAME_LEN;
        for ( i = MAX_FAT32_SHORT_FILE_NAME_LEN - 1; i > 0; i-- )
        {
            if ( src->name[i] == ' ') 
            {
                dot_pos = i;
                null_len ++;
            }
        }

        dest->len = MAX_FAT32_SHORT_FILE_NAME_LEN - null_len;
        name = (u8 *) kmalloc ( (dest->len + 2) * sizeof(u8) );     // 空字符 + '.'(不一定有)
        
        if ( dot_pos > MAX_FAT32_SHORT_FILE_NAME_BASE_LEN )
            dot_pos = MAX_FAT32_SHORT_FILE_NAME_BASE_LEN;
        
        // 先转换应该是“.”之前的部分
        for ( i = 0; i < dot_pos; i++ ) 
        {
            if (src->name[i] <= 'z' && src->name[i] >= 'a' && (mode == UBASE_LEXT || mode == UCASE) )
                name[i] = src->name[i] - 'a' + 'A';
            else if (src->name[i] <= 'Z' && src->name[i] >= 'A' && (mode == LBASE_UEXT || mode == LCASE) )
                name[i] = src->name[i] - 'A' + 'a';
            else
                name[i] = src->name[i];
        }
        
        i = dot_pos;
        j = MAX_FAT32_SHORT_FILE_NAME_BASE_LEN;
        if (src->name[j] != ' ')
        {
            name[i] = '.';
            for ( i = dot_pos + 1; j < MAX_FAT32_SHORT_FILE_NAME_LEN && src->name[j] != ' '; i++, j++ )
            {
                if (src->name[j] <= 'z' && src->name[j] >= 'a' && (mode == LBASE_UEXT || mode == UCASE) )
                    name[i] = src->name[j] - 'a' + 'A';
                else if (src->name[j] <= 'Z' && src->name[j] >= 'A' && (mode == UBASE_LEXT || mode == LCASE))
                    name[i] = src->name[j] - 'A' + 'a';
                else
                    name[i] = src->name[j];
            }
            dest->len += 1;
        }
        
        name[i] = '\0';
        dest->name = name;
    }
        return;
}
// 根据相对页号得到物理页号
u32 fat32_bitmap(struct inode* _inode, u32 pageNo)
{
    if(pageNo < 0) return -EIO;
    return _inode->i_data.a_page[pageNo];
}

u32 fat32_readpage(struct vfs_page* page)
{
    u32 err, base, abs_sect_addr;
    struct inode* tempinode;
    //找出绝对扇区地址
    tempinode = page->p_address_space->a_host;
    abs_sect_addr = ((struct fat32_basic_information*)(tempinode->i_sb->s_fs_info)) \
    ->fat32_FAT1->data_sec + (page->page_address - 2) * (tempinode->i_block_size >> SECTOR_LOG_SIZE);
    //第一、第二个扇区是系统扇区
    page->page_data = (u8*)kmalloc(sizeof(u8) * tempinode->i_block_size);
    if(page->page_data == 0)
    {
        err = -ENOMEM;
        return err;
    }
    err = read_block(page->page_data, abs_sect_addr, tempinode->i_block_size);
    if(err == 0)
    {
        err = -EIO;
        return err;
    }
    return 0;
}

u32 fat32_writepage(struct vfs_page* page)
{
    u32 err, base, abs_sect_addr;
    struct inode* tempinode;
    //找出绝对扇区地址
    tempinode = page->p_address_space->a_host;
    abs_sect_addr = ((struct fat32_basic_information*)(tempinode->i_sb->s_fs_info)) \
    ->fat32_FAT1->data_sec + (page->page_address - 2) * (tempinode->i_block_size >> SECTOR_LOG_SIZE);
    //第一、第二个扇区是系统扇区
    //写回外存即可
    err = write_block(page->page_data, abs_sect_addr, tempinode->i_block_size);
    if(err)
    {
        err = -EIO;
        return err;
    }
    return 0;
}
u32 fat32_readdir(struct file * file, struct getdent * getdent)
{
    struct qstr entryname_str;
    struct qstr normalname_str;
    u8 name[MAX_FAT32_SHORT_FILE_NAME_LEN];
    u32 err;
    u32 realPageNo;
    int i, j;
    struct inode* file_inode;
    struct vfs_page* tempPage;
    struct condition* conditions;
    struct fat32_dir_entry* temp_dir_entry;


    file_inode = file->f_dentry->d_inode;
    getdent->count = 0;
    getdent->dirent = (struct dirent *) kmalloc ( sizeof(struct dirent) * (file_inode->i_block_count * file_inode->i_block_size / FAT32_DIR_ENTRY_LEN));
    if (getdent->dirent == 0)
        return -ENOMEM;
    for(i = 0;i < file_inode->i_block_count;i++)
    {
        realPageNo = file_inode->i_data.a_op->bmap(file_inode, i);
        if(!realPageNo) return -ENOMEM;

        conditions->cond1 = &(realPageNo);
        conditions->cond2 = file_inode;
        tempPage = (struct vfs_page*)pcache->c_op->look_up(pcache, conditions);
        //页不在高速缓存中就需要调重新分配
        if(tempPage == 0)
        {
            tempPage = (struct vfs_page*)kmalloc(sizeof(struct vfs_page));
            if(!tempPage) return -ENOMEM;

            tempPage->page_state =  P_CLEAR;
            tempPage->page_address = realPageNo;
            tempPage->p_address_space = &(file_inode->i_data);
            INIT_LIST_HEAD(tempPage->p_lru);
            INIT_LIST_HEAD(tempPage->page_hashtable);
            INIT_LIST_HEAD(tempPage->page_list);

            //fat32系统读入页
            err = file_inode->i_data.a_op->readpage(tempPage);
            if(IS_ERR_VALUE(err))
            {
                release_page(tempPage);
                return 0;
            }
            tempPage->page_state = P_CLEAR;
            pcache_add(pcache, (void*)tempPage);
            //将文件已缓冲的页接入链表
            list_add(tempPage->page_list, &(file_inode->i_data.a_cache));
        }

    }

    //page_data数据中都是dentry项(暂时都按照短文件名处理)，遍历每个目录项
    for(i = 0;i < file_inode->i_block_size;i+=FAT32_DIR_ENTRY_LEN)
    {
        temp_dir_entry = (struct fat32_dir_entry*) (tempPage + i);
        //0x08表示卷标(虽然没有)  0F是长文件名(其实也没有)
        if(temp_dir_entry->attr == ATTR_VOLUMN || temp_dir_entry->attr == ATTR_LONG_FILENAME)
        {
            continue;
        }
        //空文件名，其实没有目录项了
        if(temp_dir_entry->name[0] == '\0')
        {
            break;
        }
        //或者是被删除了 ??
        if(temp_dir_entry->name[0] == 0xE5)
        {
            continue;
        }
        //把一般形式文件名复制，准备处理
        for ( j = 0; j < MAX_FAT32_SHORT_FILE_NAME_LEN; j++ )
            name[j] = temp_dir_entry->name[j];

        entryname_str.name = name;
        entryname_str.len = MAX_FAT32_SHORT_FILE_NAME_LEN;

        fat32_convert_filename(&normalname_str, &entryname_str, temp_dir_entry->lcase, FAT32_FILE_NAME_LONG_TO_NORMAL);
        getdent->dirent[getdent->count].name = normalname_str.name;
        getdent->dirent[getdent->count].ino = temp_dir_entry->startlo + (temp_dir_entry->starthi << 16);

        if(temp_dir_entry->attr & ATTR_DIRECTORY)
        {
            getdent->dirent[getdent->count].type = FTYPE_DIR;
        }
        else if(temp_dir_entry->attr & ATTR_NORMAL)
        {
            getdent->dirent[getdent->count].type = FTYPE_NORM;
        }
        else
        {
            getdent->dirent[getdent->count].type = FTYPE_UNKOWN;
        }
        getdent->count++;

    }
    return 0;
}