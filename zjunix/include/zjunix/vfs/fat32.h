#ifndef _ZJUNIX_VFS_FAT32_H
#define _ZJUNIX_VFS_FAT32_H

#include <zjunix/vfs/vfs.h>
#include <zjunix/debug/debug.h>
#define MAX_FAT32_SHORT_FILE_NAME_BASE_LEN      8//文件名不超过8个字节
#define MAX_FAT32_SHORT_FILE_NAME_EXT_LEN       3//扩展名不超过3个字节
#define MAX_FAT32_SHORT_FILE_NAME_LEN           11//( MAX_FAT32_SHORT_FILE_NAME_BASE_LEN  + MAX_FAT32_SHORT_FILE_NAME_EXT_LEN )
#define FAT32_FAT_ENTRY_LEN_SHIFT               2
#define FAT32_DIR_ENTRY_LEN                     32
#define FAT32_CLUSTER_NUM                       2097143
#define FAT32_CLUSTER_SIZE                      4 * 1024  //一个cluster 4096，包含8个sector
#define LCASE                                   0x18    //文件名和扩展名都小写。
#define UBASE_LEXT                              0x10    //文件名大写而扩展名小写。
#define LBASE_UEXT                              0x08    //文件名小写而扩展名大写。
#define UCASE                                   0x00    //文件名和扩展名都大写。

#define FAT32_FILE_NAME_NORMAL_TO_LONG          0
#define FAT32_FILE_NAME_LONG_TO_NORMAL          1

//file dentry属性attr字段意义
#define ATTR_NORMAL                             0x00
#define ATTR_READONLY                           0x01
#define ATTR_HIDDEN                             0x02
#define ATTR_SYSTEM                             0x04
#define ATTR_LONG_FILENAME                      0x0F
#define ATTR_DIRECTORY                          0x10
#define ATTR_ARCHIVE                            0x20
#define ATTR_VOLUMN                             0x08


struct fat32_basic_information 
{
    struct fat32_dos_boot_record* fat32_DBR;               // DBR 扇区信息
    struct fat32_file_system_information* fat32_FSINFO;    // FSINFO 辅助信息
    struct fat32_file_allocation_table* fat32_FAT1;         // FAT 文件分配表
    struct fat32_file_allocation_table* fat32_FAT2;
};
struct fat32_dos_boot_record
{
    u32 base;                              // 基地址（绝对扇区地址）
    u8 system_sign_and_version;            // 文件系统标志和版本号        
    u32 reserved;                         // 保留扇区（文件分配表之前）
    u8 sec_per_clu;                      // 每一簇的扇区数
    u8 fat_num;                           // 文件分配表的个数
    u32 sec_num;                          //fat32文件系统总扇区数
    u32 fat_size;                          // 一张文件分配表所占的扇区数
    u8 root_clu;                          // 根目录起始所在簇号（算上0号和1号簇），
    u16 fat32_version;                     //fat32版本号
    char system_format_ASCII[8];           //文件系统格式ASCII码
    u8 data[SECTOR_BYTE_SIZE];                 // 数据
    //0x03~0x0A：8字节，文件系统标志和版本号，这里为MSDOS5.0。
//0x0B~0x0C：2字节，每扇区字节数，0x0200=512
//0x0D~0x0D：1字节，每簇扇区数，0x08。
//0x0E~0x0F：2字节，保留扇区数，0x0C22=3106
///0x10~0x10：1字节，FAT表个数，0x02。
//0x20~0x23：4字节，文件系统总扇区数，0x00E83800=15218688
//0x24~0x27：4字节，每个FAT表占用扇区数，0x000039EF=14831
//0x2A~0x2B：2字节，FAT32版本号0.0，FAT32特有。
//0x2C~0x2F：4字节，根目录所在第一个簇的簇号，0x02。（虽然在FAT32文件系统下，根目录可以存放在数据区的任何位置，但是通常情况下还是起始于2号簇）
//0x30~0x31：2字节，FSINFO（文件系统信息扇区）扇区号0x01，该扇区为操作系统提供关于空簇总数及下一可用簇的信息。
//0x52~0x59：8字节，文件系统格式的ASCII码，FAT32。
//0x5A~0x1FD：共410字节，引导代码。
};

struct fat32_file_system_information
{
    u32 base;                              // 基地址（绝对扇区地址）
    u8 data[SECTOR_BYTE_SIZE];                               // 数据     // 数据
};

struct fat32_file_allocation_table
{
    u32 base;    
//    u32 clu_situ[FAT32_CLUSTER_NUM];
    u32 data_sec;                                       // （FAT表无关）数据区起始位置的绝对扇区(方便)
    u32 root_sec;                                       // （FAT表无关）根目录内容所在绝对扇区（方便）
      //表项数值	对应含义
                                                    //0x00000000 	空闲簇，即表示可用
                                                    //0x00000001	保留簇
                                                    //0x00000002 - 0x0FFFFFEF	被占用的簇，其值指向下一个簇号
                                                    //0x0FFFFFF0 - 0x0FFFFFF6	保留值
                                                    //0x0FFFFFF7 	坏簇
                                                    //0x0FFFFFF8 - 0x0FFFFFFF 	文件最后一个簇
};

struct __attribute__((__packed__)) fat32_dir_entry {
    u8 name[MAX_FAT32_SHORT_FILE_NAME_LEN + 1];             // 文件名(含拓展名)
    u8 attr;                                            // 属性
    u8 lcase;                                           // 系统保留 Case for base and extension
    u8 ctime_cs;                                        // 创建时间的10毫秒位
    u16 ctime;                                          // 创建时间
    u16 cdate;                                          // 创建日期
    u16 adate;                                          // 最后访问日期
    u16 starthi;                                        // 文件起始簇（相对物理帧）的高16位
    u16 time;                                           // 文件最后修改时间
    u16 date;                                           // 文件最后修改日期
    u16 startlo;                                        // 文件起始簇（相对物理帧）的低16位
    u32 size;                                           // 文件长度（字节）
};
// function declaration, implemented in fat32.c
u32 init_fat32(u32);
u32 fat32_delete_inode(struct dentry *);
u32 fat32_write_inode(struct inode *, struct dentry *);
struct dentry* fat32_inode_lookup(struct inode *, struct dentry *, struct nameidata *);
u32 fat32_create_inode(struct inode *, struct dentry *, struct nameidata *);
u32 fat32_readdir(struct file *, struct getdent *);
u32 fat32_mkdir(struct inode*, struct dentry*, u32);
u32 fat32_rmdir(struct inode*, struct dentry*);
u32 fat32_rename (struct inode*, struct dentry*, struct inode*, struct dentry*);
void fat32_convert_filename(struct qstr*, const struct qstr*, u8, u32);
u32 fat32_readpage(struct vfs_page *);
u32 fat32_writepage(struct vfs_page *);
u32 fat32_bitmap(struct inode *, u32);
u32 generic_compare_filename();
u32 read_fat(struct inode *, u32);
u32 write_fat(struct inode *, u32, u32);
#endif