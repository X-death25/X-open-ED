/**
 *  \file OpenEd.h
 *  \brief Function to control Open Everdrive custom mapper
 *  \author Krikkz , X-death
 *  \date 10/2024
 *
 * Please visit https://github.com/krikzz/open-ed for more info 
 */

#include "types.h"
 
#define CTRL_REG 0xA130F0 //control register


/**
 *  \brief
 *      Turn ON the Debug Led
 */
void OpenEd_DebugLed_ON(void);

/**
 *  \brief
 *      Turn OFF the Debug Led
 */
void OpenEd_DebugLed_OFF(void);

/**
 *  \brief
 *      Activate Bank 0 used for the ROM area
 */
void OpenEd_Set_Bank0(void);

/**
 *  \brief
 *      Activate Bank 1 used for the bios area
 */
void OpenEd_Set_Bank1(void);

/**
 *  \brief
 *      Activate Bank 0 and jump to it for start the ROM
 */
void OpenEd_Start_ROM(void);