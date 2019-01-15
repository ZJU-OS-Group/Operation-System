

#include <zjunix/debug/debug.h>

#define DEBUG_EN 0

void debug_start(const char* information){
    if (DEBUG_EN) kernel_puts(information,VGA_START,VGA_BLACK);
}

void debug_end(const char* information){
    if (DEBUG_EN) kernel_puts(information,VGA_END,VGA_BLACK);
}

void debug_err(const char* information){
    if (DEBUG_EN) kernel_puts(information,VGA_ERR,VGA_BLACK);
}

void debug_warning(const char* information){
    if (DEBUG_EN) kernel_puts(information,VGA_WARNING,VGA_BLACK);
}

void debug_normal(const char* information){
    if (DEBUG_EN) kernel_puts(information,VGA_NORMAL,VGA_BLACK);
}

void debug_info(const char* information){
    if (DEBUG_EN) kernel_puts(information,VGA_INFO,VGA_BLACK);
}