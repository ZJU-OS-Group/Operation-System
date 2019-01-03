#include <zjunix/vfs/vfs.h>

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

