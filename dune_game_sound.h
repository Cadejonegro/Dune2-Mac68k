#ifndef DUNE_GAME_SOUND_H
#define DUNE_GAME_SOUND_H
#include <Sound.h>
OSErr DuneGameSound_Init(void);
OSErr DuneGameSound_PlayTest(void);
void DuneGameSound_Shutdown(void);
void DuneGameSound_QueueShot(int infantry);
void DuneGameSound_Update(void);
void DuneGameSound_Toggle(void);
int DuneGameSound_IsEnabled(void);
#endif
