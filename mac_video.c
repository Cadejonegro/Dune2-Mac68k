/*
 * Stage 10G: OpenDUNE Macintosh 68k video adapter.
 *
 * Provides the original engine with a linear 320x200
 * framebuffer and a VGA-compatible palette.
 *
 * QuickDraw presentation will be connected separately.
 */

#include "../../OpenDUNE/include/types.h"
#include "../../OpenDUNE/src/video/video.h"

static uint8 s_macFrameBuffer[320 * 200];
static uint8 s_macPalette[256 * 3];
static uint16 s_macVideoOffset = 0;

void *Video_GetFrameBuffer(uint16 size)
{
    if (size > sizeof(s_macFrameBuffer))
        return NULL;

    return s_macFrameBuffer;
}

void Video_SetPalette(void *palette, int from, int length)
{
    const uint8 *source = (const uint8 *)palette;
    int i;

    if (source == NULL || from < 0 || length < 0 ||
        from > 256 || length > 256 - from)
        return;

    for (i = 0; i < length * 3; i++)
        s_macPalette[from * 3 + i] = source[i] & 0x3F;
}

void Video_SetOffset(uint16 offset)
{
    s_macVideoOffset = offset;
}

/*
 * Return the linear framebuffer for Macintosh QuickDraw.
 * The caller must not free this memory.
 */
const uint8 *MacVideo_GetPixels(void)
{
    return s_macFrameBuffer;
}
