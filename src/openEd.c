#include "genesis.h"
#include "OpenEd.h"

unsigned short ed_cfg;

/* Macro protection interruptions pour tous les accès au registre 0xA130E0 */
#define CTRL_WRITE(val) \
    asm("move.w #0x2700, %sr"); \
    *((vu16 *) 0xA130E0) = (val); \
    asm("move.w #0x2300, %sr");

void OpenEd_Init(void)
{
    ed_cfg = 0x04;  /* BANK_GAME, LED off, SS off */
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

void OpenEd_Start_ROM(void)
{
    ed_cfg &= ~CTRL_ROM_BANK;
    CTRL_WRITE(ed_cfg)
    asm("move.l 0, %sp");
    asm("move.l 4, %a0");
    asm("jmp (%a0)");
}

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