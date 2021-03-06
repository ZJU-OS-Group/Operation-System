#include <zjunix/vfs/vfs.h>
#include <zjunix/debug/debug.h>
#include <intr.h>
#include <zjunix/vfs/ext3.h>


/********************************** 外部变量 ************************************/
extern struct cache     *dcache;
extern struct dentry    *root_dentry;
extern struct dentry    *pwd_dentry;
extern struct vfsmount  *root_mnt;
extern struct vfsmount  *pwd_mnt;

// 打开操作中的主要函数，通过路径名得到相应的nameidata结构
// 通过path_work()轮流调用real_lookup()函数，再调用各文件系统自己的inode_op->lookup
// 得到给定路径名对应的dentry和vfsmount结构
u32 open_namei(const u8 *pathname, u32 flag, struct nameidata *nd){
    debug_start("[namei.c: open_namei:16]\n");
    u32 err = 0;
    struct dentry *dentry;
    struct dentry *dir;

    /* 仅查找，不用create */
    if (!(flag&O_CREAT)) {
        kernel_printf("open_namei: %s\n", pathname);
        err = path_lookup(pathname, LOOKUP_FOLLOW, nd);
        if (err)
            return err;
//        dentry = nd->dentry;
        debug_end("[namei.c: open_namei:27]\n");
        return 0;
    }
    /* create，查找父目录 */
    err = path_lookup(pathname, LOOKUP_PARENT, nd);
    if(err)
        return err;

    /* 拥有parent和最后一个分量，但是不知道这个分量是否已经存在，即是否需要新建 */
    /* 判断是否需要新建 */
    if (nd->last_type != LAST_NORM || nd->last.name[nd->last.len]) {
        dput(nd->dentry); // dentry放到cache里面
        debug_end("[namei.c: open_namei:39]\n");
        return err;
    }

    dir = nd->dentry;
    nd->flags &= ~LOOKUP_PARENT;
    dentry = __lookup_hash(&nd->last, nd->dentry, nd); // 根据父目录和分量名字查找对应项

    /* 打不开目录 */
    if (IS_ERR(dentry)) {
        dput(nd->dentry);
        debug_end("[namei.c: open_namei:50]\n");
        return PTR_ERR(dentry);
    }

    /* 目录不存在，则证明该分量需要新建 */
    if (!dentry->d_inode) {
        err = dir->d_inode->i_op->create(dir->d_inode, dentry, nd);
        dput(nd->dentry);
        nd->dentry = dentry; // 把nd中的dentry更换为新建出来的
        if (err) {
            dput(nd->dentry);
            return err;
        } else {
            debug_end("[namei.c: open_namei:63]\n");
            return 0;
        }
    }

    /* 若分量已经存在 */
    dput(nd->dentry);
    nd->dentry = dentry;
    debug_end("[namei.c: open_namei:71]\n");
    return 0;
}

u32 path_lookup(const u8 * name, u32 flags, struct nameidata *nd) {
    debug_start("[namei.c: path_lookup:76]\n");
    nd->last_type = LAST_ROOT;
    nd->flags = flags;
    nd->depth = 0;
    if ( *name == '/' ) { // 绝对路径
        dget(root_dentry);
        nd->mnt = root_mnt;
        nd->dentry = root_dentry;
    } else { // 相对路径
        dget(pwd_dentry);
        nd->mnt = pwd_mnt;
        nd->dentry = pwd_dentry;
    }
//    kernel_printf("namei.c 92 root->dentry: %d\n", root_dentry);
//    kernel_printf("namei.c 93 root->mnt: %d\n", root_mnt);
    debug_end("[namei.c: path_lookup:90]\n");
//    kernel_printf("namei.c 92 nd->dentry: %d\n", nd->dentry);
//    kernel_printf("namei.c 93 nd->mnt: %d\n", nd->mnt);
//    kernel_printf("path_lookup: %s\n", name);
    return link_path_walk(name, nd);
}

// 基本的路径解析函数，有路径名查找到最终的dentry结构，将信息保存在nd中返回
u32 link_path_walk(const u8 *name, struct nameidata *nd) {
    debug_start("[namei.c: link_path_walk:96]\n");
    u32 err = 0;
    u32 lookup_flags = nd->flags;
    struct path next;
    struct qstr this;

    int flag_mount=0; // 是否已经到了挂载点

    // 跳过开始的'/'，'/test/zc'变成'test/zc'
    while(*name=='/'||*name==' ') name++;

    if (!*name) {
        kernel_printf("/ or .\n");
        // 直接返回，因为就只有空格或根目录
        return 0;
    }
    // 解析每一个分量
    while (1) {
        u8 c;
        this.name = name;
        kernel_printf("namei.c 119: name: %s\n", name);
        do { // 处理到'/'为止
            name++;
            c = *name;
        } while (c && (c != '/'));
        this.len = (u32) (name - this.name);

        if (!c) { // 解析完毕了，后面没有'/'了
            debug_info("[namei.c: link_path_walk:124] go to last component\n");
            goto last_component;
        }

        while (*++name == '/'); // 跳过所有的'/'，如'/test'变成'test'

        if (!*name) { // 传进来的name是以'/'结尾的
            debug_info("[namei.c: link_path_walk:131] go to last_with_slashes\n");
            goto last_with_slashes;
        }

        if (this.name[0]=='.') {
            if (this.len==1) { // './'当前目录
                debug_info("[namei.c: link_path_walk:137] ./\n");
                continue;
            }
            if (this.len==2 && this.name[1]=='.') { // '../'父目录
                debug_info("[namei.c: link_path_walk:141] go to ../\n");
//                follow_dotdot(nd);
                follow_dotdot(&nd->mnt, &nd->dentry);
            } else { // '.s/test'这样子的目录真的不处理吗？隐藏目录
                break;
            }
        }
        // 书上说这里要hash，不清楚是拿来干嘛的
        nd->flags |= LOOKUP_CONTINUE;               // 表示还有下一个分量要分析
        kernel_printf("namei.c: 148 name: %s, %d\n", this.name, this.len);
        err = do_lookup(nd, &this, &next);          // 真正的查找，得到与给定的父目录（nd->dentry）和文件名，把下一个分量对应的目录赋给next
        if (err) {                                   // (this.name)相关的目录项对象（next.dentry）
            debug_warning("namei.c: 155, do_lookup not found\n");
            break;
        }

//        kernel_printf("%d\n", next.dentry);
//        kernel_printf("%d\n", next.dentry->d_inode);

        // 检查next.dentry是否指向某个文件系统的安装点
        // 如果是的话，更新乘这个文件系统的上级的dentry和mount
        flag_mount = follow_mount(&next.mnt,&next.dentry);
//        if (kernel_strcmp(next.dentry->d_name.name, "/") == 0)
        if (kernel_strcmp(next.dentry->d_name.name, "ext3") == 0 && next.dentry->d_parent == root_dentry)
        {
            root_dentry = next.dentry;
            root_mnt = next.mnt;
            debug_warning("namei.c 165 change to / after follow_mount");
            kernel_printf("next dentry: %d, nd dentry: %d\n", next.dentry, nd->dentry);
            return 0;
        }

        // 检查next.dentry对应的inode是否为空
        if (!next.dentry->d_inode) {
            err = -ENOENT;
            debug_err("[namei.c: link_path_walk:160] inode is empty\n");
            goto out_dput;
        }

        // 检查next.dentry对应的inode的i_op是否存在，如果没有inode operation肯定不是目录
        // 而这里检查的都不是最后一个分量，所以必须得是目录，不然就返回错误
        if (!next.dentry->d_inode->i_op) {
            err = -ENOTDIR;
            debug_err("[namei.c: link_path_walk:168] not a dentry\n");
            goto out_dput;
        }
        dput(nd->dentry);
        // 更新nd的mnt和dentry为next的，相当于往下走了一层
        nd->dentry=next.dentry;
        nd->mnt=next.mnt;

        // 而这里检查的都不是最后一个分量，所以必须得是目录，不然就返回错误
        err = -ENOTDIR;
        if (next.dentry->d_inode->i_op->lookup)
            break;
        continue; // 正常结束一个循环

last_with_slashes: // name以'/'结尾
        lookup_flags |= LOOKUP_FOLLOW | LOOKUP_DIRECTORY;
last_component:
        // 除了最后一个分量以外都已解析，处理最后一项，除了去掉非目录检查以外，其他都一样
        nd->flags &= ~LOOKUP_CONTINUE; // 标志这是最后一项了，去掉里面的continue flag
        if (lookup_flags&LOOKUP_PARENT)
            goto lookup_parent;
        if (this.name[0]=='.') {
            if (this.len==1) { // './'当前目录
                return 0;
            }
            if (this.len==2 && this.name[1]=='.') { // '../'父目录
//                follow_dotdot(nd);
                follow_dotdot(&nd->mnt,&nd->dentry);
            } else { // '.s/test'这样子的目录真的不处理吗？隐藏目录
                break;
            }
        }
        // 书上说这里要hash，不清楚是拿来干嘛的
//        kernel_printf("namei.c: 215 name: %s, %d\n", this.name, this.len);
        err = do_lookup(nd, &this, &next);          // 真正的查找，得到与给定的父目录（nd->dentry）和文件名，把下一个分量对应的目录赋给next
        if (err) {                                    // (this.name)相关的目录项对象（next.dentry）
            kernel_printf("namei.c 212, the do_lookup err: %d\n", err);
            break;
        }

        // 检查next.dentry是否指向某个文件系统的安装点
        // 如果是的话，更新成这个文件系统的上级的dentry和mount
        follow_mount(&next.mnt,&next.dentry);

        nd->dentry = next.dentry;
        nd->mnt = next.mnt;
        err = -ENOENT;
        kernel_printf("hhhhhhhhhhhh %s\n",next.dentry->d_name.name);
//        if (kernel_strcmp(next.dentry->d_name.name, "/") == 0)
        if (kernel_strcmp(next.dentry->d_name.name, "ext3") == 0 && next.dentry->d_parent == root_dentry)
        {
            root_dentry = next.dentry;
            root_mnt = next.mnt;
            debug_warning("namei.c 222 change to / after follow_mount");
            kernel_printf("next dentry: %d, nd dentry: %d\n", next.dentry, nd->dentry);
            return 0;
        }
        if (!next.dentry->d_inode) {
            kernel_printf("namei.c 218: not a dentry, the dentry's name: %s\n", next.dentry->d_name);
            kernel_printf("namei.c 219: not a dentry, the dentry's inode: %d\n", next.dentry->d_inode);
            break;
        }
        if (lookup_flags&LOOKUP_DIRECTORY) { // 如果要求最后一个分量是目录，那就必须要判断，不然无所谓
            err = -ENOTDIR;

            if (!next.dentry->d_inode->i_op || !next.dentry->d_inode->i_op->lookup) {
                kernel_printf("namei.c: 222: not a dentry, the i_op: %d, %d\n", next.dentry->d_inode->i_op, next.dentry->d_inode->i_op->lookup);
                break;
            }
        }
        debug_end("[namei.c: link_path_walk:215]\n");
        return 0;
lookup_parent:
        nd->last = this;
        nd->last_type = LAST_NORM; // 正常文件
        if (this.name[0]!='.')
            return 0;
        if (this.len==1) // 当前目录
            nd->last_type = LAST_DOT;
        else if (this.len==2&&this.name[1]=='.')
            nd->last_type = LAST_DOTDOT;
        else
            return 0;
out_dput:
        dput(next.dentry);
        break;
    }
    dput(nd->dentry);
    debug_end("[namei.c: link_path_walk:233]\n");
    return err;
}

// 回退到父目录
void follow_dotdot(struct vfsmount **mnt, struct dentry **dentry){
    debug_start("[namei.c: follow_dotdot:239]\n");
    while(1) {
        struct vfsmount *parent;
        struct dentry *old = *dentry;

        // 如果当前所处的目录不为当前路径所属文件系统的根目录，可以直接向上退一级，然后退出
        if (*dentry != root_dentry && *dentry != (*mnt)->mnt_root) {
            debug_warning("[namei.c: follow_dotdot:294] case 2 not root");
            kernel_printf("nd.dentry: %d, %s, mnt_root: %d, %s\n", (*dentry),(*dentry)->d_name.name, (*mnt)->mnt_root,
                          (*mnt)->mnt_root->d_name.name );
            *dentry = (*dentry)->d_parent;
            dget(*dentry);
            dput(old);
            break;
        }

        // 如果当前所处的目录即为根目录则退出
        if (*dentry == root_dentry && *mnt == root_mnt ){
            debug_warning("[namei.c: follow_dotdot:285] case 1 already root\n");
            break;
        }

//        // 如果当前所处的目录不为当前路径所属文件系统的根目录，可以直接向上退一级，然后退出
//        if (*dentry != (*mnt)->mnt_root) {
//            debug_warning("[namei.c: follow_dotdot:294] case 2 not root");
//            kernel_printf("nd.dentry: %d, %s, mnt_root: %d, %s\n", (*dentry),(*dentry)->d_name.name, (*mnt)->mnt_root,
//            (*mnt)->mnt_root->d_name.name );
//            *dentry = (*dentry)->d_parent;
//            dget(*dentry);
//            dput(old);
//            break;
//        }

        // 当前所处的目录为当前路径所属文件系统的根目录
        parent = (*mnt)->mnt_parent;
        // 文件系统即为本身，则表明没有父文件系统，退出
        if (parent == *mnt)
            break;

        // 取当前文件系统的挂载点，退回父文件系统
        *dentry = (*mnt)->mnt_mountpoint;
        dget(*dentry);
        dput(old);
        *mnt = parent;
        // 回到前面两种情况
    }
}
//void follow_dotdot(struct nameidata *nd) {
//    debug_start("[namei.c: follow_dotdot:239]\n");
//
//    while (1) {
//        // 如果已经是根目录了，没有办法回退了
//        if (nd->dentry==root_dentry && nd->mnt == root_mnt) {
//
//            debug_warning("[namei.c: follow_dotdot:285] case 1 already root\n");
//            break;
//        }
//
//        // 如果当前不在所属文件系统的根目录，向上一级再退出，此时nd已经被更新了
//        if (nd->dentry != nd->mnt->mnt_root) {
//            kernel_printf("nd.dentry: %d, %s, mnt_root: %d, %s\n", nd->dentry, nd->dentry->d_name.name,
//                          nd->mnt->mnt_root, nd->mnt->mnt_root->d_name.name);
//            nd->dentry=nd->dentry->d_parent;
//            dget(nd->dentry);
//            debug_warning("[namei.c: follow_dotdot:294] case 2 not root");
//            break;
//        }
//        // 如果当前在所属文件系统的根目录，如果没有父文件系统，没有办法回退了
//        if (nd->mnt->mnt_parent==nd->mnt) {
//            debug_warning("[namei.c: follow_dotdot:294] case 3 no father fs\n");
//            break;
//        }
//
//        // 返回父文件系统，就可以继续判断是继续返回爷爷文件系统还是目录直接回到上一级
//        nd->dentry = nd->mnt->mnt_mountpoint;
//        dget(nd->dentry);
//        nd->mnt=nd->mnt->mnt_parent;
//    }
//    debug_warning("[namei.c: follow_dotdot:302] ");
//    kernel_printf("nd.dentry:%d, root.dentry:%d\n", nd->dentry, root_dentry);
//    debug_end("[namei.c: follow_dotdot:261]\n");
//}

// 搜索父目录parent的孩子是否有名为name的entry结构
u32 do_lookup(struct nameidata *nd, struct qstr *name, struct path *path) {
    debug_start("[namei.c: do_lookup:266]\n");
    struct vfsmount *mnt = nd->mnt;
    // 在目录项高速缓存中寻找，查找文件名和父目录项相符的目录缓冲项
    struct condition cond;
//    kernel_printf("namei.c: name: %s, %d\n", name->name, name->len);
    cond.cond1 = (void*) nd->dentry;
    cond.cond2 = (void*) name;
    struct dentry* dentry = (struct dentry*) dcache->c_op->look_up(dcache, &cond);
//    kernel_printf("namei.c:275: dentry:%d\n", dentry);
    // 目录项高速缓冲（内存）没有，到外存中找
    if (!dentry)
        goto need_lookup;

done:
    debug_end("[namei.c: do_lookup:279] done\n");
    // 找到，修改path的字段，返回无错误
    path->mnt = mnt;
    path->dentry = dentry;
    dget(dentry);
    return 0;

need_lookup:
    debug_info("[namei.c: do_lookup:287] need lookup\n");
    // 即将使用底层文件系统在外存中查找，并构建需要的目录项
    dentry = real_lookup(nd->dentry, name, nd);
//    kernel_printf("namei.c: 308: real_lookup result: %d\n", dentry);
    if (IS_ERR(dentry))
        goto fail;
    if (IS_ERR_OR_NULL(dentry))
        goto fail2; // 因为null引起的错误
    goto done;

fail:
    debug_err("[namei.c: do_lookup:295] fail\n");
    return PTR_ERR(dentry);
fail2:
    debug_err("[namei.c: do_lookup:319] fail2\n");
    return -ENOENT;
}

// 对实际文件系统进行查找，调用具体文件系统节点的查找函数执行查找
struct dentry * real_lookup(struct dentry *parent, struct qstr *name, struct nameidata *nd) {
    debug_start("[namei.c: real_lookup:301]\n");
    struct dentry *result;
    struct inode *dir = parent->d_inode;

    // 新建一个目录项
    struct dentry *dentry = d_alloc(parent, name);
    result = ERR_PTR(-ENOMEM);
    if (dentry) {
//        debug_info("namei.c: real_lookup: 327:");
//        kernel_printf("%d, result: %d\n", dentry, result);
        // 查找需要的dentry对应的inode。若找到，相应的inode会被新建并加入高速缓存，dentry与之的联系也会被建立
        result = dir->i_op->lookup(dir, dentry, nd);
//        kernel_printf("i_op: lookup result: %d\n", result);
        if (!result) dput(dentry);
        else result = dentry;
    }
    debug_end("[namei.c: real_lookup:314]\n");
    return result;
}

// 根据父目录和名字查找对应的目录项，去外存找需要新建目录项
struct dentry * __lookup_hash(struct qstr *name, struct dentry *base, struct nameidata *nd) {
    debug_start("[namei.c: __lookup_hash:320]\n");
    struct dentry   *dentry;
    struct inode    *inode;

    debug_warning("[namei.c: __lookup_hash:375]\t");
    kernel_printf("base addr:%d", base);
    kernel_printf(", base name: %s\n", base->d_name.name);
    inode = base->d_inode;
    // 在目录项高速缓存中寻找，查找文件名和父目录项相符的目录缓冲项
    struct condition cond;
    cond.cond1 = (void*) nd->dentry;
    cond.cond2 = (void*) name;
    debug_warning("[namei.c: __lookup_hash:383]\t");
    kernel_printf("base addr: %d, root addr: %d", base, root_dentry);
    kernel_printf(", base name: %s, root name: %s\n", base->d_name.name, root_dentry->d_name.name);

    dentry = (struct dentry*) dcache->c_op->look_up(dcache, &cond);

    // 如果没有找到，尝试在外存找
    if (!dentry) {
        // 新dentry首先需要被创建
        struct dentry *new = d_alloc(base, name);
        dentry = ERR_PTR(-ENOMEM);
        if (!new) {
            debug_err("[namei.c: __lookup_hash:337] alloc failed\n");
            return dentry;
        }
        kernel_printf("%d\n",inode->i_op);
        // 尝试在外存中查找需要的dentry对应的inode。若找到，相应的inode会被新建并加入高速缓存，dentry与之的联系也会被建立
        dentry = inode->i_op->lookup(inode, new, nd);
        if (!dentry)
            dentry = new;   // 若相应的inode并没能找到，则需要进一步创建inode。dentry的引用计数暂时需要保持
        else
            dput(new);
    }
    debug_end("[namei.c: __lookup_hash:349]\n");
    return dentry;
}

// 寻找一个dentry，如果不存在则新建
struct dentry *lookup_create(struct nameidata *nd, int is_dir) {
    debug_start("[namei.c: lookup_create:355]\n");
    struct dentry *dentry;

    dentry = ERR_PTR(-EEXIST);
    if (nd->last_type != LAST_NORM)
        goto fail;
    nd->flags &= ~LOOKUP_PARENT;
    dentry = __lookup_hash(&nd->last, nd->dentry, nd);
    if (IS_ERR(dentry))
        goto fail;
    if (!is_dir && nd->last.name[nd->last.len] && !dentry->d_inode)
        goto enoent;
    debug_end("[namei.c: lookup_create:367]\n");
    return dentry;
enoent:
    dput(dentry);
    dentry = ERR_PTR(-ENOENT);
    debug_info("[namei.c: lookup_create:372] go to enoent\n");
fail:
    debug_end("[namei.c: lookup_create:374] fail end\n");
    return dentry;
}
