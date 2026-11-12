#ifndef DUNE_AUDIO_H
#define DUNE_AUDIO_H
#include <Types.h>
#include <Errors.h>
OSErr DuneAudio_Init(void);
OSErr DuneAudio_Play(short soundID);
int DuneAudio_Busy(void);
void DuneAudio_Shutdown(void);
#endif
