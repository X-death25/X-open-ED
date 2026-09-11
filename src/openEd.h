/**
 *  \file OpenEd.h
 *  \brief Function to control Open Everdrive custom mapper
 *  \author Krikzz , X-death
 *  \date 09/2026
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
/* SRAM hardware layout                                                 */
/* ------------------------------------------------------------------ */
#define SRAM_BASE       0x200000UL  /* base address, GAME bank, SRM_ON=1 */
#define SRAM_SIZE       0x20000UL   /* 128KB real data (ISSI IS62WV1288BLL) */

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
/* GAME TYPE on Bank0                                                    */
/* ------------------------------------------------------------------ */
#define GAME_TYPE_NONE  0x00  /* Bank0 vide (0xFF partout)             */
#define GAME_TYPE_ROM   0x01  /* Jeu sans SRAM                          */
#define GAME_TYPE_SRAM  0x02  /* Jeu avec SRAM ("RA" dans header)      */

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
 *  \brief Detect current Bank0 game type from header, configure SRAM
 *         Reads offset $1B0-$1B1 ("RA" = SRAM present)
 *         Automatically enables/disables SRAM according to detected type
 */
void OpenEd_Game_Init(void);

/**
 *  \brief Get last detected game type
 *  \return GAME_TYPE_NONE / GAME_TYPE_ROM / GAME_TYPE_SRAM
 */
u8 OpenEd_Game_Type(void);

/**
 *  \brief Switch to Bank 0 and jump to ROM start vector
 *         Must run from RAM — cuts Bank1 (BIOS) access
 */
void OpenEd_Start_ROM(void);

/* ------------------------------------------------------------------ */
/* SRAM control                                                         */
/* ------------------------------------------------------------------ */

/**
 *  \brief Enable 128KB SRAM at $200000-$21FFFF
 *         Warning: makes ROM $200000-$3FFFFF unreachable while enabled
 */
void OpenEd_SRAM_Enable(void);

/**
 *  \brief Disable SRAM, restore full 4MB ROM mapping
 */
void OpenEd_SRAM_Disable(void);

/**
 *  \brief Check if SRAM is currently enabled
 *  \return 1 if enabled, 0 otherwise
 */
u8 OpenEd_SRAM_IsEnabled(void);

/**
 *  \brief Read raw data from SRAM into a buffer
 *  \param dst    destination buffer
 *  \param offset logical byte offset within SRAM data (0 to SRAM_SIZE-1)
 *  \param len    number of real data bytes to read
 *
 *  Switches to GAME bank and enables SRAM for the duration of the call,
 *  then restores the previous state. Reads the high byte of each 16-bit
 *  word (68k big-endian, real data lane confirmed by hardware testing).
 *  Output buffer is fully packed (no padding) — NOT directly compatible
 *  with .srm emulator format, use SRAM_DumpToSD_Emu for that.
 */
void OpenEd_SRAM_ReadBlock(u8 *dst, u32 offset, u32 len);

/**
 *  \brief Write raw data from a buffer into SRAM
 *  \param src    source buffer (packed, no padding)
 *  \param offset logical byte offset within SRAM data (0 to SRAM_SIZE-1)
 *  \param len    number of real data bytes to write
 *
 *  Switches to GAME bank and enables SRAM for the duration of the call,
 *  then restores the previous state. Writes to the high byte of each
 *  16-bit word, matching OpenEd_SRAM_ReadBlock.
 */
void OpenEd_SRAM_WriteBlock(const u8 *src, u32 offset, u32 len);

/**
 *  \brief Erase the whole SRAM chip, filling it with 0xFF
 *         Used before a restore to avoid stale data outside the
 *         restored range
 */
void OpenEd_SRAM_Erase(void);

/* ------------------------------------------------------------------ */
/* SRAM — diagnostic / debug helpers                                    */
/* ------------------------------------------------------------------ */

/**
 *  \brief Isolated single-byte SRAM write/read test, result printed
 *         on screen. Diagnostic tool used to track down the GAME-bank
 *         switch requirement — kept for reference, not used in normal
 *         operation
 */
void OpenEd_SRAM_SingleTest(void);

/**
 *  \brief Read count 16-bit words directly from SRAM_BASE + offset,
 *         no byte extraction. Diagnostic tool used to confirm the
 *         real data lane (high byte) before OpenEd_SRAM_ReadBlock was
 *         fixed — kept for reference, not used in normal operation
 */
RAM_SECT NO_INL void OpenEd_SRAM_TestWide(u16 *dst, u32 offset, u16 count);

/**
 *  \brief Dump len bytes starting at startAddr to a raw 16-bit-word
 *         file on SD, no byte filtering. Used to validate GAME-bank
 *         switching across ROM and SRAM in one pass — kept for
 *         reference, not used in normal operation
 *  \param path     destination file path on SD
 *  \param startAddr starting CPU address (e.g. 0x000000 or SRAM_BASE)
 *  \param len      number of bytes to dump (word-aligned)
 *  \return 1 on success, 0 on failure (SD open error)
 */
u8 SRAM_DumpWide(const char *path, u32 startAddr, u32 len);

/* ------------------------------------------------------------------ */
/* SRAM <-> SD backup / restore                                         */
/* ------------------------------------------------------------------ */

/**
 *  \brief Dump SRAM content to a file on SD, in compact format
 *         (packed real data only, no padding — half the size of the
 *         emulator-compatible format). Intended for fast internal
 *         backup/restore, not directly loadable in an emulator
 *  \param path         destination file path on SD (e.g. "/SAVES/game.srm")
 *  \param realDataSize number of real SRAM data bytes to dump
 *  \return 1 on success, 0 on failure (SD open/write error)
 */
u8 SRAM_DumpToSD(const char *path, u32 realDataSize);

/**
 *  \brief Dump SRAM content to a .srm