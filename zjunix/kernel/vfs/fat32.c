#include <zjunix/vfs/vfs.h>
#include <zjunix/vfs/vfscache.h>
#include <zjunix/vfs/fat32.h>

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
    .delete_dentry_inode   = fat32_delete_inode,
    .write_inode    = fat32_write_inode,
};

struct inode_operations fat32_inode_operations[2] = {
    {
        .lookup = fat32_inode_lookup,
        .create = fat32_create_inode,
        .mkdir = fat32_mkdir,
        .rmdir = fat32_rmdir,
        .rename = fat32_rename,
    },
    {
        .create = fat32_create_inode,
    }
};

struct dentry_operations fat32_dentry_operations = {
    .d_compare    = generic_qstr_compare,
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
    kernel_printf("base: %d", base);
    u32 i, j, k;
    u32 err;
    u32 cluNo;
    struct fat32_basic_information* fat32_BI;
    struct file_system_type* fat32_fs_type;
    struct super_block* fat32_sb;
    struct vfs_page* tempPage;
    struct inode* root_inode;
    debug_start("fat32.c init_fat32 start\n");
    // 构建 fat32_basic_information 结构
    fat32_BI = (struct fat32_basic_information *) kmalloc ( sizeof(struct fat32_basic_information) );
    if (fat32_BI == 0)
    {
        err = -ENOMEM;
        debug_err("fat32.c:69 fat32_BI allocate memory error!\n");
        return err;
    }

    // 构建 fat32_dos_boot_record 结构
    fat32_BI->fat32_DBR = (struct fat32_dos_boot_record *) kmalloc ( sizeof(struct fat32_dos_boot_record) );
    if (fat32_BI->fat32_DBR == 0)
    {
        err = -ENOMEM;
        debug_err("fat32.c:78 fat32_DBR allocate memory error!\n");
        return err;
    }
    fat32_BI->fat32_DBR->base = base;
    kernel_memset(fat32_BI->fat32_DBR->data, 0, sizeof(fat32_BI->fat32_DBR->data));
    err = vfs_read_block(fat32_BI->fat32_DBR->data, fat32_BI->fat32_DBR->base, 1);        // DBR在基地址所在的一个扇区
    if (err)
    {
        debug_err("fat32.c:86 read the block data of fat32_DBR error!\n");
        err = -EIO;
        return err;
    }
    fat32_BI->fat32_DBR->system_sign_and_version = vfs_get_u32(fat32_BI->fat32_DBR->data + 0x03);
    fat32_BI->fat32_DBR->sec_per_clu   = *(fat32_BI->fat32_DBR->data + 0x0D);
    fat32_BI->fat32_DBR->reserved      = vfs_get_u16 (fat32_BI->fat32_DBR->data + 0x0E);
    fat32_BI->fat32_DBR->fat_num       = *(fat32_BI->fat32_DBR->data + 0x10);
    fat32_BI->fat32_DBR->sec_num       = vfs_get_u16 (fat32_BI->fat32_DBR->data + 0x20);
    fat32_BI->fat32_DBR->fat_size      = vfs_get_u32 (fat32_BI->fat32_DBR->data + 0x24);
    fat32_BI->fat32_DBR->fat32_version = vfs_get_u16 (fat32_BI->fat32_DBR->data + 0x2A);
    fat32_BI->fat32_DBR->root_clu      = vfs_get_u32 (fat32_BI->fat32_DBR->data + 0x2C);
    kernel_printf("root_clu: %d", fat32_BI->fat32_DBR->root_clu);
    fat32_BI->fat32_DBR->system_format_ASCII[0] = *(fat32_BI->fat32_DBR->data + 0x52);
    fat32_BI->fat32_DBR->system_format_ASCII[1] = *(fat32_BI->fat32_DBR->data + 0x53);
    fat32_BI->fat32_DBR->system_format_ASCII[2] = *(fat32_BI->fat32_DBR->data + 0x54);
    fat32_BI->fat32_DBR->system_format_ASCII[3] = *(fat32_BI->fat32_DBR->data + 0x55);
    fat32_BI->fat32_DBR->system_format_ASCII[4] = *(fat32_BI->fat32_DBR->data + 0x56);
    fat32_BI->fat32_DBR->system_format_ASCII[5] = *(fat32_BI->fat32_DBR->data + 0x57);
    fat32_BI->fat32_DBR->system_format_ASCII[6] = *(fat32_BI->fat32_DBR->data + 0x58);
    fat32_BI->fat32_DBR->system_format_ASCII[7] = *(fat32_BI->fat32_DBR->data + 0x59);
    // 构建 fat32_file_system_information 结构
    debug_start("start read in fat32_file_system_information\n");
    fat32_BI->fat32_FSINFO = (struct fat32_file_system_information *) kmalloc \
        ( sizeof(struct fat32_file_system_information) );
    debug_warning("FAT32 FSINFO : ");
    if (fat32_BI->fat32_FSINFO == 0)
    {
        err = -ENOMEM;
        return err;
    }
    fat32_BI->fat32_FSINFO->base = fat32_BI->fat32_DBR->base + 1;                     // FSINFO在基地址后一个扇区
    kernel_memset(fat32_BI->fat32_FSINFO->data, 0, sizeof(fat32_BI->fat32_FSINFO->data));
    debug_info("start to read in FSINFO data\n");
    err = vfs_read_block(fat32_BI->fat32_FSINFO->data, fat32_BI->fat32_FSINFO->base, 1);

    if (err)
    {
        err = -EIO;
        return err;
    }

    kernel_printf("fat32.c:123 load fat32 basic information ok!\n");
        // 构建 fat32_file_allocation_table 结构
    debug_start("start read in fat32_FAT\n");
    fat32_BI->fat32_FAT1 = (struct fat32_file_allocation_table *) kmalloc ( sizeof(struct fat32_file_allocation_table) );
    debug_warning("FAT32 FAT1 : \n");
    kernel_printf("%d\n",fat32_BI);
    if (fat32_BI->fat32_FAT1 == 0)
        return -ENOMEM;
    fat32_BI->fat32_FAT1->base = base + fat32_BI->fat32_DBR->reserved;                 // FAT起始于非保留扇区开始的扇区
    kernel_printf("FAT1-BASE %d\n", fat32_BI->fat32_FAT1->base);
    fat32_BI->fat32_FAT1->data_sec = fat32_BI->fat32_FAT1->base + fat32_BI->fat32_DBR->fat_num * fat32_BI->fat32_DBR->fat_size;
    kernel_printf("FAT32-datasec %d\n",  fat32_BI->fat32_FAT1->data_sec);
    fat32_BI->fat32_FAT1->root_sec = fat32_BI->fat32_FAT1->data_sec + ( fat32_BI->fat32_DBR->root_clu - 2 ) * fat32_BI->fat32_DBR->sec_per_clu;
/*    for(i = 0;i < FAT32_CLUSTER_NUM;i++)
        fat32_BI->fat32_FAT1->clu_situ[i] = 0x00000000;*/

    kernel_printf("fat32.c:135 load fat32 file allocation table ok!\n");
     // 构建 file_system_type 结构
    fat32_fs_type = (struct file_system_type *) kmalloc ( sizeof(struct file_system_type) );
    if (fat32_fs_type == 0)
    {
        err = -ENOMEM;
        return err;
    }
    fat32_fs_type->name = "fat32";
    // 构建根目录关联的 dentry 结构
    debug_start("start constructing root dentry\n");
    root_dentry = (struct dentry *) kmalloc ( sizeof(struct dentry) );
    if (root_dentry == 0)
    {
        err = -ENOMEM;
        return err;
    }
    INIT_LIST_HEAD(&(root_dentry->d_hash));
    INIT_LIST_HEAD(&(root_dentry->d_lru));
    INIT_LIST_HEAD(&(root_dentry->d_subdirs));
    INIT_LIST_HEAD(&(root_dentry->d_u.d_child));
    INIT_LIST_HEAD(&(root_dentry->d_alias));
    root_dentry->d_name.name = "/";
    root_dentry->d_name.len = 1;
    dcache->c_op->add(dcache, root_dentry);

    //相关联的信息初始化
    pwd_dentry = root_dentry;

    //构建super_block 结构
    debug_start("start constructing superblock for fat32\n");
    fat32_sb = (struct super_block*)kmalloc(sizeof(struct super_block));
    if (fat32_sb == 0)
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
    debug_start("start constructing inode for root dir\n");
    root_inode = (struct inode *) kmalloc ( sizeof(struct inode) );
    if (root_inode == 0)
    {
        err = -ENOMEM;
        return err;
    }
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
    //32位，0x0FFFFFFF表示文件结束
    while ( 0x0FFFFFFF != cluNo ){
        root_inode->i_blocks++;
        cluNo = read_fat(root_inode, cluNo);          // 读FAT32表
        debug_warning("cluNo:");
        kernel_printf("%d\n", cluNo);
    }
    debug_info("read fat1 for root_dentry end\n");
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
    kernel_printf("fat32.c:249 load fat32 root inode ok!");
    //读取根目录文件夹数据
    for(i = 0;i < root_inode->i_blocks;i++)
    {
        tempPage = (struct vfs_page*) kmalloc(sizeof(struct vfs_page));
        if(tempPage == 0)
        {
            err = -ENOMEM;
            return err;
        }
        debug_info("tempPage allocate memory ok!\n");
        tempPage->page_state = P_CLEAR;
        tempPage->page_address = root_inode->i_data.a_page[i];
        tempPage->p_address_space = &(root_inode->i_data);
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
        debug_info("read tempPage ok!\n");

        pcache_add(pcache, tempPage);
        list_add(tempPage->page_list, &(tempPage->p_address_space->a_cache));
    }
    kernel_printf("fat32.c:276 load fat32 root dir page ok!");
    // 构建本文件系统关联的 vfsmount挂载
    debug_start("start constructing fat32 mount\n");
    root_mnt = (struct vfsmount*)kmalloc(sizeof(struct vfsmount));
    if(root_mnt == 0)
    {
        err = -ENOMEM;
        return err;
    }
    root_mnt->mnt_root = root_dentry;
    root_mnt->mnt_parent = root_mnt;
    root_mnt->mnt_sb = fat32_sb;
    root_mnt->mnt_mountpoint = root_dentry;
    root_mnt->mnt_flags = 1;
    INIT_LIST_HEAD(&root_mnt->mnt_child);
    INIT_LIST_HEAD(&root_mnt->mnt_expire);
    INIT_LIST_HEAD(&root_mnt->mnt_hash);
    INIT_LIST_HEAD(&root_mnt->mnt_share);
    INIT_LIST_HEAD(&root_mnt->mnt_slave);
    INIT_LIST_HEAD(&root_mnt->mnt_slave_list);
    INIT_LIST_HEAD(&root_mnt->mnt_mounts);
    INIT_LIST_HEAD(&root_mnt->mnt_list);

    pwd_mnt = root_mnt;
    debug_end("fat32.c:305 init_fat32 ok!\n");
    return 0;
}

//为fat32实现的super_operation
struct dentry* fat32_inode_lookup(struct inode *temp_inode, struct dentry* temp_dentry, struct nameidata *_nameidata)
{
    struct qstr entryname_str;
    struct qstr normalname_str;
    u8 name[MAX_FAT32_SHORT_FILE_NAME_LEN];
    u32 err;
    u32 addr;
    u32 found = 0;
    u32 tempPageNo;
    int i, j;
    struct inode* file_inode;
    struct vfs_page* tempPage;
    struct condition conditions;
    struct fat32_dir_entry* temp_dir_entry;
    struct inode* new_inode;
    // 对目录关联的每一页
    for ( i = 0; i < temp_inode->i_blocks; i++){
        tempPageNo = temp_inode->i_data.a_op->bmap(temp_inode, i);
        // 首先在页高速缓存中寻找
        conditions.cond1 = (void*)(&tempPageNo);
        conditions.cond2 = (void*)(temp_inode);
        tempPage = (struct vfs_page *)pcache->c_op->look_up(pcache, &conditions);

        // 如果页高速缓存中没有，则需要在外存中寻找（一定能够找到，因为不是创建文件）
        if ( tempPage == 0 ){


            tempPage = (struct vfs_page *) kmalloc ( sizeof(struct vfs_page) );
            if (!tempPage)
                return ERR_PTR(-ENOMEM);

            tempPage->page_state   = P_CLEAR;
            tempPage->page_address = tempPageNo;
            tempPage->p_address_space = &(temp_inode->i_data);
            INIT_LIST_HEAD(tempPage->page_hashtable);
            INIT_LIST_HEAD(tempPage->p_lru);
            INIT_LIST_HEAD(tempPage->page_list);

            err = temp_inode->i_data.a_op->readpage(tempPage);
            if ( IS_ERR_VALUE(err) ){
                release_page(tempPage);
                return 0;
            }

            tempPage->page_state= P_CLEAR;
            pcache->c_op->add(pcache, (void*)tempPage);
            list_add(tempPage->page_list, &(temp_inode->i_data.a_cache));
        }

        //现在p_data指向的数据就是页的数据。假定页里面的都是fat32短文件目录项。对每一个目录项

        for (i = 0;i < temp_inode->i_block_size;i += FAT32_DIR_ENTRY_LEN ){

            temp_dir_entry = (struct fat32_dir_entry *)(tempPage->page_data + i);

            // 先判断是不是短文件名，如果不是的话跳过（08 卷标、0F长文件名）
            if (temp_dir_entry->attr == ATTR_VOLUMN || temp_dir_entry->attr == ATTR_LONG_FILENAME)
                continue;

            // 再判断是不是已删除的文件，是的话跳过
            if (temp_dir_entry->name[0] == 0xE5)
                continue;

            // 再判断是不是没有目录项了
            if (temp_dir_entry->name[0] == '\0')
                break;

            // 有目录项的话，提取其名字
            for ( j = 0; j < MAX_FAT32_SHORT_FILE_NAME_LEN; j++ )
                name[j] = temp_dir_entry->name[j];
            entryname_str.name = name;
            entryname_str.len = MAX_FAT32_SHORT_FILE_NAME_LEN;

            // 转换名字
            fat32_convert_filename(&normalname_str, &entryname_str, temp_dir_entry->lcase, FAT32_FILE_NAME_LONG_TO_NORMAL);

            // 如果与待找的名字相同，则建立相应的inode

            if (generic_qstr_compare(&normalname_str, &(temp_dentry->d_name)) == 0 ){
                // 获得起始簇号（相对物理地址）
                addr    = (temp_dir_entry->starthi << 16) + temp_dir_entry->startlo;

                // 填充inode的相应信息
                new_inode = (struct inode*) kmalloc ( sizeof(struct inode) );
                new_inode->i_count          = 1;
                new_inode->i_ino            = addr;           // 本fat32系统设计inode的ino即为起始簇号
                new_inode->i_block_size_bit = temp_inode->i_block_size_bit;
                new_inode->i_block_size     = temp_inode->i_block_size;
                new_inode->i_sb             = temp_inode->i_sb;
                new_inode->i_size           = temp_dir_entry->size;
                new_inode->i_blocks         = 0;
                new_inode->i_fop            = &(fat32_file_operations);
                INIT_LIST_HEAD(&(new_inode->i_dentry));
                INIT_LIST_HEAD(&(new_inode->i_sb_list));
                INIT_LIST_HEAD(&(new_inode->i_hash));
                INIT_LIST_HEAD(&(new_inode->i_list));
                INIT_LIST_HEAD(&(new_inode->i_data.a_cache));

                if (temp_dir_entry->attr & ATTR_DIRECTORY )   // 目录和非目录有不同的inode方法
                    new_inode->i_op         = &(fat32_inode_operations[0]);
                else
                    new_inode->i_op         = &(fat32_inode_operations[1]);

                // 填充关联的address_space结构
                new_inode->i_data.a_host        = new_inode;
                new_inode->i_data.a_pagesize    = new_inode->i_block_size;
                new_inode->i_data.a_op          = &(fat32_address_space_operations);

                while ( 0x0FFFFFFF != addr ){
                    new_inode->i_blocks++;
                    addr = read_fat(new_inode, addr);          // 读FAT32表
                }

                new_inode->i_data.a_page = (u32*) kmalloc (new_inode->i_blocks * sizeof(u32) );
                kernel_memset(new_inode->i_data.a_page, 0, new_inode->i_blocks);

                addr = new_inode->i_ino;
                for(j = 0; j < new_inode->i_blocks; j++){
                    new_inode->i_data.a_page[j] = addr;
                    addr = (new_inode, addr);
                }
                // 把inode放入高速缓存
                // icache->c_op->add(icache, (void*)new_inode);
                found = 1;
                break;                          // 跳出的是对每一个目录项的循环
            }
        }
        if (found)
            break;                              // 跳出的是对每一页的循环
    }

    // 完善nameidata的信息
    _nameidata->dentry = temp_dentry;
    _nameidata->mnt = root_mnt;
    _nameidata->flags = found;
    // 如果没找到相应的inode
    if (!found)
        return 0;

    // 完善dentry的信息
    temp_dentry->d_inode = new_inode;
    temp_dentry->d_op = &fat32_dentry_operations;
    list_add(&(temp_dentry->d_alias), &new_inode->i_dentry);
    return temp_dentry;
}
// 删除内存中的VFS索引节点和磁盘上文件数据及元数据
u32 fat32_delete_inode(struct dentry* temp_dentry)
{
    u8 name[MAX_FAT32_SHORT_FILE_NAME_LEN];
    u32 i, j;
    u32 err;
    u32 found = 0;
    u32 tempPageNo;
    struct qstr entryname_str;
    struct qstr normalname_str;
    struct  vfs_page* tempPage;
    struct inode* temp_inode;
    struct condition conditions;
    struct fat32_dir_entry* temp_dir_entry;

    temp_inode = temp_dentry->d_inode;

    for ( i = 0; i < temp_inode->i_blocks; i++){
        tempPageNo = temp_inode->i_data.a_op->bmap(temp_inode, i);

        // 首先在页高速缓存中寻找
        conditions.cond1 = (void*)(&tempPageNo);
        conditions.cond2 = (void*)(temp_inode);
        tempPage = (struct vfs_page *)pcache->c_op->look_up(pcache, &conditions);

        // 如果页高速缓存中没有，则需要在外存中寻找（一定能够找到，因为不是创建文件）
        if (tempPage == 0 ){
            tempPage = (struct vfs_page *) kmalloc(sizeof(struct vfs_page));
            if (!tempPage)
            {
                err = -ENOMEM;
                return err;
            }
            tempPage->page_state    = P_CLEAR;
            tempPage->page_address = tempPageNo;
            tempPage->p_address_space = &(temp_inode->i_data);
            INIT_LIST_HEAD(tempPage->page_hashtable);
            INIT_LIST_HEAD(tempPage->p_lru);
            INIT_LIST_HEAD(tempPage->page_list);

            err = temp_inode->i_data.a_op->readpage(tempPage);
            if ( IS_ERR_VALUE(err) ){
                release_page(tempPage);
                return 0;
            }

            tempPage->page_state = P_CLEAR;
            pcache->c_op->add(pcache, (void*)tempPage);
            list_add(tempPage->page_list, &(temp_inode->i_data.a_cache));
        }

        //现在p_data指向的数据就是页的数据。假定页里面的都是fat32短文件目录项。对每一个目录项
        for (i = 0;i < temp_inode->i_block_size;i += FAT32_DIR_ENTRY_LEN ){
            temp_dir_entry = (struct fat32_dir_entry *)(tempPage->page_data + i);

            // 先判断是不是短文件名，如果不是的话跳过（08 卷标、0F长文件名）
            if (temp_dir_entry->attr == ATTR_VOLUMN || temp_dir_entry->attr == ATTR_LONG_FILENAME)
                continue;

            // 再判断是不是已删除的文件，是的话跳过
            if (temp_dir_entry->name[0] == 0xE5)
                continue;

            // 再判断是不是没有目录项了
            if (temp_dir_entry->name[0] == '\0')
                break;

            // 有目录项的话，提取其名字
            kernel_memset( name, 0, MAX_FAT32_SHORT_FILE_NAME_LEN * sizeof(u8) );
            for ( j = 0; j < MAX_FAT32_SHORT_FILE_NAME_LEN; j++ )
                name[j] = temp_dir_entry->name[j];
            entryname_str.name = name;
            entryname_str.len = MAX_FAT32_SHORT_FILE_NAME_LEN;

            // 转换名字
            fat32_convert_filename(&normalname_str, &entryname_str, temp_dir_entry->lcase, FAT32_FILE_NAME_LONG_TO_NORMAL);

            // 如果与待找的名字相同，则标记此目录项为删除
            if ( generic_qstr_compare( &normalname_str, &(temp_dentry->d_name)) == 0 ){
                kernel_printf("fat32.c:528 get %s in delete inode\n", normalname_str.name);
                temp_dir_entry->name[0] = 0xE5;
                found = 1;
                break;                          // 跳出的是对每一个目录项的循环
            }
        }
        if (found)
            break;                              // 跳出的是对每一页的循环
    }

    // 如果没找inode在外存上相应的数据
    if (!found){
        err = -ENOENT;
        return err;
    }
    else
    {
        tempPage->page_state = P_DIRTY;
        //这里是直接写入外存
        /* err = fat32_writepage(tempPage);
           if(err)
           {
           return err
           }
           else
           return 0;
            */
    }
    debug_end("fat32.c:556 fat32_delete_inode ok!\n");
    return 0;
}
// 用通过传递参数指定的索引节点对象的内容更新一个文件系统的索引节点
u32 fat32_write_inode(struct inode * temp_inode, struct dentry * parent)
{
    u8 name[MAX_FAT32_SHORT_FILE_NAME_LEN];
    u32 i, j;
    u32 err;
    u32 found = 0;
    u32 tempPageNo;
    struct qstr entryname_str;
    struct qstr normalname_str;
    struct  vfs_page* tempPage;
    struct inode* parent_inode;
    struct dentry* temp_dentry;
    struct condition conditions;
    struct fat32_dir_entry* parent_dir_entry;
    debug_start("fat32.c:574 fat32_write_inode debug start\n");
    parent_inode = parent->d_inode;
    temp_dentry = container_of(temp_inode->i_dentry.next, struct dentry, d_alias);
    for ( i = 0; i < parent_inode->i_blocks; i++){
        tempPageNo = parent_inode->i_mapping->a_op->bmap(parent_inode, i);
        // 首先在页高速缓存中寻找
        conditions.cond1 = (void*)(&tempPageNo);
        conditions.cond2 = (void*)(parent_inode);
        tempPage = (struct vfs_page *)pcache->c_op->look_up(pcache, &conditions);


        // 如果页高速缓存中没有，则需要在外存中寻找（一定能够找到，因为不是创建文件）
        if ( tempPage == 0 )
        {
            kernel_printf("fat32.c:587 dcache not found!\n");
            tempPage = (struct vfs_page *) kmalloc ( sizeof(struct vfs_page) );
            if (!tempPage)
            {
                err = -ENOMEM;
                return err;
            }
            tempPage->page_state = P_CLEAR;
            tempPage->page_address  = tempPageNo;
            tempPage->p_address_space = &(parent_inode->i_data);
            INIT_LIST_HEAD(tempPage->page_hashtable);
            INIT_LIST_HEAD(tempPage->p_lru);
            INIT_LIST_HEAD(tempPage->page_list);

            err = parent_inode->i_data.a_op->readpage(tempPage);
            if ( IS_ERR_VALUE(err) ){
                release_page(tempPage);
                return -ENOENT;
            }

            tempPage->page_state = P_CLEAR;
            pcache->c_op->add(pcache, (void*)tempPage);
            list_add(tempPage->page_list, &(parent_inode->i_data.a_cache));
        }
        //现在p_data指向的数据就是页的数据。假定页里面的都是fat32短文件目录项。对每一个目录项
        for ( i = 0; i < parent_inode->i_block_size; i += FAT32_DIR_ENTRY_LEN ){
            parent_dir_entry = (struct fat32_dir_entry *)(tempPage->page_data + i);

            // 先判断是不是短文件名，如果不是的话跳过（08 卷标、0F长文件名）
            if (parent_dir_entry->attr == ATTR_VOLUMN || parent_dir_entry->attr == ATTR_LONG_FILENAME)
                continue;

            // 再判断是不是已删除的文件，是的话跳过
            if (parent_dir_entry->name[0] == 0xE5)
                continue;

            // 再判断是不是没有目录项了
            if (parent_dir_entry->name[0] == '\0')
                break;

            // 有目录项的话，提取其名字
            kernel_memset( name, 0, MAX_FAT32_SHORT_FILE_NAME_LEN * sizeof(u8) );
            for ( j = 0; j < MAX_FAT32_SHORT_FILE_NAME_LEN; j++ )
                name[j] = parent_dir_entry->name[j];

            entryname_str.name = name;
            entryname_str.len = MAX_FAT32_SHORT_FILE_NAME_LEN;

            // 转换名字
            fat32_convert_filename(&normalname_str, &entryname_str, parent_dir_entry->lcase, FAT32_FILE_NAME_LONG_TO_NORMAL);

            // 如果与待找的名字相同，则修改相应的文件元信息
            if ( generic_qstr_compare(&normalname_str, &(temp_dentry->d_name) ) == 0 ){
                parent_dir_entry->size         = temp_inode->i_size;
                found = 1;
                break;                          // 跳出的是对每一个目录项的循环
            }
        }
        if (found)
            break;                              // 跳出的是对每一页的循环
    }

    if(found)
    {
        tempPage->page_state = P_DIRTY;
        return 0;
        //下面是直接写回外存部分
        /*err = parent_inode->i_data.a_op->writepage(tempPage);
        if(err)
        {
            err = -ENOENT;
            return err;
        }*/
    }
    else
    {
        err = -ENOENT;
        return err;
    }
    debug_end("fat32.c:667 fat32_write_inode ok!\n");
    return 0;
}
// 在传递参数指定的索引节点对象下创建一个新的文件系统的索引节点
u32 fat32_create_inode(struct inode* parent_inode, struct dentry* temp_dentry, struct nameidata* _nameidata)
{
    u32 err;
    u32 allocable = 0;
    u32 addr;
    u32 i;
    u32 realPageNo;
    struct inode* temp_inode;
    struct dentry* parent_dentry;
    struct vfs_page* tempPage;
    struct address_space* temp_address_space;
    debug_start("fat32.c:682 debug fat32_create_inode start\n");
    parent_dentry = container_of(parent_inode->i_dentry.next, struct dentry, d_alias);
    temp_inode = (struct inode*) kmalloc(sizeof(struct inode));
    temp_dentry->d_sb = parent_dentry->d_sb;
    if(temp_inode == 0)
    {
        err = -ENOMEM;
        return err;
    }
    //为新文件分配簇,创建一个空文件
    for(i = 2;i < FAT32_CLUSTER_NUM;i++)
    {
        addr = read_fat(parent_inode, i);
        if(addr == 0x00000000)
        {
            write_fat(parent_inode, i, 0x0FFFFFFF);
            allocable = 1;
            break;
        }
    }
    if(!allocable)
    {
        err = -ENOMEM;
        kfree(temp_inode);
        return err;
    }
        //得到文件起始簇号后初始化新建inode信息
    temp_inode->i_count  = 1;
    temp_inode->i_ino    = addr;                     //inode号等于文件起始簇号
    temp_inode->i_op     = &(fat32_inode_operations[0]);
    temp_inode->i_fop    = &(fat32_file_operations);
    temp_inode->i_sb     = root_mnt->mnt_sb;
    temp_inode->i_blocks = 1;
    temp_inode->i_size = 0;
    temp_inode->i_state = S_CLEAR;
    temp_inode->i_block_count = 1;
    INIT_LIST_HEAD(&(temp_inode->i_dentry));
    INIT_LIST_HEAD(&(temp_inode->i_hash));
    INIT_LIST_HEAD(&(temp_inode->i_sb_list));
    temp_inode->i_block_size = temp_inode->i_sb->s_block_size;
    temp_inode->i_block_size_bit = parent_inode->i_block_size_bit;

    temp_address_space = (struct address_space*)kmalloc(sizeof(struct address_space));
    if(temp_address_space == 0)
    {
        err = -ENOMEM;
        return err;
    }
    temp_inode->i_data = *(temp_address_space);

    temp_address_space->a_host = temp_inode;
    temp_address_space->a_op = &(fat32_address_space_operations);
    INIT_LIST_HEAD(&(temp_address_space->a_cache));

    temp_address_space->a_page = (u32*)kmalloc(sizeof(u32) * temp_inode->i_blocks);
    temp_address_space->a_page[0] = addr;


    //直接重新分配一个新页给此inode
    if (tempPage == 0) {
        tempPage = (struct vfs_page *) kmalloc(sizeof(struct vfs_page));
        if (tempPage == 0) {
            err = -ENOMEM;
            return err;
        }
        tempPage->page_state = P_CLEAR;
        tempPage->page_address = realPageNo;
        tempPage->p_address_space = &(temp_inode->i_data);
        INIT_LIST_HEAD(tempPage->p_lru);
        INIT_LIST_HEAD(tempPage->page_hashtable);
        INIT_LIST_HEAD(tempPage->page_list);
        //fat32系统读入页
        err = temp_inode->i_data.a_op->readpage(tempPage);
        if (IS_ERR_VALUE(err))
        {
            release_page(tempPage);
            return 0;
        }
        tempPage->page_state = P_CLEAR;
        pcache_add(pcache, (void *) tempPage);
        //将文件已缓冲的页接入链表
        list_add(tempPage->page_list, &(temp_inode->i_data.a_cache));
    }
    temp_address_space->a_pagesize = parent_inode->i_data.a_pagesize;
    //将temp_dentry与新建inode关联
    temp_dentry->d_inode = temp_inode;
    list_add(&(temp_dentry->d_alias), &(temp_inode->i_dentry));
    debug_end("fat32.c:768 fat32_create_inode ok!\n");
    return 0;
}

u32 fat32_rename (struct inode* old_inode, struct dentry* old_dentry, struct inode* new_inode, struct dentry* new_dentry)
{

    return 0;
}
u32 fat32_rmdir(struct inode* parent_inode, struct dentry* temp_dentry)
{
    u32 err;
   // struct dentry* parent_dentry;

  //  parent_dentry = container_of(parent_inode->i_dentry.next, struct dentry, d_alias);
    err = fat32_delete_inode(temp_dentry);
    if(err)
    {
        return err;
    }
    u8 *target_buffer = (u8*)kmalloc(sizeof(u8)*MAX_FAT32_SHORT_FILE_NAME_LEN);
    if (target_buffer == 0) return -ENOMEM;
    kernel_memset(target_buffer,0,MAX_FAT32_SHORT_FILE_NAME_LEN);
    *(target_buffer) = 0xE5;
//    temp_dentry->d_name.name[0] = 0xE5;
    temp_dentry->d_name.name = target_buffer;
    list_del(&(temp_dentry->d_lru));
    list_del(&(temp_dentry->d_hash));
    list_del(&(temp_dentry->d_subdirs));
    return 0;
}
u32 fat32_mkdir(struct inode* parent_inode, struct dentry* temp_dentry, u32 mode)
{
    u32 err;
    struct dentry* parent_dentry;
    struct nameidata* _nameidata;
    parent_dentry = container_of(parent_inode->i_dentry.next, struct dentry, d_alias);

    temp_dentry->d_parent = parent_dentry;

    list_add(&(parent_dentry->d_subdirs), &(temp_dentry->d_alias));
    err = fat32_create_inode(parent_inode, temp_dentry, _nameidata);
    if(err)
    {
        debug_err("fat32:813 create inode in mkdir error!\n");
        err = -EEXIST;
        return err;
    }
    debug_end("fat32.c:815 fat32_mkdir ok!\n");
    return err;
}
u32 read_fat(struct inode * temp_inode, u32 index)
{
    struct fat32_basic_information* fat32_BI;
    u8 buffer[SECTOR_BYTE_SIZE];
    u32 sec_addr;
    u32 sec_index;
    debug_start("fat32.c:826 read_fat start\n");
    fat32_BI = (struct fat32_basic_information*)(temp_inode->i_sb->s_fs_info);
    sec_addr = fat32_BI->fat32_FAT1->base + (index >> (SECTOR_LOG_SIZE - FAT32_FAT_ENTRY_LEN_SHIFT));
    //只取这些位
    kernel_printf("sec_addr %d\n", sec_addr);
    sec_index = index & ((1 << (SECTOR_LOG_SIZE - FAT32_FAT_ENTRY_LEN_SHIFT)) - 1);
    kernel_printf("sec_index %d\n", sec_index);
    vfs_read_block(buffer, sec_addr, 1);
    debug_end("fat32.c:832 read_fat end\n");
    kernel_printf("%d\n", vfs_get_u32(buffer + (sec_index << (FAT32_FAT_ENTRY_LEN_SHIFT))) );
    return vfs_get_u32(buffer + (sec_index << (FAT32_FAT_ENTRY_LEN_SHIFT)));
}

u32 write_fat(struct inode* temp_inode, u32 index, u32 content)
{
    struct fat32_basic_information* fat32_BI;
    u8 buffer[SECTOR_BYTE_SIZE];
    u32 sec_addr;
    u32 sec_index;
    u32 err;

    fat32_BI = (struct fat32_basic_information*)(temp_inode->i_sb->s_fs_info);
    sec_addr = fat32_BI->fat32_FAT1->base + (index >> (SECTOR_LOG_SIZE - FAT32_FAT_ENTRY_LEN_SHIFT));
    //只取这些位
    sec_index = index & ((1 << (SECTOR_LOG_SIZE - FAT32_FAT_ENTRY_LEN_SHIFT)) - 1);
    vfs_read_block(buffer, sec_addr, 1);
    //32位数据直接修改buffer
    buffer[sec_index << ((SECTOR_LOG_SIZE - FAT32_FAT_ENTRY_LEN_SHIFT))] = content;
    err = vfs_write_block(buffer, sec_addr, 1);
    if(err)
    {
        debug_err("fat32.c:854 write FAT in write_fat error!");
        err = -EIO;
        return err;
    }
    return 0;
}

//长短文件名转换
void fat32_convert_filename(struct qstr* dest, const struct qstr* src, u8 mode, u32 direction){
    u8* name;
    u32 i;
    u32 j;
    u32 err;
    u32 dot;
    u32 end, null_len, dot_pos;

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
    if(pageNo < 0) {
        debug_err("fat32.c:984 page number is less than 0!\n");
        return -EIO;
    }
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
    err = vfs_read_block(page->page_data, abs_sect_addr, tempinode->i_block_size);
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
    err = vfs_write_block(page->page_data, abs_sect_addr, tempinode->i_block_size);
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
    u32 i, j;
    struct inode* file_inode;
    struct vfs_page* tempPage;
    struct condition conditions;
    struct fat32_dir_entry* temp_dir_entry;
    debug_start("fat32.c:1044 fat32_readdir start\n");

    file_inode = file->f_dentry->d_inode;
    getdent->count = 0;
    getdent->dirent = (struct dirent *) kmalloc ( sizeof(struct dirent) * (file_inode->i_block_count * file_inode->i_block_size / FAT32_DIR_ENTRY_LEN));
    if (getdent->dirent == 0)
        return -ENOMEM;
    for(i = 0;i < file_inode->i_block_count;i++)
    {
        realPageNo = file_inode->i_data.a_op->bmap(file_inode, i);
        if(!realPageNo) return -ENOMEM;

        conditions.cond1 = &(realPageNo);
        conditions.cond2 = file_inode;
        tempPage = (struct vfs_page*)pcache->c_op->look_up(pcache, &conditions);
        //页不在高速缓存中就需要调重新分配
        if(tempPage == 0)
        {
            tempPage = (struct vfs_page*)kmalloc(sizeof(struct vfs_page));
            if(!tempPage)
            {
                debug_err("fat32.c:1065 get page of file information node error!\n");
                return -ENOMEM;
            }

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