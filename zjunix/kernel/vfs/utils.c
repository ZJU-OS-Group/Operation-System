#include <zjunix/vfs/vfs.h>

// 从addr的绝对扇区地址开始读count个扇区的数据
u32 read_block(u8 *buf, u32 addr, u32 count) {
    return sd_read_block(buf, addr, count);
}

// 从addr的绝对扇区地址开始写count个扇区的数据
u32 write_block(u8 *buf, u32 addr, u32 count) {
    return sd_write_block(buf, addr, count);
}

