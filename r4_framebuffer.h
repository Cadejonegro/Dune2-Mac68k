#ifndef R4_FRAMEBUFFER_H
#define R4_FRAMEBUFFER_H
#include <Quickdraw.h>

typedef struct R4_Framebuffer {
    PixMapHandle pixmap;
    unsigned char *pixels;
    short stride;
} R4_Framebuffer;

/* Acquires one locked 8-bit GWorld framebuffer; caller must release. */
Boolean R4_FramebufferAcquire(GWorldPtr world, short minimumWidth,
                              R4_Framebuffer *frame);
void R4_FramebufferRelease(R4_Framebuffer *frame);
#endif
