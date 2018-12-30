#include <zjunix/vfs/vfs.h>
#include <zjunix/vfs/err.h>
#include <zjunix/vfs/errno.h>

/****************************** 外部变量 *******************************/
extern struct dentry                    * pwd_dentry;   /* 当前工作目录 */
extern struct vfsmount                  * pwd_mnt;
// TODO: vfs_open
// TODO: 文件打开方式定义（先定义了一部分，之后需要再加）

// cat：连接文件并打印到标准输出设备上
u32 vfs_cat(const u8 *path) {
    u8 *buf;
    u32 err;
    u32 base;
    u32 file_size;
    struct file *file;

    // 打开文件
    file = vfs_open(path, O_RDONLY, base);
    // 处理打开文件的错误
    if (IS_ERR_OR_NULL(file)){
        if ( PTR_ERR(file) == -ENOENT )
            kernel_printf("File not found!\n");
        return PTR_ERR(file);
    }

    // 读取文件内容到缓存区
     base = 0;
     file_size = file->f_dentry->d_inode->i_size;

    // buf = (u8*) kmalloc (file_size + 1);
    // if ( vfs_read(file, buf, file_size, &base) != file_size )
    //     return 1;

    // // 打印buf里面的内容
    // buf[file_size] = 0;
    // kernel_printf("%s\n", buf);

    // // 关闭文件并释放内存
    // err = vfs_close(file);
    // if (err)
    //     return err;

    // kfree(buf);
    return 0;
}

// mkdir：新建目录
u32 vfs_mkdir(const u8 *) {

}

// rm：删除文件
u32 vfs_rm(const u8 *) {

}

// ls：列出目录项下的文件信息
u32 vfs_ls(const u8 *) {

}

// cd：切换当前工作目录
u32 vfs_cd(const u8 * path) {
    u32 err;
    struct nameidata nd;

    /* 查找目的目录 */
    err = path_lookup(path, LOOK_DIRECTORY, &nd);
    if (err = -ENOENT) { // 返回No such file or directory的错误信息
        kernel_printf("No such directory.\n");
        return err;
    } else if (IS_ERR_VALUE(err)) { // 如果有其他错误
        return err;
    }

    /* 一切顺利，更改dentry和mnt */
    pwd_dentry = nd.dentry;
    pwd_mnt = nd.mnt;
    return 0;
}

// mv：移动文件或文件夹（同目录下移动则为重命名）
u32 vfs_mv(const u8 *) {

}
