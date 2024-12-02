/**
 *  \file OpenEd.h
 *  \brief Function to control Open Everdrive custom mapper
 *  \author Krikkz , X-death
 *  \date 10/2024
 *
 * Please visit https://github.com/krikzz/open-ed for more info 
 */

#include "types.h"
 
//#define CTRL_REG        0xA130E0 //control register
#define CTRL_REG        *((vu16 *) 0xA130E0

#define CTRL_SRM_ON     0x01// 4MB ROM ro 2MB ROM + 128KB SRAM
#define CTRL_RESERVED   0x02// unused
#define CTRL_ROM_BANK   0x04// ROM bank select (1=MENU or 0=GAME)
#define CTRL_LED        0x08// led controll
#define CTRL_EXP_SS     0x10// expansion SPI chip select
#define CTRL_SDC_SS     0x20// SD card chip select (SPI)
#define CTRL_SPI_CLK    0x40// SD card clock
#define CTRL_SPI_MOSI   0x80// SD card data input

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