#include "genesis.h"
#include "sram_manager.h"
#include "OpenEd.h"
#include "config.h"
#include "gfx.h"

/* ------------------------------------------------------------------ */
/* Structures                                                           */
/* ------------------------------------------------------------------ */

typedef struct {
    char name[SD_NAME_MAX_LEN + 1];   /* nom fichier sans extension */
    u8   blocks;                        /* nombre de blocs 8KB utilisés */
} SDSaveEntry;

/* ------------------------------------------------------------------ */
/* État interne                                                         */
/* ------------------------------------------------------------------ */

static FATFS       *g_fs        = NULL;
static u8           g_is_open   = 0;

static SDSaveEntry  sd_entries[SD_LIST_MAX_ENTRIES];
static u16          sd_count    = 0;      /* nombre de saves trouvées */
static u16          sd_selected = 0;      /* index sélectionné dans sd_entries */
static u16          sd_page     = 0;      /* page actuelle (0-indexed) */
static u16          sd_total_pages = 1;

static u8           sram_map[SRAM_TOTAL_BLOCKS];  /* état de chaque bloc (0=vide, 1=plein) */
static u8           sram_used_count = 0;

static char         current_game_name[32];
static char         current_game_file[16];

/* ------------------------------------------------------------------ */
/* Placeholders — à remplacer par la vraie logique plus tard            */
/* ------------------------------------------------------------------ */

static void load_sd_list_placeholder(void)
{
    /* TODO : parcourir /SAVES/ et remplir sd_entries à partir des .srm réels */
    sd_count = 6;
    strcpy(sd_entries[0].name, "sonic3");      sd_entries[0].blocks = 4;
    strcpy(sd_entries[1].name, "monsterw4");   sd_entries[1].blocks = 2;
    strcpy(sd_entries[2].name, "beyond_o");    sd_entries[2].blocks = 3;
    strcpy(sd_entries[3].name, "pgolf");       sd_entries[3].blocks = 1;
    strcpy(sd_entries[4].name, "streets2");    sd_entries[4].blocks = 1;
    strcpy(sd_entries[5].name, "nbajam");      sd_entries[5].blocks = 6;

    sd_total_pages = (sd_count + SD_LIST_MAX_VISIBLE - 1) / SD_LIST_MAX_VISIBLE;
    if (sd_total_pages == 0) sd_total_pages = 1;
}

static void load_sram_map_placeholder(void)
{
    /* TODO : scanner la SRAM réelle et marquer les blocs utilisés */
    for (u8 i = 0; i < SRAM_TOTAL_BLOCKS; i++) sram_map[i] = 0;
    sram_map[0] = 1; sram_map[1] = 1; sram_map[2] = 1; sram_map[3] = 1;
    sram_map[4] = 1; sram_map[5] = 1;
    sram_used_count = 6;
}

static void load_current_game_placeholder(void)
{
    /* TODO : lire depuis le header ROM + config */
    strcpy(current_game_name, "MONSTER WORLD IV");
    strcpy(current_game_file, "monsterw4.bin");
}

/* ------------------------------------------------------------------ */
/* API publique                                                         */
/* ------------------------------------------------------------------ */


void SRAM_Manager_Open(FATFS *fs)
{
    g_fs      = fs;
    g_is_open = 1;

    /* Charge les données (placeholders pour l'instant) */
    load_sd_list_placeholder();
    load_sram_map_placeholder();
    load_current_game_placeholder();

    /* Reset navigation */
    sd_selected = 0;
    sd_page     = 0;

    SRAM_Manager_Draw();
}


static void draw_sheet_block(u8 col_start, u8 row_start, u8 w, u8 h, u8 dest_x, u8 dest_y)
{
    for (u8 row = 0; row < h; row++) {
        for (u8 col = 0; col < w; col++) {
            u16 sheet_index = (row_start + row) * SHEET_WIDTH_TILES + (col_start + col);
            u16 vram_index  = SRAM_TILE_BASE + sheet_index;
            VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, vram_index),   // BG_A -> BG_B
                              dest_x + col, dest_y + row);
        }
    }
}



void SRAM_Manager_Draw(void)
{
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);

    draw_sheet_block(COPY_COL_START,   COPY_ROW_START,   COPY_WIDTH,   COPY_HEIGHT,   16, 5);
    draw_sheet_block(ICON_COL_START,   ICON_ROW_START,   ICON_WIDTH,   ICON_HEIGHT,   16, 13);
    draw_sheet_block(DELETE_COL_START, DELETE_ROW_START, DELETE_WIDTH, DELETE_HEIGHT, 16, 9);
    draw_sheet_block(FORMAT_COL_START, FORMAT_ROW_START, FORMAT_WIDTH, FORMAT_HEIGHT, 16, 17);
	draw_sheet_block(SD_COL_START,      SD_ROW_START,      SD_WIDTH,      SD_HEIGHT,      3, 1);
	draw_sheet_block(SRAM_TAB_COL_START, SRAM_TAB_ROW_START, SRAM_TAB_WIDTH, SRAM_TAB_HEIGHT, 27, 1);
	
	// Bandeau SD
	draw_sheet_block(BANNER_COL_START, BANNER_ROW_START, BANNER_WIDTH, BANNER_HEIGHT, 2, 20);  // bandeau save
	// Bandeau SRAM
	draw_sheet_block(BANNER_COL_START, BANNER_ROW_START, BANNER_WIDTH, BANNER_HEIGHT, 2, 24);  // bandeau cartouche
	

	//
	PAL_setColor(63-16, 0xFFFF);
	VDP_setTextPalette(2);   // 
	//VDP_drawText(current_game_name, 3, 20);
	VDP_drawText("SONIC 3 (8Kb) [1 SLOT]", 3, 21);
	
	VDP_setTextPalette(3);   // 
	//VDP_drawText(current_game_name, 3, 20);
	VDP_drawText("Cartridge : Monster World IV", 3, 25);

	//VDP_setTextPalette(PAL_CART_INFO);   // ex: PAL1, couleur différente pour la ligne cartouche
	//VDP_drawText(current_game_file,5, 22);
	

	 // Grille SD Card
	 
		for (u8 gy = 0; gy < SD_GRID_ROWS; gy++) {
			for (u8 gx = 0; gx < SD_GRID_COLS; gx++) {
				u16 entry_index = gy * SD_GRID_COLS + gx;
				u8 dest_x = 1 + gx * SD_CELL_SPACING_X;
				u8 dest_y = 5 + gy * SD_CELL_SPACING_Y;

				draw_sheet_block(SD_GRID_COL_START, SD_GRID_ROW_START, SD_GRID_WIDTH, SD_GRID_HEIGHT, dest_x, dest_y);

				if (entry_index < sd_count) {
					/* TODO : icône dynamique */
				}
			}
		}
		
	// Grille SRAM
			for (u8 gy = 0; gy < SRAM_GRID_ROWS; gy++) {
				for (u8 gx = 0; gx < SRAM_GRID_COLS; gx++) {
					u8 block_index = gy * SRAM_GRID_COLS + gx;
					u8 dest_x = 27 + gx * SRAM_CELL_SPACING_X;
					u8 dest_y = 6 + gy * SRAM_CELL_SPACING_Y;

					if (block_index < SRAM_TOTAL_BLOCKS && sram_map[block_index]) {
						draw_sheet_block(SRAM_FULL_COL_START, SRAM_FULL_ROW_START, SRAM_FULL_WIDTH, SRAM_FULL_HEIGHT, dest_x, dest_y);
					} else {
						draw_sheet_block(SRAM_EMPTY_COL_START, SRAM_EMPTY_ROW_START, SRAM_EMPTY_WIDTH, SRAM_EMPTY_HEIGHT, dest_x, dest_y);
					}
				}
			}
	
	
}

void SRAM_Manager_HandleInput(u16 joy, u16 changed, u16 state)
{
    if (!g_is_open) return;

    /* Y : retour menu principal */
    if (changed & state & BUTTON_Y) {
        g_is_open = 0;
        return;
    }

    /* Haut/Bas : navigation dans la liste */
    if (changed & state & BUTTON_UP) {
        if (sd_selected > 0) sd_selected--;
        if (sd_selected < sd_page * SD_LIST_MAX_VISIBLE) sd_page--;
        SRAM_Manager_Draw();
        return;
    }
    if (changed & state & BUTTON_DOWN) {
        if (sd_selected < sd_count - 1) sd_selected++;
        if (sd_selected >= (sd_page + 1) * SD_LIST_MAX_VISIBLE) sd_page++;
        SRAM_Manager_Draw();
        return;
    }

    /* Gauche/Droite : changer de page */
    if (changed & state & BUTTON_LEFT) {
        if (sd_page > 0) {
            sd_page--;
            sd_selected = sd_page * SD_LIST_MAX_VISIBLE;
            SRAM_Manager_Draw();
        }
        return;
    }
    if (changed & state & BUTTON_RIGHT) {
        if (sd_page < sd_total_pages - 1) {
            sd_page++;
            sd_selected = sd_page * SD_LIST_MAX_VISIBLE;
            SRAM_Manager_Draw();
        }
        return;
    }

    /* Actions — placeholders pour l'instant */
    if (changed & state & BUTTON_A)     { /* TODO : Backup SRAM -> SD  */ }
    if (changed & state & BUTTON_B)     { /* TODO : Restore SD -> SRAM */ }
    if (changed & state & BUTTON_C)     { /* TODO : Copy save          */ }
    if (changed & state & BUTTON_START) { /* TODO : Format SRAM        */ }
}

u8 SRAM_Manager_IsOpen(void)
{
    return g_is_open;
}