/**
 * CREATED BY DESMOND
 */
#include <zjunix/vfs/vfs.h>
#include <zjunix/vfs/ext3.h>
#include <zjunix/utils.h>
#include <driver/vga.h>
#include <zjunix/slab.h>
#include <zjunix/debug/debug.h>
#include "../../usr/myvi.h"

extern struct dentry *root_dentry;              // vfs.c
extern struct dentry *pwd_dentry;       //当前工作目录
extern struct vfsmount *root_mnt;
extern struct vfsmount *pwd_mnt;

extern struct cache *dcache;                   // vfscache.c
extern struct cache *pcache;
extern struct cache *icache;

struct address_space_operations ext3_address_space_operations = {  //地址空间操作
        .writepage  = ext3_writepage,
        .readpage   = ext3_readpage,
        .bmap       = ext3_bmap,
};

struct file_system_type ext3_fs_type = {
        .name = "ext3",
};


struct super_operations ext3_super_ops = {
        .delete_dentry_inode = ext3_delete_dentry_inode,
        .write_inode = ext3_write_inode,
};

struct file_operations ext3_file_operations = {
        .write   = generic_file_write,
        .read = generic_file_read,
        .readdir = ext3_readdir,
        .flush = generic_file_flush
};

struct dentry_operations ext3_dentry_operations = {
        .d_compare = generic_qstr_compare
};

struct inode_operations ext3_inode_operations[2] = {{
        .create = ext3_create,
        .lookup = ext3_lookup,
        .mkdir = ext3_mkdir,
        .rmdir = ext3_rmdir
},{
        .create = ext3_create
}};


//通过相对文件页号计算相对物理页号， inode是该页所在的inode
u32 ext3_bmap(struct inode *inode, u32 target_page) {
    // 0-11：数据块
    // 12：一级索引
    // 13：二级索引
    // 14：三级索引
//    debug_start("Hello I'm getting into ext3_bmap!\n");
    u32 *pageTable = inode->i_data.a_page;
    u32 entry_num = inode->i_block_size >> EXT3_BLOCK_ADDR_SHIFT;
    if (target_page < EXT3_FIRST_MAP_INDEX) {
        return pageTable[target_page];  //因为初始化的时候就已经把所有能直接访问到的数据块都添加到页缓存里了
    }
    if (target_page < EXT3_FIRST_MAP_INDEX + entry_num) {
        u8 *index_block = (u8 *) kmalloc(inode->i_block_size * sizeof(u8));
        if (index_block == 0) return -ENOMEM;
        u32 err = vfs_read_block(index_block, pageTable[EXT3_FIRST_MAP_INDEX], inode->i_block_size >> SECTOR_LOG_SIZE);
        if (err) return -EIO;
        //index_block块里的都是地址
        u32 index = (target_page - EXT3_FIRST_MAP_INDEX) << EXT3_BLOCK_ADDR_SHIFT;
        u32 actual_addr = vfs_get_u32(index_block + index);
//        kfree(index_block);
        return actual_addr;
    }
    if (target_page < EXT3_FIRST_MAP_INDEX + (entry_num + 1) * entry_num) {
        u8 *index_block = (u8 *) kmalloc(inode->i_block_size * sizeof(u8));
        if (index_block == 0) return -ENOMEM;
        u32 err = vfs_read_block(index_block, pageTable[EXT3_SECOND_MAP_INDEX], inode->i_block_size >> SECTOR_LOG_SIZE);
        //读取这个二级索引块里的所有地址
        if (err) return -EIO;
        //下面需要寻找一级索引块地址
        u32 pre_index = (target_page - EXT3_FIRST_MAP_INDEX - entry_num);
        u32 index = (pre_index / entry_num) << EXT3_BLOCK_ADDR_SHIFT;
        //寻找到一级索引块地址以后读取对应的块
        u32 index1_addr = vfs_get_u32(index_block + index);
        err = vfs_read_block(index_block, index1_addr, inode->i_block_size >> SECTOR_LOG_SIZE);
        index = (pre_index % entry_num) << EXT3_BLOCK_ADDR_SHIFT;
        u32 actual_addr = vfs_get_u32(index_block + index);
//        kfree(index_block);
        return actual_addr;
    }
    if (target_page < EXT3_FIRST_MAP_INDEX + entry_num * (entry_num * (entry_num + 1) + 1)) {
        u8 *index_block = (u8 *) kmalloc(inode->i_block_size * sizeof(u8));
        if (index_block == 0) return -ENOMEM;
        u32 err = vfs_read_block(index_block, pageTable[EXT3_THIRD_MAP_INDEX], inode->i_block_size >> SECTOR_LOG_SIZE);
        //读取这个三级索引块里的所有地址
        if (err) return -EIO;
        //下面需要寻找二级索引块地址
        u32 pre_index = (target_page - EXT3_FIRST_MAP_INDEX - entry_num * (entry_num + 1));
        u32 index = (pre_index / (entry_num * entry_num)) << EXT3_BLOCK_ADDR_SHIFT;
        //寻找到二级索引块地址以后读取对应的块
        u32 index2_addr = vfs_get_u32(index_block + index);
        err = vfs_read_block(index_block, index2_addr, inode->i_block_size >> SECTOR_LOG_SIZE);
        if (err) return -EIO;
        index = (pre_index % (entry_num * entry_num) / entry_num) << EXT3_BLOCK_ADDR_SHIFT;   //Attention: 助教原版的代码这里有bug
        u32 index1_addr = vfs_get_u32(index_block + index);
        err = vfs_read_block(index_block, index1_addr, inode->i_block_size >> SECTOR_LOG_SIZE);
        if (err) return -EIO;
        index = (pre_index % entry_num) << EXT3_BLOCK_ADDR_SHIFT;
        u32 actual_addr = vfs_get_u32(index_block + index);
//        kfree(index_block);
        return actual_addr;
    }
    return -EFAULT;
}

u32 ext3_writepage(struct vfs_page *page) {
    struct inode *target_inode = page->p_address_space->a_host;  //获得对应的inode
    u32 sector_num = target_inode->i_block_size >> SECTOR_LOG_SIZE; //由于一块大小和一页大小相等，所以需要写出的扇区是这么大
    u32 base_addr = ((struct ext3_base_information *) target_inode->i_sb->s_fs_info)->base;  //计算文件系统基地址
    u32 target_addr = base_addr + page->page_address * (target_inode->i_block_size >> SECTOR_LOG_SIZE); //加上页地址
    u32 err = vfs_write_block(page->page_data, target_addr, sector_num);      //向目标地址写目标数量个扇区
    if (err) return -EIO;
    return 0;
}

u32 ext3_readpage(struct vfs_page *page) {
    struct inode *source_inode = page->p_address_space->a_host;
    u32 sector_num = source_inode->i_block_size >> SECTOR_LOG_SIZE;
    u32 base_addr = ((struct ext3_base_information *) source_inode->i_sb->s_fs_info)->base;  //计算文件系统基地址
    u32 source_addr = base_addr + page->page_address * (source_inode->i_block_size >> SECTOR_LOG_SIZE); //加上页地址
    page->page_data = (u8 *) kmalloc(sizeof(u8) * source_inode->i_block_size);
    if (page->page_data == 0) return -ENOMEM;
    kernel_memset(page->page_data, 0, sizeof(u8) * source_inode->i_block_size);
    u32 err = vfs_read_block(page->page_data, source_addr, sector_num);
    if (err) return -EIO;
    return 0;
}

struct super_block *ext3_init_super(struct ext3_base_information *information) {
    debug_start("EXT3-Init Super Block\n");
    struct super_block *ans = (struct super_block *) kmalloc(sizeof(struct super_block));
    if (ans == 0) return ERR_PTR(-ENOMEM);
    ans->s_dirt = S_CLEAR;  //标记当前超级块是否被写脏
    ans->s_root = 0;  //留待下一步构造根目录
    ans->s_op = (&ext3_super_ops);
    ans->s_block_size = EXT3_BLOCK_SIZE_BASE << information->super_block.content->block_size;
    ans->s_fs_info = (void *) information;
    ans->s_type = (&ext3_fs_type);
    debug_end("EXT3-Init Super Block\n");
    return ans;
}

struct ext3_base_information *ext3_init_base_information(u32 base) {
    debug_start("EXT3-Init base information\n");
    struct ext3_base_information *ans = (struct ext3_base_information *) kmalloc(sizeof(struct ext3_base_information));
    if (ans == 0) return ERR_PTR(-ENOMEM);
    ans->base = base;
    ans->first_sb_sect = base + EXT3_BOOT_SECTOR_SIZE;  //跨过引导区数据
    ans->super_block.fill = (u8 *) kmalloc(sizeof(u8) * EXT3_SUPER_SECTOR_SIZE * SECTOR_BYTE_SIZE);  //初始化super_block区域
    if (ans->super_block.fill == 0) return ERR_PTR(-ENOMEM);
    u32 err = vfs_read_block(ans->super_block.fill, ans->first_sb_sect, EXT3_SUPER_SECTOR_SIZE);  //从指定位置开始读取super_block

    if (err) return ERR_PTR(-EIO);
    //SECTOR是物理的， BASE_BLOCK_SIZE是逻辑的
    u32 ratio = EXT3_BLOCK_SIZE_BASE << ans->super_block.content->block_size >> SECTOR_LOG_SIZE;   //一个block里放多少个sector
    if (ratio <= 2) ans->first_gdt_sect = ans->first_sb_sect + EXT3_SUPER_SECTOR_SIZE;  //如果只能放2个以内的sector，那么gb和sb将紧密排列
    else ans->first_gdt_sect = base + ratio;  //否则直接跳过第一个块
    ans->group_count = (ans->super_block.content->block_count - ans->super_block.content->first_data_block_no + ans->super_block.content->blocks_per_group - 1) / ans->super_block.content->blocks_per_group;
    debug_end("EXT3-Init base information\n");
    return ans;
}


struct dentry *ext3_init_dir_entry(struct super_block *super_block) {
    debug_start("EXT3-Init dir entry\n");
    struct dentry *ans = (struct dentry *) kmalloc(sizeof(struct dentry));
    if (ans == 0) return ERR_PTR(-ENOMEM);
    ans->d_name.name = "/";
    ans->d_mounted = 0;
    ans->d_name.len = 1;
    ans->d_count = 1;
    ans->d_parent = 0;
    ans->d_op = (&ext3_dentry_operations);
    ans->d_inode = 0;
    ans->d_sb = super_block;
    ans->d_parent = 0;
    INIT_LIST_HEAD(&(ans->d_alias));
    INIT_LIST_HEAD(&(ans->d_lru));
    INIT_LIST_HEAD(&(ans->d_subdirs));
    debug_end("EXT3-Init dir entry\n");
    return ans;
}

struct inode *ext3_init_inode(struct super_block *super_block, u32 ino_num) {
    debug_start("EXT3-Init inode\n");
    struct inode *ans = (struct inode *) kmalloc(sizeof(struct inode));
    if (ans == 0) return ERR_PTR(-ENOMEM);
    ans->i_op = &(ext3_inode_operations[0]);
    ans->i_ino = ino_num;
    ans->i_fop = &ext3_file_operations;
    ans->i_count = 1;
    ans->i_sb = super_block;
    ans->i_block_size = super_block->s_block_size;
    INIT_LIST_HEAD(&(ans->i_list)); //初始化索引节点链表
    //INIT_LIST_HEAD(&(ans->i_dentry));  //初始化目录项链表
    INIT_LIST_HEAD(&(ans->i_hash));  //初始化散列表
    switch (ans->i_block_size) {
        case 1024:
            ans->i_block_size_bit = 10;
            break;
        case 2048:
            ans->i_block_size_bit = 11;
            break;
        case 4096:
            ans->i_block_size_bit = 12;
            break;
        case 8192:
            ans->i_block_size_bit = 13;
            break;
        default:
            return ERR_PTR(-EFAULT);
    }
    ans->i_data.a_host = ans;
    ans->i_data.a_pagesize = super_block->s_block_size;
    ans->i_data.a_op = (&ext3_address_space_operations);
    INIT_LIST_HEAD(&(ans->i_data.a_cache));
    debug_end("EXT3-Init inode\n");
    return ans;
}

u32 get_group_info_base(struct inode *inode, u8 block_offset) {
    u8 target_buffer[SECTOR_BYTE_SIZE];  //存储目标组描述符内容
    struct ext3_base_information *base_information = (struct ext3_base_information *) inode->i_sb->s_fs_info;
    //获取当前inode内的文件系统信息
    u32 ext3_base = base_information->base;
    //获得ext3的基址地址
    u32 block_size = inode->i_block_size;
    //获得ext3的块大小
    u32 inodes_per_group = base_information->super_block.content->inodes_per_group;
    //获得ext3的每组内的inode数目
    if (inode->i_ino > base_information->super_block.content->inode_count) return -EFAULT;
    //如果inode编号超出总数量则抛出异常
    u32 group_num = (u32) ((inode->i_ino - 1) / inodes_per_group);
    //获取根据当前节点的节点号获取节点的组号
    //下一步目标：找到这一个inode对应的组描述符表
    u32 sect = base_information->first_gdt_sect + group_num / (SECTOR_BYTE_SIZE / EXT3_GROUP_DESC_BYTE);
    //后面部分的算式求一个扇区有多少个组，用组号除以该数据得到inode所在组的组描述符的扇区位置
    u32 offset = group_num % (SECTOR_BYTE_SIZE / EXT3_GROUP_DESC_BYTE);
    //计算当前扇区里第几个组是inode所在的组
    u32 err = vfs_read_block(target_buffer, sect, 1);  //组标识符的全部信息都保存在target_buffer里
    if (err) return 0;
    u32 group_block_num = vfs_get_u32(target_buffer + offset * EXT3_GROUP_DESC_BYTE); //获取组标识符，读取块位图所在块编号
    u32 group_sector_base = base_information->base + group_block_num * (inode->i_block_size >> SECTOR_LOG_SIZE);
    //定位到块位图所在块的起始扇区位置
    u32 group_target_base = group_sector_base + block_offset * (inode->i_block_size >> SECTOR_LOG_SIZE);
    return group_target_base;
}

//todo:根目录下cd..就回到fat32
u32 ext3_fill_inode(struct inode *inode) {  //从硬件获得真实的inode信息并填充到vfs块内
    u32 i;  //For loop
    u8 target_buffer[SECTOR_BYTE_SIZE];
    struct ext3_base_information *ext3_base_information = inode->i_sb->s_fs_info;
    u32 inode_size = ext3_base_information->super_block.content->inode_size;
    u32 inode_table_base = get_group_info_base(inode, EXT3_INODE_TABLE_OFFSET);
    u32 inner_index = (u32) ((inode->i_ino - 1) % ext3_base_information->super_block.content->inodes_per_group);
    //求该inode在组内的序号
    u32 offset_sect = inner_index / (SECTOR_BYTE_SIZE / inode_size);
    //求组内扇区偏移量：计算方式：下标*大小/扇区大小，之所以用两个除法是为了能够避免每个SECTOR里不能刚好容纳若干inode的情况
    u32 inode_sect = inode_table_base + offset_sect;
    kernel_printf("EXT3 : ---------- %d %d\n",inode->i_ino,inode_sect);
    u32 err = vfs_read_block(target_buffer, inode_sect, 1);
    if (err) return -EIO;

    u32 inode_sect_offset = inner_index % (SECTOR_BYTE_SIZE / inode_size);
    // 求inode在扇区内的偏移量
    struct ext3_inode *target_inode = (struct ext3_inode *) (target_buffer + inode_sect_offset * inode_size);
    //计算该inode在指定扇区内的地址
    inode->i_blocks = target_inode->i_blocks;
    inode->i_size = target_inode->i_size;
    inode->i_atime = target_inode->i_atime;
    inode->i_ctime = target_inode->i_ctime;
    inode->i_dtime = target_inode->i_dtime;
    inode->i_mtime = target_inode->i_mtime;
    inode->i_uid = target_inode->i_uid;
    inode->i_gid = target_inode->i_gid;
    //填充文件页到逻辑页的映射表
    inode->i_data.a_page = (u32 *) kmalloc(EXT3_N_BLOCKS * sizeof(u32));
    if (inode->i_data.a_page == 0) return -ENOMEM;
    for (i = 0; i < EXT3_N_BLOCKS; i++)
    {
        inode->i_data.a_page[i] = target_inode->i_block[i];
//        kernel_printf("%d ,",inode->i_data.a_page[i]);
    }
//    kernel_printf("\n");
//    while(1);
    //拷贝数据块
    return 0;
}

u32 fetch_root_data(struct inode *root_inode) {
    debug_start("EXT3-fetch root data\n");
    u32 i; //Loop
    //获取根目录数据
    for (i = 0; i < root_inode->i_blocks; ++i) {
        u32 target_location = root_inode->i_data.a_op->bmap(root_inode, i);
        //计算第i个块的物理地址
        if (target_location < 0) return -EFAULT;
        struct vfs_page *current_page;
        current_page = (struct vfs_page *) kmalloc(sizeof(struct vfs_page));
        if (current_page == 0) return -ENOMEM;
        current_page->page_state = P_CLEAR;
        current_page->page_address = target_location;  //物理页号
        current_page->p_address_space = &(root_inode->i_data);
        INIT_LIST_HEAD(&(current_page->p_lru));
        INIT_LIST_HEAD(&(current_page->page_hashtable));
        INIT_LIST_HEAD(&(current_page->page_list));
        u32 err = root_inode->i_data.a_op->readpage(current_page);
        //读取当前页
        if (err < 0) {
            release_page(current_page);
            return -EFAULT;
        }
        pcache->c_op->add(pcache, (void *) current_page);
        //把current_page加入到pcache
        list_add(&(current_page->page_list), &(current_page->p_address_space->a_cache));
    }
    debug_end("EXT3-fetch root data\n");
    return 0;
};

struct vfsmount *ext3_init_mount(struct dentry *root_entry, struct super_block *super_block) {
    debug_start("EXT3-init mount\n");
    struct vfsmount *ans = (struct vfsmount *) kmalloc(sizeof(struct vfsmount));
    if (ans == 0) return ERR_PTR(-ENOMEM);
    ans->mnt_parent = ans;
    ans->mnt_mountpoint = root_entry; //挂载点
    ans->mnt_root = root_entry; //根目录项
    ans->mnt_sb = super_block; //超级块
    INIT_LIST_HEAD(&(ans->mnt_hash));
    //mnt_hash加入root_mnt链表
    list_add(&(ans->mnt_hash), &(root_mnt->mnt_hash));
    debug_end("EXT3-init mount\n");
    return ans;
}

u32 init_ext3(u32 base) {
    debug_start("EXT3-init ext3\n");
    struct ext3_base_information *base_information = ext3_init_base_information(base);  //读取ext3基本信息
    if (IS_ERR_OR_NULL(base_information)) goto err;

    struct super_block *super_block = ext3_init_super(base_information);         //初始化超级块
    if (IS_ERR_OR_NULL(super_block)) goto err;

    struct dentry *root_dentry = ext3_init_dir_entry(super_block);  //初始化目录项
    if (IS_ERR_OR_NULL(root_dentry)) goto err;
    super_block->s_root = root_dentry;

    struct inode *root_inode = ext3_init_inode(super_block,EXT3_ROOT_INO);      //初始化索引节点
    if (IS_ERR_OR_NULL(root_inode)) goto err;
    root_inode->i_dentry = root_dentry;
    root_dentry->d_inode = root_inode;

    u32 result = ext3_fill_inode(root_inode);                   //填充索引节点
    if (IS_ERR_VALUE(result)) goto err;
    if (IS_ERR_VALUE(fetch_root_data(root_inode))) goto err;

    //将root_inode和root_dentry进行关联
    root_dentry->d_inode = root_inode;
   // list_add(&(root_dentry->d_alias), &(root_inode->i_dentry));

    struct vfsmount *root_mount = ext3_init_mount(root_dentry, super_block);
    goto end;
    err:
    {
        kernel_printf("ERROR: fail to initialize VFS!");
        return -1;
    } //pass
    end:
    debug_end("EXT3-init ext3\n");
    return 0;
}

u32 ext3_check_inode_exists(struct inode *inode) { //返回0说明不存在该inode的位图，返回1则存在且为1
    u8 target_buffer[SECTOR_BYTE_SIZE];
    u32 target_inode_base = get_group_info_base(inode, EXT3_INODE_BITMAP_OFFSET);
    //找到inode数据区的基址
    //然后往前推一个block就是inode位图所在的block
    u32 block_size = ((struct ext3_base_information *) inode->i_sb->s_fs_info)->super_block.content->block_size;
    u32 inodes_per_group = ((struct ext3_base_information *) inode->i_sb->s_fs_info)->super_block.content->inodes_per_group;
    u32 target_sect = target_inode_base;
//    kernel_printf("TARGET INODE BITMAP : %d \n",target_sect);
    //此处获得了inode对应的块的inode位图所在的首个扇区
    u32 group_inner_index = (inode->i_ino - 1) % inodes_per_group;
    //计算inode在这一组内的下标
    //每个inode位图位1位，所以前面有sect_num这么多扇区
    //后面的部分计算的是一个sector里面多少个bit
    u32 sect_addr = target_sect + group_inner_index / (BITS_PER_BYTE * SECTOR_BYTE_SIZE);
    u32 sect_index = group_inner_index % (BITS_PER_BYTE * SECTOR_BYTE_SIZE);  //该sector内的定位
    u32 err = vfs_read_block(target_buffer, sect_addr, 1); //读一块就行，因为一个扇区肯定能包含这个bit
    if (err) return 0;
//    kernel_printf("!!!!!!!!! sect-index : %d\n, group_inner_index : %d\n, sect_addr : %d\n",sect_index,group_inner_index,sect_addr);
    u8 ans = get_bit(target_buffer, sect_index);
//    kernel_printf("target ans: %d\n",ans);
    return ans;
}

/* 返回目录列表中的下一个目录，调由系统调用readdir()用它 */
u32 ext3_readdir(struct file *file, struct getdent *getdent) {
    debug_start("EXT3-readdir\n");
    u32 err, i;
    u32 offset, block;
    u32 actual_page_num;
    struct inode *curInode;   //当前目录项对应的Inode
    struct inode *inode = file->f_dentry->d_inode;  //获取目录文件对应的inode
    struct super_block *super_block = inode->i_sb;  //获取inode对应的超级块
//    struct condition find_condition;
//    struct address_space *target_address_space = inode->i_mapping;
    struct vfs_page *curPage, *targetPage;  //这里没动过page里面的东西，所以不需要标注成dirty
    struct ext3_dir_entry *curDentry;
    u8 *pageTail;
    u8 *curAddr;
//    debug_warning("hello I'm here!!!!!!!\n");
    getdent->count = 0;  //初始化当前目标填充区域的计数器为0
    getdent->dirent = (struct dirent *) kmalloc(sizeof(struct dirent) * (MAX_DIRENT_NUM));
//    debug_warning("hello I'm here!\n");
    kernel_printf("%d\n",inode->i_blocks);
    for (i = 0; i < inode->i_blocks; i++) { //遍历这个目录文件内的所有块
        // 这里证实了inode确实是root_inode
        curPage = ext3_fetch_page(inode, i);
        if (IS_ERR_OR_NULL(curPage)) continue;
        //这里curPage一定已经加载进来了，现在是第i块，现在需要遍历每一个目录项
//        debug_warning("hello I'm here!!!!\n");
        curAddr = curPage->page_data;
        pageTail = curPage->page_data + inode->i_block_size;  //当前页的尾部地址
//        debug_warning("WWWWWWWWWWWWWWW!\n");
        while (*curAddr != 0 && curAddr < pageTail) {
            curDentry = (struct ext3_dir_entry *) curAddr;  //这里不需要做文件类型判断
            curInode = (struct inode *) kmalloc(sizeof(struct inode));
            kernel_printf("curDentry : %s\n",curDentry->file_name);
            if (curInode == 0) return -ENOMEM;
            curInode->i_ino = curDentry->inode_num;
            curInode->i_sb = super_block;
            curInode->i_block_size = inode->i_block_size; //其他的都没有用到所以这里不做初始化
            u32 test = ext3_check_inode_exists(curInode);
//            kernel_printf("TEST ! : %d\n",test);
            if (ext3_check_inode_exists(curInode) == 0) {  //这个inode不存在就往后挪，寻找下一个
                curAddr += curDentry->entry_len;
//                kfree(curInode);
//                kernel_printf("!!!!!!!!");
//                kernel_printf("???????? %d\n",curInode->i_ino);
                continue;
            }
            //能走到这里说明inode对应的文件是存在的
            u8 *file_name = (u8 *) kmalloc(sizeof(u8)*(curDentry->file_name_len + 1)); //一定要拷贝出去，否则可能会出现指针对应的内容被销毁的问题
            if (file_name == 0) return -ENOMEM;
            kernel_strcpy(file_name, curDentry->file_name);
            getdent->dirent[getdent->count].name = file_name;
            getdent->dirent[getdent->count].ino = curInode->i_ino;
            getdent->dirent[getdent->count].type = curDentry->file_type;
            getdent->count++;
            curAddr += curDentry->entry_len;
        }  //页内的目录遍历
    }  //遍历每一页
    debug_end("EXT3-readdir\n");
    return 0;
}

//lpn是target_inode里的逻辑页号
struct vfs_page *ext3_fetch_page(struct inode *target_inode, u32 logical_page_num) {
    struct address_space *target_address_space = &(target_inode->i_data);  //寻找父级目录的索引节点地址
    struct condition find_condition;
    kernel_printf("%d %d %d\n",target_address_space,target_inode,logical_page_num);
    u32 actual_page_num = target_address_space->a_op->bmap(target_inode, logical_page_num);
    if (actual_page_num == 0) return ERR_PTR(-ENOENT);
    find_condition.cond1 = (void *) (&actual_page_num);
    find_condition.cond2 = (void *) target_inode;
    struct vfs_page *curPage = (struct vfs_page *) pcache->c_op->look_up(pcache, &find_condition);
//    debug_info("HHHHHHHHHHHHHHHHHHHH\n");
    if (curPage == 0) {  //说明在pcache里面没找到，这样的话要去外存上加载这一页
        debug_warning("Loading from Disk!\n");
        curPage = (struct vfs_page *) kmalloc(sizeof(struct vfs_page));
        if (curPage == 0) return ERR_PTR(-ENOMEM);
        curPage->p_address_space = target_address_space;
        curPage->page_address = actual_page_num;
        curPage->page_state = P_CLEAR;
        INIT_LIST_HEAD(&(curPage->p_lru));
        INIT_LIST_HEAD(&(curPage->page_list));
        INIT_LIST_HEAD(&(curPage->page_hashtable));
        kernel_printf("OOOOOOOOOOOQQQQQQQ : %d\n",target_address_space->a_op);
        u32 err = target_address_space->a_op->readpage(curPage);  //填完最后一项data就大功告成啦！这里完成了页的预处理
        if (IS_ERR_VALUE(err)) {
            release_page(curPage);
            ERR_PTR(err);
        }
        pcache->c_op->add(pcache, curPage);
        list_add(&(curPage->page_list), &(target_address_space->a_cache)); //把当前已缓存的页添加到已缓存的页链表首部
    }
    return curPage;
}

u32 ext3_write_inode(struct inode *target_inode, struct dentry *parent) {  //因为没有icache所以直接写回外存就可以
    u8 target_buffer[SECTOR_BYTE_SIZE];
    struct ext3_base_information *ext3_base_information = target_inode->i_sb->s_fs_info;
    u32 inode_size = ext3_base_information->super_block.content->inode_size;
    u32 inode_table_base = get_group_info_base(target_inode, EXT3_INODE_TABLE_OFFSET);
    u32 inner_index = (u32) ((target_inode->i_ino - 1) % ext3_base_information->super_block.content->inodes_per_group);
    //求该inode在组内的序号
    u32 offset_sect = inner_index / (SECTOR_BYTE_SIZE / inode_size);
    //求组内扇区偏移量：计算方式：下标*大小/扇区大小，之所以用两个除法是为了能够避免每个SECTOR里不能刚好容纳若干inode的情况
    u32 inode_sect = inode_table_base + offset_sect;
    u32 err = vfs_read_block(target_buffer, inode_sect, 1);
    if (err) return -EIO;
    u32 inode_sect_offset = inner_index % (SECTOR_BYTE_SIZE / inode_size);
    // 求inode在扇区内的偏移量
    struct ext3_inode *new_inode = (struct ext3_inode *) (target_buffer + inode_sect_offset * inode_size);
    new_inode->i_size = target_inode->i_size;
    //修改以后写回
    err = vfs_write_block(target_buffer, inode_sect, 1);
    if (err) return -EIO;
    return 0;
}

u32 ext3_delete_dentry_inode(struct dentry *target_dentry) {
    //注意：索引节点和对应的数据块不一定在同一个块组里，所以块位图和索引节点位图未必在同一个块组里
    //首先清除块位图
    //首先获取块位图
    debug_start("EXT3_DELETE_DENTRY");
    u8 target_sect[SECTOR_BYTE_SIZE];
    struct inode *target_inode = target_dentry->d_inode;
    struct ext3_super_block *super_block = ((struct ext3_base_information *) (target_dentry->d_sb->s_fs_info))->super_block.content;
    u32 inodes_per_group = super_block->inodes_per_group;
    u32 blocks_per_group = super_block->blocks_per_group;
    u32 i, err; //for loop
    struct inode *data_inode = (struct inode *) kmalloc(sizeof(struct inode));  //块所对应的inode
    if (data_inode == 0) return -ENOMEM;
    data_inode->i_block_size = target_inode->i_block_size;
    data_inode->i_sb = target_dentry->d_sb;
    for (i = 0; i < target_inode->i_blocks; i++) {
        u32 actual_block_num = (&(target_inode->i_data))->a_op->bmap(target_inode, i);  //获得真实块号
        u32 index = actual_block_num / (blocks_per_group);  //第几个块
        u32 offset = actual_block_num % (blocks_per_group);  //第几个块内的第几个位图位
        data_inode->i_ino = index * inodes_per_group + offset;  //精确求出目标块的i_ino
        u32 target_group_base = get_group_info_base(data_inode, EXT3_BLOCK_BITMAP_OFFSET); //计算目标块的i_ino所在的块
        u32 sect_addr = target_group_base + offset / (SECTOR_BYTE_SIZE * BITS_PER_BYTE); //计算目标扇区位置
        u32 inner_offset = offset % (SECTOR_BYTE_SIZE * BITS_PER_BYTE); //计算目标地址的扇区内偏移
        //! 注意这里inner_offset计算的时候不要乘任何东西
        err = vfs_read_block(target_sect, sect_addr, 1);
        if (err) {
//            kfree(super_block);
//            kfree(data_inode);
            return -EIO;
        }
        reset_bit(target_sect, inner_offset);
        err = vfs_write_block(target_sect, sect_addr, 1);
        if (err) {
//            kfree(super_block);
//            kfree(data_inode);
            return -EIO;
        }
    }
    //然后清除索引节点位图
    kfree(data_inode);
    u32 target_group_base = get_group_info_base(target_inode, EXT3_INODE_BITMAP_OFFSET);
    u32 offset = (target_inode->i_ino - 1) % (inodes_per_group);  //组内第几个inode
    u32 bitmap_sect_addr = target_group_base + offset / (SECTOR_BYTE_SIZE * BITS_PER_BYTE);  //寻找这个inode位图位的扇区地址
    u32 bitmap_inner_offset = offset % (SECTOR_BYTE_SIZE * BITS_PER_BYTE); //这个inode位图位的扇区内偏移
    err = vfs_read_block(target_sect, bitmap_sect_addr, 1);
    if (err) {
//        kfree(super_block);
        return -EIO;
    }
    reset_bit(target_sect, bitmap_inner_offset);
    err = vfs_write_block(target_sect, bitmap_sect_addr, 1);
    if (err) {
//        kfree(super_block);
        return -EIO;
    }
    //然后清除inode表内数据
    u32 target_inode_table_base = get_group_info_base(target_inode, EXT3_INODE_TABLE_OFFSET);
    u32 data_sect_addr = target_inode_table_base + offset / (SECTOR_BYTE_SIZE / super_block->inode_size);
    u32 data_inner_offset = offset % (SECTOR_BYTE_SIZE / super_block->inode_size);
    //这里继续使用上一步产生的offset，计算在inode表里的位移
    err = vfs_read_block(target_sect, data_sect_addr, 1);
    if (err) {
//        kfree(super_block);
        return -EIO;
    }
    struct ext3_inode* fetched_inode = (struct ext3_inode*) (target_sect + super_block->inode_size * data_inner_offset);
    fetched_inode->i_size = 0;
    fetched_inode->i_blocks = 0;
    fetched_inode->i_gid = 0;
    fetched_inode->i_uid = 0;
    fetched_inode->i_atime = 0;
    fetched_inode->i_ctime = 0;
    fetched_inode->i_mtime = 0;
    fetched_inode->i_dtime = 0;
//    kernel_memset(target_sect + super_block->inode_size * data_inner_offset, 0, super_block->inode_size);
    //直接把整个空间清空太暴力了，这样的话会把重建的功能搞错
    //指针移动到目标地址，并且把指定长度都写0
    err = vfs_write_block(target_sect, data_sect_addr, 1);
    if (err) {
//        kfree(super_block);
        return -EIO;
    }
    //修改sb和gdt
    super_block->free_inode_num--;
    //清除父目录中的该目录项，并把后面的目录项向前移动
    u8 *curAddr, *pageTail;
    u8 *sourceHead = 0;
    u8 *sourceTail = 0;
    u8 *targetHead = 0;
    u8 *targetTail = 0;
    struct qstr newQstr;
    struct inode *dir = target_dentry->d_inode;
    struct vfs_page *target_page;
    //获得该dentry下的a-page
    for (i = 0; i < dir->i_blocks; i++) {  //对该目录下的所有数据块进行扫描
        target_page = ext3_fetch_page(dir, i);
        if (IS_ERR_OR_NULL(target_page)) continue;
        curAddr = target_page->page_data;
        pageTail = curAddr + dir->i_block_size;
        while (*curAddr != 0 && curAddr < pageTail) {
            struct ext3_dir_entry *curDentry = (struct ext3_dir_entry *) curAddr;
            newQstr.name = curDentry->file_name;
            newQstr.len = curDentry->file_name_len;
            if (generic_qstr_compare(&newQstr, &(target_dentry->d_name)) == 0) {     //说明找到了这个inode
                targetHead = curAddr;
                targetTail = curAddr + curDentry->entry_len;
            } else {
                if (targetHead != 0) { // 如果被删除的目录项后面有目录项，需要前移
                    if (sourceHead == 0) sourceHead = curAddr;
                    sourceTail = curAddr + curDentry->entry_len;
                }
            }
            curAddr += curDentry->entry_len;
        }
        if (sourceHead != 0) break;
    }
    if (sourceHead == 0) return -ENOENT;
    if (targetHead != 0) { //如果后面没有的话就不用前移
        kernel_memcpy(sourceHead, targetHead, (int) (targetTail - targetHead));  //后面的向前拷贝
        kernel_memset(sourceHead + (int) (targetTail - targetHead), 0, (int) (sourceTail - sourceHead)); //后面清空
    } else kernel_memset(sourceHead, 0, (int) (sourceTail - sourceHead));  //但是如果后面没有的话还是要删除的
    target_page->page_state = P_DIRTY; //写脏该页，如果找到了的话肯定target_page是有值的
    debug_end("EXT3_DELETE_DENTRY");
    return 0;
}

/* 在特定文件夹中寻找索引节点，该索引节点要对应于dentry中给出的文件名 */
struct dentry *ext3_lookup(struct inode *target_inode, struct dentry *target_dentry, struct nameidata *nd) {
    debug_start("EXT3_LOOKUP");
    struct ext3_base_information *base_information = target_inode->i_sb->s_fs_info;
    u32 i; //for loop
    u8 *pageHead, *pageTail;
    struct qstr newStr;
    /*
//        debug_warning("WWWWWWWWWWWWWWW!\n");
        while (*curAddr != 0 && curAddr < pageTail) {
            curDentry = (struct ext3_dir_entry *) curAddr;  //这里不需要做文件类型判断
            curInode = (struct inode *) kmalloc(sizeof(struct inode));
            kernel_printf("curDentry : %s\n",curDentry->file_name);
            if (curInode == 0) return -ENOMEM;
            curInode->i_ino = curDentry->inode_num;
            curInode->i_sb = super_block;
            curInode->i_block_size = inode->i_block_size; //其他的都没有用到所以这里不做初始化
            u32 test = ext3_check_inode_exists(curInode);
//            kernel_printf("TEST ! : %d\n",test);
            if (ext3_check_inode_exists(curInode) == 0) {  //这个inode不存在就往后挪，寻找下一个
                curAddr += curDentry->entry_len;
//                kfree(curInode);
//                kernel_printf("!!!!!!!!");
//                kernel_printf("???????? %d\n",curInode->i_ino);
                continue;
            }
            //能走到这里说明inode对应的文件是存在的
            u8 *file_name = (u8 *) kmalloc(sizeof(u8)*(curDentry->file_name_len + 1)); //一定要拷贝出去，否则可能会出现指针对应的内容被销毁的问题
            if (file_name == 0) return -ENOMEM;
            kernel_strcpy(file_name, curDentry->file_name);
            getdent->dirent[getdent->count].name = file_name;
            getdent->dirent[getdent->count].ino = curInode->i_ino;
            getdent->dirent[getdent->count].type = curDentry->file_type;
            getdent->count++;
            curAddr += curDentry->entry_len;
        }  //页内的目录遍历*/
    for (i = 0; i < target_inode->i_blocks; i++) {
        struct vfs_page *target_page = ext3_fetch_page(target_inode, i); //加载目标页
        if (IS_ERR_OR_NULL(target_page)) continue;
        pageHead = target_page->page_data;
        pageTail = pageHead + target_inode->i_block_size;  //标记该页的首尾
        while (*pageHead != 0 && pageHead < pageTail) {
            struct ext3_dir_entry *curDentry = (struct ext3_dir_entry *) pageHead;
            newStr.len = curDentry->file_name_len;
            newStr.name = curDentry->file_name;
            if (generic_qstr_compare(&(target_dentry->d_name), &newStr) == 0) {  //如果找到了的话
                nd->dentry = target_dentry;  //写回nd
                struct inode* real_inode = ext3_init_inode(target_inode->i_sb,curDentry->inode_num);
                ext3_fill_inode(real_inode);
                target_dentry->d_inode = real_inode;  //这里要拿到inode绑定上
                target_dentry->d_op = &ext3_dentry_operations;
//                debug_warning("YYYYYYYYYYYYYYYYY\n");
                return target_dentry;       //返回目标dentry
            }
            pageHead += curDentry->entry_len;
        }
    }
    debug_end("EXT3_LOOKUP");
    return 0;
}

//创建ext3文件节点
//返回ERRVALUE说明出错，返回0说明成功
u32 ext3_create(struct inode *dir, struct dentry *target_dentry, struct nameidata * nd){
    debug_start("EXT3_CREATE");
    u8 buffer[SECTOR_BYTE_SIZE];
    struct ext3_base_information* base_information = dir->i_sb->s_fs_info;
    u32 total_group_num = base_information->group_count;
    u32 i,j;  //for loop
    struct ext3_super_block* super_block = base_information->super_block.content;
    u32 inode_per_group = super_block->inodes_per_group;
    if (super_block->free_inode_num <= 0) return -ENOMEM;
    struct inode* new_inode = (struct inode *)kmalloc(sizeof(struct inode));
    if (new_inode == 0) return -ENOMEM;
    new_inode->i_block_size = dir->i_block_size;
    new_inode->i_sb = dir->i_sb;
    u32 position = -1;
    for (i = 0; i < total_group_num; i++) {  //遍历所有的组
        new_inode->i_ino = inode_per_group * i + 1;  //因为0号inode没有映像
        u32 inode_bitmap_base = get_group_info_base(new_inode,EXT3_INODE_BITMAP_OFFSET);  //拿到INODE_BITMAP
        u32 sect_addr = inode_bitmap_base;
        u32 sect_num = EXT3_BLOCK_SIZE_BASE << super_block->block_size >> SECTOR_LOG_SIZE; //把一整块都导入进来
        for (j = 0; j < sect_num; j++){
            err = vfs_read_block(buffer,sect_addr,1);
            if (err) return -EIO;
            position = get_next_zero_bit(buffer,SECTOR_BYTE_SIZE);
            if (position != -1)   //找到了一个合适的位置
            {
                set_bit(buffer,position);  //置位该位图
                new_inode->i_ino = inode_per_group * i + position + 1;  //0号inode问题
                err = vfs_write_block(buffer,sect_addr,1);
                if (err) return -EIO;
            }
            sect_addr += SECTOR_BYTE_SIZE; //向后移动一个扇区，这样可以省一省buffer
            if (position != -1) break;
        }
        if (position != -1) break;
    }
    //下一步：在磁盘上找到这个inode
    struct inode* allocated_inode = ext3_init_inode(dir->i_sb,new_inode->i_ino);
    if (IS_ERR_OR_NULL(allocated_inode)) return *((u32*) allocated_inode);  //如果错误的话这里一定会返回错误码
    ext3_fill_inode(allocated_inode);
    allocated_inode->i_type = EXT3_NORMAL;
//    kfree(new_inode);
    u32 inode_size = base_information->super_block.content->inode_size;
    u32 inode_table_base = get_group_info_base(allocated_inode, EXT3_INODE_TABLE_OFFSET);
    u32 inner_index = (u32) ((new_inode->i_ino - 1) % base_information->super_block.content->inodes_per_group);
    u32 offset_sect = inner_index / (SECTOR_BYTE_SIZE / inode_size);
    u32 inode_sect = inode_table_base + offset_sect;
    u32 err = vfs_read_block(buffer, inode_sect, 1);
    if (err) return -EIO;
    u32 inode_sect_offset = inner_index % (SECTOR_BYTE_SIZE / inode_size);
    kernel_memcpy(buffer + inode_sect_offset * inode_size,allocated_inode,inode_size);
    err = vfs_write_block(buffer, inode_sect, 1);
    if (err) return -EIO;
    target_dentry->d_inode = allocated_inode;
//    target_dentry->d_parent = container_of(dir,struct dentry,d_inode);
    target_dentry->d_parent = dir->i_dentry;
    nd->dentry = target_dentry;
    debug_end("EXT3_CREATE");
    return 0;
}


u32 ext3_mkdir(struct inode *dir, struct dentry *target_dentry, u32 mode) {  //忽略mode
    debug_start("EXT3-MKDIR");
    struct nameidata* nd = (struct nameidata*) kmalloc(sizeof(struct nameidata));
    if (nd == 0) return -ENOMEM;
    u32 err = ext3_create(dir,target_dentry,nd);
    target_dentry->d_inode->i_op = &(ext3_inode_operations[0]);
    if (IS_ERR_VALUE(err)) return err;
    target_dentry->d_inode->i_type = EXT3_DIR;
    debug_end("EXT3-MKDIR");
    return 0;
}

u32 ext3_rmdir(struct inode* dir, struct dentry* target_dentry){
    debug_start("EXT3-RMDIR");
    u32 err = ext3_delete_dentry_inode(target_dentry);
    list_del(&(target_dentry->d_alias));
    list_del(&(target_dentry->d_lru));
    list_del(&(target_dentry->d_subdirs));
    if (IS_ERR_VALUE(err)) return err;
    debug_end("EXT3-RMDIR");
    return 0;
}

struct inode* fetch_journal_inode(struct super_block *super_block){
    struct ext3_base_information* base_information = super_block->s_fs_info;
    struct inode* allocated_inode = ext3_init_inode(super_block,base_information->super_block.content->journal_inum);
    if (IS_ERR_OR_NULL(allocated_inode)) return allocated_inode;
    ext3_fill_inode(allocated_inode);
    return allocated_inode;
}