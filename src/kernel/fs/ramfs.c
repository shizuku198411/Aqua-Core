#include "fs_private.h"
#include "core/commonlibs.h"

void ramfs_init_instance(struct nodefs *fs) {
    if (!fs) {
        return;
    }
    memset(fs, 0, sizeof(*fs));
    fs->mount_idx = -1;
    fs->persistent = 0;
    fs->nodes[0].used = 1;
    fs->nodes[0].type = FS_TYPE_DIR;
    fs->nodes[0].parent = -1;
    fs->nodes[0].name[0] = '/';
    fs->nodes[0].name[1] = '\0';
}
