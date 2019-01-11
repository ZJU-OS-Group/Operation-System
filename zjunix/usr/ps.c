#include "ps.h"
#include <driver/ps2.h>
#include <driver/sd.h>
#include <driver/vga.h>
#include <zjunix/bootmm.h>
#include <zjunix/buddy.h>
#include <zjunix/slab.h>
#include <zjunix/time.h>
#include <zjunix/vfs/vfs.h>
#include "ls.h"
#include "exec.h"
#include "myvi.h"

char ps_buffer[64];
int ps_buffer_index;

void test_proc() {
    unsigned int timestamp;
    unsigned int currTime;
    unsigned int data;
    asm volatile("mfc0 %0, $9, 6\n\t" : "=r"(timestamp));
    data = timestamp & 0xff;
    while (1) {
        asm volatile("mfc0 %0, $9, 6\n\t" : "=r"(currTime));
        if (currTime - timestamp > 100000000) {
            timestamp += 100000000;
            *((unsigned int *)0xbfc09018) = data;
        }
    }
}

/*int proc_demo_create() {
    int asid = pc_peek();
    if (asid < 0) {
        kernel_puts("Failed to allocate pid.\n", 0xfff, 0);
        return 1;
    }
    unsigned int init_gp;
    asm volatile("la %0, _gp\n\t" : "=r"(init_gp));
    pc_create("test", (void*)test_proc, 0, 0, 0, 0, NORMAL_PRIORITY_CLASS);
    return 0;
}*/

void ps() {
    kernel_printf("Press any key to enter shell.\n");
    kernel_getchar();
    char c;
    ps_buffer_index = 0;
    ps_buffer[0] = 0;
    kernel_clear_screen(31);
    kernel_puts("PowerShell\n", 0xfff, 0);
    kernel_puts("PS>", 0xfff, 0);
    while (1) {
//        kernel_printf("");
        c = kernel_getchar();
//        kernel_printf("I'm getting into PS! after get char\n");
        if (c == '\n') {
            ps_buffer[ps_buffer_index] = 0;
            if (kernel_strcmp(ps_buffer, "exit") == 0) {
                ps_buffer_index = 0;
                ps_buffer[0] = 0;
                kernel_printf("\nPowerShell exit.\n");
            } else
                parse_cmd();
            ps_buffer_index = 0;
            kernel_puts("PS>", 0xfff, 0);
        } else if (c == 0x08) {
            if (ps_buffer_index) {
                ps_buffer_index--;
                kernel_putchar_at(' ', 0xfff, 0, cursor_row, cursor_col - 1);
                cursor_col--;
                kernel_set_cursor();
            }
        } else {
            if (ps_buffer_index < 63) {
                ps_buffer[ps_buffer_index++] = c;
                kernel_putchar(c, 0xfff, 0);
            }
        }
    }
}

void parse_cmd() {
    u32 result = 0;
    char dir[32];
    char c;
    kernel_putchar('\n', 0, 0);
    char sd_buffer[8192];
    int i = 0;
    char *param;
    for (i = 0; i < 63; i++) {
        if (ps_buffer[i] == ' ' || ps_buffer[i] == '\n') {
            // 以空格或者换行作为命令的切换
            ps_buffer[i] = 0;
            break;
        }
    }
    if (i == 63)
        param = ps_buffer;
    else
        param = ps_buffer + i + 1;
    if (ps_buffer[0] == 0) {
        return;
    }
    else if (kernel_strcmp(ps_buffer, "clear") == 0) {
        kernel_clear_screen(31);
    }
    else if (kernel_strcmp(ps_buffer, "echo") == 0) {
        kernel_printf("%s\n", param);
    }
    else if (kernel_strcmp(ps_buffer, "gettime") == 0) {
        char buf[10];
        get_time(buf, sizeof(buf));
        kernel_printf("%s\n", buf);
    }
    else if (kernel_strcmp(ps_buffer, "sdwi") == 0) {
        for (i = 0; i < 512; i++)
            sd_buffer[i] = i;
        sd_write_block(sd_buffer, 7, 1);
        kernel_puts("sdwi\n", 0xfff, 0);
    }
    else if (kernel_strcmp(ps_buffer, "sdr") == 0) {
        sd_read_block(sd_buffer, 7, 1);
        for (i = 0; i < 512; i++) {
            kernel_printf("%d ", sd_buffer[i]);
        }
        kernel_putchar('\n', 0xfff, 0);
    }
    else if (kernel_strcmp(ps_buffer, "sdwz") == 0) {
        for (i = 0; i < 512; i++) {
            sd_buffer[i] = 0;
        }
        sd_write_block(sd_buffer, 7, 1);
        kernel_puts("sdwz\n", 0xfff, 0);
    }
    else if (kernel_strcmp(ps_buffer, "mminfo") == 0) {
        bootmap_info("bootmm");
        buddy_info();
    }
    else if (kernel_strcmp(ps_buffer, "mmtest") == 0) {
        kernel_printf("kmalloc : %x, size = 1KB\n", kmalloc(1024));
    }
    else if (kernel_strcmp(ps_buffer, "ps") == 0) {
        result = print_proc();
        kernel_printf("ps return with %d\n", result);
    }
    else if (kernel_strcmp(ps_buffer, "kill") == 0) {
        pid_t pid = (pid_t) (param[0] - '0');
        if (param[1]!=0) {
            pid = (pid_t) (pid * 10 + param[1] - '0');
        }
        kernel_printf("Killing process %d\n", pid);
        result = pc_kill(pid);
        kernel_printf("kill return with %d\n", result);
    }
    else if (kernel_strcmp(ps_buffer, "time") == 0) {
        unsigned int init_gp;
        asm volatile("la %0, _gp\n\t" : "=r"(init_gp));
        pc_create("time", (void*)system_time_proc, 0, 0, 0, 0, HIGH_PRIORITY_CLASS);
    }
    else if (kernel_strcmp(ps_buffer, "loop") == 0) {
        pc_create("loop", (void*)system_loop_proc,0,0,0,0,ABOVE_NORMAL_PRIORITY_CLASS);
//        pc_create(2, system_time_proc, (unsigned int)kmalloc(4096), init_gp, "time");

    } else if (kernel_strcmp(ps_buffer, "suicide") == 0) {
        pc_create("suicide", (void*)system_suicide_proc,0,0,0,0,NORMAL_PRIORITY_CLASS);
    } else if (kernel_strcmp(ps_buffer, "father") == 0) {
        pc_create("father", (void*)system_father,0,0,0,0,NORMAL_PRIORITY_CLASS);
    } else if (kernel_strcmp(ps_buffer, "child") == 0) {
        pc_create("child", (void*)system_child,0,0,0,0,NORMAL_PRIORITY_CLASS);
    }
        /*else if (kernel_strcmp(ps_buffer, "proc") == 0) {
            result = proc_demo_create();
            kernel_printf("proc return with %d\n", result);
        }
        else if (kernel_strcmp(ps_buffer, "cat") == 0) {
        } */
    else if (kernel_strcmp(ps_buffer, "cat") == 0) {
        result = vfs_cat(param);
        kernel_printf("ext3 cat return with %d\n", result);
    }
    else if (kernel_strcmp(ps_buffer, "vi") == 0) {
        result = myvi(param);
        kernel_printf("vi return with %d\n", result);
    }
    else if (kernel_strcmp(ps_buffer, "exec") == 0) {
        result = exec(param);
        kernel_printf("exec return with %d\n", result);
    }
    else if (kernel_strcmp(ps_buffer, "mkdir") == 0){
    	result = vfs_mkdir(param);
    	kernel_printf("ext3 mkdir return with %d\n", result);
   	}
    else if (kernel_strcmp(ps_buffer, "touch") == 0){
        result = vfs_touch(param);
        kernel_printf("fat32 touch return with %d\n", result);
    }
    else if (kernel_strcmp(ps_buffer, "rm") == 0){
        if (param[0]=='-'&&param[1]=='r') {
            while (*param!=' ') {
                param++;
            }
            result = vfs_rm_r(param);
        } else {
            result = vfs_rm(param);
        }
		kernel_printf("vfs rm return with %d\n", result);
	}
	else if (kernel_strcmp(ps_buffer, "ls") == 0){
        result = vfs_ls(param);
		kernel_printf("vfs ls return with %d\n", result);
	} 
	else if (kernel_strcmp(ps_buffer, "cd") == 0){
		result = vfs_cd(param);
		kernel_printf("vfs cd return with %d\n", result);
	}
	else if (kernel_strcmp(ps_buffer, "mv") == 0){
		result = vfs_mv(param);
		kernel_printf("vfs mv return with %d\n", result);
	}
	else {
        kernel_puts(ps_buffer, 0xfff, 0);
        kernel_puts(": command not found\n", 0xfff, 0);
    }
}
