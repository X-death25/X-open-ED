
/* ================================================================== */
/* rom_parser.c                                                         */
/* ================================================================== */
#include "genesis.h"
#include "rom_parser.h"
#include "ff.h"

RomInfo currentRomInfo;

/* Helper : compare n chars */
static u8 startsWith(const char *str, const char *prefix, u8 n)
{
    for (u8 i = 0; i < n; i++)
        if (str[i] != prefix[i]) return 0;
    return 1;
}

/* Helper : cherche un char dans une string */
static u8 hasChar(const char *str, u8 len, char c)
{
    for (u8 i = 0; i < len; i++)
        if (str[i] == c) return 1;
    return 0;
}

/* Helper strlen */
static u8 slen(const char *s)
{
    u8 i = 0; while (s[i]) i++; return i;
}

u8 ROM_parseHeader(const char *path)
{
    FIL fil;
    UINT br;
    u8 buf[512];
    u16 storedChk, calcChk;

    /* Réinitialise */
    currentRomInfo.sysType    = SYS_UNKNOWN;
    currentRomInfo.sysStr[0]  = 0;
    currentRomInfo.hasJPN     = 0;
    currentRomInfo.hasUSA     = 0;
    currentRomInfo.hasEUR     = 0;
    currentRomInfo.chkOk      = 0;
    currentRomInfo.hasBackup  = 0;
    currentRomInfo.backupSize = 0;
    currentRomInfo.isEEPROM   = 0;
    currentRomInfo.hasJoypad3 = 0;
    currentRomInfo.hasJoypad6 = 0;
    currentRomInfo.hasMultitap= 0;
    currentRomInfo.hasMouse   = 0;
    currentRomInfo.hasKeyboard= 0;
    currentRomInfo.hasPrinter = 0;
    currentRomInfo.hasSCD     = 0;
    currentRomInfo.hasModem   = 0;

    if (f_open(&fil, path, FA_READ) != FR_OK) return 0;
    if (f_read(&fil, buf, 512, &br) != FR_OK || br < 512) {
        f_close(&fil);
        return 0;
    }
    f_close(&fil);

    /* --- Système 0x100 --- */
    char *sys = (char *)(buf + HDR_SYSTEM);
    if      (startsWith(sys, "SEGA MEGA DRIVE", 15)) { currentRomInfo.sysType = SYS_MD;   buf[0]='M'; buf[1]='D'; buf[2]=0; }
    else if (startsWith(sys, "SEGA GENESIS",    12)) { currentRomInfo.sysType = SYS_MD;   buf[0]='M'; buf[1]='D'; buf[2]=0; }
    else if (startsWith(sys, "SEGA 32X",         8)) { currentRomInfo.sysType = SYS_32X;  buf[0]='3'; buf[1]='2'; buf[2]='X'; buf[3]=0; }
    else if (startsWith(sys, "SEGA SEGACD",     11)) { currentRomInfo.sysType = SYS_SCD;  buf[0]='S'; buf[1]='C'; buf[2]='D'; buf[3]=0; }
    else if (startsWith(sys, "SEGA SMS",         8)) { currentRomInfo.sysType = SYS_SMS;  buf[0]='S'; buf[1]='M'; buf[2]='S'; buf[3]=0; }
    else if (startsWith(sys, "SEGA BIOS",        9)) { currentRomInfo.sysType = SYS_BIOS; buf[0]='B'; buf[1]='I'; buf[2]='O'; buf[3]='S'; buf[4]=0; }
    else                                             { currentRomInfo.sysType = SYS_UNKNOWN; buf[0]='?'; buf[1]='?'; buf[2]=0; }

    /* Copie sysStr depuis buf temporaire */
    u8 si = 0;
    while (buf[si] && si < 7) { currentRomInfo.sysStr[si] = buf[si]; si++; }
    currentRomInfo.sysStr[si] = 0;

    /* --- Checksum 0x18E --- */
    storedChk = ((u16)buf[HDR_CHECKSUM] << 8) | buf[HDR_CHECKSUM + 1];
    calcChk = 0;
    for (u16 i = 0x200; i < 512; i += 2)
        calcChk += ((u16)buf[i] << 8) | buf[i + 1];
    currentRomInfo.chkOk = (storedChk == calcChk) ? 1 : 0;

    /* --- Devices 0x190 --- */
    char *dev = (char *)(buf + HDR_DEVICES);
    currentRomInfo.hasJoypad3  = hasChar(dev, 16, 'J');
    currentRomInfo.hasJoypad6  = hasChar(dev, 16, '6');
    currentRomInfo.hasMultitap = hasChar(dev, 16, '4');
    currentRomInfo.hasMouse    = hasChar(dev, 16, 'M');
    currentRomInfo.hasKeyboard = hasChar(dev, 16, 'K');
    currentRomInfo.hasPrinter  = hasChar(dev, 16, 'P');
    currentRomInfo.hasSCD      = hasChar(dev, 16, 'C');
    currentRomInfo.hasModem    = hasChar(dev, 16, 'R');

    /* --- Backup memory 0x1B0 --- */
    /* Format : "RA" + type + flag + addr_start(4) + addr_end(4) */
    if (buf[HDR_BACKUP] == 'R' && buf[HDR_BACKUP + 1] == 'A') {
        currentRomInfo.hasBackup = 1;
        currentRomInfo.isEEPROM  = (buf[HDR_BACKUP + 2] & 0x40) ? 1 : 0;
        u32 start = ((u32)buf[HDR_BACKUP+4]<<24)|((u32)buf[HDR_BACKUP+5]<<16)|
                    ((u32)buf[HDR_BACKUP+6]<<8) | buf[HDR_BACKUP+7];
        u32 end   = ((u32)buf[HDR_BACKUP+8]<<24)|((u32)buf[HDR_BACKUP+9]<<16)|
                    ((u32)buf[HDR_BACKUP+10]<<8)| buf[HDR_BACKUP+11];
        currentRomInfo.backupSize = (end - start) + 1;
    }

    /* --- Région 0x1F0 --- */
    char *reg = (char *)(buf + HDR_REGION);
    currentRomInfo.hasJPN = hasChar(reg, 3, 'J');
    currentRomInfo.hasUSA = hasChar(reg, 3, 'U');
    currentRomInfo.hasEUR = hasChar(reg, 3, 'E');

    return 1;
}

void ROM_drawInfo(void)
{
    #define RC  27   /* colonne départ panneau droit */
    #define RL  4    /* ligne départ */

    char buf[14];

    /* Efface panneau droit */
    for (u8 y = RL; y < 26; y++)
        VDP_drawText("             ", RC, y);

    /* Titre section */
    VDP_setTextPalette(1);
    VDP_drawText("ROM INFO", RC, RL);
    VDP_setTextPalette(0);

    /* Système */
    VDP_drawText("Sys:", RC, RL+1);
    VDP_setTextPalette(1);
    VDP_drawText(currentRomInfo.sysStr, RC+4, RL+1);
    VDP_setTextPalette(0);

    /* Région */
    VDP_drawText("Reg:", RC, RL+2);
    VDP_setTextPalette(1);
    u8 rc = RC + 4;
    if (currentRomInfo.hasJPN) { VDP_drawText("J", rc, RL+2); rc++; }
    if (currentRomInfo.hasUSA) { VDP_drawText("U", rc, RL+2); rc++; }
    if (currentRomInfo.hasEUR) { VDP_drawText("E", rc, RL+2); rc++; }
    if (!currentRomInfo.hasJPN && !currentRomInfo.hasUSA && !currentRomInfo.hasEUR)
    VDP_drawText("???", RC+4, RL+2);
    VDP_setTextPalette(0);

	/* Checksum  */
	VDP_drawText("Chk:", RC, RL+3);
	VDP_setTextPalette(1);
	VDP_drawText(currentRomInfo.chkOk ? "OK" : "BAD", RC+4, RL+3);
	VDP_setTextPalette(0);

    /* Backup */
    VDP_drawText("Bak:", RC, RL+5);
    if (currentRomInfo.hasBackup) {
        VDP_setTextPalette(1);
        VDP_drawText(currentRomInfo.isEEPROM ? "EEPROM" : "SRAM  ", RC+4, RL+5);
        /* Taille */
        u32 sz = currentRomInfo.backupSize;
        if (sz >= 1024) {
            intToStr(sz/1024, buf, 1);
            u8 l = 0; while(buf[l]) l++;
            buf[l]='K'; buf[l+1]='B'; buf[l+2]=0;
        } else {
            intToStr(sz, buf, 1);
            u8 l = 0; while(buf[l]) l++;
            buf[l]='B'; buf[l+1]=0;
        }
        VDP_drawText(buf, RC+4, RL+6);
    } else {
        VDP_setTextPalette(0);
        VDP_drawText("NO", RC+4, RL+5);
    }
    VDP_setTextPalette(0);

    /* Devices */
    VDP_drawText("Devices:", RC, RL+8);
    VDP_setTextPalette(1);
    u8 dl = RL+9;
	/* Noms des devices */
	if (currentRomInfo.hasJoypad3)  { VDP_drawText("Joypad 3 Btn", RC, dl++); }
	if (currentRomInfo.hasJoypad6)  { VDP_drawText("Joypad 6 Btn", RC, dl++); }
	if (currentRomInfo.hasMultitap) { VDP_drawText("Multitap    ", RC, dl++); }
	if (currentRomInfo.hasMouse)    { VDP_drawText("Mouse       ", RC, dl++); }
	if (currentRomInfo.hasKeyboard) { VDP_drawText("Keyboard    ", RC, dl++); }
	if (currentRomInfo.hasPrinter)  { VDP_drawText("Printer     ", RC, dl++); }
	if (currentRomInfo.hasSCD)      { VDP_drawText("Sega CD     ", RC, dl++); }
	if (currentRomInfo.hasModem)    { VDP_drawText("Modem       ", RC, dl++); }
    VDP_setTextPalette(0);
}