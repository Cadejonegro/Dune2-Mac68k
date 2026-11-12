#ifndef DUNE_EFFECTS_H
#define DUNE_EFFECTS_H
#include <stdio.h>
void DuneEffects_Update(short step,unsigned long elapsed,int voicePending,FILE *logFile);
void DuneEffects_Shutdown(void);
#endif
