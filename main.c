#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "commandline.h"
#include "structs.h"
#include "filesystem.h"



int main(int argc, char *argv[]) {

    char *filename = "filesystem";//default jméno, pokud se nezadá jiné
    if (argc > 1) {
        filename = argv[1];
    }

    filesystem_t fs = {0};
    fs.filename = filename;
    //pokus o otevření nebo nytvoření souboru
    fs.file = fopen(filename, "r+b");
    if (!fs.file) {
        fs.file = fopen(filename, "w+b");
        if (!fs.file) {
            fprintf(stderr, "Soubor nelze vytvořit '%s'\n", filename);
            return 1;
        }
        printf("Vytvořen nový filesystém '%s'\n", filename);
        printf("Použijte příkaz 'format <size>' pro jeho naformátování.\n");
    }

    bool is_formatted = false;
    
    // načtení existujícího fs
    fseek(fs.file, 0, SEEK_END);
    if (ftell(fs.file) >= sizeof(superblock_t)) {   //soubor menší než superblock nemůže být validní fs
        fseek(fs.file, 0, SEEK_SET);
        if (load_superblock(&fs)) {
            load_bitmaps(&fs);
            strcpy(fs.current_path, "/");
            is_formatted = true;
            printf("Načítám filesystem\n");
        } else {
            printf("Soubor není validní filesystém.\n");
            printf("Použijte příkaz 'format <size>' pro jeho naformátování.\n");
        }
    }
    

    char line[512];
    while (1) {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin)) break;
        
        char cmd[64], arg1[256] = {0}, arg2[256] = {0}, arg3[256] = {0};
        sscanf(line, "%s %s %s %s", cmd, arg1, arg2, arg3);
        
        if (strcmp(cmd, "exit") == 0) break;
        else if (strcmp(cmd, "format") == 0) {
            if (format(&fs, arg1)) {
                is_formatted = true;
            }
        }
        else if (!is_formatted) {
            printf("Filesystém není naformátovaný. Použijte příkaz 'format <size>'\n");
            continue;
        }
        else if (strcmp(cmd, "mkdir") == 0) mkdir(&fs, arg1);
        else if (strcmp(cmd, "pwd") == 0) pwd(&fs);
        else if (strcmp(cmd, "ls") == 0) ls(&fs, arg1[0] ? arg1 : NULL);
        else if (strcmp(cmd, "cd") == 0) cd(&fs, arg1);
        else if (strcmp(cmd, "cat") == 0) cat(&fs, arg1);
        else if (strcmp(cmd, "incp") == 0) incp(&fs, arg1, arg2);
        else if (strcmp(cmd, "statfs") == 0) statfs(&fs);
        else if (strcmp(cmd, "info") == 0) info(&fs, arg1);
        else if (strcmp(cmd, "cp") == 0) cp(&fs, arg1, arg2);
        else if (strcmp(cmd, "rm") == 0) rm(&fs, arg1);
        else if (strcmp(cmd, "rmdir") == 0) rmdir(&fs, arg1);
        else if (strcmp(cmd, "mv") == 0) mv(&fs, arg1, arg2);
        else if (strcmp(cmd, "outcp") == 0) outcp(&fs, arg1, arg2);
        else if (strcmp(cmd, "load") == 0) load(&fs, arg1);
        else if (strcmp(cmd, "xcp") == 0) xcp(&fs, arg1, arg2, arg3);
        else if (strcmp(cmd, "add") == 0) add(&fs, arg1, arg2);
        else printf("Neznámý příkaz\n");
    }
    
    if (fs.inode_bitmap) free(fs.inode_bitmap);
    if (fs.data_bitmap) free(fs.data_bitmap);
    fclose(fs.file);
    
    return 0;
}