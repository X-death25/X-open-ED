#include "genesis.h"
#include "explorer.h"
#include "ff.h"

char currentPath[128];
ExplorerEntry entries[16];
u8 entryCount    = 0;
u8 selectedIndex = 0;
u8 scrollOffset  = 0;
RomInfo currentRomInfo;

/* ------------------------------------------------------------------ */
/* Helpers string                                                       */
/* ------------------------------------------------------------------ */
static void strncpy_safe(char *dst, const char *src, u8 n)
{
    u8 i = 0;
    while (i < n - 1 && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

static u8 strlen_s(const char *s)
{
    u8 i = 0; while (s[i]) i++; return i;
}

static void sizeToStr(u32 size, char *out)
{
    if (size == 0) { out[0]='0'; out[1]=0; return; }
    if (size >= 1048576) {
        u32 m = size / 1048576;
        intToStr(m, out, 1);
        u8 l = strlen_s(out);
        out[l]='M'; out[l+1]='B'; out[l+2]=0;
    } else if (size >= 1024) {
        u32 k = size / 1024;
        intToStr(k, out, 1);
        u8 l = strlen_s(out);
        out[l]='K'; out[l+1]='B'; out[l+2]=0;
    } else {
        intToStr(size, out, 1);
        u8 l = strlen_s(out);
        out[l]='B'; out[l+1]=0;
    }
}

static u8 getFileType(const char *name)
{
    u8 len = strlen_s(name);
    if (len < 4) return ROM_TYPE_UNKNOWN;
    const char *ext = name + len - 3;
    if ((ext[0]=='B'||ext[0]=='b') &&
        (ext[1]=='I'||ext[1]=='i') &&
        (ext[2]=='N'||ext[2]=='n')) return ROM_TYPE_MD;
    if ((ext[0]=='M'||ext[0]=='m') &&
        (ext[1]=='D'||ext[1]=='d')) return ROM_TYPE_MD;
    if ((ext[0]=='I'||ext[0]=='i') &&
        (ext[1]=='N'||ext[1]=='n') &&
        (ext[2]=='I'||ext[2]=='i')) return ROM_TYPE_CFG;
    return ROM_TYPE_UNKNOWN;
}

/* ------------------------------------------------------------------ */
/* Données dummy                                                        */
/* ------------------------------------------------------------------ */
static void fillDummyRomInfo(u8 idx)
{
    currentRomInfo.romType = getFileType(entries[idx].name);
    currentRomInfo.hasSram = (idx % 3 == 0) ? 1 : 0;
    currentRomInfo.sramSize = currentRomInfo.hasSram ? 65536 : 0;
    if (idx % 3 == 0)      strncpy_safe(currentRomInfo.region, "JPN", 4);
    else if (idx % 3 == 1) strncpy_safe(currentRomInfo.region, "EUR", 4);
    else                   strncpy_safe(currentRomInfo.region, "USA", 4);
    if (entries[idx].type == ENTRY_DIR)
        currentRomInfo.romType = ROM_TYPE_DIR;
}

static void loadDummyRoot(void)
{
    strncpy_safe(entries[0].name,  "GAMES",       13); entries[0].type=ENTRY_DIR;  entries[0].size=0;
    strncpy_safe(entries[1].name,  "SAVES",       13); entries[1].type=ENTRY_DIR;  entries[1].size=0;
    strncpy_safe(entries[2].name,  "MUSIC",       13); entries[2].type=ENTRY_DIR;  entries[2].size=0;
    strncpy_safe(entries[3].name,  "SONIC.BIN",   13); entries[3].type=ENTRY_FILE; entries[3].size=524288;
    strncpy_safe(entries[4].name,  "STREETS.BIN", 13); entries[4].type=ENTRY_FILE; entries[4].size=1048576;
    strncpy_safe(entries[5].name,  "SHINOBI.BIN", 13); entries[5].type=ENTRY_FILE; entries[5].size=524288;
    strncpy_safe(entries[6].name,  "CONTRA.BIN",  13); entries[6].type=ENTRY_FILE; entries[6].size=262144;
    strncpy_safe(entries[7].name,  "THUNDER.BIN", 13); entries[7].type=ENTRY_FILE; entries[7].size=524288;
    strncpy_safe(entries[8].name,  "CASTLEV.BIN", 13); entries[8].type=ENTRY_FILE; entries[8].size=1048576;
    strncpy_safe(entries[9].name,  "PHANTASY.BIN",13); entries[9].type=ENTRY_FILE; entries[9].size=524288;
    strncpy_safe(entries[10].name, "SHINING.BIN", 13); entries[10].type=ENTRY_FILE;entries[10].size=1048576;
    strncpy_safe(entries[11].name, "CONFIG.INI",  13); entries[11].type=ENTRY_FILE;entries[11].size=128;
    entryCount = 12;
}

static void loadDummySubDir(void)
{
    strncpy_safe(entries[0].name, "ACTION",     13); entries[0].type=ENTRY_DIR;  entries[0].size=0;
    strncpy_safe(entries[1].name, "RPG",        13); entries[1].type=ENTRY_DIR;  entries[1].size=0;
    strncpy_safe(entries[2].name, "ECCO.BIN",   13); entries[2].type=ENTRY_FILE; entries[2].size=524288;
    strncpy_safe(entries[3].name, "GOLDEN.BIN", 13); entries[3].type=ENTRY_FILE; entries[3].size=1048576;
    strncpy_safe(entries[4].name, "ALIEN3.BIN", 13); entries[4].type=ENTRY_FILE; entries[4].size=262144;
    entryCount = 5;
}

/* ------------------------------------------------------------------ */
/* Dessin                                                               */
/* ------------------------------------------------------------------ */

/* Layout */
#define SEP_COL      26
#define RIGHT_COL    27
#define RIGHT_WIDTH  13
#define LIST_WIDTH   24
#define HEADER_LINE  0
#define INFO_LINE    1
#define LABEL_LINE   2
#define LIST_START   4
#define LIST_LINES   18
#define STATUS_LINE  26


static void drawSDInfo(void)
{
    FATFS *fs;
    DWORD fre_clust;
    char buf[12];

    VDP_drawText("                                        ", 0, INFO_LINE);

    /* Récupère espace libre via FatFs */
    if (f_getfree("", &fre_clust, &fs) == FR_OK)
    {
        /* Calcul espace libre en KB */
        u32 freeKB  = (fre_clust * fs->csize) / 2;
        /* Calcul espace total en KB */
        u32 totalKB = ((fs->n_fatent - 2) * fs->csize) / 2;

        VDP_drawText("Free:", 0, INFO_LINE);
        VDP_setTextPalette(1);
        intToStr(freeKB, buf, 1);
        VDP_drawText(buf, 6, INFO_LINE);
        VDP_drawText("KB", 6 + strlen_s(buf), INFO_LINE);
        VDP_setTextPalette(0);

        VDP_drawText("Tot:", 17, INFO_LINE);
        VDP_setTextPalette(1);
        intToStr(totalKB, buf, 1);
        VDP_drawText(buf, 22, INFO_LINE);
        VDP_drawText("KB", 22 + strlen_s(buf), INFO_LINE);
        VDP_setTextPalette(0);
    }
    else
    {
        VDP_drawText("Free: ??? Tot: ???", 0, INFO_LINE);
    }

    /* Chemin courant */
    VDP_drawText("Path:", 33, INFO_LINE);
    VDP_setTextPalette(1);
    VDP_drawText(currentPath, 39 - strlen_s(currentPath), INFO_LINE);
    VDP_setTextPalette(0);
}

static void drawPanelLabels(void)
{
    /* Ligne 2 — labels panneaux fond vert via palette 3 */
    VDP_setTextPalette(3);
    VDP_drawText(" SD CARD                  ", 0, LABEL_LINE);
    VDP_drawText(" FILE INFO               ", SEP_COL, LABEL_LINE);
    VDP_setTextPalette(0);

    /* Ligne 3 — séparateur horizontal */
    VDP_drawText("----------------------------------------", 0, 3);
}

static void drawSeparator(void)
{
    for (u8 y = LABEL_LINE; y <= STATUS_LINE - 1; y++)
        VDP_drawText("|", SEP_COL, y);
}

static void drawFileList(void)
{
    for (u8 i = 0; i < LIST_LINES; i++)
    {
        u8 idx  = scrollOffset + i;
        u8 line = LIST_START + i;

        /* Efface la zone gauche */
        VDP_drawText("                          ", 0, line);

        if (idx >= entryCount) continue;

        /* Curseur + surlignage sélection */
        if (idx == selectedIndex) {
            VDP_setTextPalette(2);
            VDP_drawText("                          ", 0, line);
            VDP_drawText(">", 0, line);
        }

        /* Icone + nom */
        if (entries[idx].type == ENTRY_DIR) {
            VDP_setTextPalette(1);
            VDP_drawText("[", 2, line);
            VDP_drawText(entries[idx].name, 3, line);
            u8 nl = strlen_s(entries[idx].name);
            if (3 + nl < SEP_COL) VDP_drawText("]", 3 + nl, line);
        } else {
            if (idx == selectedIndex) VDP_setTextPalette(2);
            else VDP_setTextPalette(0);
            VDP_drawText(entries[idx].name, 2, line);

            /* Taille alignée à droite */
            char sz[8];
            sizeToStr(entries[idx].size, sz);
            u8 slen = strlen_s(sz);
            if (SEP_COL - slen - 1 > 2)
                VDP_drawText(sz, SEP_COL - slen - 1, line);
        }
        VDP_setTextPalette(0);
    }
}

static void drawRightPanel(void)
{
    char buf[16];

    if (entryCount == 0) return;

    ExplorerEntry *e = &entries[selectedIndex];

    /* Efface le panneau droit */
    for (u8 y = LIST_START; y < STATUS_LINE; y++)
        VDP_drawText("             ", RIGHT_COL, y);

    if (currentRomInfo.romType == ROM_TYPE_MD)
    {
        /* Titre */
        VDP_setTextPalette(1);
        VDP_drawText("ROM HEADER", RIGHT_COL, LIST_START);
        VDP_setTextPalette(0);

        /* Type */
        VDP_drawText("Type:", RIGHT_COL, LIST_START + 2);
        VDP_setTextPalette(1);
        VDP_drawText("MD ROM", RIGHT_COL + 5, LIST_START + 2);
        VDP_setTextPalette(0);

        /* Taille */
        VDP_drawText("Size:", RIGHT_COL, LIST_START + 3);
        sizeToStr(e->size, buf);
        VDP_setTextPalette(1);
        VDP_drawText(buf, RIGHT_COL + 5, LIST_START + 3);
        VDP_setTextPalette(0);

        /* Région */
        VDP_drawText("Reg: ", RIGHT_COL, LIST_START + 5);
        VDP_setTextPalette(1);
        VDP_drawText(currentRomInfo.region, RIGHT_COL + 5, LIST_START + 5);
        VDP_setTextPalette(0);

        /* SRAM */
        VDP_drawText("Save:", RIGHT_COL, LIST_START + 6);
        VDP_setTextPalette(currentRomInfo.hasSram ? 1 : 0);
        VDP_drawText(currentRomInfo.hasSram ? "YES" : "NO", RIGHT_COL + 5, LIST_START + 6);
        VDP_setTextPalette(0);

        if (currentRomInfo.hasSram) {
            sizeToStr(currentRomInfo.sramSize, buf);
            VDP_drawText("SRAM:", RIGHT_COL, LIST_START + 7);
            VDP_setTextPalette(1);
            VDP_drawText(buf, RIGHT_COL + 5, LIST_START + 7);
            VDP_setTextPalette(0);
        }
    }
    else if (e->type == ENTRY_DIR)
    {
        VDP_setTextPalette(1);
        VDP_drawText("FOLDER", RIGHT_COL, LIST_START + 2);
        VDP_setTextPalette(0);
        VDP_drawText("Press A", RIGHT_COL, LIST_START + 4);
        VDP_drawText("to open", RIGHT_COL, LIST_START + 5);
    }
    else
    {
        VDP_setTextPalette(0);
        VDP_drawText("Type:", RIGHT_COL, LIST_START + 2);
        VDP_setTextPalette(1);
        const char *t = currentRomInfo.romType == ROM_TYPE_CFG ? "CONFIG" : "FILE";
        VDP_drawText(t, RIGHT_COL + 5, LIST_START + 2);
        VDP_setTextPalette(0);
        VDP_drawText("Size:", RIGHT_COL, LIST_START + 3);
        sizeToStr(e->size, buf);
        VDP_setTextPalette(1);
        VDP_drawText(buf, RIGHT_COL + 5, LIST_START + 3);
        VDP_setTextPalette(0);
    }
}

static void drawStatusBar(void)
{
    VDP_drawText("                                        ", 0, STATUS_LINE);
    VDP_setTextPalette(1);
    VDP_drawText("A", 0, STATUS_LINE);
    VDP_setTextPalette(0);
    VDP_drawText(":Launch ", 1, STATUS_LINE);
    VDP_setTextPalette(1);
    VDP_drawText("B", 9, STATUS_LINE);
    VDP_setTextPalette(0);
    VDP_drawText(":Back ", 10, STATUS_LINE);
    VDP_setTextPalette(1);
    VDP_drawText("C", 16, STATUS_LINE);
    VDP_setTextPalette(0);
    VDP_drawText(":Info ", 17, STATUS_LINE);
    VDP_setTextPalette(1);
    VDP_drawText("START", 23, STATUS_LINE);
    VDP_setTextPalette(0);
    VDP_drawText(":Menu", 28, STATUS_LINE);

    /* Compteur à droite */
    char cnt[8];
    intToStr(entryCount, cnt, 3);
    u8 l = strlen_s(cnt);
    cnt[l]=' '; cnt[l+1]='i'; cnt[l+2]='t'; cnt[l+3]='e';
    cnt[l+4]='m'; cnt[l+5]='s'; cnt[l+6]=0;
    VDP_drawText("                                        ", 0, 25);
	VDP_drawText(cnt, 40 - strlen_s(cnt), 25);  /* aligné à droite ligne 25 */
}

/* ------------------------------------------------------------------ */
/* API publique                                                         */
/* ------------------------------------------------------------------ */
s8 Explorer_loadDir(const char *path)
{
    entryCount    = 0;
    selectedIndex = 0;
    scrollOffset  = 0;
    strncpy_safe(currentPath, path, 128);

    if (strlen_s(path) <= 1) loadDummyRoot();
    else                     loadDummySubDir();

    fillDummyRomInfo(0);
    return entryCount;
}

void Explorer_draw(void)
{
    drawSDInfo();
    drawPanelLabels();
    drawSeparator();
    drawFileList();
    drawRightPanel();
    drawStatusBar();
}

void Explorer_moveDown(void)
{
    if (selectedIndex < entryCount - 1) {
        selectedIndex++;
        if (selectedIndex >= scrollOffset + LIST_LINES) scrollOffset++;
        fillDummyRomInfo(selectedIndex);
        Explorer_draw();
    }
}

void Explorer_moveUp(void)
{
    if (selectedIndex > 0) {
        selectedIndex--;
        if (selectedIndex < scrollOffset) scrollOffset--;
        fillDummyRomInfo(selectedIndex);
        Explorer_draw();
    }
}

u8 Explorer_select(void)
{
    if (entryCount == 0) return 0;
    if (entries[selectedIndex].type == ENTRY_DIR) {
        char newPath[128];
        u8 len = strlen_s(currentPath);
        strncpy_safe(newPath, currentPath, 128);
        if (len > 0 && currentPath[len-1] != '/') {
            newPath[len]='/'; newPath[len+1]=0; len++;
        }
        strncpy_safe(newPath + len, entries[selectedIndex].name, 128 - len);
        Explorer_loadDir(newPath);
        Explorer_draw();
        return 1;
    }
    return 0;
}

void Explorer_goBack(void)
{
    char newPath[128];
    strncpy_safe(newPath, currentPath, 128);
    s16 lastSlash = -1;
    u8 len = strlen_s(newPath);
    for (s16 j = len - 1; j >= 0; j--) {
        if (newPath[j] == '/') { lastSlash = j; break; }
    }
    if (lastSlash <= 0) Explorer_loadDir("/");
    else { newPath[lastSlash] = 0; Explorer_loadDir(newPath); }
    Explorer_draw();
}