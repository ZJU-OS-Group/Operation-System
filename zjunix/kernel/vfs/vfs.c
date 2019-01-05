#include <zjunix/vfs/vfs.h>
#include <zjunix/log.h>
#include <driver/vga.h>
#include "fat32.h"

// global
struct master_boot_record   * MBR;
struct dentry               * root_dentry;      /* 根目录 */
struct dentry               * pwd_dentry;       /* 工作目录 */
struct vfsmount             * root_mnt;         /* 根挂载点 */
struct vfsmount             * pwd_mnt;          /* 当前挂载点 */

// 初始化虚拟文件系统
u32 init_vfs() {
    u32 err;

    /* 读取主引导记录 */
    err = vfs_read_MBR();
    if ( IS_ERR_VALUE(err) ){
        log(LOG_FAIL, "vfs_read_MBR()");
        goto vfs_init_err;
    }
    log(LOG_OK, "vfs_read_MBR()");

    /* 初始化缓存区域 */
    err = init_cache();
    if ( IS_ERR_VALUE(err) ){
        log(LOG_FAIL, "init_cache()");
        goto vfs_init_err;
    }
    log(LOG_OK, "init_cache()");

    /* 初始化第一个分区FAT32 */
    err = init_fat32(MBR->m_base[0]);           // 第一个分区为FAT32，读取元数据
    if ( IS_ERR_VALUE(err) ){
        log(LOG_FAIL, "init_fat32()");
        goto vfs_init_err;
    }
    log(LOG_OK, "init_fat32()");

    /* 初始化第二个分区EXT3 */
    err = init_ext3(MBR->m_base[1]);
    if ( IS_ERR_VALUE(err) ){
        log(LOG_FAIL, "init_ext3()");
        goto vfs_init_err;
    }
    log(LOG_OK, "init_ext3()");

    /* 挂载EXT3 */
    err = mount_ext3();
    if ( IS_ERR_VALUE(err) ){
        log(LOG_FAIL, "mount_ext3()");
        goto vfs_init_err;
    }
    log(LOG_OK, "mount_ext3()");

    return 0;

vfs_init_err:
    kernel_printf("vfs_init_err: %d\n", (int)(-err));  // 发生错误，则打印错误代码
    return err;
}

// 读取MBR（主引导分区）
u32 vfs_read_MBR() {
    u8  *DPT_cur;
    u32 err;
    u32 part_base;

    // 从外存读入MBR信息
    MBR = (struct master_boot_record*) kmalloc( sizeof(struct master_boot_record) );
    if (MBR == 0)
        return -ENOMEM;

    kernel_memset(MBR->m_data, 0, sizeof(MBR->m_data));
    if ( err = read_block(MBR->m_data, 0, 1) )              // MBR在外存的0号扇区
        goto vfs_read_MBR_err;

    // 完善MBR相关信息
    MBR->m_count = 0;
    DPT_cur = MBR->m_data + 446 + 8;
    while ((part_base = get_u32(DPT_cur)) && MBR->m_count != DPT_MAX_ENTRY_COUNT ) {
        MBR->m_base[MBR->m_count++] = part_base;
        DPT_cur += DPT_ENTRY_LEN;
    }
    return 0;

    vfs_read_MBR_err:
    kfree(MBR);
    return -EIO;
}