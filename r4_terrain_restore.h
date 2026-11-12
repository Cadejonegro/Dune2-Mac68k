#ifndef R4_TERRAIN_RESTORE_H
#define R4_TERRAIN_RESTORE_H
#include <Quickdraw.h>
/* Returns number of 16x16 terrain cells restored, or 240 for full fallback. */
unsigned int R4_RestoreTerrainCells(unsigned char *dst, short stride,
    const unsigned char *terrain, const unsigned char dirty[12][20],
    Boolean forceFull);
#endif
