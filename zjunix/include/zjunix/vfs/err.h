#ifndef OPERATION_SYSTEM_ERRNO_H
#define OPERATION_SYSTEM_ERRNO_H

#include <zjunix/vfs/errno.h>
#include <zjunix/type.h>

#define MAX_ERRNO	4095
#define IS_ERR_VALUE(x) ((x) >= (u32)-MAX_ERRNO)

// 判断是不是错误指针或者空指针
static inline u32 IS_ERR_OR_NULL(const void *ptr){
	return (!ptr) || IS_ERR_VALUE((u32)ptr);
}

#endif