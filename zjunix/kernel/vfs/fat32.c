#include <zjunix/vfs/vfs.h>
#include <zjunix/vfs/vfscache.h>
#include "fat32.h"

#include <zjunix/log.h>
#include <zjunix/slab.h>
#include <driver/vga.h>
#include <zjunix/utils.h>
extern struct dentry                    * root_dentry;              // vfs.c
extern struct dentry                    * pwd_dentry;
extern struct vfsmount                  * root_mnt;
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
    },
    {
        .create = fat32_create,
    }
};

struct dentry_operations fat32_dentry_operations = {
    .compare    = generic_compare_filename,
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
    .bitmap       = fat32_bitmap,
};

u32 init_fat32(u32 base)
{
    u32 i, j, k;
    struct fat32_basic_information* fat32_BI;
    struct file_system_type* fat32_fs_type;
    struct super_block* fat32_sb;
    struct vfs_page* curPage;
     // 构建 fat32_basic_information 结构
    fat32_BI = (struct fat32_basic_information *) kmalloc ( sizeof(struct fat32_basic_information) );
    if (fat32_BI == 0)
        return -ENOMEM;

    // 构建 fat32_dos_boot_record 结构
    fat32_BI->fa_DBR = (struct fat32_dos_boot_record *) kmalloc ( sizeof(struct fat32_dos_boot_record) );
    if (fat32_BI->fa_DBR == 0)
        return -ENOMEM;
    fat32_BI->fa_DBR->base = base;
    kernel_memset(fat32_BI->fa_DBR->data, 0, sizeof(fat32_BI->fa_DBR->data));
    err = read_block(fat32_BI->fa_DBR->data, fat32_BI->fa_DBR->base, 1);        // DBR在基地址所在的一个扇区
    if (err)
        return -EIO;
    fat32_BI->fa_DBR->system_sign_and_version = get_u32(fat32_BI->fa_DBR->data + 0x03);
    fat32_BI->fa_DBR->sec_per_clu   = *(fat32_BI->fa_DBR->data + 0x0D);
    fat32_BI->fa_DBR->reserved      = get_u16 (fat32_BI->fa_DBR->data + 0x0E);
    fat32_BI->fa_DBR->fat_num       = *(fat32_BI->fa_DBR->data + 0x10);
    fat32_BI->fa_DBR->sec_num       = get_u16 (fat32_BI->fa_DBR->data + 0x20);
    fat32_BI->fa_DBR->fat_size      = get_u32 (fat32_BI->fa_DBR->data + 0x24);
    fat32_BI->fa_DBR->fat32_version = get_u16 (fat32_BI->fa_DBR->data + 0x2A)
    fat32_BI->fa_DBR->root_clu      = get_u32 (fat32_BI->fa_DBR->data + 0x2C);
    fat32_BI->fa_DBR->system_format_ASCII = (char*) (fat32_BI->fat32_DBR->data + 0x52)

    // 构建 fat32_file_system_information 结构
    fat32_BI->fa_FSINFO = (struct fat32_file_system_information *) kmalloc \
        ( sizeof(struct fat32_file_system_information) );
    if (fat32_BI->fa_FSINFO == 0)
        return -ENOMEM;
    fat32_BI->fa_FSINFO->base = fat32_BI->fa_DBR->base + 1;                     // FSINFO在基地址后一个扇区
    kernel_memset(fat32_BI->fa_FSINFO->data, 0, sizeof(fat32_BI->fa_FSINFO->data));
    err = read_block(fat32_BI->fa_FSINFO->data, fat32_BI->fa_FSINFO->base, 1);
    if (err)
        return -EIO;

        // 构建 fat32_file_allocation_table 结构
    fat32_BI->fa_FAT1 = (struct fat32_file_allocation_table *) kmalloc ( sizeof(struct fat32_file_allocation_table) );
    if (fat32_BI->fa_FAT1 == 0)
        return -ENOMEM;
    fat32_BI->fa_FAT1->base = base + fat32_BI->fa_DBR->reserved;                 // FAT起始于非保留扇区开始的扇区

    fat32_BI->fa_FAT1->data_sec = fat32_BI->fa_FAT1->base + fat32_BI->fa_DBR->fat_num * fat32_BI->fa_DBR->fat_size;
    fat32_BI->fa_FAT1->root_sec = fat32_BI->fa_FAT1->data_sec + ( fat32_BI->fa_DBR->root_clu - 2 ) * fat32_BI->fa_DBR->sec_per_clu;
    for(i = 0;i < FAT32_CLUSTER_NUM;i++)
        fat32_BI->fat32_FAT1->clu_situ[i] = 0x00000000;
        
     // 构建 file_system_type 结构
    fat32_fs_type = (struct file_system_type *) kmalloc ( sizeof(struct file_system_type) );
    if (fat32_fs_type == 0)
        return -ENOMEM;
    fat32_fs_type->name = "fat32";                                                                            // 因为0、1号簇没有对应任何扇区
}

//长短文件名转换
void fat32_convert_filename(struct qstr* dest, const struct qstr* src, u8 mode, u32 direction){
    u8* name;
    int i;
    u32 j;
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
            if (src->name[i] <= 'z' && src->name[i] >= 'a' && (mode == 0x10 || mode == 0x00) )
                name[i] = src->name[i] - 'a' + 'A';
            else if (src->name[i] <= 'Z' && src->name[i] >= 'A' && (mode == 0x18 || mode == 0x08) )
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
                if (src->name[j] <= 'z' && src->name[j] >= 'a' && (mode == 0x08 || mode == 0x00) )
                    name[i] = src->name[j] - 'a' + 'A';
                else if (src->name[j] <= 'Z' && src->name[j] >= 'A' && (mode == 0x18 || mode == 0x10))
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
u32 fat32_bmap(struct inode* _inode, u32 pageNo)
{
    if(pageNo < 0) return -EIO;
    return _inode->i_data.a_page[pageNo];
}

u32 fat32_readpage(struct vfs_page* page)
{

}

u32 fat32_writepage(struct vfs_page* page)
{

}