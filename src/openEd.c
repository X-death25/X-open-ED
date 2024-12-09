
#include "OpenEd.h"

// Put function in .data (RAM) instead of the default .text
#define RAM_SECT __attribute__((section(".ramprog")))
// Avoid function to be inlined by compiler
#define NO_INL __attribute__((noinline))

unsigned short ed_cfg;


RAM_SECT NO_INL void OpenEd_DebugLed_ON(void)
{
	 ed_cfg |= CTRL_LED;
	 *((vu16 *) 0xA130E0) = ed_cfg;
}

RAM_SECT NO_INL void OpenEd_DebugLed_OFF(void)
{
	 ed_cfg &= ~CTRL_LED;
	 *((vu16 *) 0xA130E0) = ed_cfg;
}

RAM_SECT NO_INL void OpenEd_Set_Bank0(void)
{
	ed_cfg &= ~CTRL_ROM_BANK;
	*((vu16 *) 0xA130E0) = ed_cfg;
}

RAM_SECT NO_INL void OpenEd_Set_Bank1(void)
{
	//*(vu16*) (CTRL_REG) |=(0x04);
	ed_cfg |= CTRL_ROM_BANK;
	*((vu16 *) 0xA130E0) = ed_cfg;
}

RAM_SECT NO_INL void OpenEd_Start_ROM(void)
{
	ed_cfg &= ~CTRL_ROM_BANK;
	*((vu16 *) 0xA130E0) = ed_cfg;
	asm("move.l 0, %sp");
    asm("move.l 4, %a0");
    asm("jmp (%a0)");
}

unsigned char OpenEd_SPI_Read(void)
{

    unsigned char ret = 0;

	ed_cfg = ed_cfg | CTRL_SPI_MOSI;
    *((vu16 *) 0xA130E0) = ed_cfg;

	for (unsigned short i = 0; i < 8; i++) {

        *((vu16 *) 0xA130E0) = ed_cfg | CTRL_SPI_MOSI | CTRL_SPI_CLK;
        ret <<= 1;
        ret |= (*((vu16 *) 0xA130E0));
        *((vu16 *) 0xA130E0) = ed_cfg | CTRL_SPI_MOSI;
    }

    return ret;

}

void OpenEd_SPI_Write(unsigned char val)
{

	for (unsigned short i = 0; i < 8; i++) {

        *((vu16 *) 0xA130E0) = ed_cfg | (val & CTRL_SPI_MOSI);
        *((vu16 *) 0xA130E0) = ed_cfg | (val & CTRL_SPI_MOSI) | CTRL_SPI_CLK;
        *((vu16 *) 0xA130E0) = ed_cfg | (val & CTRL_SPI_MOSI);
        val <<= 1;
    }

}

void OpenEd_SPI_Select(unsigned char target) 
{

    ed_cfg &= ~(CTRL_SDC_SS);

    if (target == SPI_SEL_SDC) {
        ed_cfg |= CTRL_SDC_SS;
    } else if (target == SPI_SEL_EXP) {
        ed_cfg |= CTRL_EXP_SS;
    }

    *((vu16 *) 0xA130E0) = ed_cfg;
}