#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "clusters.h"
#include "structs.h"
#include "commandline.h"
#include "filesystem.h"




int32_t alloc_cluster(filesystem_t *fs) {
    printf("[DEBUG] alloc_cluster called\n");
    // začátek od 1, 0 je rezervováno pro "null" ukazatel
    for (int32_t i = 1; i < fs->sb.cluster_count; i++) {
        if (!is_bit_set(fs->data_bitmap, i)) {
            printf("[DEBUG] Found free cluster: %d\n", i);
            set_bit(fs->data_bitmap, i);
            save_bitmaps(fs);
            return i;
        }
    }
    
    printf("[DEBUG] No free clusters found!\n");
    return -1;
}

void free_cluster(filesystem_t *fs, int32_t cluster) {
    if (cluster >= 0 && cluster < fs->sb.cluster_count) {
        clear_bit(fs->data_bitmap, cluster);
        save_bitmaps(fs);
    }
}



int32_t get_file_cluster(filesystem_t *fs, inode_t *inode, int32_t cluster_index) {
    if (cluster_index == 0 && inode->direct1) return inode->direct1;
    if (cluster_index == 1 && inode->direct2) return inode->direct2;
    if (cluster_index == 2 && inode->direct3) return inode->direct3;
    if (cluster_index == 3 && inode->direct4) return inode->direct4;
    if (cluster_index == 4 && inode->direct5) return inode->direct5;
    
    cluster_index -= DIRECT_LINKS;
    
    //Nepřímé bloky
    if (cluster_index < PTRS_PER_CLUSTER && inode->indirect1) {
        int32_t pointers[PTRS_PER_CLUSTER];
        read_cluster(fs, inode->indirect1, pointers);
        return pointers[cluster_index];
    }
    
    cluster_index -= PTRS_PER_CLUSTER;
    

    if (cluster_index < PTRS_PER_CLUSTER * PTRS_PER_CLUSTER && inode->indirect2) {
        int32_t l1_index = cluster_index / PTRS_PER_CLUSTER;
        int32_t l2_index = cluster_index % PTRS_PER_CLUSTER;
        
        int32_t l1_pointers[PTRS_PER_CLUSTER];
        read_cluster(fs, inode->indirect2, l1_pointers);
        
        if (l1_pointers[l1_index] == 0) return 0;
        
        int32_t l2_pointers[PTRS_PER_CLUSTER];
        read_cluster(fs, l1_pointers[l1_index], l2_pointers);
        return l2_pointers[l2_index];
    }
    
    return 0;
}

int set_file_cluster(filesystem_t *fs, inode_t *inode, int32_t cluster_index, int32_t cluster_num) {
    printf("[DEBUG] set_file_cluster: index=%d, cluster=%d\n", cluster_index, cluster_num);
    
    if (cluster_index == 0) { 
        inode->direct1 = cluster_num; 
        printf("[DEBUG] Set direct1 = %d\n", cluster_num);
        return 0; 
    }
    if (cluster_index == 1) { 
        inode->direct2 = cluster_num; 
        printf("[DEBUG] Set direct2 = %d\n", cluster_num);
        return 0; 
    }
    if (cluster_index == 2) { 
        inode->direct3 = cluster_num; 
        printf("[DEBUG] Set direct3 = %d\n", cluster_num);
        return 0; 
    }
    if (cluster_index == 3) { 
        inode->direct4 = cluster_num; 
        printf("[DEBUG] Set direct4 = %d\n", cluster_num);
        return 0; 
    }
    if (cluster_index == 4) { 
        inode->direct5 = cluster_num; 
        printf("[DEBUG] Set direct5 = %d\n", cluster_num);
        return 0; 
    }
    
    cluster_index -= DIRECT_LINKS;
    
    // Nepřímé bloky
    if (cluster_index < PTRS_PER_CLUSTER) {
        if (inode->indirect1 == 0) {
            inode->indirect1 = alloc_cluster(fs);
            if (inode->indirect1 < 0) return -1;
            int32_t zeros[PTRS_PER_CLUSTER] = {0};
            write_cluster(fs, inode->indirect1, zeros);
        }
        
        int32_t pointers[PTRS_PER_CLUSTER];
        read_cluster(fs, inode->indirect1, pointers);
        pointers[cluster_index] = cluster_num;
        write_cluster(fs, inode->indirect1, pointers);
        return 0;
    }
    
    return -1;
}