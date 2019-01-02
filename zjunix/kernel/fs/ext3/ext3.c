#define     EXT2_MAX_NAME_LEN      255

struct ext2_super_block {
    u32 inodes_count;           //索引节点总数
    u32 blocks_count;           //块为单位的文件系统大小
    u32 free_inodes_count;      //空闲索引节点数量
    u32 free_blocks_count;      //空闲块数量
    u32 first_block;            //第一个使用的块号
    u32 log_block_size;         //块大小对2取对数的结果
    u16 inode_size;             //索引节点结构的大小
};

enum {
    EXT2_UNKNOWN = 0,
    EXT2_NORMAL = 1,
    EXT2_DIR = 2,
    EXT2_LINK = 7
};

struct ext2_dir_entry{
    u32     inode_num;          //索引节点号
    u16     entry_len;          //目录项长度
    u8      file_name_len;      //文件名长度
    u8      file_type;          //文件类型
    char   file_name[EXT2_MAX_NAME_LEN];  //文件名
};

struct ext2_inode {
    u32	                i_size;                             // 文件大小（字节数）
    u32	                i_blocks;                           // 关联的块数
    u32	                i_flags;                            // 打开的标记
    u32	                i_block[EXT2_N_BLOCKS];             // 存放所有相关的块地址
};

struct ext2_group_description {
    u32	                block_bitmap;                       // 块位图所在块
    u32	                inode_bitmap;                       // inode位图所在块
    u32	                inode_table;                        // inode列表所在块
    u16	                free_blocks_count;                  // 空闲块数
    u16	                free_inodes_count;                  // 空闲节点数
    u16	                dirs_count;                         // 目录数
    u16	                padding;
    u32	                reserved[3];                        // 保留
};

struct ext2_base_information {
    u32                 base;                            // 启动块的基地址（绝对扇区地址，下同）
    u32                 first_sb_sect;                   // 第一个super_block的基地址
    u32                 first_gdt_sect;                  // 第一个组描述符表的基地址
    union {
        u8                        *data;
        struct ext2_super_block   *attr;
    } super_block;                                                   // 超级块数据
};


