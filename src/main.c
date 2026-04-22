#include "genesis.h"
#include "explorer.h"
#include "rom_parser.h"
#include "music_player.h"

#define logo_lib sgdk_logo
#define font_lib font_default
#define font_pal_lib font_pal_default

#define RAM_SECT __attribute__((section(".ramprog")))
#define NO_INL   __attribute__((noinline))

#include "gfx.h"
#include "OpenEd.h"
#include "ffconf.h"
#include "ff.h"
#include "diskio.h"

/* Globals */
volatile u8 appMode   = 0;
volatile int PosX     = 10;
volatile int PosY     = 8;
static unsigned long i = 0;
static char displaychar[4];
static u16 repeatDelay = 0;
static u16 lastButton  = 0;

FATFS FatFs;

static void joyEvent(u16 joy, u16 changed, u16 state);
static void UpdateCursor(int PosX, int PosY);
static void ClearMenu(void);
static void ClearMenuFull(void);
static void UpdateMenu(int PosX, int PosY);

int main(bool hardReset)
{
    u16 ind, idx1, idx2;
    FIL    Fil;
    UINT   bw;
    FRESULT res;

    SYS_disableInts();
    VDP_setScreenWidth320();
    OpenEd_Init();
    OpenEd_Flash_Init();
    JOY_setEventHandler(joyEvent);

    PAL_setColors(0, (u16*)main_title.palette->data, 16, CPU);

    ind = TILE_USER_INDEX;
    ind += main_title.tileset->numTile;
    idx1 = ind;
    ind += main_bottom.tileset->numTile;
    idx2 = ind;

    VDP_drawImageEx(BG_A, &main_title,  TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, idx1), 0,  1, FALSE, DMA);
    VDP_drawImageEx(BG_A, &main_bottom, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, idx2), 0, 20, FALSE, DMA);

    VDP_drawText(">",                10,  8);
    VDP_drawText("Start Game",       12,  8);
    VDP_drawText("SD Card Explorer", 12, 10);
    VDP_drawText("SRAM Manager",     12, 12);
    VDP_drawText("Hardware Info",    12, 14);
    VDP_drawText("Flash Erase Test", 12, 16);
    VDP_drawText("Flash Write Test", 12, 18);

    PosX = 10;
    PosY = 8;

    Z80_loadDriver(Z80_DRIVER_XGM, TRUE);
    XGM_setLoopNumber(-1);

    SYS_enableInts();
    VDP_drawText("Avant mount...", 0, 20);

    OpenEd_DebugLed_ON();
    dly_10ms();
    OpenEd_DebugLed_OFF();

    /* Log type flash */
    char ftype[4];
    intToStr(OpenEd_Flash_Type(), ftype, 1);
    VDP_drawText("Flash type: ", 0, 23);
    VDP_drawText(ftype, 12, 23);

    /* 80 dummy clocks */
    {
        BYTE buf[1];
        OpenEd_SPI_Select(SPI_SEL_OFF);
        for (u8 n = 10; n; n--)
            buf[0] = OpenEd_SPI_Read_Write(0xFF);
        VDP_drawText("SD: 80clk ok", 0, 21);
    }

    /* Init SD / FatFS */
    res = f_mount(&FatFs, "", 1);

    if (res == FR_OK) {
        VDP_drawText("SD: MOUNT OK !          ", 0, 22);
        MUSIC_init();
    } else {
        char errbuf[26];
        const char *hex = "0123456789ABCDEF";
        errbuf[0]='S'; errbuf[1]='D'; errbuf[2]=':'; errbuf[3]=' ';
        errbuf[4]='E'; errbuf[5]='R'; errbuf[6]='R'; errbuf[7]=' ';
        errbuf[8]='0'; errbuf[9]='x';
        errbuf[10]=hex[(res>>4)&0xF];
        errbuf[11]=hex[res&0xF];
        errbuf[12]=' ';
        const char *msg = "UNKNOWN     ";
        if (res == FR_NOT_READY)     msg = "NOT_READY   ";
        if (res == FR_NO_FILESYSTEM) msg = "NO_FATFS    ";
        if (res == FR_DISK_ERR)      msg = "DISK_ERR    ";
        if (res == FR_INT_ERR)       msg = "INT_ERR     ";
        for (u8 k = 0; k < 12; k++) errbuf[13+k] = msg[k];
        errbuf[25] = 0;
        VDP_drawText(errbuf, 0, 22);
    }

    while (TRUE)
    {
        if (appMode == 1 && lastButton != 0) {
            if (repeatDelay > 0) repeatDelay--;
            else {
                if (lastButton == BUTTON_UP)   Explorer_moveUp();
                if (lastButton == BUTTON_DOWN) Explorer_moveDown();
                repeatDelay = 2;
            }
        }
        SYS_doVBlankProcess();
    }

    return 0;
}

static void joyEvent(u16 joy, u16 changed, u16 state)
{
    if (changed & state & BUTTON_Y) MUSIC_togglePause();
    if (changed & state & BUTTON_Z) MUSIC_stop();

    if (appMode == 1)
    {
        if (changed & state & BUTTON_A) Explorer_select();
        if (changed & state & BUTTON_B) Explorer_goBack();
        if (changed & state & BUTTON_C) {
            fullscreen = !fullscreen;
            if (fullscreen && entries[selectedIndex].type == ENTRY_FILE) {
                char fullPath[128];
                u8 len = strlen(currentPath);
                strncpy(fullPath, currentPath, 128);
                if (len > 0 && currentPath[len-1] != '/') {
                    fullPath[len]='/'; fullPath[len+1]=0; len++;
                }
                strncpy(fullPath + len, entries[selectedIndex].name, 128 - len);
                ROM_parseHeader(fullPath);
            }
            Explorer_draw(&FatFs);
        }
        if (changed & state & BUTTON_UP)   { Explorer_moveUp();   lastButton = BUTTON_UP;   repeatDelay = 10; }
        if (changed & state & BUTTON_DOWN) { Explorer_moveDown(); lastButton = BUTTON_DOWN; repeatDelay = 10; }
        if (!(state & BUTTON_UP) && !(state & BUTTON_DOWN)) {
            lastButton = 0;
            repeatDelay = 0;
        }
        return;
    }

    if (changed & state & BUTTON_A)  UpdateMenu(PosX, PosY);
    if (changed & state & BUTTON_B) {
        asm("move.l (4),%a0\n");
        asm("jmp (%a0)\n");
    }
    if (changed & state & BUTTON_UP) {
        PosY -= 2; if (PosY < 8) PosY = 8;
        UpdateCursor(PosX, PosY);
    }
    if (changed & state & BUTTON_DOWN) {
        PosY += 2; if (PosY > 18) PosY = 18;
        UpdateCursor(PosX, PosY);
    }
}

static void UpdateCursor(int PosX, int PosY)
{
    for (int row = 8; row <= 18; row += 2)
        VDP_drawText(" ", 10, row);
    VDP_drawText(">", PosX, PosY);
}

static void ClearMenu(void)
{
    VDP_drawText("                                        ", 0, 6);
    for (i = 8; i < 24; i++)
        VDP_drawText("                                        ", 0, i);
}

static void ClearMenuFull(void)
{
    VDP_drawText("                                        ", 0, 6);
    for (i = 0; i < 32; i++)
        VDP_drawText("                                        ", 0, i);
}

static void UpdateMenu(int PosX, int PosY)
{
    if (PosX == 10 && PosY == 8)
    {
        OpenEd_Start_ROM();
    }

    if (PosX == 10 && PosY == 10)
    {
        appMode = 1;
        ClearMenuFull();
        Explorer_setFatFs(&FatFs);
        Explorer_loadDir("/");
        Explorer_draw(&FatFs);
    }

    if (PosX == 10 && PosY == 14)
    {
        ClearMenu();
        VDP_drawText("           SYSTEM INFORMATION           ", 0, 6);
        VDP_setTextPalette(0);
        VDP_drawText("Sega Model  :", 2,  9);
        VDP_drawText("Region      :", 2, 11);
        VDP_drawText("Frequency   :", 2, 13);
        VDP_drawText("TMSS        :", 2, 15);
        VDP_drawText("32X Present :", 2, 17);
        VDP_drawText("SCD Present :", 2, 19);
        VDP_setTextPalette(1);
        VDP_drawText("Genesis Model 1", 16, 9);

        unsigned int Region = *(u8 *)0xA10001;
        intToStr(Region, displaychar, 5);
        VDP_drawText(displaychar, 15, 22);

        if      (Region >> 6 == 2) { VDP_drawText("North American", 16, 11); VDP_drawText("60Hz", 16, 13); }
        else if (Region >> 6 == 3) { VDP_drawText("Europe",         16, 11); VDP_drawText("50Hz", 16, 13); }
        else if (Region >> 6 == 0) { VDP_drawText("Japan",          16, 11); VDP_drawText("60Hz", 16, 13); }
        else if (Region >> 6 == 1) { VDP_drawText("Japan",          16, 11); VDP_drawText("50Hz", 16, 13); }

        VDP_drawText((Region & 0x01) ? "YES" : "NO", 16, 15);
        VDP_drawText((Region & 0x10) ? "YES" : "NO", 16, 19);
    }

    /* Flash Erase Test */
    if (PosX == 10 && PosY == 16)
    {
        VDP_drawText("Erasing 0x080000...     ", 0, 24);
        u8 r = OpenEd_Flash_TestErase(0x080000);
        VDP_drawText(r ? "Erase OK !          " : "Erase FAIL !        ", 0, 24);
    }

    /* Flash Write Test — secteur doit être effacé avant ! */
    if (PosX == 10 && PosY == 18)
    {
        VDP_drawText("Writing 0x080000...     ", 0, 24);
        u8 r = OpenEd_Flash_TestWrite(0x080000);
        VDP_drawText(r ? "Write OK !          " : "Write FAIL !        ", 0, 24);
    }
}