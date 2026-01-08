#pragma once
#include "structs.h"
#include <stdbool.h>

// přečtení obsahu i-uzlu
bool read_inode(filesystem_t *fs, int32_t inode_id, inode_t *inode);

// zápis do i-uzlu
bool write_inode(filesystem_t *fs, int32_t inode_id, const inode_t *inode);

//alokuje inode - najde první volný v bitmapě, označí ho jako obsazený a vrátí jeho číslo
int32_t alloc_inode(filesystem_t *fs);