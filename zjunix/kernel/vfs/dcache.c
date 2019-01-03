#include <zjunix/vfs/vfs.h>
#include <zjunix/utils.h>
#include <stddef.h>
#include <zjunix/vfs/vfscache.h>
#include <zjunix/vfs/hash.h>

#define L1_CACHE_BYTES 32
#define D_HASHBITS     d_hash_shift
#define D_HASHMASK     d_hash_mask
static u32 d_hash_mask;
static u32 d_hash_shift;
/********************************** 外部变量 ************************************/
extern struct cache     *dcache;
extern struct dentry    *root_dentry;

// dentry将路径名与节点联系起来，动态生成，只存在于缓存中
static struct hlist_head *dentry_hashtable;
static LIST_HEAD(dentry_unused); // 处于unused和negtive

void dput(struct dentry *dentry) {
    dentry->d_count -= 1;
}

// 搜索父目录为parent的孩子中是否有名为name的dentry结构，查找到就返回
//struct dentry * d_lookup(struct dentry * parent, struct qstr * name) {
//    u32 len = name->len;
//    u32 hash = name->hash;
//    u8  str = name->name;
//}


// 为字符串计算哈希值
u32 __stringHash(struct qstr * qstr, u32 size){
    u32 i = 0;
    u32 value = 0;
    for (i = 0; i < qstr->len; i++)
        value = value * 31 + (u32)(qstr->name[i]);            // 参考Java

    u32 mask = size - 1;                        // 求余
    return value & mask;
}

static inline struct hlist_head *d_hash(struct dentry *parent,
                                        u32 hash)
{
    hash += ((u32) parent ^ GOLDEN_RATIO_PRIME) / L1_CACHE_BYTES;
    hash = (u32) (hash ^ ((hash ^ GOLDEN_RATIO_PRIME) >> D_HASHBITS));
    return dentry_hashtable + (hash & D_HASHMASK);
}

void* dcache_lookup(struct cache *this, struct condition *cond) {
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
        list_del(&tested->d_hash);
        list_add(&tested->d_hash, &(this->c_hashtable[hash]));
        list_del(&(tested->d_lru));
        list_add(&(tested->d_lru), this->c_lru);
        return (void*)tested;
    }
    else{
        return 0;
    }
}


void dget(struct dentry *dentry) {
    dentry->d_count += 1;
}

// 根据相应信息新建一个dentry项，填充相关信息，并放进dentry高速缓存
struct dentry * d_alloc(struct dentry *parent, const struct qstr *name) {
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
    return dentry;
}

