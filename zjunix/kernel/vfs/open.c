#include <zjunix/vfs/vfs.h>

//
struct file *vfs_open(const u8 *filename, u32 flags, u32 mode) {
    u32 err;
    struct nameidata nd;

    /* 根据参数flags判断不同的打开方式 */
    if ((flags+1) & O_ACCMODE) {
        flags++;
    }
    if (flags & O_TRUNC) {
        flags |= 2;
    }

    err = open_namei(filename,flags,mode,&nd);
    if (!err) {
        return dentry_open(nd.dentry, nd.mnt, flags);
    }
    return ERR_PTR(err);
}

struct file *get_empty_file_pointer() {
    struct file* f = (struct file* ) kmalloc ( sizeof(struct file) );
    INIT_LIST_HEAD(&f->f_list);
    return f;
}

// 根据查询到的nameidata结构填写file结构，从而完成open的操作
struct file *dentry_open(struct dentry* dentry, struct vfsmount* mnt, u32 flags) {
    struct file *f;
    struct inode *inode;
    u32 err;

    err = -ENFILE;
    f = get_empty_file_pointer(); // 分配file结构f
    if (!f) {
        goto cleanup_dentry;
    }

    f->f_flags = flags;
    f->f_mode = (flags+1) & O_ACCMODE;
    inode = dentry->d_inode;
    if (f->f_mode & FMODE_WRITE) {
    }
    f->f_dentry = dentry;
    // TODO: continue...

cleanup_dentry:
    dput(dentry);
    return ERR_PTR(error);
}
