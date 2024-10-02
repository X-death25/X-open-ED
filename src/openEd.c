
#include "OpenEd.h"


void OpenEd_DebugLed_ON(void)
{
	*(vu16*) (CTRL_REG) |=(0x08);
}

void OpenEd_DebugLed_OFF(void)
{
	*(vu16*) (CTRL_REG) &= ~(0x08);
}

void OpenEd_Set_Bank0(void)
{
	*(vu16*) (CTRL_REG) &= ~(0x04);
}

void OpenEd_Set_Bank1(void)
{
	*(vu16*) (CTRL_REG) |=(0x04);
}

void OpenEd_Start_ROM(void)
{
	*(vu16*) (CTRL_REG) &= ~(0x04);
	asm("move.l 0, %sp");
    asm("move.l 4, %a0");
    asm("jmp (%a0)");
}