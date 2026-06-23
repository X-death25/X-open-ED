#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "genesis.h"

/* Valeurs par défaut */
#define CFG_DEFAULT_AUTO_START_ROM  0

typedef struct {
    u8 auto_start_rom;   /* 0/1 — lance la ROM directement après le flash */
} ConfigData;

extern ConfigData g_config;

void Config_Init(void);          /* À appeler au boot après FatFS */
u8   Config_Load(void);          /* Lit /config.txt → g_config */
u8   Config_Save(void);          /* Écrit g_config → /config.txt */
void Config_SetDefaults(void);   /* Remplit g_config avec valeurs par défaut */

#endif