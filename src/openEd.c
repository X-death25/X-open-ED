#include "genesis.h"
#include "OpenEd.h"
#include "ff.h"

#define RAM_SECT __attribute__((section(".ramprog")))
#define NO_INL   __attribute__((noinline))

unsigned short ed_cfg;
static u8 flash_type = FLASH_TYPE_UNK;

/* Macro protection interruptions */
#define CTRL_WRITE(val) \
    asm("move.w #0x2700, %sr"); \
    *((vu16 *) 0xA130E0) = (val); \
    asm("move.w #0x2300, %sr");

/* ------------------------------------------------------------------ */
/* Prototypes internes flash                                            */
/* ------------------------------------------------------------------ */
static u8   flashInit_ram(void);
static void flashWrite_ram(u16 *src, u16 *dst, u32 len);
static u8   flashInit_m29(void);
static void flashWordProgram(u32 addr, u16 value);
static void flashWrite_m29(u16 *src, u16 *dst, u32 len);
static void flashErase_m29(u32 addr);
static void flashUnlock(void);
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
/* Flash — fonctions internes bas niveau style SGDK                     */
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
    static BYTE buf[512];
    u32 numSectors = (romSize + 0xFFFF) / 0x10000;
    u32 flashAddr  = 0x000000;
    u32 bytesLeft  = romSize;

    /* Affichage initial */
    VDP_drawText("Flashing: [                    ]", 0, 27);

    if (f_open(&fil, path, FA_READ) != FR_OK) {
        VDP_drawText("Flashing: ERR OPEN            ", 0, 27);
        return 0;
    }

    for (u32 s = 0; s < numSectors; s++)
    {
        /* Barre de progression — interruptions encore actives */
        u8 filled = (u8)((s * 20) / numSectors);
        char bar[24];
        bar[0]='[';
        for (u8 i = 0; i < 20; i++)
            bar[i+1] = (i < filled) ? '#' : ' ';
        bar[21]=']'; bar[22]=0;
        VDP_drawText(bar, 10, 27);

        /* Erase secteur — Bank0, ints coupées */
        asm("move.w #0x2700, %sr");
        ed_cfg &= ~CTRL_ROM_BANK;
        *((vu16 *) 0xA130E0) = ed_cfg;

        OpenEd_Flash_Erase64K(flashAddr, 1);
        *(vu8*)(0x1) = 0xF0;
        volatile u16 dummy = *(vu16*)flashAddr; (void)dummy;

        /* Retour Bank1 après erase */
        ed_cfg |= CTRL_ROM_BANK;
        *((vu16 *) 0xA130E0) = ed_cfg;
        asm("move.w #0x2300, %sr");

        /* Ecrit le secteur par chunks de 512 */
        u32 sectorBytes = (bytesLeft > 0x10000) ? 0x10000 : bytesLeft;
        u16 *dst = (u16*)flashAddr;
        u32 written = 0;

        while (written < sectorBytes)
        {
            UINT toRead = ((sectorBytes - written) > 512) ? 512 : (sectorBytes - written);

            /* Lecture SD — Bank1, interruptions ON */
            if (f_read(&fil, buf, toRead, &br) != FR_OK || br == 0) goto flash_done;

            /* Ecriture flash — Bank0, ints coupées */
            asm("move.w #0x2700, %sr");
            ed_cfg &= ~CTRL_ROM_BANK;
            *((vu16 *) 0xA130E0) = ed_cfg;

            OpenEd_Flash_Write((u16*)buf, dst, br);

            /* Retour Bank1 */
            ed_cfg |= CTRL_ROM_BANK;
            *((vu16 *) 0xA130E0) = ed_cfg;
            asm("move.w #0x2300, %sr");

            dst     += br / 2;
            written += br;
        }

        flashAddr += 0x10000;
        bytesLeft -= sectorBytes;
    }

flash_done:
    f_close(&fil);
    VDP_drawText("Flashing: [####################]", 0, 27);
    return 1;
}