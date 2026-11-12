#include "r4_partial_present.h"
#include "r4_present.h"
#include <string.h>

/* R4-M3B: bounded, row-run CopyBits. Never compares pixel history.
 * A 13x20 coverage map includes the eight-pixel bottom strip.
 * When fragmented, prefer the proven full-frame transfer. */
#define R4_MAX_RECTS 8
#define R4_MAX_CELLS 80
Boolean R4_PresentCandidate(PixMapHandle source, WindowPtr window,
    const unsigned char cells[13][20], Boolean forceFull)
{
    Rect fullSrc, fullDst;
    Rect src[R4_MAX_RECTS], dst[R4_MAX_RECTS];
    unsigned int n = 0, count = 0;
    int y, x, first, end, height;
    SetRect(&fullSrc, 0, 0, 320, 200);
    SetRect(&fullDst, 20, 20, 340, 220);
    if (!forceFull && cells != NULL) {
        for (y = 0; y < 13; ++y)
            for (x = 0; x < 20; ++x)
                if (cells[y][x]) ++count;
        if (count > R4_MAX_CELLS) forceFull = true;
    } else forceFull = true;
    if (!forceFull) {
        for (y = 0; y < 13 && !forceFull; ++y) {
            height = (y == 12) ? 200 : (y + 1) * 16;
            x = 0;
            while (x < 20) {
                while (x < 20 && !cells[y][x]) ++x;
                if (x >= 20) break;
                first = x;
                while (x < 20 && cells[y][x]) ++x;
                end = x;
                if (n >= R4_MAX_RECTS) { forceFull = true; break; }
                SetRect(&src[n], first * 16, y * 16, end * 16, height);
                SetRect(&dst[n], first * 16 + 20, y * 16 + 20,
                        end * 16 + 20, height + 20);
                ++n;
            }
        }
    }
    if (forceFull) {
        R4_PresentFullFrame(source, window, &fullSrc, &fullDst);
        return true;
    }
    for (x = 0; x < (int)n; ++x)
        CopyBits((BitMap *)*source,
                 (BitMap *)*((CGrafPtr)window)->portPixMap,
                 &src[x], &dst[x], srcCopy, NULL);
    return false;
}
