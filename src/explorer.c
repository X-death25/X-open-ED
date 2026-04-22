#include "genesis.h"
#include "explorer.h"
#include "ff.h"
#include "rom_parser.h"

char currentPath[128];
ExplorerEntry entries[64];
u8 entryCount    = 0;
u8 selectedIndex = 0;
u8 scrollOffset  = 0;
static FATFS *fatfs_ptr = NULL;
u8 fullscreen = 0;

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

void Explorer_setFatFs(FATFS *fs)
{
    fatfs_ptr = fs;
}

static void drawSDInfo(FATFS *fs)
{
    char buf[12];

    VDP_drawText("                                        ", 0, INFO_LINE);

    /* Espace libre */
    VDP_drawText("Free:", 0, INFO_LINE);
    VDP_setTextPalette(1);
    if (fs->free_clst == 0xFFFFFFFF) {
        VDP_drawText("---", 6, INFO_LINE);
    } else {
        u32 freeKB = (fs->free_clst >> 1) * (fs->csize >> 1);
        u32 freeMB = freeKB / 1024;
        if (freeMB >= 1024) {
            intToStr(freeMB / 1024, buf, 1);
            u8 l = strlen_s(buf);
            buf[l]='G'; buf[l+1]='B'; buf[l+2]=0;
        } else {
            intToStr(freeMB, buf, 1);
            u8 l = strlen_s(buf);
            buf[l]='M'; buf[l+1]='B'; buf[l+2]=0;
        }
        VDP_drawText(buf, 6, INFO_LINE);
    }
    VDP_setTextPalette(0);

    /* Taille totale SD */
    VDP_drawText("SD:", 14, INFO_LINE);
    VDP_setTextPalette(1);
    u32 totalMB = ((u32)(fs->n_fatent - 2) / 2) * fs->csize / 1024;
    if (totalMB >= 1024) {
        intToStr(totalMB / 1024, buf, 1);
        u8 l = strlen_s(buf);
        buf[l]='G'; buf[l+1]='B'; buf[l+2]=0;
    } else {
        intToStr(totalMB, buf, 1);
        u8 l = strlen_s(buf);
        buf[l]='M'; buf[l+1]='B'; buf[l+2]=0;
    }
    VDP_drawText(buf, 18, INFO_LINE);
    VDP_setTextPalette(0);

    /* Chemin courant */
    VDP_drawText("Path:", 26, INFO_LINE);
    VDP_setTextPalette(1);
    VDP_drawText(currentPath, 32, INFO_LINE);
    VDP_setTextPalette(0);
}

static void drawPanelLabels(void)
{
    VDP_setTextPalette(3);
    VDP_drawText(" SD CARD                  ", 0, LABEL_LINE);
    if (fullscreen)
        VDP_drawText(" FILE INFO               ", SEP_COL, LABEL_LINE);
    else
        VDP_drawText("              ", SEP_COL, LABEL_LINE);
    VDP_setTextPalette(0);
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
            VDP_setTextPalette(3);
            VDP_drawText("                          ", 0, line);
            VDP_drawText(">", 0, line);
        }

        /* Longueur max selon le mode */
        u8 maxLen = fullscreen ? 20 : 32;

        if (entries[idx].type == ENTRY_DIR) {
            /* Dossier — tronque entre [ ] */
            char truncName[36];
            u8 nameLen = strlen_s(entries[idx].name);
            if (nameLen <= maxLen - 2)
                strncpy_safe(truncName, entries[idx].name, 36);
            else
                strncpy_safe(truncName, entries[idx].name, maxLen - 1);

            VDP_setTextPalette(1);
            VDP_drawText("[", 2, line);
            VDP_drawText(truncName, 3, line);
            u8 nl = strlen_s(truncName);
            u8 closeBracketCol = fullscreen ? SEP_COL : 38;
            if (3 + nl < closeBracketCol)
                VDP_drawText("]", 3 + nl, line);
            VDP_setTextPalette(0);

        } else {
            /* Fichier — troncature intelligente avec extension */
            char truncName[36];
            u8 nameLen = strlen_s(entries[idx].name);

            if (nameLen <= maxLen) {
                strncpy_safe(truncName, entries[idx].name, 36);
            } else {
                /* Trouve l'extension (dernier '.') */
                s8 dotPos = -1;
                for (s8 j = (s8)nameLen - 1; j >= 0; j--) {
                    if (entries[idx].name[j] == '.') { dotPos = j; break; }
                }
                if (dotPos > 0) {
                    u8 extLen = nameLen - dotPos;
                    strncpy_safe(truncName, entries[idx].name, maxLen - extLen + 1);
                    u8 tl = strlen_s(truncName);
                    strncpy_safe(truncName + tl, entries[idx].name + dotPos, extLen + 1);
                } else {
                    strncpy_safe(truncName, entries[idx].name, maxLen + 1);
                }
            }

            if (idx == selectedIndex) VDP_setTextPalette(3);
            else VDP_setTextPalette(0);
            VDP_drawText(truncName, 2, line);

            /* Taille alignée à droite */
            char sz[8];
            sizeToStr(entries[idx].size, sz);
            u8 slen = strlen_s(sz);
            u8 sizeCol = fullscreen ? (SEP_COL - slen - 1) : (35 - slen);
            if (sizeCol > 2)
                VDP_drawText(sz, sizeCol, line);
        }

        VDP_setTextPalette(0);
    }
}

static void drawRightPanel(void)
{
    ROM_drawInfo();
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
    DIR dir;
    FILINFO fno;
    u8 dirCount  = 0;
    u8 fileCount = 0;

    /* Tableaux temporaires pour trier dossiers d'abord */
    ExplorerEntry tmpDirs[32];
    ExplorerEntry tmpFiles[32];

    entryCount    = 0;
    selectedIndex = 0;
    scrollOffset  = 0;

    strncpy_safe(currentPath, path, 128);

    if (f_opendir(&dir, path) != FR_OK) {
        VDP_drawText("ERR: f_opendir failed   ", 0, 24);
        return -1;
    }

    while ((dirCount + fileCount) < 62)
    {
        if (f_readdir(&dir, &fno) != FR_OK) break;
        if (fno.fname[0] == 0) break;      /* fin de liste */
        if (fno.fname[0] == '.') continue; /* ignore . et .. */

        if (fno.fattrib & AM_DIR)
        {
            if (dirCount < 32) {
                strncpy_safe(tmpDirs[dirCount].name, fno.fname, EXPLORER_NAME_LEN);
                tmpDirs[dirCount].type = ENTRY_DIR;
                tmpDirs[dirCount].size = 0;
                dirCount++;
            }
        }
        else
        {
            if (fileCount < 32) {
                strncpy_safe(tmpFiles[fileCount].name, fno.fname, EXPLORER_NAME_LEN);
                tmpFiles[fileCount].type = ENTRY_FILE;
                tmpFiles[fileCount].size = fno.fsize;
                fileCount++;
            }
        }
    }

    f_closedir(&dir);

    /* Dossiers en premier, puis fichiers */
    for (u8 i = 0; i < dirCount; i++)
        entries[entryCount++] = tmpDirs[i];
    for (u8 i = 0; i < fileCount; i++)
        entries[entryCount++] = tmpFiles[i];

    if (entryCount > 0)
        /* Dossiers en premier, puis fichiers */
    for (u8 i = 0; i < dirCount; i++)
        entries[entryCount++] = tmpDirs[i];
    for (u8 i = 0; i < fileCount; i++)
        entries[entryCount++] = tmpFiles[i];

    /* Parse le premier fichier de la liste */
    if (entryCount > 0) {
        /* Cherche le premier fichier (pas un dossier) */
        u8 firstFile = 0;
        for (u8 i = 0; i < entryCount; i++) {
            if (entries[i].type == ENTRY_FILE) {
                firstFile = i;
                break;
            }
        }
        selectedIndex = firstFile;

        if (entries[firstFile].type == ENTRY_FILE) {
            char fullPath[128];
            u8 len = strlen_s(currentPath);
            strncpy_safe(fullPath, currentPath, 128);
            if (len > 0 && currentPath[len-1] != '/') {
                fullPath[len]='/'; fullPath[len+1]=0; len++;
            }
            strncpy_safe(fullPath + len, entries[firstFile].name, 128 - len);
            if (fullscreen && entries[selectedIndex].type == ENTRY_FILE) 
			{
				ROM_parseHeader(fullPath);
			}
        }
    }

    return entryCount;
}

void Explorer_draw(FATFS *fs)
{
    drawSDInfo(fs);
    drawPanelLabels();
    if (fullscreen) {
        drawSeparator();
        drawRightPanel();
    } else {
        /* Efface la zone droite et le séparateur */
        for (u8 y = LABEL_LINE; y <= STATUS_LINE; y++) {
            VDP_drawText("|", SEP_COL, y);  /* efface séparateur */
            VDP_drawText("             ", RIGHT_COL, y);  /* efface panneau droit */
        }
        /* Efface vraiment le séparateur */
        for (u8 y = LABEL_LINE; y <= STATUS_LINE; y++)
            VDP_drawText(" ", SEP_COL, y);
    }
    drawFileList();
    drawStatusBar();
}

void Explorer_moveDown(void)
{
    if (selectedIndex < entryCount - 1) {
        selectedIndex++;
        if (selectedIndex >= scrollOffset + LIST_LINES) scrollOffset++;
        
        if (entries[selectedIndex].type == ENTRY_FILE) {
            char fullPath[128];
            u8 len = strlen_s(currentPath);
            strncpy_safe(fullPath, currentPath, 128);
            if (len > 0 && currentPath[len-1] != '/') {
                fullPath[len]='/'; fullPath[len+1]=0; len++;
            }
            strncpy_safe(fullPath + len, entries[selectedIndex].name, 128 - len);
            if (fullscreen && entries[selectedIndex].type == ENTRY_FILE) 
			{
				ROM_parseHeader(fullPath);
			}
        }
        
        Explorer_draw(fatfs_ptr);
    }
}

void Explorer_moveUp(void)
{
    if (selectedIndex > 0) {
        selectedIndex--;
        if (selectedIndex < scrollOffset) scrollOffset--;

        if (entries[selectedIndex].type == ENTRY_FILE) {
            char fullPath[128];
            u8 len = strlen_s(currentPath);
            strncpy_safe(fullPath, currentPath, 128);
            if (len > 0 && currentPath[len-1] != '/') {
                fullPath[len]='/'; fullPath[len+1]=0; len++;
            }
            strncpy_safe(fullPath + len, entries[selectedIndex].name, 128 - len);
            if (fullscreen && entries[selectedIndex].type == ENTRY_FILE) 
			{
				ROM_parseHeader(fullPath);
			}
        }  
        Explorer_draw(fatfs_ptr);  
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
        Explorer_draw(fatfs_ptr);
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
    Explorer_draw(fatfs_ptr);
}