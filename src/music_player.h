#ifndef MUSIC_PLAYER_H
#define MUSIC_PLAYER_H

#include "types.h"

/* Dossier musique sur la SD */
#define MUSIC_ROOT_PATH     "/MUSIC"

/* Taille max fichier music en RAM — 48KB */
#define MUSIC_MAX_SIZE      49152

/* Etats du player */
#define MUSIC_STATE_STOPPED  0
#define MUSIC_STATE_PLAYING  1
#define MUSIC_STATE_PAUSED   2

/* Etat courant */
extern u8 musicState;

/* Chemin du fichier en cours */
extern char currentMusicPath[128];

/**
 * Initialise le music player
 * Doit être appelé une fois après le f_mount
 */
void MUSIC_init(void);

/**
 * Joue un fichier VGM aléatoire depuis /MUSIC/ (récursif)
 * Touche X
 */
void MUSIC_playRandom(void);

/**
 * Pause / Resume
 * Touche Y
 */
void MUSIC_togglePause(void);

/**
 * Stop
 * Touche Z
 */
void MUSIC_stop(void);

/**
 * A appeler dans le while(TRUE) pour le streaming
 * Recharge les buffers si nécessaire
 */
void MUSIC_update(void);

#endif