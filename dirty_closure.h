#ifndef R4_M5C_B2_DIRTY_CLOSURE_H
#define R4_M5C_B2_DIRTY_CLOSURE_H
#define B2_COLS 20
#define B2_ROWS 12
#define B2_SLOTS 102
typedef struct { short left,top,right,bottom; unsigned char valid; } B2Bounds;
/* Expand a dirty tile mask to include the full footprints of any overlapping
 * units. Return number of newly marked cells. Fixed point is essential:
 * expanding one sprite can touch another sprite's footprint. */
unsigned B2_ExpandDirty(unsigned char dirty[B2_ROWS][B2_COLS],
                        const B2Bounds bounds[B2_SLOTS],
                        unsigned char redraw[B2_SLOTS]);
#endif
