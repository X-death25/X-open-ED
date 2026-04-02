#ifndef EXPLORER_H
#define EXPLORER_H

#include "ff.h"

/* Layout écran 40x28 */
#define EXPLORER_LEFT_WIDTH     19  /* colonnes 0-18 */
#define EXPLORER_SEP_COL        19  /* colonne séparateur */
#define EXPLORER_RIGHT_COL      20  /* colonnes 20-39 */
#define EXPLORER_VISIBLE_LINES  10
#define EXPLORER_START_LINE     4
#define EXPLORER_NAME_LEN       13 /* FF_MAX_LFN + 1 */
#define EXPLORER_MAX_ENTRIES 64

/* Types d'entrée */
#define ENTRY_DIR   0
#define ENTRY_FILE  1

/* Types de ROM pour le panneau droit */
#define ROM_TYPE_UNKNOWN  0
#define ROM_TYPE_MD       1
#define ROM_TYPE_DIR      2
#define ROM_TYPE_CFG      3

typedef struct {
    char name[EXPLORER_NAME_LEN];
    u8   type;
    u32  size;
} ExplorerEntry;

typedef struct {
    char region[4];     /* "JPN", "EUR", "USA", "???" */
    u8   hasSram;       /* 0 = NO, 1 = YES */
    u32  sramSize;      /* taille SRAM en octets */
    u8   romType;       /* ROM_TYPE_xxx */
} RomInfo;

extern char currentPath[128];
extern ExplorerEntry entries[64];
extern u8 entryCount;
extern u8 selectedIndex;
extern u8 scrollOffset;
extern RomInfo currentRomInfo;


s8  Explorer_loadDir(const char *path);
void Explorer_setFatFs(FATFS *fs);
void Explorer_draw(FATFS *fs);
void Explorer_moveDown(void);
void Explorer_moveUp(void);
u8   Explorer_select(void);
void Explorer_goBack(void);

#endif