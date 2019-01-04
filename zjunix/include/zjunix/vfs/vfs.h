#ifndef _ZJUNIX_VFS_VFS_H
#define _ZJUNIX_VFS_VFS_H
#include <zjunix/type.h>
#include <zjunix/list.h>
#include <zjunix/vfs/err.h>
#include <zjunix/vfs/errno.h>
#include <zjunix/vfs/err.h>
#include <zjunix/slab.h>
#include <driver/sd.h>
#include <zjunix/vfs/vfscache.h>
#include <zjunix/utils.h>


#define         SECTOR_BYTE_SIZE                512
#define         SECTOR_LOG_SIZE                 9
#define         S_CLEAR                         0
#define         S_DIRTY                         1
#define         P_CLEAR                         0
#define         MAX_ERRNO	                    4095
#define         BITS_PER_BYTE                   8
#define         IS_ERR_VALUE(x)                 ((x) >= (u32)-MAX_ERRNO)

// 文件打开方式，即open函数的参数flags。vfs_open的第二个参数，打开文件时用
#define O_RDONLY	                            0x0000                  // read only 只读
#define O_WRONLY	                            0x0001                  // write only 只写
#define O_RDWR		                            0x0002                  // read & write 可读可写
#define O_ACCMODE                               0x0003                  //
#define O_TRUNC                                 0x0004                  // TODO: Don't know why
#define O_CREAT		                            0x0100                   // 如果文件不存在，就创建它
#define O_APPEND	                            0x2000                   // 总是在文件末尾写

// 文件查找方式，即lookup函数的参数flags。path_lookup的第二个参数，查找文件时用
#define LOOKUP_FOLLOW                           0x0001                  // 如果最后一个分量是符号链接，则追踪（解释）它
#define LOOKUP_DIRECTORY                        0x0002                  // 最后一个分量必须是目录
#define LOOKUP_CONTINUE                         0x0004                  // 在路径名中还有文件名要检查
#define LOOKUP_PARENT                           0x0010                  // 查找最后一个分量所在的目录
#define LOOKUP_CREATE                           0x0200                  // 试图创建一个文件

// 文件打开后用
#define FMODE_READ		                        0x1                     // 文件为读而打开
#define FMODE_WRITE		                        0x2                     // 文件为写而打开
#define FMODE_LSEEK		                        0x4                     // 文件可以寻址
#define FMODE_PREAD		                        0x8                     // 文件可用pread
#define FMODE_PWRITE	                        0x10                    // 文件可用pwrite

// 文件类型
#define FTYPE_NORM                              1                       // 普通文件
#define FTYPE_DIR                               2                       // 目录
#define FTYPE_LINK                              3                       // 链接
#define FTYPE_UNKOWN                            4                       // 其他类型

// LOOKUP_PARENT 中最后分量的类型
enum {LAST_NORM, LAST_ROOT, LAST_DOT, LAST_DOTDOT, LAST_BIND};

/*********************************以下定义VFS相关的其他数据结构****************************************/
/********************************* 文件系统类型 ******************************/
// 一个文件系统对应一个file_system_type
struct file_system_type {
    const char              *name;          /* 文件系统名称 */
    int                     fs_flags;       /* 文件系统类型标志 */
    /* 从磁盘中读取超级块,并且在文件系统被安装时,在内存中组装超级块对象 */
    int (*get_sb) (struct file_system_type *, int,
                   const char *, void *, struct vfsmount *);
    /* 终止访问超级块 */
    void (*kill_sb) (struct super_block *);
    struct module           *owner;         /* 文件系统模块 */
    struct file_system_type * next;         /* 链表中下一个文件系统类型 */
    struct list_head        fs_supers;      /* 超级块对象链表 */

    /* 下面都是运行时的锁 */
//    struct lock_class_key   s_lock_key;
//    struct lock_class_key   s_umount_key;
//
//    struct lock_class_key   i_lock_key;
//    struct lock_class_key   i_mutex_key;
//    struct lock_class_key   i_mutex_dir_key;
//    struct lock_class_key   i_alloc_sem_key;
};

/********************************* 文件系统实例 ******************************/
// 一个文件系统实例对应一个vfsmount，被安装时会在安装点创建一个
struct vfsmount {
    struct list_head        mnt_hash;           /* 散列表 */
    struct vfsmount         *mnt_parent;        /* 父文件系统，也就是要挂载到哪个文件系统 */
    struct dentry           *mnt_mountpoint;    /* 安装点的目录项 */
    struct dentry           *mnt_root;          /* 该文件系统的根目录项 */
    struct super_block      *mnt_sb;            /* 该文件系统的超级块 */
    struct list_head        mnt_mounts;         /* 子文件系统链表 */
    struct list_head        mnt_child;          /* 子文件系统链表 */
    int                     mnt_flags;          /* 安装标志 */
    /* 4 bytes hole on 64bits arches */
    const char              *mnt_devname;       /* 设备文件名 e.g. /dev/dsk/hda1 */
    struct list_head        mnt_list;           /* 描述符链表 */
    struct list_head        mnt_expire;         /* 到期链表的入口 */
    struct list_head        mnt_share;          /* 共享安装链表的入口 */
    struct list_head        mnt_slave_list;     /* 从安装链表 */
    struct list_head        mnt_slave;          /* 从安装链表的入口 */
    struct vfsmount         *mnt_master;        /* 从安装链表的主人 */
    struct mnt_namespace    *mnt_ns;            /* 相关的命名空间 */
    int                     mnt_id;             /* 安装标识符 */
    int                     mnt_group_id;       /* 组标识符 */
    /*
     * We put mnt_count & mnt_expiry_mark at the end of struct vfsmount
     * to let these frequently modified fields in a separate cache line
     * (so that reads of mnt_flags wont ping-pong on SMP machines)
     */
//    atomic_t                mnt_count;          /* 使用计数 */
    int                     mnt_expiry_mark;    /* 如果标记为到期，则为 True */
    int                     mnt_pinned;         /* "钉住"进程计数 */
    int                     mnt_ghosts;         /* "镜像"引用计数 */
//    atomic_t                mnt_writers;        /* 写者引用计数 */
};

/********************************* 包装字符串 ********************************/
struct qstr {
    const u8                            *name;                  // 字符串
    u32                                 len;                    // 长度
    u32                                 hash;                   // 哈希值
};

/********************************* 文件打开标志 ******************************/
struct open_intent {
    u32	                                flags;
    u32	                                create_mode;
};

/********************************* 查找操作结果 ******************************/
struct nameidata {
    struct dentry       *dentry;        /* 目录项对象的地址 */
    struct vfsmount     *mnt;           /* 已安装文件系统对象的地址 */
    struct qstr         last;           /* 路径名的最后一个分量（LOOKUP_PARENT标志被设置时使用） */
    u32                 flags;          /* 查找标志 */
    u32                 last_type;      /* 最后一个分量的文件类型（LOOKUP_PARENT标志被设置时使用） */
    unsigned int        depth;          /* 符号链接嵌套的当前级别，必须小于6 */
    union {                             /* 单个成员联合体，指定如何访问文件 */
        struct open_intent open;
    } intent;
};

/****************************************vfs页 ************************************************/
struct vfs_page {
    u8*     page_data;
    u32     page_state;
    u32     page_address;
    struct list_head*           page_hashtable;                     // 哈希表链表
    struct list_head*           p_lru;                              // LRU链表
    struct list_head*           page_list;                          // 同一文件已缓冲页的链表
    struct address_space*       p_address_space;                    // 所属的address_space结构

};

/********************************* 已缓存的页 ********************************/
// 管理缓存项和页I/O操作
struct address_space {
    u32                                 a_pagesize;             /* 页大小(字节) */
    u32                                 *a_page;                /* 文件页到逻辑页的映射表 */
    u32                                 nrpages;                /* 页总数 */
    struct inode                        *a_host;                /* 相关联的inode */
    struct list_head                    a_cache;                /* 已缓冲的页链表 */
    struct address_space_operations     *a_op;                  /* 操作函数 */
};
// 缓存页相关操作
struct address_space_operations {
    /* 将一页写回外存 */
    int (*writepage) (struct vfs_page*);
    /* 从外存读入一页 */
    u32 (*readpage)(struct vfs_page *);
    /* 映射，根据由相对文件页号得到相对物理页号 */
    u32 (*bmap)(struct inode *, u32);
};

/********************************* 查找用目录结构 *****************************/
struct path {
    struct vfsmount                     *mnt;                   // 对应目录项
    struct dentry                       *dentry;                // 对应文件系统挂载项
};

/********************************* 查找条件结构 ******************************/
struct condition {
    void    *cond1; // parent 目录 // pageNum
    void    *cond2; // name
    void    *cond3;
};

/********************************* 单个目录项结构 *****************************/
struct dirent {
    u32                                 ino;                    // inode 号
    u8                                  type;                   // 文件类型
    const u8                            *name;                  // 文件名
};

/********************************* 获取目录项结构 *****************************/
struct getdent {
    u32                                 count;                  // 目录项数
    struct dirent                       *dirent;                // 目录项数组
};

/*********************************以下定义VFS的四个主要对象********************************************/
/********************************* 超级块 *********************************/
// 文件系统的有关信息
struct super_block {
    struct list_head                s_list;         /* 指向所有超级块的链表 */
    const struct super_operations   *s_op;          /* 超级块方法 */
    struct dentry                   *s_root;        /* 目录挂载点 */
    struct list_head                s_inodes;       /* inode链表 */
    u8                              s_dirt;         /* 是否被写脏 */
    u32                             s_block_size;   /* 以字节为单位的块大小 */
    void                            *s_fs_info;     /* 指向文件系统基本信息的指针 */
    u32                             s_count;        /* 超级块引用计数 */
//    struct list_head                s_inodes;       /* inode链表 */
    struct file_system_type         *s_type;        /* 文件系统类型 */
    struct mtd_info                 *s_mtd;         /* 存储磁盘信息 */
};
// 超级块操作函数
struct super_operations {
    struct inode *(*alloc_inode)(struct super_block *);         /* 创建和初始化一个索引节点对象 */
    void (*destroy_inode)(struct inode *);                      /* 释放给定的索引节点 */

    void (*dirty_inode) (struct inode *);                       /* VFS在索引节点被修改时会调用这个函数 */
    int (*write_inode) (struct inode *, int);                   /* 将索引节点写入磁盘，wait表示写操作是否需要同步 */
    void (*drop_inode) (struct inode *);                        /* 最后一个指向索引节点的引用被删除后，VFS会调用这个函数 */
    void (*delete_inode) (struct inode *);                      /* 从磁盘上删除指定的索引节点 */
    void (*put_super) (struct super_block *);                   /* 卸载文件系统时由VFS调用，用来释放超级块 */
    void (*write_super) (struct super_block *);                 /* 用给定的超级块更新磁盘上的超级块 */
    int (*sync_fs)(struct super_block *, int);                  /* 使文件系统中的数据与磁盘上的数据同步 */
    int (*statfs) (struct dentry *, struct kstatfs *);          /* VFS调用该函数获取文件系统状态 */
    int (*remount_fs) (struct super_block *, int *, char *);    /* 指定新的安装选项重新安装文件系统时，VFS会调用该函数 */
    void (*clear_inode) (struct inode *);                       /* VFS调用该函数释放索引节点，并清空包含相关数据的所有页面 */
    void (*umount_begin) (struct super_block *);                /* VFS调用该函数中断安装操作 */
    u32 (*delete_dentry_inode) (struct dentry *);               /* 删除目录项对应的内存中的VFS索引节点和磁盘上文件数据及元数 */
};

/******************************* 索引节点 *********************************/
// 具体文件的一般信息，索引节点号唯一标识
struct inode {
    struct list_head                    i_hash;         /* 散列表，用于快速查找inode */
    struct list_head                    i_list;         /* 索引节点链表 */
    struct list_head                    i_sb_list;      /* 超级块链表超级块  */
    struct list_head                    i_dentry;       /* 目录项链表 */
    u32                                 i_ino;          /* 节点号 */
    u32                                 i_blocks;       /* inode对应的文件所用块数 */
    u32                                 i_size;         /* inode对应文件的字节数 */
    u32                                 i_type;         /* inode对应文件的类型 */
    u32                                 i_state;        /* 索引节点的状态标志 */
    u32                                 i_count;        /* 引用计数 */
    unsigned int                        i_nlink;        /* 硬链接数 */
    u32                                 i_block_count;   /* 文件所占块数 */
    u32                                 i_block_size;   /* 块大小 */
    u32                                 i_block_size_bit;   /* 块大小位数 */
    u16                                 i_uid;          /* 使用者id */
    u16                                 i_gid;          /* 使用组id */
    u32                                 i_atime;        /* 最后访问时间 */
    u32                                 i_mtime;        /* 最后修改时间 */
    u32                                 i_ctime;        /* 最后改变时间 */
    u32	                                i_dtime;        // 删除时间
    const struct inode_operations       *i_op;          /* 索引节点操作函数 */
    const struct file_operations        *i_fop;         /* 缺省的索引节点操作 */
    struct super_block                  *i_sb;          /* 相关的超级块 */
    struct address_space                *i_mapping;     /* 相关的地址映射 */
    struct address_space                i_data;         /* 设备地址映射 */
    unsigned int                        i_flags;        /* 文件系统标志 */
    void                                *i_private;     /* fs 私有指针 */
};
// i_state: I_DIRTY_SYNC, I_DIRTY_DATASYNC, I_DIRTY_PAGES对应的磁盘索引节点必须被更新（I_DIRTY宏可检测）
// 索引节点操作函数
struct inode_operations {
    /* 为dentry对象创造一个新的索引节点 */
    int (*create) (struct inode *,struct dentry *, struct nameidata *);
    /* 在特定文件夹中寻找索引节点，该索引节点要对应于dentry中给出的文件名 */
    struct dentry * (*lookup) (struct inode *,struct dentry *, struct nameidata *);
    /* 创建硬链接 */
    int (*link) (struct dentry *,struct inode *,struct dentry *);
    /* 被系统调用mkdir()调用，创建一个新目录，mode指定创建时的初始模式 */
    int (*mkdir) (struct inode*, struct dentry*, u32);
    /* 被系统调用rmdir()调用，删除父目录inode中的子目录dentry */
    int (*rmdir) (struct inode*, struct dentry*);
    /* 该函数由VFS调用，重命名，前两个是原文件和原目录 */
    int (*rename) (struct inode*, struct dentry*, struct indoe*, struct dentry*);
    /* 从一个符号链接查找它指向的索引节点 */
    void * (*follow_link) (struct dentry *, struct nameidata *);
    /* 在 follow_link调用之后，该函数由VFS调用进行清除工作 */
    void (*put_link) (struct dentry *, struct nameidata *, void *);
    /* 该函数由VFS调用，用于修改文件的大小 */
    void (*truncate) (struct inode *);
};

/********************************* 目录项 *********************************/
// 目录项与对应文件进行链接的相关信息
struct dentry {
    u32                             d_count;                /* 使用计数 */
    unsigned int                    d_flags;                /* 目录项标识 */
//    spinlock_t                      d_lock;                 /* 单目录项锁 */
    u32                             d_mounted;              /* 是否登录点的目录项 */
    struct inode                    *d_inode;               /* 相关联的索引节点 */
    struct list_head                d_hash;                 /* 散列表 */
    struct dentry                   *d_parent;              /* 父目录的目录项对象 */
    struct qstr                     d_name;                 /* 目录项名称 */
    struct list_head                d_lru;                  /* 未使用的链表 */
    /*
     * d_child and d_rcu can share memory
     */
    union {
        struct list_head            d_child;                /* 目录项内部形成的链表 */
//        struct rcu_head             d_rcu;                  /* RCU加锁 */
    } d_u;
    struct list_head                d_subdirs;              /* 子目录链表 */
    struct list_head                d_alias;                /* 索引节点别名链表 */
    unsigned long                   d_time;                 /* 重置时间 */
    const struct dentry_operations  *d_op;                  /* 目录项操作相关函数 */
    struct super_block              *d_sb;                  /* 文件的超级块 */
    void                            *d_fsdata;              /* 文件系统特有数据 */

//    unsigned char   d_iname[DNAME_INLINE_LEN_MIN];          /* 短文件名 */
};
// 目录项操作函数
struct dentry_operations {
    /* 该函数判断目录项对象是否有效。VFS准备从dcache中使用一个目录项时会调用这个函数 */
    int (*d_revalidate)(struct dentry *, struct nameidata *);
    /* 为目录项对象生成hash值 */
    int (*d_hash) (struct dentry *, struct qstr *);
    /* 比较 qstr 类型的2个文件名 */
    int (*d_compare) (struct qstr *, struct qstr *);
    /* 当目录项对象的 d_count 为0时，VFS调用这个函数 */
    int (*d_delete)(struct dentry *);
    /* 当目录项对象将要被释放时，VFS调用该函数 */
    void (*d_release)(struct dentry *);
    /* 当目录项对象丢失其索引节点时（也就是磁盘索引节点被删除了），VFS会调用该函数 */
    void (*d_iput)(struct dentry *, struct inode *);
};

/********************************** 文件 *********************************/
// 仅进程访问文件期间存在于内核内存中
struct file {
    union {
        struct list_head            fu_list;        /* 文件对象链表 */
//        struct rcu_head             fu_rcuhead;     /* 释放之后的RCU链表 */
    } f_u;
    u32 		                    f_flags;        /* 当打开文件时所指定的标志 */
    u32			                    f_mode;         /* 进程的访问方式 */
    struct dentry		            *f_dentry;      /* 与文件相关的目录项对象 */
    struct vfsmount                 *f_vfsmnt;      /* 含有该文件的已安装文件系统 */
    u32                             f_pos;          /* 文件当前的读写位置 */
    const struct file_operations    *f_op;          /* 文件操作函数 */
    u32                             f_error;        /* 文件写操作的错误码 */
    u32                             f_count;        /* 文件对象引用计数 */
};
// 文件操作函数
struct file_operations {
    /* 用于更新偏移量指针,由系统调用lleek()调用它 */
//    loff_t (*llseek) (struct file *, loff_t, int);
    /* 由系统调用read()调用它 */
    u32 (*read) (struct file *, char* , u32,  long long *);
    /* 由系统调用write()调用它 */
    u32 (*write) (struct file *, const char* , u32, long long *);
    /* 返回目录列表中的下一个目录，调由系统调用readdir()用它 */
    u32 (*readdir) (struct file *, struct getdent *);
    /* 创建一个新的文件对象,并将它和相应的索引节点对象关联起来 */
    u32 (*open) (struct inode *, struct file *);
    /* 当已打开文件的引用计数减少时,VFS调用该函数，将修改后的内容写回磁盘 */
    u32 (*flush) (struct file *);
};
/****************************************vfs页 ************************************************/

/****************************************** 以下是函数声明 ***************************************/
// open.c for file open system call
struct file * vfs_open(const u8 *, u32); // 打开文件
struct file * dentry_open(struct dentry *, struct vfsmount *, u32);
int vfs_close(struct file *); // 关闭并释放文件

// namei.c for open_namei related functions
u32 open_namei(const u8 *, u32, struct nameidata *);
u32 path_lookup(const u8 *, u32, struct nameidata *);
inline void follow_dotdot(struct nameidata *);
u32 link_path_walk(const u8 *, struct nameidata *);
u32 do_lookup(struct nameidata *, struct qstr *, struct path *);
struct dentry * real_lookup(struct dentry *, struct qstr *, struct nameidata *);
struct dentry * __lookup_hash(struct qstr *, struct dentry *, struct nameidata *);
struct dentry * d_alloc(struct dentry *, const struct qstr *);

// read_write.c for file read and write system call
u32 vfs_read(struct file *file, char *buf, u32 count, u32 *pos);
u32 vfs_write(struct file *file, char *buf, u32 count, u32 *pos);
u32 generic_file_read(struct file *, u8 *, u32, u32 *);
u32 generic_file_write(struct file *, u8 *, u32, u32 *);
u32 generic_file_flush(struct file *);

// mount.c for file system mount
u32 follow_mount(struct vfsmount **, struct dentry **);
struct vfsmount * lookup_mnt(struct vfsmount *, struct dentry *);

// usr.c for user command
u32 vfs_cat(const u8 *);
u32 vfs_mkdir(const u8 *);
u32 vfs_rm(const u8 *);
u32 vfs_rm_r(const u8 *);
u32 vfs_ls(const u8 *);
u32 vfs_cd(const u8 *);
u32 vfs_mv(const u8 *);

u32 read_block(u8 *buf, u32 addr, u32 count);
u32 write_block(u8 *buf, u32 addr, u32 count);
u32 get_u32(u8 *ch);
u8 get_bit(const u8 *source, u32 index);
#endif