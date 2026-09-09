#include "genesis.h"
#include "OpenEd.h"
#include "ff.h"
#include "config.h"

#define RAM_SECT __attribute__((section(".ramprog")))
#define NO_INL   __attribute__((noinline))

unsigned short ed_cfg;
static u8 flash_type = FLASH_TYPE_UNK;
static u8 game_type  = GAME_TYPE_NONE;

/* Macro protection interruptions */
#define CTRL_WRITE(val) \
    asm("move.w #0x2700, %sr"); \
    *((vu16 *) 0xA130E0) = (val); \
    asm("move.w #0x2300, %sr");

/* ------------------------------------------------------------------ */
/* Prototypes internes flash                                          */
/* ------------------------------------------------------------------ */

static u8   flashInit_ram(void);
static void flashWrite_ram(u16 *src, u16 *dst, u32 len);
static u8   flashInit_m29(void);
static void flashWordProgram(u32 addr, u16 value);
static void flashWrite_m29(u16 *src, u16 *dst, u32 len);
static void flashErase_m29(u32 addr);
static void flashUnlock(void);
static void flashUnlockBypassEnter(void);  /* Unlock Bypass mode entry */
static void flashUnlockBypassExit(void);   /* Unlock Bypass mode exit  */
static void flashReset(void);

/* ------------------------------------------------------------------ */
/* Contrôle mapper                                                      */
/* ------------------------------------------------------------------ */

void OpenEd_Init(void)
{
    ed_cfg = 0x04;
    CTRL_WRITE(ed_cfg)
}

void OpenEd_DebugLed_ON(void)
{
    ed_cfg |= CTRL_LED;
    CTRL_WRITE(ed_cfg)
}

void OpenEd_DebugLed_OFF(void)
{
    ed_cfg &= ~CTRL_LED;
    CTRL_WRITE(ed_cfg)
}

void OpenEd_Set_Bank0(void)
{
    ed_cfg &= ~CTRL_ROM_BANK;
    CTRL_WRITE(ed_cfg)
}

void OpenEd_Set_Bank1(void)
{
    ed_cfg |= CTRL_ROM_BANK;
    CTRL_WRITE(ed_cfg)
}

RAM_SECT NO_INL void OpenEd_Start_ROM(void)
{
    asm("move.w #0x2700, %sr");
    ed_cfg &= ~CTRL_ROM_BANK;
    *((vu16 *) 0xA130E0) = ed_cfg;
    asm("move.l 0, %sp");
    asm("move.l 4, %a0");
    asm("jmp (%a0)");
}

/* ------------------------------------------------------------------ */
/* SRAM control                                                         */
/* ------------------------------------------------------------------ */

void OpenEd_SRAM_Enable(void)
{
    ed_cfg |= CTRL_SRM_ON;
    CTRL_WRITE(ed_cfg)
}

void OpenEd_SRAM_Disable(void)
{
    ed_cfg &= ~CTRL_SRM_ON;
    CTRL_WRITE(ed_cfg)
}

u8 OpenEd_SRAM_IsEnabled(void)
{
    return (ed_cfg & CTRL_SRM_ON) ? 1 : 0;
}

/* ------------------------------------------------------------------ */
/* SRAM                                                                  */
/* ------------------------------------------------------------------ */

RAM_SECT NO_INL static u16 read_header_bank0(u32 addr)
{
    u16 saved = ed_cfg;
    u16 result;

    /* Passage en Bank0 */
    ed_cfg &= ~CTRL_ROM_BANK;
    *((vu16 *) 0xA130E0) = ed_cfg;

    /* Lecture atomique de 2 octets */
    result = *(vu16*)addr;

    /* Restauration du bank d'origine */
    ed_cfg = saved;
    *((vu16 *) 0xA130E0) = ed_cfg;

    return result;
}

RAM_SECT NO_INL void OpenEd_SRAM_ReadBlock(u8 *dst, u32 offset, u32 len)
{
    u16 saved = ed_cfg;

    asm("move.w #0x2700, %sr");
    ed_cfg &= ~CTRL_ROM_BANK;
    ed_cfg |= CTRL_SRM_ON;
    *((vu16 *) 0xA130E0) = ed_cfg;

    vu16 *src = (vu16 *)(SRAM_BASE + (offset * 2));
    while (len--) {
        u16 word = *src++;
        *dst++ = (u8)((word >> 8) & 0xFF);   /* octet HAUT, pas bas */
    }

    ed_cfg = saved;
    *((vu16 *) 0xA130E0) = ed_cfg;
    asm("move.w #0x2300, %sr");
}

void OpenEd_SRAM_WriteBlock(const u8 *src, u32 offset, u32 len)
{
    vu8 *dst = (vu8 *)(SRAM_BASE + (offset * 2) + 1);

    asm("move.w #0x2700, %sr");

    ed_cfg |= CTRL_SRM_ON;
    *((vu16 *) 0xA130E0) = ed_cfg;

    while (len--) {
        *dst = *src++;
        dst += 2;
    }

    ed_cfg &= ~CTRL_SRM_ON;
    *((vu16 *) 0xA130E0) = ed_cfg;

    asm("move.w #0x2300, %sr");
}

RAM_SECT NO_INL u8 SRAM_DumpToSD(const char *path, u32 realDataSize)
{
    FIL fil;
    UINT bw;
    static u8 chunk[256];
    u32 offset = 0;
    u16 saved = ed_cfg;

    if (f_open(&fil, path, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) return 0;

    while (offset < realDataSize) {
        u16 n = (realDataSize - offset > 256) ? 256 : (realDataSize - offset);

        asm("move.w #0x2700, %sr");
        ed_cfg &= ~CTRL_ROM_BANK;
        ed_cfg |= CTRL_SRM_ON;
        *((vu16 *) 0xA130E0) = ed_cfg;

        vu8 *src = (vu8 *)(SRAM_BASE + (offset * 2));
        for (u16 i = 0; i < n; i++) {
            chunk[i] = *src;
            src += 2;
        }

        ed_cfg = saved;
        *((vu16 *) 0xA130E0) = ed_cfg;
        asm("move.w #0x2300, %sr");

        f_write(&fil, chunk, n, &bw);
        offset += n;
    }

    f_close(&fil);
    return 1;
}

RAM_SECT NO_INL u8 SRAM_DumpToSD_Emu(const char *path, u32 realDataSize)
{
    FIL fil;
    UINT bw;
    static u8 chunk[512];
    u32 offset = 0;
    u16 saved = ed_cfg;

    if (f_open(&fil, path, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) return 0;

    while (offset < realDataSize) {
        u16 n = (realDataSize - offset > 256) ? 256 : (realDataSize - offset);

        asm("move.w #0x2700, %sr");
        ed_cfg &= ~CTRL_ROM_BANK;
        ed_cfg |= CTRL_SRM_ON;
        *((vu16 *) 0xA130E0) = ed_cfg;

        vu8 *src = (vu8 *)(SRAM_BASE + (offset * 2));
        for (u16 i = 0; i < n; i++) {
            chunk[i*2]     = 0x00;   /* padding EN PREMIER */
            chunk[i*2 + 1] = *src;   /* donnée réelle ENSUITE */
            src += 2;
        }

        ed_cfg = saved;
        *((vu16 *) 0xA130E0) = ed_cfg;
        asm("move.w #0x2300, %sr");

        f_write(&fil, chunk, n * 2, &bw);
        offset += n;
    }

    f_close(&fil);
    return 1;
}


u8 SRAM_RestoreFromSD(const char *path, u32 realDataSize)
{
    FIL fil;
    UINT br;
    static u8 chunk[512];
    u32 offset = 0;

    if (f_open(&fil, path, FA_READ) != FR_OK) return 0;

    while (offset < realDataSize) {
        u16 n = (realDataSize - offset > 512) ? 512 : (realDataSize - offset);

        if (f_read(&fil, chunk, n, &br) != FR_OK || br < n) {
            f_close(&fil);
            return 0;
        }

        OpenEd_SRAM_WriteBlock(chunk, offset, n);
        offset += n;
    }

    f_close(&fil);
    return 1;
}


void OpenEd_Game_Init(void)
{
    /* Lit les 2 premiers octets du marqueur SRAM depuis Bank0 */
    u16 hdr = read_header_bank0(0x1B0);
    u8 b0 = (hdr >> 8) & 0xFF;
    u8 b1 = hdr & 0xFF;

    /* Détecte le type de jeu */
    if (b0 == 0xFF && b1 == 0xFF) {
        game_type = GAME_TYPE_NONE;
    } else if (b0 == 'R' && b1 == 'A') {
        game_type = GAME_TYPE_SRAM;
    } else {
        game_type = GAME_TYPE_ROM;
    }

    /* Configure la SRAM en conséquence */
    if (game_type == GAME_TYPE_SRAM) {
        OpenEd_SRAM_Enable();
    } else {
        OpenEd_SRAM_Disable();
    }
}

u8 OpenEd_Game_Type(void)
{
    return game_type;
}

/* ------------------------------------------------------------------ */
/* SRAM — test isolé (1 octet, écriture puis lecture immédiate)        */
/* ------------------------------------------------------------------ */

void OpenEd_SRAM_SingleTest(void)
{
    u8 testVal = 0x11;
    u8 readVal = 0;
	
	    ed_cfg |= CTRL_SRM_ON;
    *((vu16 *) 0xA130E0) = ed_cfg;

    u16 ctrlCheck = *((vu16 *) 0xA130E0);

    char cbuf[8];
    intToStr(ctrlCheck, cbuf, 1);
    VDP_drawText("CTRL after write: ", 0, 24);
    VDP_drawText(cbuf, 19, 24);

    ed_cfg &= ~CTRL_SRM_ON;
    *((vu16 *) 0xA130E0) = ed_cfg;

    asm("move.w #0x2700, %sr");

    ed_cfg |= CTRL_SRM_ON;
    *((vu16 *) 0xA130E0) = ed_cfg;

    *(vu8*)(SRAM_BASE + 1) = testVal;
    readVal = *(vu8*)(SRAM_BASE + 1);

    ed_cfg &= ~CTRL_SRM_ON;
    *((vu16 *) 0xA130E0) = ed_cfg;

    asm("move.w #0x2300, %sr");

    char buf[8];
    intToStr(readVal, buf, 1);
    VDP_drawText("SRAM test: ", 0, 25);
    VDP_drawText(buf, 12, 25);
}

RAM_SECT NO_INL u8 SRAM_DumpWide(const char *path, u32 startAddr, u32 len)
{
    FIL fil;
    UINT bw;
    static u16 chunk[256];   /* 512 octets par chunk, en mots */
    u32 offset = 0;
    u16 saved = ed_cfg;

    VDP_drawText("Dump wide: starting...   ", 0, 26);

    if (f_open(&fil, path, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) {
        VDP_drawText("Dump wide: OPEN FAIL     ", 0, 26);
        return 0;
    }

    while (offset < len) {
        u16 n = (len - offset > 512) ? 256 : (len - offset) / 2;

        asm("move.w #0x2700, %sr");
        ed_cfg &= ~CTRL_ROM_BANK;   /* bascule sur bank GAME */
        ed_cfg |= CTRL_SRM_ON;
        *((vu16 *) 0xA130E0) = ed_cfg;

        vu16 *src = (vu16 *)(startAddr + offset);
        for (u16 i = 0; i < n; i++) {
            chunk[i] = *src++;
        }

        ed_cfg = saved;   /* restaure bank BIOS + SRAM off */
        *((vu16 *) 0xA130E0) = ed_cfg;
        asm("move.w #0x2300, %sr");

        f_write(&fil, chunk, n * 2, &bw);
        offset += n * 2;

        if ((offset & 0xFFFF) == 0) {
            char buf[12];
            intToStr(offset, buf, 1);
            VDP_drawText("Dump wide: progress:      ", 0, 26);
            VDP_drawText(buf, 20, 26);
        }
    }

    f_close(&fil);
    VDP_drawText("Dump wide: DONE!          ", 0, 26);
    return 1;
}

/* Dans OpenEd.c */
RAM_SECT NO_INL void OpenEd_SRAM_TestWide(u16 *dst, u32 offset, u16 count)
{
    u16 saved = ed_cfg;

    asm("move.w #0x2700, %sr");
    ed_cfg &= ~CTRL_ROM_BANK;
    ed_cfg |= CTRL_SRM_ON;
    *((vu16 *) 0xA130E0) = ed_cfg;

    vu16 *src = (vu16 *)(SRAM_BASE + offset);
    for (u16 i = 0; i < count; i++) {
        dst[i] = *src++;
    }

    ed_cfg = saved;
    *((vu16 *) 0xA130E0) = ed_cfg;
    asm("move.w #0x2300, %sr");
}

/* ------------------------------------------------------------------ */
/* SPI                                                                  */
/* ------------------------------------------------------------------ */

void OpenEd_SPI_Select(unsigned char target)
{
    ed_cfg &= ~(CTRL_SDC_SS | CTRL_EXP_SS);
    if (target == SPI_SEL_SDC)
        ed_cfg |= CTRL_SDC_SS;
    else if (target == SPI_SEL_EXP)
        ed_cfg |= CTRL_EXP_SS;
    CTRL_WRITE(ed_cfg)
}

unsigned char OpenEd_SPI_Read(void)
{
    unsigned char ret = 0;
    asm("move.w #0x2700, %sr");
    ed_cfg |= CTRL_SPI_MOSI;
    *((vu16 *) 0xA130E0) = ed_cfg;
    for (unsigned short i = 0; i < 8; i++) {
        *((vu16 *) 0xA130E0) = ed_cfg | CTRL_SPI_MOSI | CTRL_SPI_CLK;
        ret <<= 1;
        ret |= *((vu16 *) 0xA130E0) & CTRL_SPI_MISO;
        *((vu16 *) 0xA130E0) = ed_cfg | CTRL_SPI_MOSI;
    }
    asm("move.w #0x2300, %sr");
    return ret;
}

unsigned char OpenEd_SPI_ReadFast(void)
{
    unsigned char ret = 0;
    asm("move.w #0x2700, %sr");
    ed_cfg &= ~CTRL_SPI_CLK;
    ed_cfg |=  CTRL_SPI_MOSI;
    *((vu16 *) 0xA130E0) = ed_cfg;
    for (unsigned short i = 0; i < 8; i++) {
        ret <<= 1;
        ret |= *((vu16 *) 0xA130E0) & CTRL_SPI_MISO;
    }
    asm("move.w #0x2300, %sr");
    return ret;
}

void OpenEd_SPI_Write(unsigned char val)
{
    asm("move.w #0x2700, %sr");
    for (unsigned short i = 0; i < 8; i++) {
        *((vu16 *) 0xA130E0) = ed_cfg | (val & CTRL_SPI_MOSI);
        *((vu16 *) 0xA130E0) = ed_cfg | (val & CTRL_SPI_MOSI) | CTRL_SPI_CLK;
        *((vu16 *) 0xA130E0) = ed_cfg | (val & CTRL_SPI_MOSI);
        val <<= 1;
    }
    asm("move.w #0x2300, %sr");
}

unsigned char OpenEd_SPI_Read_Write(unsigned char val)
{
    unsigned char ret = 0;
    asm("move.w #0x2700, %sr");
    for (unsigned short i = 0; i < 8; i++) {
        *((vu16 *) 0xA130E0) = (val & CTRL_SPI_MOSI) | ed_cfg;
        *((vu16 *) 0xA130E0) = (val & CTRL_SPI_MOSI) | ed_cfg | CTRL_SPI_CLK;
        ret <<= 1;
        ret |= *((vu16 *) 0xA130E0) & CTRL_SPI_MISO;
        *((vu16 *) 0xA130E0) = (val & CTRL_SPI_MOSI) | ed_cfg;
        val <<= 1;
    }
    asm("move.w #0x2300, %sr");
    return ret;
}

/* ------------------------------------------------------------------ */
/* Flash — fonctions internes bas niveau                              */
/* ------------------------------------------------------------------ */

/* Unlock séquence AMD/M29 */
RAM_SECT NO_INL static void flashUnlock(void)
{
    *(vu8*)(0xAAB) = 0xAA;   /* 0xAAB byte = 0x555 word */
    *(vu8*)(0x555) = 0x55;   /* 0x555 byte = 0x2AA word */
}

/* Reset flash */
RAM_SECT NO_INL static void flashReset(void)
{
    *(vu8*)(0x1) = 0xF0;
}

/* Écrit un mot et attend la confirmation — data polling */
RAM_SECT NO_INL static void flashWordProgram(u32 addr, u16 value)
{
    flashUnlock();
    *(vu8*)(0xAAB) = 0xA0;          /* commande Program */
    *(vu16*)addr   = value;          /* écrit le mot */
    while (*(vu16*)addr != value);   /* data poll — attend confirmation */
}

/* Écrit len octets depuis src vers dst */
RAM_SECT NO_INL static void flashWrite_m29(u16 *src, u16 *dst, u32 len)
{
    u32 addr = (u32)dst;
    len /= 2;   /* en mots */
    while (len--) {
        flashWordProgram(addr, *src++);
        addr += 2;
    }
    flashReset();
}

/* Efface un secteur de 64K */
RAM_SECT NO_INL static void flashErase_m29(u32 addr)
{
    flashUnlock();
    *(vu8*)(0xAAB) = 0x80;          /* commande Erase */
    flashUnlock();
    *(vu8*)(addr + 1) = 0x30;       /* secteur erase */
    while (*(vu16*)addr != 0xFFFF); /* attend que le secteur soit effacé */
}

/* ------------------------------------------------------------------ */
/* Flash — RAM type (dev boards)                                        */
/* ------------------------------------------------------------------ */

RAM_SECT NO_INL static u8 flashInit_ram(void)
{
    vu16 *ptr = (vu16 *) 0x10000;
    vu16 old_val = *ptr;
    vu16 new_val;
    *ptr ^= 0xffff;
    new_val = *ptr;
    *ptr = old_val;
    if ((old_val ^ 0xffff) == new_val) return 1;
    return 0;
}

RAM_SECT NO_INL static void flashWrite_ram(u16 *src, u16 *dst, u32 len)
{
    len /= 2;
    while (len--) *dst++ = *src++;
}

RAM_SECT NO_INL static u8 flashInit_m29(void)
{
    flashReset();
    return 1;
}

/* ------------------------------------------------------------------ */
/* Flash — API publique                                                 */
/* ------------------------------------------------------------------ */

RAM_SECT NO_INL void OpenEd_Flash_Init(void)
{
    if (flashInit_ram()) {
        flash_type = FLASH_TYPE_RAM;
    } else if (flashInit_m29()) {
        flash_type = FLASH_TYPE_M29;
    } else {
        flash_type = FLASH_TYPE_UNK;
    }
}

RAM_SECT NO_INL void OpenEd_Flash_Write(u16 *src, u16 *dst, u32 len)
{
    if (flash_type == FLASH_TYPE_RAM) {
        flashWrite_ram(src, dst, len);
        return;
    }
    if (flash_type == FLASH_TYPE_M29) {
        flashWrite_m29(src, dst, len);
        return;
    }
}

RAM_SECT NO_INL void OpenEd_Flash_Erase64K(u32 addr, u8 wait_rdy)
{
    if (flash_type == FLASH_TYPE_M29) {
        flashErase_m29(addr);
        /* wait_rdy déjà géré dans flashErase_m29 via data poll */
    }
}

RAM_SECT NO_INL u8 OpenEd_Flash_Type(void)
{
    return flash_type;
}

/* ------------------------------------------------------------------ */
/* Flash — fonctions de test                                            */
/* ------------------------------------------------------------------ */

RAM_SECT NO_INL u8 OpenEd_Flash_TestErase(u32 addr)
{
    asm("move.w #0x2700, %sr");

    /* Bank0 pour accéder à la flash */
    ed_cfg &= ~CTRL_ROM_BANK;
    *((vu16 *) 0xA130E0) = ed_cfg;

    OpenEd_Flash_Erase64K(addr, 1);

    /* Reset flash → force le retour en mode lecture normale */
    *(vu8*)(0x1) = 0xF0;

    /* Dummy read pour laisser le bus se stabiliser */
    volatile u16 dummy = *(vu16*)addr;
    (void)dummy;

    /* Vérifie 0xFF */
    vu16 *ptr = (vu16 *) addr;
    u32 len = 65536 / 2;
    u8 ok = 1;
    while (len--) {
        if (*ptr++ != 0xFFFF) { ok = 0; break; }
    }

    /* Bank1 */
    ed_cfg |= CTRL_ROM_BANK;
    *((vu16 *) 0xA130E0) = ed_cfg;

    asm("move.w #0x2300, %sr");
    return ok;
}

RAM_SECT NO_INL u8 OpenEd_Flash_TestWrite(u32 addr)
{
    static u16 testBuf[256];
    for (u16 i = 0; i < 256; i++) testBuf[i] = 0xAAAA;

    asm("move.w #0x2700, %sr");

    /* Bank0 */
    ed_cfg &= ~CTRL_ROM_BANK;
    *((vu16 *) 0xA130E0) = ed_cfg;

    u32 remaining = 65536;
    u16 *dst = (u16 *) addr;
    while (remaining > 0) {
        OpenEd_Flash_Write(testBuf, dst, 512);
        dst += 256;
        remaining -= 512;
    }

    /* Vérifie 0xAAAA */
    vu16 *ptr = (vu16 *) addr;
    u32 len = 65536 / 2;
    u8 ok = 1;
    while (len--) {
        if (*ptr++ != 0xAAAA) { ok = 0; break; }
    }

    /* Bank1 */
    ed_cfg |= CTRL_ROM_BANK;
    *((vu16 *) 0xA130E0) = ed_cfg;

    asm("move.w #0x2300, %sr");
    return ok;
}
RAM_SECT NO_INL u8 ROM_flashFromSD(const char *path, u32 romSize)
{
    FIL fil;
    UINT br;
    static BYTE buf[4096];
    u32 flashAddr  = 0x000000;
    u32 bytesLeft  = romSize;

    /* Affichage initial */
    VDP_drawText("Flashing: [                    ]", 0, 27);

    /* Lit la taille de l'ancien jeu depuis le header en flash Bank0 */
    asm("move.w #0x2700, %sr");
    ed_cfg &= ~CTRL_ROM_BANK;
    *((vu16 *) 0xA130E0) = ed_cfg;

    u32 oldRomEnd  = ((u32)(*(vu8*)0x1A4) << 24) |
                     ((u32)(*(vu8*)0x1A5) << 16) |
                     ((u32)(*(vu8*)0x1A6) << 8)  |
                      (u32)(*(vu8*)0x1A7);
    u32 oldRomSize = oldRomEnd + 1;

    ed_cfg |= CTRL_ROM_BANK;
    *((vu16 *) 0xA130E0) = ed_cfg;
    asm("move.w #0x2300, %sr");

    /* Efface le max des deux tailles */
    u32 eraseSize    = (oldRomSize > romSize) ? oldRomSize : romSize;
    u32 numErSectors = (eraseSize + 0xFFFF) / 0x10000;
    u32 numWrSectors = (romSize   + 0xFFFF) / 0x10000;

    if (f_open(&fil, path, FA_READ) != FR_OK) {
        VDP_drawText("Flashing: ERR OPEN            ", 0, 27);
        return 0;
    }

    /* Passe 1 — efface tous les secteurs nécessaires */
    VDP_drawText("Erasing:  [                    ]", 0, 27);
    for (u32 s = 0; s < numErSectors; s++)
    {
        u8 filled = (u8)((s * 20) / numErSectors);
        char bar[24];
        bar[0]='[';
        for (u8 i = 0; i < 20; i++)
            bar[i+1] = (i < filled) ? '#' : ' ';
        bar[21]=']'; bar[22]=0;
        VDP_drawText(bar, 10, 27);

        asm("move.w #0x2700, %sr");
        ed_cfg &= ~CTRL_ROM_BANK;
        *((vu16 *) 0xA130E0) = ed_cfg;

        OpenEd_Flash_Erase64K(s * 0x10000, 1);
        *(vu8*)(0x1) = 0xF0;
        volatile u16 dummy = *(vu16*)(s * 0x10000); (void)dummy;

        ed_cfg |= CTRL_ROM_BANK;
        *((vu16 *) 0xA130E0) = ed_cfg;
        asm("move.w #0x2300, %sr");
    }
    VDP_drawText("Erasing:  [####################]", 0, 27);

    /* Passe 2 — écrit la ROM */
    VDP_drawText("Flashing: [                    ]", 0, 27);
    for (u32 s = 0; s < numWrSectors; s++)
    {
        u8 filled = (u8)((s * 20) / numWrSectors);
        char bar[24];
        bar[0]='[';
        for (u8 i = 0; i < 20; i++)
            bar[i+1] = (i < filled) ? '#' : ' ';
        bar[21]=']'; bar[22]=0;
        VDP_drawText(bar, 10, 27);

        u32 sectorBytes = (bytesLeft > 0x10000) ? 0x10000 : bytesLeft;
        u16 *dst = (u16*)flashAddr;
        u32 written = 0;

        while (written < sectorBytes)
        {
            UINT toRead = ((sectorBytes - written) > 4096) ? 4096 : (sectorBytes - written);

            /* Lecture SD — Bank1, interruptions ON */
            if (f_read(&fil, buf, toRead, &br) != FR_OK || br == 0) goto flash_done;

            /* Skip si chunk entièrement 0xFF */
            u8 allFF = 1;
            for (UINT k = 0; k < br; k++) {
                if (buf[k] != 0xFF) { allFF = 0; break; }
            }

            if (!allFF) {
                asm("move.w #0x2700, %sr");
                ed_cfg &= ~CTRL_ROM_BANK;
                *((vu16 *) 0xA130E0) = ed_cfg;

                OpenEd_Flash_Write((u16*)buf, dst, br);

                ed_cfg |= CTRL_ROM_BANK;
                *((vu16 *) 0xA130E0) = ed_cfg;
                asm("move.w #0x2300, %sr");
            }

            dst     += br / 2;
            written += br;
        }

        flashAddr += 0x10000;
        bytesLeft -= sectorBytes;
    }

flash_done:
    f_close(&fil);
    VDP_drawText("Flashing: [####################] DONE!", 0, 27);
    return 1;
}