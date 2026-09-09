/**
 *  \file sram_manager.h
 *  \brief PSX-like SRAM Manager UI
 *  \author X-death
 *  \date 07/2026
 */

#ifndef SRAM_MANAGER_H
#define SRAM_MANAGER_H

#include "genesis.h"
#include "ff.h"

/* ------------------------------------------------------------------ */
/* Constantes                                                           */
/* ------------------------------------------------------------------ */

#define SRAM_TOTAL_BLOCKS    16       /* 128KB / 8KB = 16 blocs */
#define SD_LIST_MAX_VISIBLE  8        /* lignes visibles côté SD */
#define SD_LIST_MAX_ENTRIES  64       /* entrées max stockées en RAM */
#define SD_NAME_MAX_LEN      12       /* longueur d'affichage du nom */
#define SRAM_TILE_BASE       374   

#define COPY_COL_START      8
#define COPY_ROW_START      3
#define COPY_WIDTH          10
#define COPY_HEIGHT          2

#define ICON_COL_START      19
#define ICON_ROW_START       6
#define ICON_WIDTH           10
#define ICON_HEIGHT           2

#define DELETE_COL_START     8
#define DELETE_ROW_START     6
#define DELETE_WIDTH         10
#define DELETE_HEIGHT         2

#define FORMAT_COL_START    19
#define FORMAT_ROW_START     3
#define FORMAT_WIDTH         10
#define FORMAT_HEIGHT         2

#define SD_COL_START    0
#define SD_ROW_START    0
#define SD_WIDTH        10
#define SD_HEIGHT        2

#define SRAM_TAB_COL_START   11
#define SRAM_TAB_ROW_START    0
#define SRAM_TAB_WIDTH        10
#define SRAM_TAB_HEIGHT        2

#define SD_GRID_COL_START   0
#define SD_GRID_ROW_START   7
#define SD_GRID_WIDTH       4
#define SD_GRID_HEIGHT      4

#define SD_GRID_COLS   3
#define SD_GRID_ROWS   3
#define SD_CELL_SPACING_X  5   // à ajuster selon l'espacement réel voulu à l'écran
#define SD_CELL_SPACING_Y  5


#define SRAM_FULL_COL_START    0
#define SRAM_FULL_ROW_START    3
#define SRAM_FULL_WIDTH        2
#define SRAM_FULL_HEIGHT       2

#define SRAM_EMPTY_COL_START   4
#define SRAM_EMPTY_ROW_START   3
#define SRAM_EMPTY_WIDTH       2
#define SRAM_EMPTY_HEIGHT      2

#define SRAM_GRID_COLS   4
#define SRAM_GRID_ROWS   4
#define SRAM_CELL_SPACING_X  3
#define SRAM_CELL_SPACING_Y  3

#define BANNER_COL_START   0
#define BANNER_ROW_START   11
#define BANNER_WIDTH       35
#define BANNER_HEIGHT       3

#define SHEET_WIDTH_TILES   35   // 280px / 8
#define SRAM_TILE_BASE      374

/**
 *  \brief Ouvre le SRAM Manager, prend le contrôle jusqu'au retour utilisateur
 *  \param fs Pointeur vers le FATFS déjà monté
 */
void SRAM_Manager_Open(FATFS *fs);

/**
 *  \brief Traite les événements pad pendant que le manager est ouvert
 *         À appeler depuis le joyEvent global quand appMode == SRAM_MANAGER_MODE
 */
void SRAM_Manager_HandleInput(u16 joy, u16 changed, u16 state);

/**
 *  \brief Redessine intégralement l'écran (utile après une opération)
 */
void SRAM_Manager_Draw(void);

/**
 *  \brief Retourne 1 si le manager est actuellement ouvert
 */
u8 SRAM_Manager_IsOpen(void);

#endif