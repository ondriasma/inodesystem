#pragma once
#include "structs.h"


// Počet položek adresáře na jeden cluster
#define ENTRIES_PER_CLUSTER (CLUSTER_SIZE / sizeof(dir_item_t))

// Počet inodů na jeden cluster
#define INODES_PER_CLUSTER (CLUSTER_SIZE / sizeof(inode_t))

// Počet ukazatelů na jeden cluster
#define PTRS_PER_CLUSTER (CLUSTER_SIZE / sizeof(int32_t))

// Alokuje volný cluster pro uložení souboru
int32_t alloc_cluster(filesystem_t *fs);

// Uvolní cluster pro další použití
void free_cluster(filesystem_t *fs, int32_t cluster);

// najde pozici clusteru v souboru a přečte jeho obsah
bool read_cluster(filesystem_t *fs, int32_t cluster_num, void *buffer);

// najde pozici clusteru v souboru a zapíše jeho obsah
bool write_cluster(filesystem_t *fs, int32_t cluster_num, const void *buffer);

// Vrací číslo clusteru pro daný index v souboru - mapuje relativní index na fyzický cluster
int32_t get_file_cluster(filesystem_t *fs, inode_t *inode, int32_t cluster_index);

// Přiřazuje clustery ukazatelům
int set_file_cluster(filesystem_t *fs, inode_t *inode, int32_t cluster_index, int32_t cluster_num);