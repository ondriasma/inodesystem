#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include "structs.h"
#include "commandline.h"
#include "inodes.h"
#include "clusters.h"
#include "filesystem.h"



void pwd(filesystem_t *fs) {
    printf("%s\n", fs->current_path);
}

bool ls(filesystem_t *fs, const char *path) {
    int32_t dir_id = path && path[0] ? find_in_dir(fs, fs->current_inode, path) : fs->current_inode;
    
    if (dir_id < 0) {
        printf("PATH NOT FOUND\n");
        return false;
    }
    
    inode_t dir_inode;
    if (!read_inode(fs, dir_id, &dir_inode) || !dir_inode.is_directory) {
        printf("PATH NOT FOUND\n");
        return false;
    }
    
    
    printf("[DEBUG] Listing directory inode %d, size=%d bytes\n", dir_id, dir_inode.file_size);
    
    
    int32_t cluster_count = (dir_inode.file_size + fs->sb.cluster_size - 1) / fs->sb.cluster_size;
    
    if (cluster_count == 0) {
        // složka je prázdná
        return true;
    }
    
    dir_item_t entries[ENTRIES_PER_CLUSTER];
    
    for (int32_t i = 0; i < cluster_count; i++) {
        int32_t cluster = get_file_cluster(fs, &dir_inode, i);
        if (cluster == 0) continue;
        
        read_cluster(fs, cluster, entries);
        for (int j = 0; j < ENTRIES_PER_CLUSTER; j++) {
            if (entries[j].inode != 0) {
                inode_t entry_inode;
                read_inode(fs, entries[j].inode, &entry_inode);
                printf("%s: %s\n", entry_inode.is_directory ? "DIR" : "FILE", entries[j].name);
            }
        }
    }
    return true;
}

bool mkdir(filesystem_t *fs, const char *name) {
    printf("[DEBUG] mkdir called for '%s'\n", name);
    printf("[DEBUG] Current inode: %d\n", fs->current_inode);
    
    if (find_in_dir(fs, fs->current_inode, name) >= 0) {
        printf("EXIST\n");
        return false;
    }
    
    int32_t new_inode_id = alloc_inode(fs);
    if (new_inode_id < 0) {
        printf("CANNOT CREATE FILE\n");
        return false;
    }
    
    printf("[DEBUG] Allocated inode: %d\n", new_inode_id);
    
    inode_t new_inode = {0};
    new_inode.nodeid = new_inode_id;
    new_inode.is_directory = true;
    new_inode.references = 1;
    new_inode.file_size = 0;
    new_inode.parent = fs->current_inode;
    write_inode(fs, new_inode_id, &new_inode);
    
    printf("[DEBUG] Wrote inode %d, calling add_to_dir\n", new_inode_id);
    
    if (add_to_dir(fs, fs->current_inode, name, new_inode_id) == false) {
        printf("PATH NOT FOUND\n");
        return false;
    }

    printf("[DEBUG] add_to_dir completed successfully\n");
    
    // Verify it was added by reading back the parent directory TODO smazat
    inode_t parent;
    read_inode(fs, fs->current_inode, &parent);
    printf("[DEBUG] Parent dir inode %d now has size: %d bytes\n", 
           fs->current_inode, parent.file_size);
    
    printf("OK\n");
    return true;
}

bool incp(filesystem_t *fs, const char *src, const char *dest) {
    int32_t dest_parent_id;
    char clean_filename[NAME_SIZE];

    if (!split_path(fs, dest, &dest_parent_id, clean_filename)) {
        printf("PATH NOT FOUND\n");
        return false;
    }


    FILE *f = fopen(src, "rb");
    if (!f) {
        printf("FILE NOT FOUND\n");
        return false;
    }
    
    fseek(f, 0, SEEK_END);
    int32_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    uint8_t *data = malloc(size);
    fread(data, 1, size, f);
    fclose(f);
    
    int32_t new_inode_id = alloc_inode(fs);
    if (new_inode_id < 0) {
        free(data);
        printf("CANNOT CREATE FILE\n");
        return false;
    }
    
    inode_t new_inode = {0};
    new_inode.nodeid = new_inode_id;
    new_inode.is_directory = false;
    new_inode.references = 1;
    new_inode.file_size = size;
    
    // zápis dat do clusterů
    int32_t clusters_needed = (size + fs->sb.cluster_size - 1) / fs->sb.cluster_size;
    for (int32_t i = 0; i < clusters_needed; i++) {
        int32_t cluster = alloc_cluster(fs);
        if (cluster < 0) {
            free(data);
            printf("CANNOT CREATE FILE\n");
            return false;
        }
        
        int32_t offset = i * fs->sb.cluster_size;
        int32_t to_write;
        if (size - offset > fs->sb.cluster_size) {
            to_write = fs->sb.cluster_size;
        } else {
            to_write = size - offset;
        }

        uint8_t buffer[CLUSTER_SIZE] = {0};
        memcpy(buffer, data + offset, to_write);
        write_cluster(fs, cluster, buffer);
        set_file_cluster(fs, &new_inode, i, cluster);
    }
    
    



    write_inode(fs, new_inode_id, &new_inode);


    if (!add_to_dir(fs, dest_parent_id, clean_filename, new_inode_id)) {
        printf("PATH NOT FOUND\n");
        free(data);
        return false;
    }
    
    free(data);
    printf("OK\n");
    return true;
}

bool format(filesystem_t *fs, const char *size_str) {
    int32_t size_mb = 600;//todo
    if (size_str && size_str[0]) {
        sscanf(size_str, "%dMB", &size_mb);
    }
    
    int32_t total_size = size_mb * 1024 * 1024;
    int32_t cluster_count = total_size / CLUSTER_SIZE;
    int32_t inode_count = cluster_count / 8;
    
    int32_t ibitmap_size = (inode_count + 7) / 8;
    int32_t dbitmap_size = (cluster_count + 7) / 8;
    int32_t inode_table_size = inode_count * sizeof(inode_t);
    
    memset(&fs->sb, 0, sizeof(superblock_t));
    strcpy(fs->sb.signature, SIGNATURE);
    strcpy(fs->sb.description, "ZOS Inodesystem");
    fs->sb.disk_size = total_size;
    fs->sb.cluster_size = CLUSTER_SIZE;
    fs->sb.cluster_count = cluster_count;
    fs->sb.inode_count = inode_count;
    
    int32_t offset = sizeof(superblock_t);
    fs->sb.bitmapi_start = offset; offset += ibitmap_size;
    fs->sb.bitmap_start = offset; offset += dbitmap_size;
    fs->sb.inode_start = offset; offset += inode_table_size;
    fs->sb.data_start = offset;
    
    save_superblock(fs);
    
    // vytvoření bitmap
    fs->inode_bitmap = calloc(1, ibitmap_size);
    fs->data_bitmap = calloc(1, dbitmap_size);
    save_bitmaps(fs);
    
    // vytvoření root adresáře
    int32_t root_id = alloc_inode(fs);
    printf("[DEBUG] format: root_id = %d\n", root_id);

    inode_t root = {0};
    root.nodeid = root_id;
    root.is_directory = true;
    root.references = 1;
    root.parent = root_id;

    bool b = write_inode(fs, root_id, &root);
    printf("[DEBUG] format: write_inode returned %s\n", b ? "true" : "false");

    if (!b) {
        printf("[DEBUG] format: Failed to write root inode!\n");
    }

    // Try to read it back to verify, tohle celé smazat TODO
    inode_t verify;
    b = read_inode(fs, root_id, &verify);
    printf("[DEBUG] format: read_inode returned %s\n", b ? "true" : "false");
    if (b) {
        printf("[DEBUG] format: Root inode verified: is_directory=%d, file_size=%d\n",
            verify.is_directory, verify.file_size);
    }
    
    fs->current_inode = root_id;
    strcpy(fs->current_path, "/");
    
    printf("OK\n");
    return true;
}



bool cd(filesystem_t *fs, const char *path) {
    if (strcmp(path, "/") == 0) {  // jedná se o root složku
        fs->current_inode = 0; 
        strcpy(fs->current_path, "/");
        printf("OK\n");
        return true;
    }
    
    // hledání cílové složky
    int32_t target_inode = resolve_path(fs, path);
    
    if (target_inode < 0) {
        printf("PATH NOT FOUND\n");
        return false;
    }
    
    // načtení i-uzlu cílové složky
    inode_t target;
    if (!read_inode(fs, target_inode, &target)) {
        printf("PATH NOT FOUND\n");
        return false;
    }


    // test, jestli se vůbec jedná o složku
    if (!target.is_directory) {
        printf("PATH NOT FOUND\n");
        return false;
    }
    

    // absolutní cesta, začínáme od rootu
    if (path[0] == '/') {
        strcpy(fs->current_path, "/");
    }


    char path_copy[256];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    
    char *token = strtok(path_copy, "/");
    while (token != NULL) {
        // aktualizace cesty po zpracování každého tokenu
        update_path(fs->current_path, token); 
        token = strtok(NULL, "/");
    }


    //aktualizace adresáře ve filesysztému
    fs->current_inode = target_inode;
        
    printf("OK\n");
    return true;
}



bool cat(filesystem_t *fs, const char *filename) {
    // hledání souboru
    int32_t file_inode_id = find_in_dir(fs, fs->current_inode, filename);
    
    if (file_inode_id < 0) {
        printf("FILE NOT FOUND\n");
        return false;
    }
    
    inode_t file_inode;
    if (!read_inode(fs, file_inode_id, &file_inode)) {
        printf("FILE NOT FOUND\n");
        return false;
    }
    
    if (file_inode.is_directory) {
        printf("FILE NOT FOUND\n");
        return false;
    }
    
    int32_t clusters_needed = (file_inode.file_size + fs->sb.cluster_size - 1) / fs->sb.cluster_size;
    uint8_t buffer[CLUSTER_SIZE];
    int32_t bytes_read = 0;
    
    for (int32_t i = 0; i < clusters_needed; i++) {
        int32_t cluster = get_file_cluster(fs, &file_inode, i);
        if (cluster == 0) break;
        
        read_cluster(fs, cluster, buffer);
        
        // velikost vypsané zprávy - ošetření posledního clusteru
        int32_t bytes_remaining = file_inode.file_size - bytes_read;
        if (bytes_remaining > fs->sb.cluster_size) {
            bytes_remaining = fs->sb.cluster_size;
        }
        
        fwrite(buffer, 1, bytes_remaining, stdout);
        bytes_read += bytes_remaining;
    }
    
    printf("\n");
    return true;
}

void statfs(filesystem_t *fs) {
    int32_t used_inodes = 0;
    int32_t used_clusters = 0;
    int32_t dir_count = 0;
    
//výpočet použitých inodů a clusterů
    for (int32_t i = 0; i < fs->sb.inode_count; i++) {
        if (is_bit_set(fs->inode_bitmap, i)) {
            used_inodes++;
            
            inode_t inode;
            if (!read_inode(fs, i, &inode) && inode.is_directory) {
                dir_count++;
            }
        }
    }
    
    for (int32_t i = 1; i < fs->sb.cluster_count; i++) {
        if (is_bit_set(fs->data_bitmap, i)) {
            used_clusters++;
        }
    }
    
    int32_t free_inodes = fs->sb.inode_count - used_inodes;
    int32_t free_clusters = fs->sb.cluster_count - 1 - used_clusters;
    
    printf("Statfs:\n");
    printf("----------------------\n");
    printf("Velikost disku:        %.2f MB\n", fs->sb.disk_size / (1024.0 * 1024.0));      
    printf("Velikost clusteru:     %d bytů\n", fs->sb.cluster_size);
    printf("Počet clusterů:   %d\n", fs->sb.cluster_count - 1);
    printf("Obsazené clustery:    %d\n", used_clusters);
    printf("Volné clustery:    %d\n", free_clusters);
    printf("Počet inodů:     %d\n", fs->sb.inode_count);
    printf("Obsazené inody:      %d\n", used_inodes);
    printf("Volné inody:      %d\n", free_inodes);
    printf("Počet složek:      %d\n", dir_count);
    
    // výpočet použitého místa
    int32_t data_space = (fs->sb.cluster_count - 1) * fs->sb.cluster_size;
    int32_t used_space = used_clusters * fs->sb.cluster_size;
    int32_t free_space = free_clusters * fs->sb.cluster_size;
    
    printf("\nPoužití místa:\n");
    printf("Celkové místo: %.2f MB\n", data_space / (1024.0 * 1024.0));        
    printf("Obsazené místo:       %.2f MB\n", used_space / (1024.0 * 1024.0));    
    printf("Volné místo:       %.2f MB\n", free_space / (1024.0 * 1024.0)); 
    printf("Obsazenost:            %.1f%%\n", (used_clusters * 100.0) / (fs->sb.cluster_count - 1));
           
}


bool info(filesystem_t *fs, const char *path) {
    if (!path || !path[0]) {
        printf("FILE NOT FOUND\n");
        return false;
    }
    
    // najití souboru
    int32_t inode_id = resolve_path(fs, path);
    if (inode_id < 0) {
        printf("FILE NOT FOUND\n");
        return false;
    }
    
    inode_t inode;
    if (!read_inode(fs, inode_id, &inode)) {
        printf("FILE NOT FOUND\n");
        return false;
    }
    
    // jméno souboru pro výpis
    char filename[256];
    const char *last_slash = strrchr(path, '/');
    if (last_slash) {
        strncpy(filename, last_slash + 1, sizeof(filename) - 1);
    } else {
        strncpy(filename, path, sizeof(filename) - 1);
    }
    filename[sizeof(filename) - 1] = '\0';
    
// Výpis veškerých informací o souboru
    printf("%s - Velikost: %d B - i-node %d - ", filename, inode.file_size, inode_id);
    
    bool has_direct = false;
    printf("přímé odkazy: ");
    if (inode.direct1 > 0) { printf("%d", inode.direct1); has_direct = true; }
    if (inode.direct2 > 0) { printf(", %d", inode.direct2); has_direct = true; }
    if (inode.direct3 > 0) { printf(", %d", inode.direct3); has_direct = true; }
    if (inode.direct4 > 0) { printf(", %d", inode.direct4); has_direct = true; }
    if (inode.direct5 > 0) { printf(", %d", inode.direct5); has_direct = true; }
    
    if (!has_direct) {
        printf("žádné");
    }
    
    if (inode.indirect1 > 0) {
        printf("1. Nepřímý blok: %d\n", inode.indirect1);
        printf("  -> Clustery: ");
        
        int32_t pointers[PTRS_PER_CLUSTER];
        read_cluster(fs, inode.indirect1, pointers);
        
        bool first = true;
        int count = 0;
        for (int i = 0; i < PTRS_PER_CLUSTER && count < 10; i++) {  // Show first 10
            if (pointers[i] > 0) {
                if (!first) printf(", ");
                printf("%d", pointers[i]);
                first = false;
                count++;
            } else {
                break;  // Stop at first zero
            }
        }
        
        // Count total
        int total = 0;
        for (int i = 0; i < PTRS_PER_CLUSTER; i++) {
            if (pointers[i] > 0) total++;
            else break;
        }
        
        if (total > 10) {
            printf(", ... (%d dalších)", total - 10);
        }
        printf("\n");
    }
    
    if (inode.indirect2 > 0) {
        printf("2. Nepřímý blok: %d\n", inode.indirect2);
        int32_t l1_pointers[PTRS_PER_CLUSTER];
        read_cluster(fs, inode.indirect2, l1_pointers);
        for (int i = 0; i < PTRS_PER_CLUSTER && l1_pointers[i] > 0; i++) {
            printf("  -> L1 blok [%d]: %d\n     -> Clustery: ", i, l1_pointers[i]);
            int32_t l2_pointers[PTRS_PER_CLUSTER];
            read_cluster(fs, l1_pointers[i], l2_pointers);
            
            bool first = true;
            for (int j = 0; j < PTRS_PER_CLUSTER && l2_pointers[j] > 0; j++) {
                if (!first) printf(", ");
                printf("%d", l2_pointers[j]);
                first = false;
            }
            printf("\n");
        }

    }
    
    printf("\n");
    return true;
}

bool cp(filesystem_t *fs, const char *src_path, const char *dest_path) {
    if (!src_path || !src_path[0] || !dest_path || !dest_path[0]) {
        printf("FILE NOT FOUND\n");
        return false;
    }
    
    int32_t src_inode_id = resolve_path(fs, src_path);
    if (src_inode_id < 0) {
        printf("FILE NOT FOUND\n");
        return false;
    }
    
    inode_t src_inode;
    if (!read_inode(fs, src_inode_id, &src_inode)) {
        printf("FILE NOT FOUND\n");
        return false;
    }
    
    // jedná se skutečně o soubor, ne o adresář
    if (src_inode.is_directory) {
        printf("FILE NOT FOUND\n");
        return false;
    }
    
    //Alokace a načtení dat z clusterů do paměti
    uint8_t *file_data = malloc(src_inode.file_size);
    if (!file_data) {
        printf("CANNOT CREATE FILE\n");
        return false;
    }
    
    int32_t bytes_read = 0;
    int32_t clusters_needed = (src_inode.file_size + fs->sb.cluster_size - 1) / fs->sb.cluster_size;
    uint8_t buffer[CLUSTER_SIZE];
    //přečtení clusterů a kopírování do paměti
    for (int32_t i = 0; i < clusters_needed; i++) {
        int32_t cluster = get_file_cluster(fs, &src_inode, i);
        if (cluster == 0) break;
        
        read_cluster(fs, cluster, buffer);
        
        int32_t bytes_to_copy = src_inode.file_size - bytes_read;
        if (bytes_to_copy > fs->sb.cluster_size) {
            bytes_to_copy = fs->sb.cluster_size;
        }
        
        memcpy(file_data + bytes_read, buffer, bytes_to_copy);
        bytes_read += bytes_to_copy;
    }
    
//Vytvoření cílového souboru
    //char dest_copy[256];
    //strncpy(dest_copy, dest_path, sizeof(dest_copy) - 1);
    //dest_copy[sizeof(dest_copy) - 1] = '\0';
    
    
    int32_t dest_parent;
    char dest_filename[NAME_SIZE];
    
    if (!split_path(fs, dest_path, &dest_parent, dest_filename)) {
        printf("PATH NOT FOUND\n");
        return false;
    }
    
    // Kontrola, zda cílový soubor neexistuje
    if (find_in_dir(fs, dest_parent, dest_filename) >= 0) {
        free(file_data);
        printf("DESTINATION ALREADY EXISTS\n");
        return false;
    }
    
    //Alokace a vytvoření i-uzlu
    int32_t dest_inode_id = alloc_inode(fs);
    if (dest_inode_id < 0) {
        free(file_data);
        printf("CANNOT CREATE FILE\n");
        return false;
    }
    
    inode_t dest_inode = {0};
    dest_inode.nodeid = dest_inode_id;
    dest_inode.is_directory = false;
    dest_inode.references = 1;
    dest_inode.file_size = src_inode.file_size;
    
    //Zapsání dat do alokovaných clusterů
    clusters_needed = (src_inode.file_size + fs->sb.cluster_size - 1) / fs->sb.cluster_size;
    for (int32_t i = 0; i < clusters_needed; i++) {
        int32_t cluster = alloc_cluster(fs);
        if (cluster < 0) {
            free(file_data);
            printf("CANNOT CREATE FILE\n");
            return false;
        }
        
        int32_t offset = i * fs->sb.cluster_size;
        int32_t to_write = src_inode.file_size - offset;
        if (to_write > fs->sb.cluster_size) {
            to_write = fs->sb.cluster_size;
        }
        
        memset(buffer, 0, CLUSTER_SIZE);
        memcpy(buffer, file_data + offset, to_write);
        write_cluster(fs, cluster, buffer);
        set_file_cluster(fs, &dest_inode, i, cluster);
    }
    

    write_inode(fs, dest_inode_id, &dest_inode);
    
    //přidání do adresáře
    if (add_to_dir(fs, dest_parent, dest_filename, dest_inode_id) == false) {
        free(file_data);
        printf("PATH NOT FOUND\n");
        return false;
    }
    
    free(file_data);
    printf("OK\n");
    return true;
}


bool rm(filesystem_t *fs, const char *path) {
    if (!path || !path[0]) {
        printf("FILE NOT FOUND\n");
        return false;
    }
    
    //nalezení inodu souboru
    int32_t file_inode_id = resolve_path(fs, path);
    
    if (file_inode_id < 0) {
        printf("FILE NOT FOUND\n");
        return false;
    }
    
    inode_t file_inode;
    if (!read_inode(fs, file_inode_id, &file_inode)) {
        printf("FILE NOT FOUND\n");
        return false;
    }
    
    //jedná se o adresář - špatný příkaz
    if (file_inode.is_directory) {
        printf("FILE NOT FOUND\n");
        return false;
    }
    
    //Uvolnění clusteru a všech obsazených datových bloků
    int32_t clusters_needed = (file_inode.file_size + fs->sb.cluster_size - 1) / fs->sb.cluster_size;
    
    if (file_inode.direct1 > 0) free_cluster(fs, file_inode.direct1);
    if (file_inode.direct2 > 0) free_cluster(fs, file_inode.direct2);
    if (file_inode.direct3 > 0) free_cluster(fs, file_inode.direct3);
    if (file_inode.direct4 > 0) free_cluster(fs, file_inode.direct4);
    if (file_inode.direct5 > 0) free_cluster(fs, file_inode.direct5);
    

    if (file_inode.indirect1 > 0) {

        int32_t pointers[PTRS_PER_CLUSTER];
        read_cluster(fs, file_inode.indirect1, pointers);
        
        for (int i = 0; i < PTRS_PER_CLUSTER && pointers[i] > 0; i++) {
            free_cluster(fs, pointers[i]);
        }
        

        free_cluster(fs, file_inode.indirect1);
    }
    
    if (file_inode.indirect2 > 0) {
        // Read L1 indirect block
        int32_t l1_pointers[PTRS_PER_CLUSTER];
        read_cluster(fs, file_inode.indirect2, l1_pointers);
        
        for (int i = 0; i < PTRS_PER_CLUSTER && l1_pointers[i] > 0; i++) {
            // Read L2 indirect block
            int32_t l2_pointers[PTRS_PER_CLUSTER];
            read_cluster(fs, l1_pointers[i], l2_pointers);
            
            // Free data clusters
            for (int j = 0; j < PTRS_PER_CLUSTER && l2_pointers[j] > 0; j++) {
                free_cluster(fs, l2_pointers[j]);
            }
            
            // Free L2 block
            free_cluster(fs, l1_pointers[i]);
        }
        
        // Free L1 block
        free_cluster(fs, file_inode.indirect2);
    }
    

    clear_bit(fs->inode_bitmap, file_inode_id);
    save_bitmaps(fs);
    

    // zjištění jména souboru a cílového umístění
    
    
    
    int32_t parent_inode;
    char filename[NAME_SIZE];
    
    if (!split_path(fs, path, &parent_inode, filename)) {
        printf("FILE NOT FOUND\n");
        return false;
    }
    
    // odstranění z nadřazeného adresáře
    if (!remove_from_dir(fs, parent_inode, filename)) {
        printf("FILE NOT FOUND\n");
        return false;
    }
    
    printf("OK\n");
    return true;
}

bool rmdir(filesystem_t *fs, const char *path) {
    if (!path || !path[0]) {
        printf("FILE NOT FOUND\n");
        return false;
    }
    
    //nalezení inodu
    int32_t dir_inode_id = resolve_path(fs, path);
    
    if (dir_inode_id < 0) {
        printf("FILE NOT FOUND\n");
        return false;
    }
    
    //nemůže se jednat o root adresář
    if (dir_inode_id == 0) {
        printf("FILE NOT FOUND\n");
        return false;
    }
    

    inode_t dir_inode;
    if (!read_inode(fs, dir_inode_id, &dir_inode)) {
        printf("FILE NOT FOUND\n");
        return false;
    }
    
    //musí se jednat o adresář
    if (!dir_inode.is_directory) {
        printf("FILE NOT FOUND\n");
        return false;
    }
    

    int32_t cluster_count = (dir_inode.file_size + fs->sb.cluster_size - 1) / fs->sb.cluster_size;
    dir_item_t entries[ENTRIES_PER_CLUSTER];
    
    for (int32_t i = 0; i < cluster_count; i++) {
        int32_t cluster = get_file_cluster(fs, &dir_inode, i);
        if (cluster == 0) continue;
        
        read_cluster(fs, cluster, entries);
        
        for (int j = 0; j < ENTRIES_PER_CLUSTER; j++) {
            if (entries[j].inode != 0) {
                //Sloěka není prázdná
                printf("NOT EMPTY\n");
                return false;
            }
        }
    }
    
    //uvolnění inodu a clusterů
    if (dir_inode.direct1 > 0) free_cluster(fs, dir_inode.direct1);
    if (dir_inode.direct2 > 0) free_cluster(fs, dir_inode.direct2);
    if (dir_inode.direct3 > 0) free_cluster(fs, dir_inode.direct3);
    if (dir_inode.direct4 > 0) free_cluster(fs, dir_inode.direct4);
    if (dir_inode.direct5 > 0) free_cluster(fs, dir_inode.direct5);
    

    clear_bit(fs->inode_bitmap, dir_inode_id);
    save_bitmaps(fs);
    
    //odstranění záznamu - zjištění jména a nadřazeného adresáře
    //char path_copy[256];
    //strncpy(path_copy, path, sizeof(path_copy) - 1);
    //path_copy[sizeof(path_copy) - 1] = '\0';
    
    
    int32_t parent_inode;
    char dirname[NAME_SIZE];
    
    if (!split_path(fs, path, &parent_inode, dirname)) {
        printf("FILE NOT FOUND\n");
        return false;
    }
    
    if (!remove_from_dir(fs, parent_inode, dirname)) {
        printf("FILE NOT FOUND\n");
        return false;
    }
    
    printf("OK\n");
    return true;
}



bool mv(filesystem_t *fs, const char *src_path, const char *dest_path) {
    int32_t src_parent, src_id;
    char src_name[NAME_SIZE];

    if (!split_path(fs, src_path, &src_parent, src_name)) {
        printf("FILE NOT FOUND\n");
        return false;
    }

    src_id = find_in_dir(fs, src_parent, src_name);
    if (src_id <= 0) { // Protect root
        printf("FILE NOT FOUND\n");
        return false;
    }

    int32_t dest_parent;
    char dest_name[NAME_SIZE];

    //KOntrola, zda cílová cesta již existuje
    int32_t dest_check = resolve_path(fs, dest_path);
    if (dest_check >= 0) {
        inode_t d_node;
        read_inode(fs, dest_check, &d_node);
        if (d_node.is_directory) {
            dest_parent = dest_check;
            strcpy(dest_name, src_name);
        } else {
            printf("FILE ALREADY EXISTS\n");
            return false;
        }
    } else {
        if (!split_path(fs, dest_path, &dest_parent, dest_name)) {
            printf("PATH NOT FOUND\n");
            return false;
        }
    }

    if (find_in_dir(fs, dest_parent, dest_name) >= 0) {
        printf("FILE ALREADY EXISTS\n");
        return false;
    }

    if (remove_from_dir(fs, src_parent, src_name)) {
        add_to_dir(fs, dest_parent, dest_name, src_id);
        printf("OK\n");
        return true;
    }
    return false;
}



bool outcp(filesystem_t *fs, const char *src, const char *dest) {
    //Nalezení souboru ve filewsystému
    int32_t file_inode_id = resolve_path(fs, src);
    
    if (file_inode_id < 0) {
        printf("FILE NOT FOUND\n");
        return false;
    }
    

    inode_t file_inode;
    if (!read_inode(fs, file_inode_id, &file_inode)) {
        printf("FILE NOT FOUND\n");
        return false;
    }
    
    //nejedná se o složku
    if (file_inode.is_directory) {
        printf("FILE NOT FOUND\n");
        return false;
    }
    
    FILE *dest_file = fopen(dest, "wb");
    if (!dest_file) {
        printf("PATH NOT FOUND\n");
        return false;
    }
    
    int32_t clusters_needed = (file_inode.file_size + fs->sb.cluster_size - 1) / fs->sb.cluster_size;
    uint8_t buffer[CLUSTER_SIZE];
    int32_t bytes_written = 0;
    
    //Čtení dat z clusterů a zápis do výsledného souboru
    for (int32_t i = 0; i < clusters_needed; i++) {
        int32_t cluster = get_file_cluster(fs, &file_inode, i);
        if (cluster == 0) break;
        
        if (!read_cluster(fs, cluster, buffer)) {
            fclose(dest_file);
            printf("PATH NOT FOUND\n");
            return false;
        }
        
        //poslední cluster
        int32_t bytes_remaining = file_inode.file_size - bytes_written;
        int32_t to_write;
        if (bytes_remaining > fs->sb.cluster_size) {
            to_write = fs->sb.cluster_size;
        } else {
            to_write = bytes_remaining;
        }

        
        if (fwrite(buffer, 1, to_write, dest_file) != (size_t)to_write) {
            fclose(dest_file);
            printf("PATH NOT FOUND\n");
            return false;
        }
        
        bytes_written += to_write;
    }
    
    fclose(dest_file);
    printf("OK\n");
    return true;
}



bool load(filesystem_t *fs, const char *filename) {
    if (!filename || !filename[0]) {
        printf("FILE NOT FOUND\n");
        return false;
    }
    

    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("FILE NOT FOUND\n");
        return false;
    }
    
    char line[512];
    int line_number = 0;
    bool success = false;
    
    //čtení a vykonávání kódu po řádcích
    while (fgets(line, sizeof(line), file)) {
        line_number++;
        

        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        
        if (line[0] == '\0') {
            continue;
        }
        
        // Parse command and arguments
        char cmd[64] = {0}, arg1[256] = {0}, arg2[256] = {0};
        sscanf(line, "%s %s %s", cmd, arg1, arg2);
        
        if (cmd[0] == '\0') {
            continue;
        }
        
        bool success = false;
        
        if (strcmp(cmd, "format") == 0) {
            success = format(fs, arg1);
        }
        else if (strcmp(cmd, "mkdir") == 0) {
            success = mkdir(fs, arg1);
        }
        else if (strcmp(cmd, "pwd") == 0) {
            pwd(fs);
            success = true;
        }
        else if (strcmp(cmd, "ls") == 0) {
            success = ls(fs, arg1[0] ? arg1 : NULL);
        }
        else if (strcmp(cmd, "cd") == 0) {
            success = cd(fs, arg1);
        }
        else if (strcmp(cmd, "cat") == 0) {
            success = cat(fs, arg1);
        }
        else if (strcmp(cmd, "incp") == 0) {
            success = incp(fs, arg1, arg2);
        }
        else if (strcmp(cmd, "outcp") == 0) {
            success = outcp(fs, arg1, arg2);
        }
        else if (strcmp(cmd, "statfs") == 0) {
            statfs(fs);
            success = true;
        }
        else if (strcmp(cmd, "info") == 0) {
            success = info(fs, arg1);
        }
        else if (strcmp(cmd, "cp") == 0) {
            success = cp(fs, arg1, arg2);
        }
        else if (strcmp(cmd, "rm") == 0) {
            success = rm(fs, arg1);
        }
        else if (strcmp(cmd, "rmdir") == 0) {
            success = rmdir(fs, arg1);
        }
        else if (strcmp(cmd, "mv") == 0) {
            success = mv(fs, arg1, arg2);
        }
        
        else {
            printf("Unknown command: %s\n", cmd);
            success = false;
        }
        
        
    }
    
    fclose(file);
    
    if (success) {
        printf("OK\n");
    }
    
    return success;
}