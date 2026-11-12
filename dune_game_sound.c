/* R3D7S2: two preloaded original Stage16M11 effects, event-driven playback. */
#include "dune_game_sound.h"
#include <OSUtils.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define FX_RATE 22050UL
#define FX_COUNT 2
#define FX_QUEUE_CAP 8
#define FX_MIN_GAP_TICKS 2UL

typedef struct Fx {
    unsigned long offset, length;
    SoundHeader *header;
} Fx;
static Fx s_fx[FX_COUNT] = {
    {215285UL, 2823UL, NULL}, /* GUNSHOT */
    {198925UL, 16360UL, NULL} /* BLASTER */
};
static SndChannelPtr s_channel = NULL;
static unsigned long s_endTick = 0;
static unsigned long s_lastStart = 0;
static unsigned char s_pending[FX_QUEUE_CAP];
static unsigned char s_read = 0, s_write = 0, s_count = 0;
static int s_enabled = 1;
static int s_ready = 0;

static void ClearQueue(void) { s_read = s_write = s_count = 0; }

OSErr DuneGameSound_Init(void) {
    FILE *f;
    OSErr err;
    int i;
    if (s_ready) return noErr;
    f = fopen("EFFECTS8.DAT", "rb");
    if (!f) return fnfErr;
    for (i = 0; i < FX_COUNT; ++i) {
        Fx *fx = &s_fx[i];
        fx->header = (SoundHeader *)malloc(sizeof(SoundHeader) + fx->length);
        if (!fx->header) { fclose(f); DuneGameSound_Shutdown(); return memFullErr; }
        memset(fx->header, 0, sizeof(SoundHeader) + fx->length);
        if (fseek(f, (long)fx->offset, SEEK_SET) != 0 ||
            fread(fx->header->sampleArea, 1, fx->length, f) != fx->length) {
            fclose(f); DuneGameSound_Shutdown(); return eofErr;
        }
        fx->header->samplePtr = NULL;
        fx->header->length = fx->length;
        fx->header->sampleRate = FX_RATE << 16;
        fx->header->encode = stdSH;
        fx->header->baseFrequency = 60;
    }
    fclose(f);
    err = SndNewChannel(&s_channel, sampledSynth, 0, NULL);
    if (err != noErr) { s_channel = NULL; DuneGameSound_Shutdown(); return err; }
    s_ready = 1;
    return noErr;
}

static OSErr PlayNow(int id) {
    SndCommand cmd;
    OSErr err;
    unsigned long now;
    if (!s_ready || !s_enabled || id < 0 || id >= FX_COUNT) return fnfErr;
    now = TickCount();
    if ((long)(now - s_endTick) < 0) return noErr;
    cmd.cmd = quietCmd; cmd.param1 = 0; cmd.param2 = 0;
    err = SndDoImmediate(s_channel, &cmd);
    if (err != noErr) return err;
    cmd.cmd = bufferCmd; cmd.param1 = 0; cmd.param2 = (long)s_fx[id].header;
    err = SndDoCommand(s_channel, &cmd, false);
    if (err == noErr) {
        s_endTick = now + (s_fx[id].length * 60UL + FX_RATE - 1) / FX_RATE + 2;
        s_lastStart = now;
    }
    return err;
}

OSErr DuneGameSound_PlayTest(void) {
    if (!s_enabled) return noErr;
    /* Manual test never interrupts a playing sample. */
    return PlayNow(0);
}

void DuneGameSound_QueueShot(int infantry) {
    if (!s_ready || !s_enabled) return;
    /* Drop oldest excess combat cues instead of delaying gameplay. */
    if (s_count == FX_QUEUE_CAP) {
        s_read = (unsigned char)((s_read + 1) % FX_QUEUE_CAP);
        --s_count;
    }
    s_pending[s_write] = infantry ? 0 : 1;
    s_write = (unsigned char)((s_write + 1) % FX_QUEUE_CAP);
    ++s_count;
}

void DuneGameSound_Update(void) {
    unsigned long now;
    unsigned char id;
    if (!s_ready || !s_enabled || !s_count) return;
    now = TickCount();
    if ((long)(now - s_endTick) < 0) return;
    if (s_lastStart && (unsigned long)(now - s_lastStart) < FX_MIN_GAP_TICKS) return;
    id = s_pending[s_read];
    s_read = (unsigned char)((s_read + 1) % FX_QUEUE_CAP);
    --s_count;
    (void)PlayNow(id);
    /* One channel: discard stale bursts rather than replaying them late. */
    ClearQueue();
}

void DuneGameSound_Toggle(void) {
    s_enabled = !s_enabled;
    ClearQueue();
    if (!s_enabled && s_channel) {
        SndCommand cmd;
        cmd.cmd = quietCmd; cmd.param1 = 0; cmd.param2 = 0;
        (void)SndDoImmediate(s_channel, &cmd);
        s_endTick = 0;
    }
}
int DuneGameSound_IsEnabled(void) { return s_enabled; }
void DuneGameSound_Shutdown(void) {
    if (s_channel) { SndDisposeChannel(s_channel, true); s_channel = NULL; }
    { int i; for (i = 0; i < FX_COUNT; ++i) {
        if (s_fx[i].header) { free(s_fx[i].header); s_fx[i].header = NULL; }
    }}
    ClearQueue(); s_endTick = 0; s_lastStart = 0; s_ready = 0;
}
