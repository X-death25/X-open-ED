#include "genesis.h"
#include "music_player.h"
#include "ff.h"

/* ------------------------------------------------------------------ */
/* Variables                                                            */
/* ------------------------------------------------------------------ */
u8   musicState = MUSIC_STATE_STOPPED;
char currentMusicPath[128];

/* Buffer musique en RAM — 32KB max */
static u8 musicBuf[MUSIC_MAX_SIZE];

/* Seed pseudo-aléatoire */
static u32 randSeed = 1;

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */
static u8 strlen_s(const char *s)
{
    u8 i = 0; while (s[i]) i++; return i;
}

static void strncpy_s(char *dst, const char *src, u8 n)
{
    u8 i = 0;
    while (i < n - 1 && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

/* Pseudo random LCG */
static u16 randNext(u16 max)
{
    randSeed = randSeed * 1664525 + 1013904223;
    if (max == 0) return 0;
    return (u16)(randSeed >> 16) % max;
}

/* Fichier caché ? */
static u8 isMusicFile(const char *name)
{
    return (name[0] != '.') ? 1 : 0;
}

/* Log sur ligne 27 */
static void musicLog(const char *msg)
{
    VDP_drawText("                                        ", 0, 27);
    VDP_drawText(msg, 0, 27);
}

/* Log avec nombre */
static void musicLogNum(const char *msg, u32 val)
{
    char buf[32];
    char num[12];
    u8 i = 0;
    while (msg[i] && i < 20) { buf[i] = msg[i]; i++; }
    intToStr(val, num, 1);
    u8 j = 0;
    while (num[j]) { buf[i++] = num[j++]; }
    buf[i] = 0;
    VDP_drawText("                                        ", 0, 27);
    VDP_drawText(buf, 0, 27);
}

/* ------------------------------------------------------------------ */
/* Reservoir Sampling récursif                                          */
/* ------------------------------------------------------------------ */
static void reservoirScan(const char *path, char *selectedPath, u16 *count)
{
    DIR dir;
    FILINFO fno;
    char subPath[128];

    if (f_opendir(&dir, path) != FR_OK) {
        musicLog("MUS: opendir err");
        return;
    }

    while (1)
    {
        if (f_readdir(&dir, &fno) != FR_OK) break;
        if (fno.fname[0] == 0) break;
        if (fno.fname[0] == '.') continue;

        /* Construit le chemin complet */
        u8 plen = strlen_s(path);
        strncpy_s(subPath, path, 128);
        if (plen > 0 && path[plen-1] != '/') {
            subPath[plen]   = '/';
            subPath[plen+1] = 0;
            plen++;
        }
        strncpy_s(subPath + plen, fno.fname, 128 - plen);

        if (fno.fattrib & AM_DIR) {
            /* Récursion */
            reservoirScan(subPath, selectedPath, count);
        } else if (isMusicFile(fno.fname)) {
            /* Filtre les fichiers > 32KB */
            if (fno.fsize > 0 && fno.fsize <= MUSIC_MAX_SIZE) {
                (*count)++;
                /* Reservoir sampling : prob 1/count */
                if (*count == 1 || randNext(*count) == 0) {
                    strncpy_s(selectedPath, subPath, 128);
                }
            }
        }
    }

    f_closedir(&dir);
}

/* ------------------------------------------------------------------ */
/* API publique                                                          */
/* ------------------------------------------------------------------ */
void MUSIC_init(void)
{
    musicState          = MUSIC_STATE_STOPPED;
    currentMusicPath[0] = 0;
    randSeed = (u32)vtimer + 42;
    
    musicLog("MUS: init ok");
}
void MUSIC_playRandom(void)
{
    MUSIC_stop();
    musicLog("MUS: scanning...");

    char selectedPath[128];
    selectedPath[0] = 0;
    u16 count = 0;

    randSeed ^= (u32)vtimer;
    reservoirScan(MUSIC_ROOT_PATH, selectedPath, &count);
    musicLogNum("MUS: found ", count);

    if (count == 0 || selectedPath[0] == 0) {
        musicLog("MUS: no file found");
        return;
    }

    VDP_drawText("                                        ", 0, 26);
    VDP_drawText(selectedPath, 0, 26);

    /* Ouvre et charge en RAM */
    FIL fil;
    if (f_open(&fil, selectedPath, FA_READ) != FR_OK) {
        musicLog("MUS: f_open err");
        return;
    }

    UINT br;
    FRESULT res = f_read(&fil, musicBuf, MUSIC_MAX_SIZE, &br);
    f_close(&fil);

    if (res != FR_OK || br == 0) {
        musicLog("MUS: f_read err");
        return;
    }

    /* Log taille + magic bytes */
    musicLogNum("MUS: br=", br);
	dly_10ms();
dly_10ms();
dly_10ms();  /* 30ms pour lire */
dly_10ms();
dly_10ms();
dly_10ms();  /* 30ms pour lire */

    char magic[14];
    const char *hex = "0123456789ABCDEF";
    magic[0]='[';
    magic[1]=hex[(musicBuf[0]>>4)&0xF]; magic[2]=hex[musicBuf[0]&0xF]; magic[3]=' ';
    magic[4]=hex[(musicBuf[1]>>4)&0xF]; magic[5]=hex[musicBuf[1]&0xF]; magic[6]=' ';
    magic[7]=hex[(musicBuf[2]>>4)&0xF]; magic[8]=hex[musicBuf[2]&0xF]; magic[9]=' ';
    magic[10]=hex[(musicBuf[3]>>4)&0xF]; magic[11]=hex[musicBuf[3]&0xF];
    magic[12]=']'; magic[13]=0;
    musicLog(magic);

    strncpy_s(currentMusicPath, selectedPath, 128);

    XGM_startPlay(musicBuf);
    musicState = MUSIC_STATE_PLAYING;
    //musicLog("MUS: playing!");
}

void MUSIC_togglePause(void)
{
    if (musicState == MUSIC_STATE_PLAYING) {
        XGM_pausePlay(); 
        musicState = MUSIC_STATE_PAUSED;
        musicLog("MUS: paused");
    } else if (musicState == MUSIC_STATE_PAUSED) {
        XGM_resumePlay();  
        musicState = MUSIC_STATE_PLAYING;
        musicLog("MUS: resumed");
    }
}

void MUSIC_stop(void)
{
    if (musicState != MUSIC_STATE_STOPPED) {
        XGM_stopPlay();
        musicState          = MUSIC_STATE_STOPPED;
        currentMusicPath[0] = 0;
        musicLog("MUS: stopped");
    }
}

void MUSIC_update(void)
{
    /* Rien à faire — pas de streaming, XGM2 gère tout */
}