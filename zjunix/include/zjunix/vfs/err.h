#ifndef OPERATION_SYSTEM_ERR_H
#define OPERATION_SYSTEM_ERR_H

#include <zjunix/vfs/errno.h>
#include <zjunix/type.h>


#define MAX_ERRNO	4095
#define IS_ERR_VALUE(x) ((x) >= (u32)-MAX_ERRNO)

// 判断是不是错误指针或者空指针
static inline u32 IS_ERR_OR_NULL(const void *ptr){
	return (!ptr) || IS_ERR_VALUE((u32)ptr);
}

static inline u32 PTR_ERR(const void *ptr){
	return (u32) ptr;
}

static inline u32 IS_ERR(const void *ptr){
	return IS_ERR_VALUE((u32)ptr);
}

static inline void * ERR_PTR(u32 error){
	return (void *) error;
}
#endif