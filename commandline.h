#pragma once
#include "structs.h"
#include <stdbool.h>


//Vypíše aktuální cestu
void pwd(filesystem_t *fs);

//Vypíše obsah adresáře
bool ls(filesystem_t *fs, const char *path);

//Vytvoří adresář
bool mkdir(filesystem_t *fs, const char *name);

//Nahraje soubor src z pevného disku do umístění dest ve vašem FS
bool incp(filesystem_t *fs, const char *src, const char *dest);

/*Příkaz provede formát souboru, který byl zadán jako parametr při spuštení programu
na souborový systém dané velikosti. Pokud už soubor nějaká data obsahoval, budou
přemazána. Pokud soubor neexistoval, bude vytvořen.*/
bool format(filesystem_t *fs, const char *size_str);

//Změní aktuální cestu do adresáře
bool cd(filesystem_t *fs, const char *path);

//Vypíše obsah textového souboru na obrazovku
bool cat(filesystem_t *fs, const char *filename);

/*vypíše statistiky souborového systému, jako je velikost, počet
obsazených a volných bloků, počet obsazených a volných i-uzlů, počet adresářů.*/
void statfs(filesystem_t *fs);

//Vypíše informace o souboru/adresáři
bool info(filesystem_t *fs, const char *path);

//Zkopíruje soubor src_path do umístění dest_path
bool cp(filesystem_t *fs, const char *src_path, const char *dest_path);

//Smaže soubor
bool rm(filesystem_t *fs, const char *path);

//Smaže prázdný adresář
bool rmdir(filesystem_t *fs, const char *path);

//Přesune soubor src_path do umístění dest_path, nebo ho přejmenuje
bool mv(filesystem_t *fs, const char *src_path, const char *dest_path);

//Nahraje soubor z FS do umístění na pevném disku
bool outcp(filesystem_t *fs, const char *src, const char *dest);

/*Načte soubor z pevného disku, ve kterém budou jednotlivé příkazy, a začne je
sekvenčně vykonávat. Formát je 1 příkaz/1 řádek*/
bool load(filesystem_t *fs, const char *filename);