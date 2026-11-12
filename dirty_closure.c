#include "dirty_closure.h"
#include <string.h>
static int intersects(const unsigned char d[B2_ROWS][B2_COLS],const B2Bounds *b)
{
    int x,y,l,t,r,bt;
    if(!b->valid) return 0;
    l=b->left<0?0:b->left; t=b->top<0?0:b->top;
    r=b->right>320?320:b->right; bt=b->bottom>192?192:b->bottom;
    if(r<=l||bt<=t) return 0;
    for(y=t/16;y<=(bt-1)/16;y++)
        for(x=l/16;x<=(r-1)/16;x++) if(d[y][x]) return 1;
    return 0;
}
static unsigned mark(unsigned char d[B2_ROWS][B2_COLS],const B2Bounds *b)
{
    int x,y,l,t,r,bt; unsigned added=0;
    l=b->left<0?0:b->left; t=b->top<0?0:b->top;
    r=b->right>320?320:b->right; bt=b->bottom>192?192:b->bottom;
    if(r<=l||bt<=t) return 0;
    for(y=t/16;y<=(bt-1)/16;y++)
        for(x=l/16;x<=(r-1)/16;x++) if(!d[y][x]) {d[y][x]=1;added++;}
    return added;
}
unsigned B2_ExpandDirty(unsigned char dirty[B2_ROWS][B2_COLS],
                        const B2Bounds bounds[B2_SLOTS],
                        unsigned char redraw[B2_SLOTS])
{
    unsigned i,added=0; int changed;
    memset(redraw,0,B2_SLOTS);
    do {
        changed=0;
        for(i=0;i<B2_SLOTS;i++) {
            if(redraw[i]||!intersects(dirty,&bounds[i])) continue;
            redraw[i]=1;
            added+=mark(dirty,&bounds[i]);
            changed=1;
        }
    } while(changed);
    return added;
}
