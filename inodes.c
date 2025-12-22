#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "structs.h"
#include "commandline.h"
#include "filesystem.h"


bool read_inode(filesystem_t *fs, int32_t inode_id, inode_t *inode) {
    if (inode_id < 0 || inode_id >= fs->sb.inode_count) return false;
    
    int32_t offset = fs->sb.inode_start + inode_id * sizeof(inode_t);
    return read_bytes(fs, offset, inode, sizeof(inode_t));
}

bool write_inode(filesystem_t *fs, int32_t inode_id, const inode_t *inode) {
    if (inode_id < 0 || inode_id >= fs->sb.inode_count) return false;
    
    int32_t offset = fs->sb.inode_start + inode_id * sizeof(inode_t);
    return write_bytes(fs, offset, inode, sizeof(inode_t));
}

int32_t alloc_inode(filesystem_t *fs) {
    for (int32_t i = 0; i < fs->sb.inode_count; i++) {
        if (!is_bit_set(fs->inode_bitmap, i)) {
            set_bit(fs->inode_bitmap, i);
            save_bitmaps(fs);
            return i;
        }
    }
    return -1;
}