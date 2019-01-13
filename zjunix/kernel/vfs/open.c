#include <zjunix/vfs/vfs.h>
#include <driver/vga.h>
#include <zjunix/debug/debug.h>

extern struct dentry  *root_dentry;
extern struct dentry  *pwd_dentry;
// 根据路径名，返回文件操作符file
struct file *vfs_open(const u8 *filename, u32 flags) {
    debug_start("[open.c: vfs_open:7]\n");
//    kernel_printf("open.c vfs_open: filename: %s\n", filename);
    u32 err;
    struct nameidata nd;

    /* 根据参数flags判断不同的打开方式 */
    if ((flags+1) & O_ACCMODE) {
        flags++;
    }
    if (flags & O_TRUNC) {
        flags |= 2;
    }

    err = open_namei(filename,flags,&nd);
    if (!err) {
        debug_end("[open.c: vfs_open:22]\n");
        return dentry_open(nd.dentry, nd.mnt, flags);
    }
    debug_err("[open.c: vfs_open:25]\n");
    return ERR_PTR(err);
}

struct file *get_empty_file_pointer() {
    struct file* f = (struct file* ) kmalloc ( sizeof(struct file) );
    INIT_LIST_HEAD(&f->f_u.fu_list);
    return f;
}

// 根据查询到的nameidata结构填写file结构，从而完成open的操作
struct file *dentry_open(struct dentry* dentry, struct vfsmount* mnt, u32 flags) {
    debug_start("[open.c: dentry_open:37]\n");
    debug_warning("dentry_open: 38  ");
    kernel_printf("open: %d, %s\n",dentry, dentry->d_name);
    kernel_printf("root dentry:%d\n", root_dentry);
    kernel_printf("pwd dentry:%d\n", pwd_dentry);
    struct file *f;
    struct inode *inode;
    u32 err;

    err = -ENFILE;
    f = get_empty_file_pointer(); // 分配file结构f
    if (!f) {
        goto cleanup_dentry;
    }

    f->f_flags = flags;
    f->f_flags &= ~(O_CREAT);
    f->f_mode = (flags+1) & O_ACCMODE;
    inode = dentry->d_inode;
    if (f->f_mode & FMODE_WRITE) {
        // TODO: continue...
    }
    f->f_mode = ((flags+1) & O_ACCMODE) | FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE | FMODE_READ | FMODE_WRITE;
    f->f_dentry = dentry;
    f->f_vfsmnt = mnt;
    f->f_pos = 0;
    f->f_op = inode->i_fop; // 附上节点的操作函数
    debug_end("[open.c: dentry_open:60]\n");
    return f;

    cleanup_dentry:
    dput(dentry);
    debug_err("[open.c: dentry_open:65]\n");
    return ERR_PTR(err);
}

int vfs_close(struct file *file) {
    debug_start("[open.c: vfs_close:70]\n");
    int retval;

    /* Report and clear outstanding errors */
    retval = file->f_error;
    if (retval)
        file->f_error = 0;

    if (!file->f_count) { // 如果文件引用计数为0，表示没有被打开过，就没必要关闭了
        kernel_puts("VFS: Close: file count is 0\n",VGA_RED,VGA_BLACK);
        debug_end("[open.c: vfs_close:80]\n");
        return retval;
    }

    // 如果flush函数存在，将内容写回磁盘
    if (file->f_op && file->f_op->flush) {
        int err = file->f_op->flush(file);
        if (!err)
            kfree(file);
        if (!retval)
            retval = err;
    }
    debug_end("[open.c: vfs_close:92]\n");
    return retval;
}