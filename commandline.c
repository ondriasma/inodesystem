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


// ============================================================================
// COMMANDS
// ============================================================================

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
    
    dir_entry_t entries[ENTRIES_PER_CLUSTER];
    
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

bool incp(filesystem_t *fs, const char *src, const char *dst) {
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
        int32_t to_write = (size - offset > fs->sb.cluster_size) ? fs->sb.cluster_size : size - offset;
        
        uint8_t buffer[CLUSTER_SIZE] = {0};
        memcpy(buffer, data + offset, to_write);
        write_cluster(fs, cluster, buffer);
        set_file_cluster(fs, &new_inode, i, cluster);
    }
    
    write_inode(fs, new_inode_id, &new_inode);
    add_to_dir(fs, fs->current_inode, dst, new_inode_id);
    
    free(data);
    printf("OK\n");
    return true;
}

bool format(filesystem_t *fs, const char *size_str) {
    int32_t size_mb = 600;
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
    strcpy(fs->sb.description, "ZOS Filesystem 2025");//todo - upravit
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
    int32_t target_inode = find_in_dir(fs, fs->current_inode, path);
    
    if (target_inode < 0) {
        printf("PATH NOT FOUND\n");
        return false;
    }
    
    // test, jestli se vůbec jedná o složku
    inode_t target;
    if (!read_inode(fs, target_inode, &target)) {
        printf("PATH NOT FOUND\n");
        return false;
    }
    
    if (!target.is_directory) {
        printf("PATH NOT FOUND\n");
        return false;
    }
    

    fs->current_inode = target_inode;
    

    if (strcmp(fs->current_path, "/") != 0) {
        strcat(fs->current_path, "/");
    }
    strcat(fs->current_path, path);
    
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
    int32_t free_clusters = fs->sb.cluster_count - 1 - used_clusters;  // -1 for reserved cluster 0
    
    printf("Filesystem Statistics:\n");
    printf("----------------------\n");
    printf("Disk size:        %d bytes (%.2f MB)\n", 
           fs->sb.disk_size, fs->sb.disk_size / (1024.0 * 1024.0));// todo - asi by stačily MB
    printf("Cluster size:     %d bytes\n", fs->sb.cluster_size);
    printf("Total clusters:   %d\n", fs->sb.cluster_count - 1);  // Don't count reserved
    printf("Used clusters:    %d\n", used_clusters);
    printf("Free clusters:    %d\n", free_clusters);
    printf("Total inodes:     %d\n", fs->sb.inode_count);
    printf("Used inodes:      %d\n", used_inodes);
    printf("Free inodes:      %d\n", free_inodes);
    printf("Directories:      %d\n", dir_count);
    
    // výpočet použitého místa
    int32_t data_space = (fs->sb.cluster_count - 1) * fs->sb.cluster_size;
    int32_t used_space = used_clusters * fs->sb.cluster_size;
    int32_t free_space = free_clusters * fs->sb.cluster_size;
    
    printf("\nSpace Utilization:\n");
    printf("Total data space: %d bytes (%.2f MB)\n", 
           data_space, data_space / (1024.0 * 1024.0));
    printf("Used space:       %d bytes (%.2f MB)\n", 
           used_space, used_space / (1024.0 * 1024.0));
    printf("Free space:       %d bytes (%.2f MB)\n", 
           free_space, free_space / (1024.0 * 1024.0));
    printf("Usage:            %.1f%%\n", 
           (used_clusters * 100.0) / (fs->sb.cluster_count - 1));
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
    printf("%s - %d B - i-node %d - ", filename, inode.file_size, inode_id);
    
    bool has_direct = false;
    printf("direct links: ");
    if (inode.direct1 > 0) { printf("%d", inode.direct1); has_direct = true; }
    if (inode.direct2 > 0) { printf(", %d", inode.direct2); has_direct = true; }
    if (inode.direct3 > 0) { printf(", %d", inode.direct3); has_direct = true; }
    if (inode.direct4 > 0) { printf(", %d", inode.direct4); has_direct = true; }
    if (inode.direct5 > 0) { printf(", %d", inode.direct5); has_direct = true; }
    
    if (!has_direct) {
        printf("none");
    }
    
    if (inode.indirect1 > 0) {
        printf("Indirect1 block: %d\n", inode.indirect1);
        printf("  -> Data clusters: ");
        
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
            printf(", ... (%d more)", total - 10);
        }
        printf("\n");
    }
    
    if (inode.indirect2 > 0) {
        printf("Indirect2 block: %d (double indirect)\n", inode.indirect2);
        // Could expand this to show L1 and L2 structure
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
    
    // jedná se skutečně o soubor
    if (src_inode.is_directory) {
        printf("FILE NOT FOUND\n");
        return false;
    }
    
    //Načtení do paměti
    uint8_t *file_data = malloc(src_inode.file_size);
    if (!file_data) {
        printf("CANNOT CREATE FILE\n");
        return false;
    }
    
    int32_t bytes_read = 0;
    int32_t clusters_needed = (src_inode.file_size + fs->sb.cluster_size - 1) / fs->sb.cluster_size;
    uint8_t buffer[CLUSTER_SIZE];
    
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
    char dest_copy[256];
    strncpy(dest_copy, dest_path, sizeof(dest_copy) - 1);
    dest_copy[sizeof(dest_copy) - 1] = '\0';
    
    char *last_slash = strrchr(dest_copy, '/');
    int32_t parent_inode;
    char dest_filename[NAME_SIZE];
    
    if (last_slash) {
        // Has directory component
        *last_slash = '\0';
        char *filename = last_slash + 1;
        
        if (dest_copy[0] == '\0') {
            // Was "/filename"
            parent_inode = 0;
        } else {
            parent_inode = resolve_path(fs, dest_copy);
            if (parent_inode < 0) {
                free(file_data);
                printf("PATH NOT FOUND\n");
                return false;
            }
        }
        
        strncpy(dest_filename, filename, NAME_SIZE - 1);
        dest_filename[NAME_SIZE - 1] = '\0';
    } else {
        // No directory component - use current directory
        parent_inode = fs->current_inode;
        strncpy(dest_filename, dest_path, NAME_SIZE - 1);
        dest_filename[NAME_SIZE - 1] = '\0';
    }
    
    if (find_in_dir(fs, parent_inode, dest_filename) >= 0) {
        free(file_data);
        printf("DESTINATION ALREADY EXISTS\n");
        return false;
    }
    
    // Allocate inode for destination
    int32_t dst_inode_id = alloc_inode(fs);
    if (dst_inode_id < 0) {
        free(file_data);
        printf("CANNOT CREATE FILE\n");
        return false;
    }
    
    // Create destination inode
    inode_t dest_inode = {0};
    dest_inode.nodeid = dst_inode_id;
    dest_inode.is_directory = false;
    dest_inode.references = 1;
    dest_inode.file_size = src_inode.file_size;
    
    // Write file data to new clusters
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
    
    // Write destination inode
    write_inode(fs, dst_inode_id, &dest_inode);
    
    // Add to parent directory
    if (add_to_dir(fs, parent_inode, dest_filename, dst_inode_id) == false) {
        free(file_data);
        printf("PATH NOT FOUND\n");
        return false;
    }
    
    free(file_data);
    printf("OK\n");
    return true;
}