
#include "OpenEd.h"

// Put function in .data (RAM) instead of the default .text
#define RAM_SECT __attribute__((section(".ramprog")))
// Avoid function to be inlined by compiler
#define NO_INL __attribute__((noinline))

unsigned short ed_cfg;


RAM_SECT NO_INL void OpenEd_DebugLed_ON(void)
{
	//*(vu16*) (CTRL_REG) |=(0x08);
	 ed_cfg |= CTRL_LED;
	 *((vu16 *) 0xA130E0) = ed_cfg;
}

RAM_SECT NO_INL void OpenEd_DebugLed_OFF(void)
{
	//*(vu16*) (CTRL_REG) &= ~(0x08);
	 ed_cfg &= ~CTRL_LED;
	 *((vu16 *) 0xA130E0) = ed_cfg;
}

RAM_SECT NO_INL void OpenEd_Set_Bank0(void)
{
	//*(vu16*) (CTRL_REG) &= ~(0x04);
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