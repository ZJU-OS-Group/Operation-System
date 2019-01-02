//
// Created by Desmond.
//
#include <zjunix/type.h>
#include <zjunix/list.h>

struct cache {
    u8 cache_size;
    u8 cache_capacity;
    struct list_head *content_list;
    struct list_head
};
