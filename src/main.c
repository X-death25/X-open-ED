#include "genesis.h"

// preserve compatibility with old resources name
#define logo_lib sgdk_logo
#define font_lib font_default
#define font_pal_lib font_pal_default

// Put function in .data (RAM) instead of the default .text
#define RAM_SECT __attribute__((section(".ramprog")))

// GFX part

#include "gfx.h"

// Open-Everdrive custom Library

#include "OpenEd.h"

// FatFS Library

#include "ffconf.h" /* Declarations of FatFs Config File */
#include "ff.h"		/* Declarations of FatFs API */

// Variable part

volatile int PosX=10;
volatile int PosY=8;
static unsigned long i=0;
static char displaychar[4];

// Joy Events

static void handleInput();
static void joyEvent(u16 joy, u16 changed, u16 state);

// BIOS Function Menu part

static void UpdateCursor(int PosX,int PosY);
static void ClearMenu();
static void UpdateMenu(int PosX,int PosY);

int main(bool hardReset)
{
    u16 ind;
    u16 idx1;
    u16 idx2;

    // disable interrupt when accessing VDP
    SYS_disableInts();
    // initialization
    VDP_setScreenWidth320();

    // init Joypad driver

    //JOY_init();
    JOY_setEventHandler(joyEvent);


    // Set correct Pal for main menu Tileset

    PAL_setColors(0, (u16*)main_title.palette->data,16, CPU);

    // Set correct VRAM addr for main menu Tileset

    ind = TILE_USER_INDEX;
    ind += main_title.tileset->numTile;
    idx1 = ind;
    ind += main_bottom.tileset->numTile;
    idx2 = ind;


    // draw backgrounds
    VDP_drawImageEx(BG_A, &main_title, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, idx1), 0, 1, FALSE, DMA);
    VDP_drawImageEx(BG_A, &main_bottom, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, idx2), 0, 20, FALSE, DMA);

    // Display Menu

    VDP_drawText(">", 10, 8);
    VDP_drawText("Start Game", 12, 8);
    VDP_drawText("SD Card Explorer", 12, 10);
    VDP_drawText("SRAM Manager", 12, 12);
    VDP_drawText("Hardware Info", 12, 14);
    VDP_drawText("Options", 12, 16); // SPI driver , Auto backup save , option file
    VDP_drawText("Credits", 12, 18);

    PosX=10;
    PosY=8;
	
	// Copy critical function to RAM
	
	//memcpy((unsigned char *)Start_RAM_Wait, (unsigned char *)RAM_Wait, sizeof(RAM_Wait));

    // VDP process done, we can re enable interrupts
    SYS_enableInts();

    // just to monitor frame CPU usage
    SYS_showFrameLoad(TRUE);

    while(TRUE)
    {
        // Read Joypad states
        //handleInput();

        // always call this method at the end of the frame
        SYS_doVBlankProcess();
    }

    return 0;
}

static void handleInput()
{
    u16 value = JOY_readJoypad(JOY_1);

}

static void joyEvent(u16 joy, u16 changed, u16 state)
{


    if (changed & state & BUTTON_A)
    {
        UpdateMenu(PosX,PosY);
    }

    if (changed & state & BUTTON_B)
    {
        asm("move.l (4),%a0\n"); // Do a soft Reset
        asm("jmp (%a0)\n");
    }

    if (changed & state & BUTTON_UP)
    {
        PosY=PosY-2;
        if (PosY< 8)
        {
            PosY=8;
        }
        UpdateCursor(PosX,PosY);
    }

    if (changed & state & BUTTON_DOWN)
    {
        PosY=PosY+2;
        if (PosY > 18)
        {
            PosY=18;
        }
        UpdateCursor(PosX,PosY);

    }

    if (changed & state & BUTTON_LEFT)
    {
        //PosX=PosX-13;
        //if (PosX< 1){PosX=1;}
        //UpdateCursor(PosX,PosY);

    }

    if (changed & state & BUTTON_RIGHT)
    {
        //PosX=PosX+13;
        //if (PosX > 27){PosX=27;}
        //UpdateCursor(PosX,PosY);
    }

    if (changed & state & BUTTON_UP & ( BUTTON_DOWN || BUTTON_RIGHT || BUTTON_LEFT ) )
    {
        //UpdateCursor(PosX,PosY);
    }

}

static void UpdateCursor(int PosX,int PosY)
{

    // Clean Cursor position

    VDP_drawText(" ",10,8);
    VDP_drawText(" ",10,10);
    VDP_drawText(" ",10,12);
    VDP_drawText(" ",10,14);
    VDP_drawText(" ",10,16);
    VDP_drawText(" ",10,18);

    // Set Cursor position

    VDP_drawText(">",PosX,PosY);
}
static void ClearMenu()
{

    VDP_drawText("                                        ",0,6);
    for ( i = 8; i <24; i++)
    {
        VDP_drawText("                                        ",0,i);
    }

}


static void UpdateMenu(int PosX,int PosY)
{
	
	if ( PosX == 10 && PosY == 8 ) ////// Start ROM //////
    {
	     OpenEd_Start_ROM();
	}

    if ( PosX == 10 && PosY == 14 ) ////// SYSTEM INFORMATION //////
    {
        // Display Menu
        ClearMenu();
        VDP_drawText("           SYSTEM INFORMATION           ",0,6);
        VDP_setTextPalette(0);
        VDP_drawText("Sega Model  :",2,9);
        VDP_drawText("Region      :",2,11);
        VDP_drawText("Frequency   :",2,13);
        VDP_drawText("TMSS        :",2,15);
        VDP_drawText("32X Present :",2,17);
        VDP_drawText("SCD Present :",2,19);
        VDP_setTextPalette(1);
        VDP_drawText("Genesis Model 1",16,9);
        unsigned int Region = *(u8 *)0xA10001;
        // Convert register value to char character
        intToStr(Region,displaychar, 5);
        VDP_drawText(displaychar,15,22);
        // Display Result
        if ( Region >> 6 == 2)
        {
            VDP_drawText("North American",16,11);
            VDP_drawText("60Hz",16,13);
        }
        if ( Region >> 6 == 3)
        {
            VDP_drawText("Europe",16,11);
            VDP_drawText("50Hz",16,13);
        }
        if ( Region >> 6 == 0)
        {
            VDP_drawText("Japan",16,11);
            VDP_drawText("60Hz",16,13);
        }
        if ( Region >> 6 == 1)
        {
            VDP_drawText("Japan",16,11);
            VDP_drawText("50Hz",16,13);
        }
        if ( (Region & 0x01) == 1)
        {
            VDP_drawText("YES",16,15);   // TMSS Present
        }
        else
        {
            VDP_drawText("NO",16,15);
        }
        ///	if ( MARS == 0x48){VDP_drawText("YES",16,17);} else {VDP_drawText("NO",16,17);} // 32X Connected
        if ( (Region & 0x10) == 1)
        {
            VDP_drawText("YES",16,19);   // Sega-CD connected
        }
        else
        {
            VDP_drawText("NO",16,19);
        }
    }
	
	if ( PosX == 10 && PosY == 16 ) ////// OPTIONS //////
    {
	     OpenEd_DebugLed_ON();
	}
	
	if ( PosX == 10 && PosY == 18 ) ////// CREDITS //////
    {
		 OpenEd_DebugLed_OFF();
	}
}
