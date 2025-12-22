#pragma once
#include "structs.h"
#include <stdbool.h>

bool read_inode(filesystem_t *fs, int32_t inode_id, inode_t *inode);

bool write_inode(filesystem_t *fs, int32_t inode_id, const inode_t *inode);

int32_t alloc_inode(filesystem_t *fs);