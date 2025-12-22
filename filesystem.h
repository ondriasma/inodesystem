#pragma once
#include "structs.h"
#include <stdbool.h>

// wrapper fseek a fread
bool read_bytes(filesystem_t *fs, int32_t offset, void *buffer, size_t size);

// wrapper fseek a fwrite
bool write_bytes(filesystem_t *fs, int32_t offset, const void *buffer, size_t size);

bool read_cluster(filesystem_t *fs, int32_t cluster_num, void *buffer);

bool write_cluster(filesystem_t *fs, int32_t cluster_num, const void *buffer);

bool load_superblock(filesystem_t *fs);

bool save_superblock(filesystem_t *fs);

//Načtení bitmapy do paměti
void load_bitmaps(filesystem_t *fs);

//Zápis změn do paměti
void save_bitmaps(filesystem_t *fs);

bool is_bit_set(uint8_t *bitmap, int32_t index);

void set_bit(uint8_t *bitmap, int32_t index);

void clear_bit(uint8_t *bitmap, int32_t index);

//Hledá položku v adresáři podle jména, vrací inode nebo -1 pokud nenalezeno
int32_t find_in_dir(filesystem_t *fs, int32_t dir_inode_id, const char *name);

bool add_to_dir(filesystem_t *fs, int32_t dir_inode_id, const char *name, int32_t inode_id);

int32_t resolve_path(filesystem_t *fs, const char *path);