#include "genesis.h"
#include "ff.h"
#include "config.h"

#define CONFIG_PATH    "/config.txt"
#define CFG_LINE_MAX   64

ConfigData g_config;

/* ------------------------------------------------------------------ */
/* Helpers parser                                                      */
/* ------------------------------------------------------------------ */

/* Trim espaces, tabs, \r, \n des deux côtés — in place */
static char *trim(char *s)
{
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
    char *end = s;
    while (*end) end++;
    while (end > s && (*(end-1) == ' ' || *(end-1) == '\t' ||
                       *(end-1) == '\r' || *(end-1) == '\n')) end--;
    *end = 0;
    return s;
}

/* Convertit une string décimale en u8 — retourne def si parse échoue */
static u8 parse_u8(const char *s, u8 def)
{
    if (!s || !*s) return def;
    u16 val = 0;
    while (*s >= '0' && *s <= '9') {
        val = val * 10 + (*s - '0');
        s++;
    }
    return (val > 255) ? def : (u8)val;
}

/* Compare deux strings, retourne 1 si égales */
static u8 str_eq(const char *a, const char *b)
{
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return (*a == *b);
}

/* Applique une paire clé=valeur dans g_config */
static void apply_kv(const char *key, const char *value)
{
    if (str_eq(key, "auto_start_rom"))
        g_config.auto_start_rom = parse_u8(value, CFG_DEFAULT_AUTO_START_ROM);
    /* Clés inconnues : ignorées silencieusement (compat montante) */
}

/* Parse une ligne — extrait clé/valeur si format key=value */
static void parse_line(char *line)
{
    char *p = trim(line);
    if (!*p || *p == '#' || *p == ';') return;   /* vide ou commentaire */

    char *eq = p;
    while (*eq && *eq != '=') eq++;
    if (*eq != '=') return;                       /* pas de = → ignoré */

    *eq = 0;
    char *key   = trim(p);
    char *value = trim(eq + 1);
    apply_kv(key, value);
}

/* ------------------------------------------------------------------ */
/* API publique                                                        */
/* ------------------------------------------------------------------ */

void Config_SetDefaults(void)
{
    g_config.auto_start_rom = CFG_DEFAULT_AUTO_START_ROM;
}

u8 Config_Load(void)
{
    FIL fil;
    UINT br;
    static char buf[512];   /* config.txt restera petit, 512 octets suffisent */

    if (f_open(&fil, CONFIG_PATH, FA_READ) != FR_OK) return 0;

    if (f_read(&fil, buf, sizeof(buf) - 1, &br) != FR_OK) {
        f_close(&fil);
        return 0;
    }
    f_close(&fil);
    buf[br] = 0;   /* null terminator */

    /* Parse ligne par ligne — split sur \n */
    char *line = buf;
    while (*line) {
        char *end = line;
        while (*end && *end != '\n') end++;
        char saved = *end;
        *end = 0;
        parse_line(line);
        *end = saved;
        if (!*end) break;
        line = end + 1;
    }
    return 1;
}

u8 Config_Save(void)
{
    FIL fil;
    UINT bw;
    char buf[96];

    if (f_open(&fil, CONFIG_PATH, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
        return 0;

    const char *header =
        "# X-Open-ED Configuration File\r\n"
        "# Lines starting with # or ; are comments\r\n"
        "# Format: key=value (no spaces around =, no quotes)\r\n"
        "# Auto-start ROM after flashing (0=no, 1=yes)\r\n";
    f_write(&fil, header, strlen(header), &bw);

    sprintf(buf, "auto_start_rom=%u\r\n", g_config.auto_start_rom);
    f_write(&fil, buf, strlen(buf), &bw);

    f_close(&fil);
    return 1;
}

void Config_Init(void)
{
    Config_SetDefaults();

    if (!Config_Load()) {
        /* Fichier absent → on le crée avec les valeurs par défaut */
        Config_Save();
    }
}