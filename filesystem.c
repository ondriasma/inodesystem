#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "structs.h"
#include "filesystem.h"
#include "inodes.h"
#include "clusters.h"



bool read_bytes(filesystem_t *fs, int32_t offset, void *buffer, size_t size) {
    if (fseek(fs->file, offset, SEEK_SET) != 0) return false;
    if (fread(buffer, size, 1, fs->file) != 1) return false;
    return true;
}

bool write_bytes(filesystem_t *fs, int32_t offset, const void *buffer, size_t size) {
    if (fseek(fs->file, offset, SEEK_SET) != 0) return false;
    if (fwrite(buffer, size, 1, fs->file) != 1) return false;
    fflush(fs->file);
    return true;
}

bool read_cluster(filesystem_t *fs, int32_t cluster_num, void *buffer) {
    int32_t offset = fs->sb.data_start + cluster_num * fs->sb.cluster_size;
    return read_bytes(fs, offset, buffer, fs->sb.cluster_size);
}

bool write_cluster(filesystem_t *fs, int32_t cluster_num, const void *buffer) {
    int32_t offset = fs->sb.data_start + cluster_num * fs->sb.cluster_size;
    return write_bytes(fs, offset, buffer, fs->sb.cluster_size);
}




bool load_superblock(filesystem_t *fs) {
    return read_bytes(fs, 0, &fs->sb, sizeof(superblock_t));
}

bool save_superblock(filesystem_t *fs) {
    return write_bytes(fs, 0, &fs->sb, sizeof(superblock_t));
}




void load_bitmaps(filesystem_t *fs) {
    int32_t ibitmap_size = (fs->sb.inode_count + 7) / 8;
    int32_t dbitmap_size = (fs->sb.cluster_count + 7) / 8;
    
    fs->inode_bitmap = malloc(ibitmap_size);
    fs->data_bitmap = malloc(dbitmap_size);
    
    read_bytes(fs, fs->sb.bitmapi_start, fs->inode_bitmap, ibitmap_size);
    read_bytes(fs, fs->sb.bitmap_start, fs->data_bitmap, dbitmap_size);
}

void save_bitmaps(filesystem_t *fs) {
    int32_t inode_bitmap_size = (fs->sb.inode_count + 7) / 8;
    int32_t data_bitmap_size = (fs->sb.cluster_count + 7) / 8;
    
    write_bytes(fs, fs->sb.bitmapi_start, fs->inode_bitmap, inode_bitmap_size);
    write_bytes(fs, fs->sb.bitmap_start, fs->data_bitmap, data_bitmap_size);
}

bool is_bit_set(uint8_t *bitmap, int32_t index){
    int32_t byte_index = index / 8;
    int32_t bit_index  = index % 8;

    uint8_t mask = (1 << bit_index);

    return (bitmap[byte_index] & mask) != 0;
}


void set_bit(uint8_t *bitmap, int32_t index)
{
    int32_t byte_index = index / 8;
    int32_t bit_index  = index % 8;

    uint8_t mask = (1 << bit_index);

    bitmap[byte_index] |= mask;
}

void clear_bit(uint8_t *bitmap, int32_t index)
{
    int32_t byte_index = index / 8;
    int32_t bit_index  = index % 8;

    uint8_t mask = (1 << bit_index);

    bitmap[byte_index] &= ~mask;
}





int32_t find_in_dir(filesystem_t *fs, int32_t dir_inode_id, const char *name) {
    printf("[DEBUG] find_in_dir: searching for '%s' in inode %d\n", name, dir_inode_id);
    
    inode_t dir_inode;
    if (!read_inode(fs, dir_inode_id, &dir_inode)) {
        printf("[DEBUG] Failed to read inode %d\n", dir_inode_id);
        return -1;
    }

    //nejedná se o složku
    if (!dir_inode.is_directory) {
        printf("[DEBUG] Inode %d is not a directory\n", dir_inode_id);
        return -1;
    }
    
    printf("[DEBUG] Directory file_size: %d bytes\n", dir_inode.file_size);
    printf("[DEBUG] Directory direct1: %d\n", dir_inode.direct1);
    
    int32_t cluster_count = (dir_inode.file_size + fs->sb.cluster_size - 1) / fs->sb.cluster_size;
    printf("[DEBUG] Calculated cluster_count: %d\n", cluster_count);
    
    //složka je prázdná
    if (cluster_count == 0) {
        printf("[DEBUG] Directory is empty (0 clusters)\n");
        return -1;
    }
    
    dir_entry_t entries[ENTRIES_PER_CLUSTER];
    
    for (int32_t i = 0; i < cluster_count; i++) {
        int32_t cluster = get_file_cluster(fs, &dir_inode, i);
        
        printf("[DEBUG] Cluster index %d -> cluster number %d\n", i, cluster);
        
        if (cluster == 0) {
            printf("[DEBUG] Cluster is 0, skipping\n");
            continue;
        }
        
        read_cluster(fs, cluster, entries);
        
        printf("[DEBUG] Reading cluster %d, checking entries:\n", cluster);
        for (int j = 0; j < ENTRIES_PER_CLUSTER; j++) {
            if (entries[j].inode != 0) {
                printf("[DEBUG]   Entry %d: name='%s', inode=%d\n", 
                       j, entries[j].name, entries[j].inode);
                
                if (strcmp(entries[j].name, name) == 0) {
                    printf("[DEBUG] FOUND! Returning inode %d\n", entries[j].inode);
                    return entries[j].inode;
                }
            }
        }
    }
    
    printf("[DEBUG] Not found, returning -1\n");
    return -1;
}


bool add_to_dir(filesystem_t *fs, int32_t dir_inode_id, const char *name, int32_t inode_id) {
    inode_t dir_inode;
    if (!read_inode(fs, dir_inode_id, &dir_inode)) return false;
    
    int32_t cluster_count = (dir_inode.file_size + fs->sb.cluster_size - 1) / fs->sb.cluster_size;
    dir_entry_t entries[ENTRIES_PER_CLUSTER];
    
    //Není prázdný cluster?
    for (int32_t i = 0; i < cluster_count; i++) {
        int32_t cluster = get_file_cluster(fs, &dir_inode, i);
        if (cluster == 0) continue;
        
        read_cluster(fs, cluster, entries);
        for (int j = 0; j < ENTRIES_PER_CLUSTER; j++) {
            if (entries[j].inode == 0) {
                strncpy(entries[j].name, name, NAME_SIZE - 1);
                entries[j].name[NAME_SIZE - 1] = '\0';
                entries[j].inode = inode_id;
                write_cluster(fs, cluster, entries);
                return true;
            }
        }
    }
    
    //pokud ne, alokuj nové
    int32_t new_cluster = alloc_cluster(fs);
    if (new_cluster < 0) return false;
    
    memset(entries, 0, sizeof(entries));
    strncpy(entries[0].name, name, NAME_SIZE - 1);
    entries[0].inode = inode_id;
    
    write_cluster(fs, new_cluster, entries);
    set_file_cluster(fs, &dir_inode, cluster_count, new_cluster);
    
    dir_inode.file_size += fs->sb.cluster_size;
    write_inode(fs, dir_inode_id, &dir_inode);
    
    return true;
}



int32_t resolve_path(filesystem_t *fs, const char *path) {
    printf("[DEBUG] resolve_path: '%s'\n", path);
    
    //prázdná cesta -> aktuální adresář
    if (!path || !path[0]) {
        return fs->current_inode;
    }
    
    //absolutní/relativní cesta
    int32_t current = (path[0] == '/') ? 0 : fs->current_inode;
    
    //root adresář
    if (strcmp(path, "/") == 0) {
        return 0;
    }
    
    char path_copy[256];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';
    
    // Skip leading slash if present
    char *p = path_copy;
    if (*p == '/') p++;
    
    // Tokenize by '/'
    char *token = strtok(p, "/");
    while (token) {
        printf("[DEBUG]   Processing token: '%s' (current inode: %d)\n", token, current);
        
        //"." -> stejná složka
        if (strcmp(token, ".") == 0) {
            // Stay in current directory
            token = strtok(NULL, "/");
            continue;
        }
        
        //".." -> přesun do rodičovské složky
        if (strcmp(token, "..") == 0) {
            inode_t node;
            if (!read_inode(fs, current, &node)) return -1;
            current = node.parent; // Directly move to parent
            token = strtok(NULL, "/");
            continue;
        }
        
        // Regular name - look it up
        int32_t next = find_in_dir(fs, current, token);
        if (next < 0) {
            printf("[DEBUG]   Not found: '%s'\n", token);
            return -1;
        }
        
        current = next;
        token = strtok(NULL, "/");
    }
    
    printf("[DEBUG]   Path copy: %s\n", path_copy);
    printf("[DEBUG]   Final inode: %d\n", current);
    return current;
}


void update_path(char *current_path, const char *input) {
    if (strcmp(input, "..") == 0) {
        char *last_slash = strrchr(current_path, '/');
        if (last_slash != NULL) {
            if (last_slash == current_path) {
                //jedná se o root adresář
                current_path[1] = '\0';
            } else {
                //odstranění posledního adresáře
                *last_slash = '\0';
            }
        }
    } else if (strcmp(input, ".") != 0) {
        //pokud nekončí lomítkem, přidáme ho se jménem adresáře
        if (current_path[strlen(current_path) - 1] != '/') {
            strcat(current_path, "/");
        }
        strcat(current_path, input);
    }
}
