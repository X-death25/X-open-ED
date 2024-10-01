/**
 *  \file OpenEd.h
 *  \brief Function to control Open Everdrive custom mapper
 *  \author Krikkz , X-death
 *  \date 10/2024
 *
 * Please visit https://github.com/krikzz/open-ed for more info 
 */

#include "types.h"
 
#define CTRL_REG 0xA130E0 //control register


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
