#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>


#define CLUSTER_SIZE 4096
#define NAME_SIZE 12
#define DIRECT_LINKS 5
#define SIGNATURE "ZOSFS25"
#define ID_ITEM_FREE 0


typedef struct {
    char signature[9];           //login autora FS
    char description[251];       //popis vygenerovaného FS (cokoliv)
    int32_t disk_size;          //celkova velikost VFS 
    int32_t cluster_size;       //velikost clusteru
    int32_t cluster_count;      //pocet clusteru
    int32_t inode_count;        //pocet inodů
    int32_t bitmapi_start;      //adresa pocatku bitmapy i-uzlů
    int32_t bitmap_start;       //adresa pocatku bitmapy datových bloků
    int32_t inode_start;        //adresa pocatku  i-uzlů
    int32_t data_start;         //adresa pocatku datovych bloku  
} superblock_t;

typedef struct {
    int32_t nodeid;             //ID i-uzlu, pokud ID = ID_ITEM_FREE, je polozka volna
    bool is_directory;          //soubor, nebo adresar
    int8_t references;          //počet odkazů na i-uzel, používá se pro hardlinky
    int32_t file_size;          //velikost souboru v bytech
    int32_t direct1;            // 1. přímý odkaz na datové bloky
    int32_t direct2;            // 2. přímý odkaz na datové bloky
    int32_t direct3;            // 3. přímý odkaz na datové bloky
    int32_t direct4;            // 4. přímý odkaz na datové bloky
    int32_t direct5;            // 5. přímý odkaz na datové bloky
    int32_t indirect1;          // 1. nepřímý odkaz (odkaz -> datové bloky)
    int32_t indirect2;          // 2. nepřímý odkaz (odkaz -> odkaz -> datové bloky)
} inode_t;

typedef struct {
    int32_t inode;              // inode odpovídající souboru
    char name[NAME_SIZE];       //8+3 + /0 C/C++ ukoncovaci string znak
} dir_entry_t;


typedef struct {
    superblock_t sb;            //superblok
    uint8_t *inode_bitmap;      //bitmapa inodů
    uint8_t *data_bitmap;       //bitmapa datových bloků
    char current_path[256];     //cesta k aktuálnímu adresáři
    int32_t current_inode;      //inode aktuálního adresáře
    char *filename;             //jméno souboru s fs
    FILE *file;                 //soubor s fs
} filesystem_t;