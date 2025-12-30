#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "commandline.h"
#include "structs.h"
#include "filesystem.h"



int main(int argc, char *argv[]) {
    // todo - umožnit zadat jméno souboru s FS jako parametr
    const char *test_filename = "testfs.img";
    filesystem_t fs = {0};
    fs.filename = test_filename;
    
    fs.file = fopen(test_filename, "r+b");
    if (!fs.file) fs.file = fopen(test_filename, "w+b");
    
    if (!fs.file) {
        fprintf(stderr, "Cannot open test filesystem file\n");
        return 1;
    }
    
    // načtení existujícího fs
    fseek(fs.file, 0, SEEK_END);
    if (ftell(fs.file) >= (long)sizeof(superblock_t)) {
        fseek(fs.file, 0, SEEK_SET);
        if (load_superblock(&fs) && strcmp(fs.sb.signature, SIGNATURE) == 0) {
            load_bitmaps(&fs);
            strcpy(fs.current_path, "/");
        }
    }
    

    char line[512];
    while (1) {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin)) break;
        
        char cmd[64], arg1[256] = {0}, arg2[256] = {0};
        sscanf(line, "%s %s %s", cmd, arg1, arg2);
        
        if (strcmp(cmd, "exit") == 0) break;
        else if (strcmp(cmd, "format") == 0) format(&fs, arg1);
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
        else printf("Unknown command\n");
    }
    
    if (fs.inode_bitmap) free(fs.inode_bitmap);
    if (fs.data_bitmap) free(fs.data_bitmap);
    fclose(fs.file);
    
    return 0;
}