#pragma once
#include "structs.h"
#include <stdbool.h>

void pwd(filesystem_t *fs);

bool ls(filesystem_t *fs, const char *path);

bool mkdir(filesystem_t *fs, const char *name);

bool incp(filesystem_t *fs, const char *src, const char *dst);

bool format(filesystem_t *fs, const char *size_str);

bool cd(filesystem_t *fs, const char *path);

bool cat(filesystem_t *fs, const char *filename);

void statfs(filesystem_t *fs);

bool info(filesystem_t *fs, const char *path);

bool cp(filesystem_t *fs, const char *src_path, const char *dst_path);
