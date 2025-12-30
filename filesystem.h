#pragma once
#include "structs.h"
#include <stdbool.h>

// wrapper fseek a fread
bool read_bytes(filesystem_t *fs, int32_t offset, void *buffer, size_t size);

// wrapper fseek a fwrite
bool write_bytes(filesystem_t *fs, int32_t offset, const void *buffer, size_t size);

// najde pozici clusteru v souboru a přečte jeho obsah
bool read_cluster(filesystem_t *fs, int32_t cluster_num, void *buffer);

// najde pozici clusteru v souboru a zapíše jeho obsah
bool write_cluster(filesystem_t *fs, int32_t cluster_num, const void *buffer);

// přečtení dat ze superbloku
bool load_superblock(filesystem_t *fs);

//zápis dat do superbloku
bool save_superblock(filesystem_t *fs);

//Načtení bitmapy do paměti
void load_bitmaps(filesystem_t *fs);

//Zápis změn do paměti
void save_bitmaps(filesystem_t *fs);

//testování hodnoty bitu
bool is_bit_set(uint8_t *bitmap, int32_t index);

//nastavení hodnoty bitu na 1
void set_bit(uint8_t *bitmap, int32_t index);

//vymazání hodnoty bitu (nastavení na 0)
void clear_bit(uint8_t *bitmap, int32_t index);

//Hledá položku v adresáři podle jména, vrací inode nebo -1 pokud nenalezeno
int32_t find_in_dir(filesystem_t *fs, int32_t dir_inode_id, const char *name);

//přidání položky do adresáře
bool add_to_dir(filesystem_t *fs, int32_t dir_inode_id, const char *name, int32_t inode_id);

//Vrátí inode číslo pro zadanou cestu, -1 pokud neexistuje
int32_t resolve_path(filesystem_t *fs, const char *path);

//Upravuje výslednou cestu zadanou uživatelem
void update_path(char *current_path, const char *input);

//Odebere položku z adresáře
bool remove_from_dir(filesystem_t *fs, int32_t dir_inode_id, const char *name);

//Rozdělí cestu na rodičovský inode a jméno souboru/adresáře
bool split_path(filesystem_t *fs, const char *path, int32_t *out_parent_inode, char *out_name);