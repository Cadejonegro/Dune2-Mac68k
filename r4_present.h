#ifndef R4_PRESENT_H
#define R4_PRESENT_H
#include <Quickdraw.h>
#include <Windows.h>

/* Caller owns the locked source PixMap and all frame timing. */
void R4_PresentFullFrame(PixMapHandle source, WindowPtr window,
                         const Rect *sourceRect, const Rect *destinationRect);
#endif
