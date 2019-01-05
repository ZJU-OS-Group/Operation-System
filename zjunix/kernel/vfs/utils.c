#include <zjunix/vfs/vfs.h>

// 从addr的绝对扇区地址开始读count个扇区的数据
inline u32 read_block(u8 *buf, u32 addr, u32 count) {
    return sd_read_block(buf, addr, count);
}

// 从addr的绝对扇区地址开始写count个扇区的数据
inline u32 write_block(u8 *buf, u32 addr, u32 count) {
    return sd_write_block(buf, addr, count);
}

inline u32 get_u32(u8 *ch) {
    return (*ch) + ((*(ch + 1)) << 8) + ((*(ch + 2)) << 16) + ((*(ch + 3)) << 24);
}

//获取位图上的某一位
inline u8 get_bit(const u8 *source, u32 index){
    return (*(source + index / BITS_PER_BYTE) & (u8) (1 << (index % BITS_PER_BYTE)));
}

inline void reset_bit(u8 *source, u32 index){
    *(source + index / BITS_PER_BYTE) &= ~(1 << (index % BITS_PER_BYTE));
}

inline void set_bit(u8 *source, u32 index){
    *(source + index / BITS_PER_BYTE) |= 1 << (index % BITS_PER_BYTE);
}

inline u32 generic_qstr_compare(struct qstr * a, struct qstr * b) {
    return kernel_strcmp(a->name,b->name);
}