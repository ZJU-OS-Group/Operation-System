#include <zjunix/vfs/vfs.h>
#include <zjunix/debug/debug.h>

extern struct dentry            * root_dentry;
extern struct vfsmount          * root_mnt;

u32 mount_ext3(){
    u32 err;
    struct      qstr            qstr;
    struct      dentry          * dentry;
    struct      vfsmount        * mnt;
    struct      list_head       * a;
    struct      list_head       * begin;

    a = &(root_mnt->mnt_hash);
    begin = a;
    a = a->next;
    while ( a != begin ){
        mnt = container_of(a, struct vfsmount, mnt_hash);
        if ( kernel_strcmp(mnt->mnt_sb->s_type->name ,"ext3") == 0 )
            break;
    }

    if ( a == begin )
        return -ENOENT;

    qstr.name = "ext3";
    qstr.len = 4;


    // 创建 /ext3 对应的dentry。而且暂时不需要创建对应的inode
    dentry = d_alloc(root_dentry, &qstr);
    if (dentry == 0)
        return -ENOENT;

    dentry->d_mounted = 1;
    dentry->d_inode = mnt->mnt_root->d_inode;
    dentry->d_parent = root_dentry;
    kernel_printf("mount ext3 in %d, %d\n", dentry->d_parent);
    mnt->mnt_mountpoint = dentry;
    mnt->mnt_parent = root_mnt;
//    mnt->mnt_root = root_dentry;
    mnt->mnt_root = dentry;
    debug_warning("Mount ext3 successfully!\n");
    return 0;
}


// TODO：现在只是抄了cy的
// 检查dentry处是否是挂载点，如果是，就更新
u32 follow_mount(struct vfsmount **mnt, struct dentry **dentry) {

    u32 res = 0;

    // 查找挂载的对象
    while ((*dentry)->d_mounted) {
        debug_warning("mount.c follow_mount 53:\n");
        struct vfsmount *mounted = lookup_mnt(*mnt, *dentry);
        if (!mounted)
            break;

        // 找到挂载点，更换dentry和mnt的信息
        *mnt = mounted;
        dput(*dentry);
        *dentry = mounted->mnt_root;
        dget(*dentry);
        res = 1;
    }

    return res;
}

// TODO: 现在只是抄了cy的
struct vfsmount * lookup_mnt(struct vfsmount *mnt, struct dentry *dentry) {
    struct list_head *head = &(mnt->mnt_hash);
    struct list_head *tmp = head;
    struct vfsmount *p, *found = 0;
    debug_start("mount.c: lookup_mnt:74\n");
    // 在字段为hash的双向链表寻找。这里有所有已安装的文件系统的对象
    // 这里并没有为其实现hash查找，仅普通链表
    for (;;) {
        tmp = tmp->next;
        p = 0;
        if (tmp == head)
            break;
        p = list_entry(tmp, struct vfsmount, mnt_hash);
        if (p->mnt_parent == mnt && p->mnt_mountpoint == dentry) {
            found = p;
            break;
        }
    }
    debug_end("mount.c lookup:88");
    kernel_printf("result: %d\n", found);

    return found;
}
