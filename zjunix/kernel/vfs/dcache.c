#include <zjunix/vfs/vfs.h>
#include <zjunix/utils.h>
#include <stddef.h>

#define DNAME_INLINE_LEN (sizeof(struct dentry)-offsetof(struct dentry,d_iname))

/********************************** 外部变量 ************************************/
extern struct cache     *dcache;
extern struct dentry    *root_dentry;

// dentry将路径名与节点联系起来，动态生成，只存在于缓存中
static struct list_head *dentry_hashtable;
static LIST_HEAD(dentry_unused); // 处于unused和negtive

//void cache_init(struct cache* this, u8 capacity, u32 tablesize){
//    u32 i;
//
//    this->cache_size = 0; // 现有项数置0
//    this->cache_capacity = capacity; // 设置最大项数
//    this->cache_tablesize = tablesize; // 设置哈希值数
//    INIT_LIST_HEAD(this->c_lru);
//    this->c_hashtable = (struct list_head*) kmalloc ( tablesize * sizeof(struct list_head) ); // 初始化一块空间来当哈希表
//    for ( i = 0; i < tablesize; i++ ) // 给hashtable里面的每个list head都init一下
//        INIT_LIST_HEAD(this->c_hashtable + i);
//    this->c_op = 0; // 指向缓冲区的操作函数指针
//}

void dput(struct dentry *dentry) {
    dentry->d_count -= 1;
}

// 搜索父目录为parent的孩子中是否有名为name的dentry结构，查找到就返回
struct dentry * d_lookup(struct dentry * parent, struct qstr * name) {
    u32 len = name->len;
    u32 hash = name->hash;
    u8  str = name->name;
}

void dget(struct dentry *dentry) {
    dentry->d_count += 1;
}

// 根据相应信息新建一个dentry项，填充相关信息，并放进dentry高速缓存
//struct dentry * d_alloc(struct dentry *parent, const struct qstr *name){
//    u8* dname;
//    struct dentry* dentry;
//
//    dentry = (struct dentry *) kmalloc ( sizeof(struct dentry) );
//    if (!dentry)
//        return 0;
//
//    if (name->len > DNAME_INLINE_LEN-1) {
//        dname = kmalloc(name->len + 1, GFP_KERNEL);
//        if (!dname) {
//            kmem_cache_free(dentry_cache, dentry);
//            return NULL;
//        }
//    } else  {
//        dname = dentry->d_iname;
//    }
//
//    dname = (u8*) kmalloc ( (name->len + 1)* sizeof(u8*) );
//    kernel_memset(dname, 0, (name->len + 1));
//    for ( i = 0; i < name->len; i++ ){
//        dname[i] = name->name[i];
//    }
//    dname[i] = '\0';
//
//
//    dentry->d_name.name         = dname;
//    dentry->d_name.len          = name->len;
//    dentry->d_count             = 1;
//    dentry->d_inode             = 0;
//    dentry->d_parent            = parent;
//    dentry->d_sb                = parent->d_sb;
//    dentry->d_op                = 0;
//
//    INIT_LIST_HEAD(&dentry->d_hash);
//    INIT_LIST_HEAD(&dentry->d_lru);
//    INIT_LIST_HEAD(&dentry->d_subdirs);
//    INIT_LIST_HEAD(&(root_dentry->d_alias));
//
//    if (parent) {
//        dentry->d_parent = parent;
//        dget(parent);
//        dentry->d_sb = parent->d_sb;
//        list_add(&dentry->d_u.d_child, &parent->d_subdirs);
//    } else {
//        INIT_LIST_HEAD(&dentry->d_u.d_child);
//    }
//
//    dcache->c_op->add(dcache, (void*)dentry);
//    return dentry;
//}

