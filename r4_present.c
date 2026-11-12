#include "r4_present.h"

/* R4 M1: isolated, behavior-preserving presentation backend.
 * Keep the known-good full-frame CopyBits path until this passes Basilisk II.
 * The caller must have successfully locked source before entering here.
 */
void R4_PresentFullFrame(PixMapHandle source, WindowPtr window,
                         const Rect *sourceRect, const Rect *destinationRect)
{
    CopyBits((BitMap *)*source,
             (BitMap *)*((CGrafPtr)window)->portPixMap,
             sourceRect, destinationRect, srcCopy, NULL);
}
