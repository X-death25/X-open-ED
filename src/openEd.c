
#include "OpenEd.h"


void OpenEd_DebugLed_ON(void)
{
	*(vu16*) (CTRL_REG) = 0x08;
}

void OpenEd_DebugLed_OFF(void)
{
	*(vu16*) (CTRL_REG) = 0x00;
}