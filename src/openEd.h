/**
 *  \file OpenEd.h
 *  \brief Function to control Open Everdrive custom mapper
 *  \author Krikzz , X-death
 *  \date 10/2024
 *
 * Please visit https://github.com/krikzz/open-ed for more info
 */
 
#ifndef RAM_SECT
#define RAM_SECT __attribute__((section(".ramprog")))
#endif
#ifndef NO_INL
#define NO_INL   __attribute__((noinline))
#endif

#ifndef OPENED_H
#define OPENED_H

#include "types.h"

/* ------------------------------------------------------------------ */
/* Registre de contrôle                                                 */
/* ------------------------------------------------------------------ */
#define CTRL_REG        *((vu16 *) 0xA130E0)

/* Bits écriture */
#define CTRL_SRM_ON     0x01  /* 4MB ROM -> 2MB ROM + 128KB SRAM      */
#define CTRL_RESERVED   0x02  /* unused                                */
#define CTRL_ROM_BANK   0x04  /* ROM bank select (1=MENU / 0=GAME)    */
#define CTRL_LED        0x08  /* LED control                           */
#define CTRL_EXP_SS     0x10  /* expansion SPI chip select (inverted)  */
#define CTRL_SDC_SS     0x20  /* SD card chip select (inverted)        */
#define CTRL_SPI_CLK    0x40  /* SD card clock                         */
#define CTRL_SPI_MOSI   0x80  /* SD card data input                    */

/* Bits lecture */
#define CTRL_SPI_MISO   0x01  /* SD card data output                   */

/* ------------------------------------------------------------------ */
/* SPI targets                                                          */
/* ------------------------------------------------------------------ */
#define SPI_SEL_OFF     0
#define SPI_SEL_SDC     1
#define SPI_SEL_EXP     2

/* ------------------------------------------------------------------ */
/* Types flash                                                          */
/* ------------------------------------------------------------------ */
#define FLASH_TYPE_UNK  0x00  /* inconnu                               */
#define FLASH_TYPE_RAM  0x01  /* RAM (dev boards)                      */
#define FLASH_TYPE_M29  0x02  /* Flash M29 series (standard)           */

/* ------------------------------------------------------------------ */
/* Mapper / contrôle général                                            */
/* ------------------------------------------------------------------ */

/**
 *  \brief Init OpenEd mapper — doit être appelé en premier
 */
void OpenEd_Init(void);

/**
 *  \brief Turn ON the Debug LED
 */
void OpenEd_DebugLed_ON(void);

/**
 *  \brief Turn OFF the Debug LED
 */
void OpenEd_DebugLed_OFF(void);

/**
 *  \brief Activate Bank 0 (ROM/GAME area)
 */
void OpenEd_Set_Bank0(void);

/**
 *  \brief Activate Bank 1 (BIOS/MENU area)
 */
void OpenEd_Set_Bank1(void);

/**
 *  \brief Switch to Bank 0 and jump to ROM start vector
 *         Must run from RAM — cuts Bank1 (BIOS) access
 */
void OpenEd_Start_ROM(void);

/* ------------------------------------------------------------------ */
/* SPI                                                                  */
/* ------------------------------------------------------------------ */

/**
 *  \brief Select SPI target
 *  \param target SPI_SEL_OFF / SPI_SEL_SDC / SPI_SEL_EXP
 */
void OpenEd_SPI_Select(unsigned char target);

/**
 *  \brief Read one byte from SPI bus (MOSI idle high)
 */
unsigned char OpenEd_SPI_Read(void);

/**
 *  \brief Read one byte using auto-CLK hardware feature
 *         Faster than OpenEd_SPI_Read — use for bulk data reads
 */
unsigned char OpenEd_SPI_ReadFast(void);

/**
 *  \brief Write one byte to SPI bus
 */
void OpenEd_SPI_Write(unsigned char val);

/**
 *  \brief Full-duplex SPI — write and read simultaneously
 *         Equivalent to spi_rw() in Krikzz original code
 */
unsigned char OpenEd_SPI_Read_Write(unsigned char val);

/* ------------------------------------------------------------------ */
/* Flash                                                                */
/* ------------------------------------------------------------------ */

/**
 *  \brief Detect and init flash chip (RAM or M29 series)
 *         Must be called after OpenEd_Init()
 */
void OpenEd_Flash_Init(void);

/**
 *  \brief Erase a 64KB sector at given address
 *  \param addr  Start address of sector (must be 64KB aligned)
 *  \param wait_rdy  Reserved — erase always waits via data poll
 */
void OpenEd_Flash_Erase64K(u32 addr, u8 wait_rdy);

/**
 *  \brief Write len bytes from src to dst (flash address)
 *  \param src  Source buffer in RAM
 *  \param dst  Destination address in flash (Bank0)
 *  \param len  Size in bytes (must be multiple of 2)
 */
void OpenEd_Flash_Write(u16 *src, u16 *dst, u32 len);

/**
 *  \brief Return detected flash type
 *  \return FLASH_TYPE_UNK / FLASH_TYPE_RAM / FLASH_TYPE_M29
 */
u8 OpenEd_Flash_Type(void);

/**
 *  \brief Test erase — erases sector and verifies 0xFF
 *  \param addr  Sector address to test
 *  \return 1 if OK, 0 if FAIL
 */
u8 OpenEd_Flash_TestErase(u32 addr);

/**
 *  \brief Test write — writes 0xAAAA and verifies
 *  \param addr  Sector address to test (must be erased first)
 *  \return 1 if OK, 0 if FAIL
 */
u8 OpenEd_Flash_TestWrite(u32 addr);

/**
 *  \brief Flash a ROM file from SD card to flash memory
 *         Erases required 64KB sectors then writes file chunk by chunk
 *         Displays a progress bar on screen during operation
 *  \param path     Full path to the ROM file on SD card
 *  \param romSize  Size of the ROM in bytes (from FAT filesystem)
 *  \return 1 if OK, 0 if FAIL (file open error)
 */
 
RAM_SECT NO_INL u8 ROM_flashFromSD(const char *path, u32 romSize);

#endif /* OPENED_H */