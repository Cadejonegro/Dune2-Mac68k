#ifndef R4_PARTIAL_PRESENT_H
#define R4_PARTIAL_PRESENT_H
#include <Quickdraw.h>
#include <Windows.h>
/* Returns true when a full-frame fallback was used. */
Boolean R4_PresentCandidate(PixMapHandle source, WindowPtr window,
    const unsigned char cells[13][20], Boolean forceFull);
#endif
