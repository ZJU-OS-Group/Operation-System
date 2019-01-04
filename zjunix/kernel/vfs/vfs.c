#include <zjunix/vfs/vfs.h>

// global
struct dentry               * root_dentry;      /* 根目录 */
struct dentry               * pwd_dentry;       /* 工作目录 */
struct vfsmount             * root_mnt;         /* 根挂载点 */
struct vfsmount             * pwd_mnt;          /* 当前挂载点 */

u32 init_vfs() {

}
