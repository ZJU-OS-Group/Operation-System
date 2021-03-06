#include <zjunix/vfs/vfs.h>
#include <zjunix/vfs/err.h>
#include <zjunix/vfs/errno.h>
#include <driver/vga.h>
#include <zjunix/debug/debug.h>

/****************************** 外部变量 *******************************/
extern struct dentry                    * pwd_dentry;   /* 当前工作目录 */
extern struct vfsmount                  * pwd_mnt;


const char* __my_strcat(const u8* dest,const u8* src)
{
    char* res = (char *) dest;
    while(*res) res++;// *res != '\0'
    while((*res++ = *src++));
    return dest;
}

// cat：连接文件并打印到标准输出设备上
u32 vfs_cat(const u8 *path) {
    debug_start("[usr.c: vfs_cat:24]\n");
    u8 *buf;
    u32 err;
    u32 base;
    u32 file_size;
    struct file *file;

    // 打开文件
    file = vfs_open(path, O_RDONLY);
    // 处理打开文件的错误
    if (IS_ERR_OR_NULL(file)){
        if ( PTR_ERR(file) == -ENOENT )
            kernel_printf("No such file.\n");
        debug_err("[usr.c: vfs_cat:37]\n");
        return PTR_ERR(file);
    }

    // 读取文件内容到缓存区
    base = 0;
    file_size = file->f_dentry->d_inode->i_size;
    kernel_printf("usr.c: 44 cat: %d", file_size);

    buf = (u8*) kmalloc ((unsigned int) (file_size + 1));
    if ( vfs_read(file, buf, file_size, &base) != file_size ) {
        debug_err("[usr.c: vfs_cat:47]read file size error\n");
        return 1;
    }

    // 打印buf里面的内容
    buf[file_size] = 0;
    kernel_printf("%s\n", buf);

    // 关闭文件并释放内存
    err = vfs_close(file);
    if (err)
        return err;

    kfree(buf);
    debug_end("[usr.c: vfs_cat:61]\n");
    return 0;
}
// touch: 新建文件
u32 vfs_touch(const u8 * path)
{
    debug_start("[usr.c: vfs_touch:67]\n");
    u32 err=0;
    struct dentry *dentry;
    struct nameidata nd;
    // 找到path对应的nd信息
    extern struct dentry* root_dentry;
    err = path_lookup(path,LOOKUP_PARENT,&nd);
    if (err)
        return err;
    // 若是没有则创建dentry
    dentry = lookup_create(&nd, 1);
//    err = PTR_ERR(dentry);
    if (!IS_ERR_OR_NULL(dentry)) {
        struct inode * dir = nd.dentry->d_inode;
        if (!dir->i_op || !dir->i_op->mkdir) {
            debug_err("[usr.c: vfs_mkdir:82] operation not permitted\n");
            return -EPERM;
        }

        // 调用文件系统对应的touch
        kernel_printf("777777777777777777777777777 %d %d\n",container_of(&(nd.dentry->d_inode),struct dentry,d_inode),root_dentry);
        err = dir->i_op->touch(nd.dentry->d_inode, dentry, 0);
        dput(dentry);
    }
    dput(nd.dentry);
    debug_end("[usr.c: vfs_touch:91]\n");
    return err;
}
// mkdir：新建目录
u32 vfs_mkdir(const u8 * path) {
    debug_start("[usr.c: vfs_mkdir:67]\n");
    u32 err=0;
    struct dentry *dentry;
    struct nameidata nd;
    // 找到path对应的nd信息
    extern struct dentry* root_dentry;
    err = path_lookup(path,LOOKUP_PARENT,&nd);
    if (err)
        return err;
    // 若是没有则创建dentry
    dentry = lookup_create(&nd, 1);
//    err = PTR_ERR(dentry);
    if (!IS_ERR_OR_NULL(dentry)) {
        struct inode * dir = nd.dentry->d_inode;
        if (!dir->i_op || !dir->i_op->mkdir) {
            debug_err("[usr.c: vfs_mkdir:82] operation not permitted\n");
            return -EPERM;
        }

        // 调用文件系统对应的mkdir
        kernel_printf("777777777777777777777777777 %d %d\n",container_of(&(nd.dentry->d_inode),struct dentry,d_inode),root_dentry);
        err = dir->i_op->mkdir(nd.dentry->d_inode, dentry, 0);
        dput(dentry);
    }
    dput(nd.dentry);
    debug_end("[usr.c: vfs_mkdir:91]\n");
    return err;
}

// rm：删除文件
u32 vfs_rm(const u8 * path) {
    debug_start("[usr.c: vfs_rm:97]\n");
    u32 err;
    struct nameidata nd;

    /* 查找目的文件 */
    err = path_lookup(path, 0, &nd);
    if (err == -ENOENT) { // 返回No such file or directory的错误信息
        kernel_printf("No such file.\n");
        return err;
    } else if (IS_ERR_VALUE(err)) { // 如果有其他错误
        kernel_printf("Other error: %d\n", err);
        return err;
    }

    /* 删除对应文件在外存上的相关信息 */
    // 由dentry去找inode去找super_block去调用相关的删除文件的操作
    err = nd.dentry->d_inode->i_sb->s_op->delete_dentry_inode(nd.dentry);
    if (err) {
        debug_err("[usr.c: vfs_rm:115]\n");
        return err;
    }

    /* 删除缓存中的inode */
    nd.dentry->d_inode = 0;
    debug_end("[usr.c: vfs_rm:121]\n");
    return 0;
}

// rm -r：递归删除目录
//u32 vfs_rm_r(const u8 * path) {
//    u32 err;
//    struct file *file;
//    struct getdent getdent;
//    struct nameidata nd;
//
//    // 打开目录
//    if (path[0] == 0) {
//        kernel_printf("No parameter.\n");
//        return -ENOENT;
//    }
//    else
//        file = vfs_open(path, LOOKUP_DIRECTORY);
//    if (file->f_dentry->d_inode->i_type!=FTYPE_DIR)
//        vfs_rm(path);
//    if (IS_ERR_OR_NULL(file)) {
//        if (PTR_ERR(file) == -ENOENT)
//            kernel_printf("Directory not found!\n");
//        else
//            kernel_printf("Other error: %d\n", -PTR_ERR(file));
//        return PTR_ERR(file);
//    }
//    err = file->f_op->readdir(file, &getdent);
//    if (err)
//        return err;
//    // 遍历目录下每一项，若是文件直接调用rm，否则递归调用vfs_rm_r
//    for (int i = 0; i < getdent.count; ++i) {
//        if (getdent.dirent[i].type == FTYPE_DIR) {
//            const u8* tmp_path = __my_strcat(path, "/");
//            const u8* new_path = __my_strcat(tmp_path, getdent.dirent[i].name);
//            vfs_rm_r(new_path);
//        } else if (getdent.dirent[i].type == FTYPE_NORM) {
//            const u8* tmp_path = __my_strcat(path, "/");
//            const u8* new_path = __my_strcat(tmp_path, getdent.dirent[i].name);
//            vfs_rm(new_path);
//        } else if (getdent.dirent[i].type == FTYPE_LINK) {
//        } else {
//            return -ENOENT;
//        }
//    }
//    // 删除目录本身的dentry和inode
//    err = path_lookup(path, 0, &nd);
//    if (err == -ENOENT) { // 返回No such file or directory的错误信息
//        kernel_printf("No such directory.\n");
//        return err;
//    } else if (IS_ERR_VALUE(err)) { // 如果有其他错误
//        kernel_printf("Other error: %d\n", err);
//        return err;
//    }
//    nd.dentry->d_inode->i_op->rmdir(nd.dentry->d_inode,nd.dentry);
//    nd.dentry->d_op->d_delete(nd.dentry);
//    return 0;
//}

// rm -r：递归删除目录
u32 vfs_rm_r(const u8 * path) {
    debug_start("[usr.c: vfs_rm_r:183]\n");
    u32 err = 0;
    struct dentry * dentry;
    struct nameidata nd;

    err = path_lookup(path, LOOKUP_PARENT, &nd);
    if (err)
        return err;

    switch (nd.last_type) {
        case LAST_DOTDOT:
            err = -ENOTEMPTY;
            dput(nd.dentry);
            debug_info("[usr.c: vfs_rm_r:196] dotdot\n");
            return err;
        case LAST_DOT:
            err = -EINVAL;
            dput(nd.dentry);
            debug_info("[usr.c: vfs_rm_r:201] dot\n");
            return err;
        case LAST_ROOT:
            err = -EBUSY;
            dput(nd.dentry);
            debug_info("[usr.c: vfs_rm_r:206] root\n");
            return err;
        default:break;
    }
    dentry = __lookup_hash(&nd.last, nd.dentry, 0);
    err = PTR_ERR(dentry);
    if (!IS_ERR(dentry)) {
        struct inode * dir = nd.dentry->d_inode;
        if (!dir->i_op || !dir->i_op->rmdir)
            return -EPERM;
        if (dentry->d_mounted) {
            dput(nd.dentry);
            return -EBUSY;
        }
        err = dir->i_op->rmdir(dir,dentry);
        if (!err) {
            dentry->d_inode->i_flags |= S_DEAD;
            dentry_iput(dentry);
        }
        dput(dentry);
    }
    dput(nd.dentry);
    debug_end("[usr.c: vfs_rm_r:228]\n");
    return err;
}

// ls：列出目录项下的文件信息
u32 vfs_ls(const u8 * path) {
    debug_start("[usr.c: vfs_ls:234]\n");
    u32 err;
    struct file *file;
    struct getdent getdent;

    // 打开目录
    if (path[0] == 0)
        file = vfs_open(".", LOOKUP_DIRECTORY);
    else
        file = vfs_open(path, LOOKUP_DIRECTORY);
    if (IS_ERR_OR_NULL(file)) {
        if (PTR_ERR(file) == -ENOENT)
            kernel_printf("Directory not found!\n");
        else
            kernel_printf("Other error: %d\n", -PTR_ERR(file));
        debug_err("[usr.c: vfs_ls:249] vfs open err\n");
        return PTR_ERR(file);
    }

    // 读取目录到gedent中
    err = file->f_op->readdir(file, &getdent);
//    kernel_printf("usr.c: vfs_ls: 255 err: %d\n", err);
    if (err)
        return err;

//    kernel_printf("usr.c vfs_ls: 259 count: %d\n", getdent.count);
    // 遍历gedent，向屏幕打印结果
    for (int i = 0; i < getdent.count; ++i) {
        if (getdent.dirent[i].type == FTYPE_DIR)
            kernel_puts(getdent.dirent[i].name, VGA_GREEN,VGA_BLACK);
        else if (getdent.dirent[i].type == FTYPE_NORM)
            kernel_puts(getdent.dirent[i].name, VGA_WHITE,VGA_BLACK);
        else if (getdent.dirent[i].type == FTYPE_LINK)
            kernel_puts(getdent.dirent[i].name, VGA_BLUE,VGA_BLACK);
        else if (getdent.dirent[i].type == FTYPE_UNKOWN)
            kernel_puts(getdent.dirent[i].name, VGA_RED, VGA_BLACK);
        kernel_printf(" ");
    }
    kernel_printf("\n");
    debug_end("[usr.c: vfs_ls:271]\n");
    return 0;
}

// cd：切换当前工作目录
u32 vfs_cd(const u8 * path) {
    debug_start("[usr.c: vfs_cd:277]\n");
    u32 err;
    struct nameidata nd;

    /* 查找目的目录 */
    err = path_lookup(path, LOOKUP_DIRECTORY, &nd);
    if (err == -ENOENT) { // 返回No such file or directory的错误信息
        kernel_printf("No such directory.\n");
        return err;
    } else if (IS_ERR_VALUE(err)) { // 如果有其他错误
        kernel_printf("Other error: %d\n", err);
        return err;
    }

    /* 一切顺利，更改dentry和mnt */
    pwd_dentry = nd.dentry;
    pwd_mnt = nd.mnt;
    debug_end("[usr.c: vfs_cd:294]\n");
    return 0;
}

// mv：移动文件（同目录下移动则为重命名）
u32 vfs_mv(const u8 * path) {

}

// 新建一个文件
u32 vfs_create(const u8 * path) {
    debug_start("[usr.c: vfs_create:305]\n");
    u32 err = 0;
    struct dentry *dentry;
    struct nameidata nd;

    // 找到path对应的nd信息
    err = path_lookup(path,LOOKUP_PARENT,&nd);
    if (err)
        return err;
    // 若是没有则创建dentry
    dentry = lookup_create(&nd, 1);
    err = PTR_ERR(dentry);
    if (!IS_ERR(dentry)) {
        struct inode * dir = nd.dentry->d_inode;
        debug_info("[usr.c: vfs_create:319] dentry error\n");
        if (!dir->i_op || !dir->i_op->mkdir)
            return -ENOSYS;
        // 调用文件系统对应的create
        err = dir->i_op->create(dir, dentry, &nd);
        dput(dentry);
    }
    dput(nd.dentry);
    debug_end("[usr.c: vfs_create:327]\n");
    return err;
}


//void vfs_pwd() {
//    kernel_puts()
//}