//
// Created by Desmond on 2019/1/6.
//

#ifndef OPERATION_SYSTEM_DEBUG_H
#define OPERATION_SYSTEM_DEBUG_H
#include <driver/vga.h>
void debug_start(const char*);

void debug_end(const char*);

void debug_err(const char*);

void debug_warning(const char*);

void debug_normal(const char*);

void debug_info(const char*);

#endif //OPERATION_SYSTEM_DEBUG_H
