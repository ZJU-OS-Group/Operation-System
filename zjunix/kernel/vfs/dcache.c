#include <zjunix/vfs/vfs.h>
#include <zjunix/utils.h>
#include <zjunix/vfs/vfscache.h>
#include <zjunix/vfs/hash.h>
#include <zjunix/debug/debug.h>

/********************************** 外部变量 ************************************/
extern struct cache     *dcache;
extern struct dentry    *root_dentry;

// dentry将路径名与节点联系起来，动态生成，只存在于缓存中
static struct hlist_head *dentry_hashtable;
static LIST_HEAD(dentry_unused); // 处于unused和negtive

void dput(struct dentry *dentry) {
    dentry->d_count -= 1;
}

// 为字符串计算哈希值
u32 __stringHash(struct qstr * qstr, u32 size) {
    u32 i = 0;
    u32 value = 0;
    for (i = 0; i < qstr->len; i++)
        value = value * 31 + (u32)(qstr->name[i]);            // 参考Java

    u32 mask = size - 1;                        // 求余
    return value & mask;
}

void* dcache_look_up(struct cache *this, struct condition *cond) {
    debug_start("[dcache.c: dcache_look_up:31]\n");
    u32 found;
    u32 hash;
    struct qstr         *name; // qstr 包装字符串
    struct qstr         *qstr;
    struct dentry       *parent; // dentry 目录项包装
    struct dentry       *tested;
    struct list_head    *current;
    struct list_head    *start;
    parent  = (struct dentry*) (cond->cond1);
    name    = (struct qstr*) (cond->cond2);

    // 计算名字对应的哈希值，找到那个哈希值对应页面的链表头
    hash = __stringHash(name, this->cache_tablesize);
    current = &(this->c_hashtable[hash]);
    start = current;

    // 遍历这个链表搜索，需要父目录及名字匹配
    found = 0;
    while ( current->next != start ){
        current = current->next;
        tested = container_of(current, struct dentry, d_hash); // 通过d_hash的指针获得结构体的指针
        qstr = &(tested->d_name);
        if ( !parent->d_op->d_compare(qstr, name) && tested->d_parent == parent ){
            found = 1; // 都匹配上了
            break;
        }
    }


    // 找到的话返回指向对应dentry的指针,同时更新哈希链表、LRU链表状态
    if (found) {
        // 以下操作可以使更新lru链表，将最近的往前插
        list_del(&tested->d_hash);
        list_add(&tested->d_hash, &(this->c_hashtable[hash]));
        list_del(&(tested->d_lru));
        list_add(&(tested->d_lru), this->c_lru);
        debug_end("[dcache.c: dcache_look_up:68]: has found\n");
        return (void*)tested; // 返回对应的dentry指针，转换为void*是为了适配统一的接口
    }
    else{
        debug_end("[dcache.c: dcache_look_up:72]: not found\n");
        return 0;
    }

}

void dget(struct dentry *dentry) {
    dentry->d_count += 1;
}

// 根据相应信息新建一个dentry项，填充相关信息，并放进dentry高速缓存
struct dentry * d_alloc(struct dentry *parent, const struct qstr *name) {
    debug_start("[dcache.c: d_alloc:83]\n");
    u8* dname;
    u32 i;
    struct dentry* dentry;

    dentry = (struct dentry *) kmalloc ( sizeof(struct dentry) );
    if (!dentry)
        return 0;

    dname = (u8*) kmalloc ( (name->len + 1)* sizeof(u8*) );
    kernel_memset(dname, 0, (name->len + 1));
    for ( i = 0; i < name->len; i++ ){
        dname[i] = name->name[i];
    }
    dname[i] = '\0';

    dentry->d_name.name         = dname;
    dentry->d_name.len          = name->len;
    dentry->d_count             = 1;
    dentry->d_inode             = 0;
    dentry->d_parent            = parent;
    dentry->d_sb                = parent->d_sb;
    dentry->d_op                = 0;

    INIT_LIST_HEAD(&dentry->d_hash);
    INIT_LIST_HEAD(&dentry->d_lru);
    INIT_LIST_HEAD(&dentry->d_subdirs);
    INIT_LIST_HEAD(&(root_dentry->d_alias));

    if (parent) {
        dentry->d_parent = parent;
        dget(parent);
        dentry->d_sb = parent->d_sb;
        list_add(&dentry->d_u.d_child, &parent->d_subdirs);
    } else {
        INIT_LIST_HEAD(&dentry->d_u.d_child);
    }

    dcache->c_op->add(dcache, (void*)dentry);
    debug_end("[dcache.c: d_alloc:123]\n");
    return dentry;
}

void dcache_add(struct cache *this, void * object) {
    debug_start("[dcache.c: dcache_add:128]\n");
    u32 hash;
    struct dentry* addend;

    // 计算目录项名字对应的哈希值
    addend = (struct dentry *) object;
    hash = __stringHash(&addend->d_name, this->cache_tablesize);

    // 如果整个目录项缓冲已满，替换一页出去
    if (cache_is_full(this))
        dcache_put_LRU(this);

    // 找到那个哈希值对应页面的链表头，建立联系
    list_add(&(addend->d_hash), &(this->c_hashtable[hash]));

    // 同时也在LRU链表的头部插入，表示最新访问
    list_add(&(addend->d_lru), this->c_lru);

    // 当前cache的size加一
    this->cache_size += 1;
    debug_end("[dcache.c: dcache_add:148]\n");
}

// 从LRU中移除一项
void dcache_put_LRU(struct cache * this) {
    debug_start("[dcache.c: dcache_put_LRU:153]\n");
    struct list_head        *put;
    struct list_head        *start;
    struct dentry           *put_dentry; // 存储要被从cache中替换出去的dentry

    // 搜寻LRU的链表尾
    start = this->c_lru;
    put = start->prev;
    put_dentry = container_of(put, struct dentry, d_lru); // 找到put对应的dentry的指针

    // put和start一致意味着一个循环回来了，是环形双向链表
    while ( put != start && put_dentry->d_count > 0 ){ // 如果找到引用数为0的就跳出，此时put和start不相等
        put = put->prev;
        put_dentry = container_of(put, struct dentry, d_lru);
    }

    // 若引用数都不为0，找最远最少使用
    if (put == start) {
        put = start->prev;
        put_dentry = container_of(put, struct dentry, d_lru);
    }

    // 在有联系的链表里清除
    list_del(&(put_dentry->d_lru));
    list_del(&(put_dentry->d_hash));
    list_del(&(put_dentry->d_alias));
    list_del(&(put_dentry->d_subdirs));
    this->cache_size -= 1;

    // 内存清理
    release_dentry(put_dentry);
    debug_end("[dcache.c: dcache_put_LRU:184]\n");

}

void dentry_iput(struct dentry * dentry) {
    debug_start("[dcache.c: dentry_iput:189]\n");
    struct inode *inode = dentry->d_inode;
    if (inode) {
        dentry->d_inode = 0;
        list_del_init(&dentry->d_alias);
        if (dentry->d_op && dentry->d_op->d_iput)
            dentry->d_op->d_iput(dentry, inode);
        else {
            if (inode->i_sb->s_op && inode->i_sb->s_op->put_inode) {
                inode->i_sb->s_op->put_inode(inode);
            }
        }
    }
    debug_end("[dcache.c: dentry_iput:202]\n");
}
