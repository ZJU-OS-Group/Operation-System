#include <zjunix/vfs/vfs.h>


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
    u32 err = 0;
    struct dentry *dentry;
    struct dentry *dir;

    /* 仅查找，不用create */
    if (!(flag&O_CREAT)) {
        err = path_lookup(pathname, LOOKUP_FOLLOW, nd);
        if (err)
            return err;
//        dentry = nd->dentry;
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
        return err;
    }

    dir = nd->dentry;
    nd->flags &= ~LOOKUP_PARENT;
    dentry = __lookup_hash(&nd->last, nd->dentry, nd); // 根据父目录和分量名字查找对应项

    /* 打不开目录 */
    if (IS_ERR(dentry)) {
        dput(nd->dentry);
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
        } else return 0;
    }

    /* 若分量已经存在 */
//    err = -ENOENT;
    dput(nd->dentry);
    nd->dentry = dentry;
    return 0;
}

u32 path_lookup(const u8 * name, u32 flags, struct nameidata *nd) {
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
    return link_path_walk(name, nd);
}

// 基本的路径解析函数，有路径名查找到最终的dentry结构，将信息保存在nd中返回
u32 link_path_walk(const u8 *name, struct nameidata *nd) {
    u32 err = 0;
    u32 lookup_flags = nd->flags;
    struct path next;

    // 跳过开始的'/'，'/test/zc'变成'test/zc'
    while (*name=='/') name++;

    if (!*name) {
        // 检查有效性，不会
        return 0;
    }

    // 初始化inode，这是查找的当前位置，之后从这开始找
    // TODO：这里先不考虑链接

    // 解析每一个分量
    while (1) {
        u8 c;
        struct qstr this;
        this.name = name;
        do { // 处理到'/'为止
            name++;
            c = *name;
        } while (c && (c != '/'));
        this.len = (u32) (name - this.name);

        if (!c) { // 解析完毕了，后面没有'/'了
            goto last_component;
        }

        while (*++name == '/'); // 跳过所有的'/'，如'/test'变成'test'

        if (!*name) { // 传进来的name是以'/'结尾的
            goto last_with_slashes;
        }

        if (this.name[0]=='.') {
            if (this.len==1) { // './'当前目录
                continue;
            }
            if (this.len==2 && this.name[1]=='.') { // '../'父目录
                follow_dotdot(nd);
            } else { // '.s/test'这样子的目录真的不处理吗？隐藏目录
                break;
            }
        }
        // 书上说这里要hash，不清楚是拿来干嘛的
        nd->flags |= LOOKUP_CONTINUE;               // 表示还有下一个分量要分析
        err = do_lookup(nd, &this, &next);          // 真正的查找，得到与给定的父目录（nd->dentry）和文件名，把下一个分量对应的目录赋给next
        if (err)                                    // (this.name)相关的目录项对象（next.dentry）
            break;

        // 检查next.dentry是否指向某个文件系统的安装点
        // 如果是的话，更新乘这个文件系统的上级的dentry和mount
        follow_mount(&next.mnt,&next.dentry);

        // 检查next.dentry对应的inode是否为空
        if (!next.dentry->d_inode) {
            err = -ENOENT;
            goto out_dput;
        }

        // 检查next.dentry对应的inode的i_op是否存在，如果没有inode operation肯定不是目录
        // 而这里检查的都不是最后一个分量，所以必须得是目录，不然就返回错误
        if (!next.dentry->d_inode->i_op) {
            err = -ENOTDIR;
            goto out_dput;
        }
        // TODO：接下来对链接情况进行处理，先不考虑
        dput(nd->dentry);
        // 更新nd的mnt和dentry为next的，相当于往下走了一层
        nd->dentry=next.dentry;
        nd->mnt=next.mnt;

        // TODO：检查next.dentry对应的inode的i_op是否存在，如果没有lookup操作就不是目录（存疑）
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
                continue;
            }
            if (this.len==2 && this.name[1]=='.') { // '../'父目录
                follow_dotdot(nd);
            } else { // '.s/test'这样子的目录真的不处理吗？隐藏目录
                break;
            }
        }
        // 书上说这里要hash，不清楚是拿来干嘛的
        err = do_lookup(nd, &this, &next);          // 真正的查找，得到与给定的父目录（nd->dentry）和文件名，把下一个分量对应的目录赋给next
        if (err)                                    // (this.name)相关的目录项对象（next.dentry）
            break;// 检查next.dentry是否指向某个文件系统的安装点
        // 如果是的话，更新乘这个文件系统的上级的dentry和mount
        follow_mount(&next.mnt,&next.dentry);
        err = -ENOENT;
        if (!next.dentry->d_inode) break;
        if (lookup_flags&LOOKUP_DIRECTORY) { // 如果要求最后一个分量是目录，那就必须要判断，不然无所谓
            err = -ENOTDIR;
            if (!next.dentry->d_inode->i_op || !next.dentry->d_inode->i_op->lookup) break;
        }
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
    return err;
}

// 回退到父目录
inline void follow_dotdot(struct nameidata *nd) {
    while (1) {
        // 如果已经是根目录了，没有办法回退了
        if (nd->dentry==root_dentry && nd->mnt == root_mnt) break;

        // 如果当前不在所属文件系统的根目录，向上一级再退出，此时nd已经被更新了
        if (nd->dentry != nd->mnt->mnt_root) {
            nd->dentry=nd->dentry->d_parent;
            dget(nd->dentry);
            break;
        }
        // 如果当前在所属文件系统的根目录，如果没有父文件系统，没有办法回退了
        //TODO：不太懂这种情况下为什么nd->dentry==root_dentry不成立，那就证明root_dentry不太对，那这个需要更新吗？
        if (nd->mnt->mnt_parent==nd->mnt) break;

        // 返回父文件系统，就可以继续判断是继续返回爷爷文件系统还是目录直接回到上一级
        // TODO：为什么要做文件系统间的上一级呢？感觉有点点乱
        nd->dentry = nd->mnt->mnt_mountpoint;
        dget(nd->dentry);
        nd->mnt=nd->mnt->mnt_parent;
    }
}

// 搜索父目录parent的孩子是否有名为name的entry结构
u32 do_lookup(struct nameidata *nd, struct qstr *name, struct path *path) {
    struct vfsmount *mnt = nd->mnt;
    // 在目录项高速缓存中寻找，查找文件名和父目录项相符的目录缓冲项
    struct condition cond;
    cond.cond1 = (void*) nd->dentry;
    cond.cond2 = (void*) name;
    struct dentry* dentry = (struct dentry*) dcache->c_op->look_up(dcache, &cond);

    // 目录项高速缓冲（内存）没有，到外存中找
    if (!dentry)
        goto need_lookup;

    done:
    // 找到，修改path的字段，返回无错误
    path->mnt = mnt;
    path->dentry = dentry;
    dget(dentry);
    return 0;

    need_lookup:
    // 即将使用底层文件系统在外存中查找，并构建需要的目录项
    dentry = real_lookup(nd->dentry, name, nd);
    if (IS_ERR(dentry))
        goto fail;
    goto done;

    fail:
    return PTR_ERR(dentry);
}

// 对实际文件系统进行查找，调用具体文件系统节点的查找函数执行查找
struct dentry * real_lookup(struct dentry *parent, struct qstr *name, struct nameidata *nd) {
    struct dentry *result;
    struct inode *dir = parent->d_inode;

    struct dentry *dentry = d_alloc(parent, name);
    result = ERR_PTR(-ENOMEM);
    if (dentry) {
        // 查找需要的dentry对应的inode。若找到，相应的inode会被新建并加入高速缓存，dentry与之的联系也会被建立
        result = dir->i_op->lookup(dir, dentry, nd);
        if (result) dput(dentry);
        else result = dentry;
    }
    return result;
}

// 根据父目录和名字查找对应的目录项（创建模式会调用）
struct dentry * __lookup_hash(struct qstr *name, struct dentry *base, struct nameidata *nd) {
    struct dentry   *dentry;
    struct inode    *inode;

    inode = base->d_inode;
    // 在目录项高速缓存中寻找，查找文件名和父目录项相符的目录缓冲项
    struct condition cond;
    cond.cond1 = (void*) nd->dentry;
    cond.cond2 = (void*) name;
    dentry = (struct dentry*) dcache->c_op->look_up(dcache, &cond);

    // 如果没有找到，尝试在外存找
    if (!dentry) {
        // 新dentry首先需要被创建
        struct dentry *new = d_alloc(base, name);
        dentry = ERR_PTR(-ENOMEM);
        if (!new)
            return dentry;

        // 尝试在外存中查找需要的dentry对应的inode。若找到，相应的inode会被新建并加入高速缓存，dentry与之的联系也会被建立
        dentry = inode->i_op->lookup(inode, new, nd);
        if (!dentry)
            dentry = new;   // 若相应的inode并没能找到，则需要进一步创建inode。dentry的引用计数暂时需要保持
        else
            dput(new);
    }

    return dentry;
}
