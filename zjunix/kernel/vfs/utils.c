#include <zjunix/vfs/vfs.h>

// 从addr的绝对扇区地址开始读count个扇区的数据
u32 read_block(u8 *buf, u32 addr, u32 count) {
    return sd_read_block(buf, addr, count);
}

// 从addr的绝对扇区地址开始写count个扇区的数据
u32 write_block(u8 *buf, u32 addr, u32 count) {
    return sd_write_block(buf, addr, count);
}

u32 get_u32(u8 *ch) {
    return (*ch) + ((*(ch + 1)) << 8) + ((*(ch + 2)) << 16) + ((*(ch + 3)) << 24);
}

//获取位图上的某一位
u8 get_bit(const u8 *source, u32 index){
    u8 byte = *(source + index / BITS_PER_BYTE);  //获取这一位所在的字节
    u8 mask = 1 << (index % BITS_PER_BYTE);    //获取这一位在字节内的定位
    return (byte & mask);
}