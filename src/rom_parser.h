/* ================================================================== */
/* rom_parser.h                                                         */
/* ================================================================== */
#ifndef ROM_PARSER_H
#define ROM_PARSER_H

#include "types.h"

/* Offsets header Mega Drive */
#define HDR_SYSTEM      0x100
#define HDR_CHECKSUM    0x18E
#define HDR_DEVICES     0x190
#define HDR_BACKUP      0x1B0
#define HDR_REGION      0x1F0

/* Types système */
#define SYS_UNKNOWN     0
#define SYS_MD          1
#define SYS_32X         2
#define SYS_SCD         3
#define SYS_SMS         4
#define SYS_BIOS        5

typedef struct {
    /* Système */
    u8  sysType;            /* SYS_xxx */
    char sysStr[8];         /* "MD", "32X", "SCD"... */

    /* Région */
    u8  hasJPN;
    u8  hasUSA;
    u8  hasEUR;

    /* Checksum */
    u8  chkOk;

    /* Backup memory */
    u8  hasBackup;
    u32 backupSize;
    u8  isEEPROM;           /* 0=SRAM, 1=EEPROM */

    /* Devices */
    u8  hasJoypad3;         /* J */
    u8  hasJoypad6;         /* 6 */
    u8  hasMultitap;        /* 4 */
    u8  hasMouse;           /* M */
    u8  hasKeyboard;        /* K */
    u8  hasPrinter;         /* P */
    u8  hasSCD;             /* C */
    u8  hasModem;           /* R */

} RomInfo;

extern RomInfo currentRomInfo;

/**
 * Parse le header d'une ROM depuis la SD
 * path : chemin complet du fichier
 * Retourne 1 si OK, 0 si erreur
 */
u8 ROM_parseHeader(const char *path);

/**
 * Affiche les infos dans le panneau droit
 */
void ROM_drawInfo(void);

#endif
