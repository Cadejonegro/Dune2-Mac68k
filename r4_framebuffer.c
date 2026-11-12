#include "r4_framebuffer.h"

Boolean R4_FramebufferAcquire(GWorldPtr world, short minimumWidth,
                              R4_Framebuffer *frame)
{
    PixMapHandle pixmap;
    short stride;
    unsigned char *pixels;
    if (frame == NULL) return false;
    frame->pixmap = NULL;
    frame->pixels = NULL;
    frame->stride = 0;
    if (world == NULL || minimumWidth <= 0) return false;
    pixmap = GetGWorldPixMap(world);
    if (pixmap == NULL || !LockPixels(pixmap)) return false;
    stride = (**pixmap).rowBytes & 0x3FFF;
    pixels = (unsigned char *)GetPixBaseAddr(pixmap);
    if (pixels == NULL || stride < minimumWidth || (**pixmap).pixelSize != 8) {
        UnlockPixels(pixmap);
        return false;
    }
    frame->pixmap = pixmap;
    frame->pixels = pixels;
    frame->stride = stride;
    return true;
}

void R4_FramebufferRelease(R4_Framebuffer *frame)
{
    if (frame == NULL) return;
    if (frame->pixmap != NULL) UnlockPixels(frame->pixmap);
    frame->pixmap = NULL;
    frame->pixels = NULL;
    frame->stride = 0;
}
