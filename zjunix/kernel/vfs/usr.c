

// TODO: vfs_open
// TODO: 文件打开方式定义（先定义了一部分，之后需要再加）

// cat：输出文件内容
u32 vfs_cat(const u8 *path){
    u8 *buf;
    u32 err;
    u32 base;
    u32 file_size;
    struct file *file;

    // 打开文件
    file = vfs_open(path, O_RDONLY, base);
    // 处理打开文件的错误
    if (IS_ERR_OR_NULL(file)){
        if ( PTR_ERR(file) == -ENOENT )
            kernel_printf("File not found!\n");
        return PTR_ERR(file);
    }

    // 读取文件内容到缓存区
     base = 0;
     file_size = file->f_dentry->d_inode->i_size;

    // buf = (u8*) kmalloc (file_size + 1);
    // if ( vfs_read(file, buf, file_size, &base) != file_size )
    //     return 1;

    // // 打印buf里面的内容
    // buf[file_size] = 0;
    // kernel_printf("%s\n", buf);

    // // 关闭文件并释放内存
    // err = vfs_close(file);
    // if (err)
    //     return err;

    // kfree(buf);
    return 0;
}