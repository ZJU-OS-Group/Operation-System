
// TODO：现在只是抄了cy的
// 检查dentry处是否是挂载点，如果是，就更新
u32 follow_mount(struct vfsmount **mnt, struct dentry **dentry) {
    u32 res = 0;

    // 查找挂载的对象
    while ((*dentry)->d_mounted) {
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

    return found;
}