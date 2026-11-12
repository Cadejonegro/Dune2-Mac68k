#include "r4_terrain_restore.h"
#include <string.h>
unsigned int R4_RestoreTerrainCells(unsigned char *dst, short stride,
    const unsigned char *terrain, const unsigned char dirty[12][20],
    Boolean forceFull)
{
    unsigned int count = 0;
    int x, y, row;
    if (dst == NULL || terrain == NULL || dirty == NULL || stride < 320)
        return 0;
    for (y = 0; y < 12; ++y)
        for (x = 0; x < 20; ++x)
            count += dirty[y][x] != 0;
    if (forceFull || count > 160) {
        if (stride == 320) memcpy(dst, terrain, 320UL * 192UL);
        else for (row = 0; row < 192; ++row)
            memcpy(dst + row * stride, terrain + row * 320, 320);
        return 240;
    }
    /* R4-M5B: coalesce adjacent dirty tiles horizontally.  The set
     * of restored pixels is identical to the original per-tile loop,
     * but contiguous cells require one memcpy per row, not per tile. */
    for (y = 0; y < 12; ++y) {
        x = 0;
        while (x < 20) {
            int first, width;
            while (x < 20 && !dirty[y][x]) ++x;
            if (x == 20) break;
            first = x;
            while (x < 20 && dirty[y][x]) ++x;
            width = (x - first) * 16;
            for (row = y * 16; row < y * 16 + 16; ++row)
                memcpy(dst + row * stride + first * 16,
                       terrain + row * 320 + first * 16,
                       (size_t)width);
        }
    }
    return count;
}
