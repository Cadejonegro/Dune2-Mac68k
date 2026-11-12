
/* Stage 10J: Original OpenDUNE unit sprite collection. */
extern void Sprites_Init(void);
extern unsigned char **g_sprites;
static int g_unitSpriteTestResult = 0;
extern int File_Exists(const char *filename);

/* Stage 10H: Original OpenDUNE scenario test. */
extern void GFX_Init(void);

static int g_missionTestResult = -1;

static int g_moveScriptReady = 0;
static int g_teamScriptReady = 0;
static int g_structureScriptReady = 0;
static unsigned int g_missionUnitCount = 0;
static unsigned int g_missionStructureCount = 0;

extern int MacMission_InitializePools(void);

/* Original OpenDUNE shared scenario parsing buffer. */
extern void *g_readBuffer;
extern unsigned int g_readBufferSize;

extern void MacTimer_Update(void);


#include "../OpenDUNE/include/types.h"

/* R3D8D-A: OpenDUNE campaign level diagnostic. */
extern uint16 g_campaignID;
#include "../OpenDUNE/src/unit.h"
#include "../OpenDUNE/src/pool/unit.h"
#include <stdio.h>
#include "../OpenDUNE/src/structure.h"
#include "../OpenDUNE/src/pool/structure.h"
#include "../OpenDUNE/src/codec/format80.h"
#include <Quickdraw.h>
#include <stdlib.h>
#include <string.h>
#include <Windows.h>
#include <Events.h>
#include <Fonts.h>
#include <OSUtils.h>
#include <Timer.h> /* Microseconds / UnsignedWide */
#include <Menus.h>
#include <Dialogs.h>
#include "dune_game_sound.h"
#include "r4_present.h"
#include "r4_framebuffer.h"
#include "r4_terrain_restore.h"
#include "r4_partial_present.h"

#include "types.h"
#include "format80.h"
#include "../OpenDUNE/src/file.h"
#include "../OpenDUNE/src/tools.h"
#include "../OpenDUNE/src/tile.h"
#include "../OpenDUNE/src/timer.h"
#include "../OpenDUNE/src/house.h"
#include "../OpenDUNE/src/pool/house.h"
#include "../OpenDUNE/src/pool/pool.h"
#include "../OpenDUNE/src/scenario.h"

#include "../OpenDUNE/src/animation.h"
#include "../OpenDUNE/src/explosion.h"
#include "../OpenDUNE/src/script/script.h"

/* P3A: default control is original full-screen path. Press 8 to toggle. */
static Boolean p3aPartialMode = false;
static Boolean p3aForceFull = true;
static unsigned long p3aCompareUs = 0, p3aTransferUs = 0;
static unsigned long p3aFrames = 0, p3aFull = 0, p3aSkipped = 0;
static unsigned long p3aDirtyBands = 0, p3aRects = 0;
static unsigned long p3aPaletteEpoch = 0, p3aDisplayedPaletteEpoch = 0;
static void P3A_PaletteChanged(void) { p3aPaletteEpoch++; }
static unsigned long p3aSnapCompareUs = 0, p3aSnapTransferUs = 0;
static unsigned long p3aSnapFrames = 0, p3aSnapFull = 0, p3aSnapSkipped = 0;
static unsigned long p3aSnapDirtyBands = 0, p3aSnapRects = 0;
static void P3A_Snapshot(void) {
    p3aSnapCompareUs = p3aCompareUs; p3aCompareUs = 0;
    p3aSnapTransferUs = p3aTransferUs; p3aTransferUs = 0;
    p3aSnapFrames = p3aFrames; p3aFrames = 0;
    p3aSnapFull = p3aFull; p3aFull = 0;
    p3aSnapSkipped = p3aSkipped; p3aSkipped = 0;
    p3aSnapDirtyBands = p3aDirtyBands; p3aDirtyBands = 0;
    p3aSnapRects = p3aRects; p3aRects = 0;
}


/* R3D7P1: passive wall-clock instrumentation. Times are inclusive and
 * measured on the IIci; do not interpret idle/wait time as CPU utilization.
 * The 32-bit microsecond clock wraps after ~71 minutes; each timed call
 * is short and subtraction is modulo 2^32. Accumulators reset every 10s. */
enum { P1_SIM, P1_BUILD, P1_COPY, P1_SOUND, P1_N };
static unsigned long p1Total[P1_N] = {0,0,0,0};
static unsigned long p1Calls[P1_N] = {0,0,0,0};
static unsigned long p1Max[P1_N] = {0,0,0,0};
static unsigned long p1Snapshot[P1_N] = {0,0,0,0};
static unsigned long p1SnapCalls[P1_N] = {0,0,0,0};
static unsigned long p1SnapMax[P1_N] = {0,0,0,0};
static unsigned long p1WindowTick = 0;
static unsigned long p1WallUs = 0;
static unsigned long p1WindowStartUs = 0;
/* R3D7P2: 0=cycle, 1=hold percentages, 2=hold milliseconds. */
static unsigned char p2DisplayMode = 0;
/* Time inside CopyBits, excluding LockPixels, metadata, and unlock. */
static unsigned long p2BlitTotalUs = 0;
static unsigned long p2BlitCalls = 0;
static unsigned long p2BlitMaxUs = 0;
static unsigned long p2BlitSnapUs = 0;
static unsigned long p2BlitSnapCalls = 0;
static unsigned long p2BlitSnapMaxUs = 0;
/* R3D7P3: palette comparison is diagnostic only, not a speed claim.
 * Compare RGB values at each palette index (not ctSeed alone). */
static unsigned int p3PaletteMismatch = 999;
static unsigned int p3PaletteSourceCount = 0;
static unsigned int p3PaletteDestCount = 0;
static unsigned int p3PaletteSamples = 0;
/* System 7 Microseconds trap, $A193.
 * Low 32 bits returned in D0.
 * Diagnostic timing only; not used for game scheduling.
 */
static unsigned long P1_Now(void) {
    register unsigned long low __asm__("d0");

    __asm__ volatile (
        ".word 0xA193"
        : "=d" (low)
        :
        : "a0", "d1", "a1", "cc", "memory"
    );

    return low;
}
static void P1_Add(int category, unsigned long start) {
    unsigned long duration = P1_Now() - start;
    p1Total[category] += duration;
    p1Calls[category]++;
    if (duration > p1Max[category]) p1Max[category] = duration;
}
static void P1_Snapshot(void) {
    unsigned long tick = TickCount();
    int i;
    if (p1WindowTick == 0) {
        p1WindowTick = tick;
        p1WindowStartUs = P1_Now();
        return;
    }
    if ((unsigned long)(tick - p1WindowTick) < 600UL) return;
    p1WallUs = P1_Now() - p1WindowStartUs;
    P3A_Snapshot();
    p2BlitSnapUs = p2BlitTotalUs;
    p2BlitSnapCalls = p2BlitCalls;
    p2BlitSnapMaxUs = p2BlitMaxUs;
    p2BlitTotalUs = p2BlitCalls = p2BlitMaxUs = 0;
    for (i = 0; i < P1_N; i++) {
        p1Snapshot[i] = p1Total[i];
        p1SnapCalls[i] = p1Calls[i];
        p1SnapMax[i] = p1Max[i];
        p1Total[i] = p1Calls[i] = p1Max[i] = 0;
    }
    p1WindowTick = tick;
    p1WindowStartUs = P1_Now();
}
static unsigned long P1_Pct(int category) {
    /* Scale before multiplying to avoid 32-bit overflow. */
    unsigned long denom = p1WallUs / 1000UL;
    return denom ? (p1Snapshot[category] / 1000UL) * 100UL / denom : 0;
}

/* Stage 15O-L.2 timer diagnostics. */
extern volatile uint32 g_macTimerCapHits;
extern volatile uint32 g_macTimerDiscardedTicks;
extern volatile uint32 g_macTimerMaximumElapsed;


/* Stage 15O-K.1: set to 0 to restore J.1 scheduling. */
#ifndef STAGE15OK_BOUNDED_CATCHUP
#define STAGE15OK_BOUNDED_CATCHUP 1
#endif


/* Stage 12A: Original unit movement. */
/* Performance diagnostic: simulation ticks discarded. */
static uint32 g_perfSkippedTicks = 0;

extern void GameLoop_Unit(void);
extern void GameLoop_Team(void);
extern void Mac_House_EnsureHarvesterAvailable(uint8 houseID);
extern void GameLoop_Structure(void);
extern bool Mac_CancelYardProduction(Structure *s);
extern void House_CalculatePowerAndCredit(House *h);




/* Stage 15O-Q.5D: passive original-unit movement measurement. */
#include "../OpenDUNE/src/config.h"

static uint16 q5dTrackedIndex = 0xFFFF;
static uint32 q5dLastSample = 0;
static uint32 q5dLastTitle = 0;
static uint16 q5dLastX = 0;
static uint16 q5dLastY = 0;
static uint32 q5dPositionChanges = 0;
static uint32 q5dMovementCalls = 0;
static uint32 q5dTotalChanges = 0;
static uint32 q5dFirstTick = 0;
static uint16 q5dFirstX = 0;
static uint16 q5dFirstY = 0;

static Unit *Q5D_FindTrike(void)
{
    unsigned int i;

    if (q5dTrackedIndex < UNIT_INDEX_MAX) {
        Unit *u = Unit_Get_ByIndex(q5dTrackedIndex);

        if (u != NULL &&
            u->o.flags.s.used &&
            !u->o.flags.s.isNotOnMap &&
            u->o.type == UNIT_TRIKE &&
            u->o.houseID == g_playerHouseID)
            return u;
    }

    for (i = 0; i < UNIT_INDEX_MAX; i++) {
        Unit *u = Unit_Get_ByIndex((uint16)i);

        if (u == NULL)
            continue;

        if (!u->o.flags.s.used ||
            u->o.flags.s.isNotOnMap)
            continue;

        if (u->o.type != UNIT_TRIKE ||
            u->o.houseID != g_playerHouseID)
            continue;

        q5dTrackedIndex = (uint16)i;
        q5dLastSample = 0;
        q5dFirstTick = 0;
        q5dPositionChanges = 0;
        q5dMovementCalls = 0;
        q5dTotalChanges = 0;
        return u;
    }

    q5dTrackedIndex = 0xFFFF;
    return NULL;
}

static void Q5D_RecordMovement(void)
{
    Unit *u = Q5D_FindTrike();

    if (u == NULL)
        return;

    if (q5dFirstTick == 0) {
        q5dFirstTick = g_timerGame;
        q5dFirstX = u->o.position.x;
        q5dFirstY = u->o.position.y;
        q5dLastX = u->o.position.x;
        q5dLastY = u->o.position.y;
        q5dLastSample = g_timerGame;
    }

    q5dMovementCalls++;

    if (u->o.position.x != q5dLastX ||
        u->o.position.y != q5dLastY) {
        q5dPositionChanges++;
        q5dTotalChanges++;
        q5dLastX = u->o.position.x;
        q5dLastY = u->o.position.y;
    }
}

static void Q5D_ShowMovement(WindowPtr window)
{
    Unit *u;
    uint32 now = TickCount();
    char message[240];
    Str255 title;
    size_t length;

    if (q5dLastTitle != 0 &&
        (uint32)(now - q5dLastTitle) < 60)
        return;

    q5dLastTitle = now;
    u = Q5D_FindTrike();

    if (u == NULL) {
        sprintf(message, "Q5D NO PLAYER TRIKE SPEED:%u",
                (unsigned int)g_gameConfig.gameSpeed);
    } else {
        unsigned int calls = (unsigned int)q5dMovementCalls;
        unsigned int changes = (unsigned int)q5dPositionChanges;

        sprintf(
            message,
            "Q5D T:%lu ID:%u GS:%u XY:%u,%u D:%ld,%ld V:%u P:%u A:%u M:%u/%u TOT:%lu",
            (unsigned long)g_timerGame,
            (unsigned int)q5dTrackedIndex,
            (unsigned int)g_gameConfig.gameSpeed,
            (unsigned int)u->o.position.x,
            (unsigned int)u->o.position.y,
            (long)((int32)u->o.position.x - (int32)q5dFirstX),
            (long)((int32)u->o.position.y - (int32)q5dFirstY),
            (unsigned int)u->speed,
            (unsigned int)u->speedPerTick,
            (unsigned int)u->speedRemainder,
            changes,
            calls,
            (unsigned long)q5dTotalChanges
        );

        q5dPositionChanges = 0;
        q5dMovementCalls = 0;
        q5dLastSample = g_timerGame;
    }

    length = strlen(message);
    if (length > 255)
        length = 255;

    title[0] = (unsigned char)length;
    memcpy(title + 1, message, length);
    SetWTitle(window, title);
}

/* Stage 15O-Q.5B: frame-pacing diagnostics.
 * Times are measured in Macintosh TickCount ticks (~1/60 sec).
 * All measurements are passive: no engine scheduling changes.
 */
static uint32 q5bLastSim = 0;
static uint32 q5bLastRender = 0;
static uint32 q5bLastCopy = 0;
static uint32 q5bSampleTick = 0;

static uint32 q5bMaxSimGap = 0;
static uint32 q5bMaxRenderGap = 0;
static uint32 q5bMaxCopyGap = 0;
static uint32 q5bMaxSimCost = 0;
static uint32 q5bMaxRenderCost = 0;
static uint32 q5bMaxCopyCost = 0;

static uint32 q5bSimStartTick = 0;
static uint32 q5bRenderStartTick = 0;
static uint32 q5bCopyStartTick = 0;

static uint32 q5bSimCalls = 0;
static uint32 q5bRenderCalls = 0;
static uint32 q5bCopyCalls = 0;
static uint32 q5bSimBursts = 0;

static void Q5B_RecordGap(
    uint32 now, uint32 *previous, uint32 *maximum)
{
    if (*previous != 0) {
        uint32 gap = now - *previous;
        if (gap > *maximum) *maximum = gap;
    }
    *previous = now;
}

static void Q5B_SimStart(void)
{
    uint32 now = TickCount();
    if (q5bLastSim != 0 && now == q5bLastSim)
        q5bSimBursts++;
    Q5B_RecordGap(now, &q5bLastSim, &q5bMaxSimGap);
    q5bSimStartTick = now;
    q5bSimCalls++;
}

static void Q5B_SimEnd(void)
{
    uint32 elapsed = TickCount() - q5bSimStartTick;
    if (elapsed > q5bMaxSimCost)
        q5bMaxSimCost = elapsed;
}

static void Q5B_RenderStart(void)
{
    uint32 now = TickCount();
    Q5B_RecordGap(now, &q5bLastRender, &q5bMaxRenderGap);
    q5bRenderStartTick = now;
    q5bRenderCalls++;
}

static void Q5B_RenderEnd(void)
{
    uint32 elapsed = TickCount() - q5bRenderStartTick;
    if (elapsed > q5bMaxRenderCost)
        q5bMaxRenderCost = elapsed;
}

static void Q5B_CopyStart(void)
{
    uint32 now = TickCount();
    Q5B_RecordGap(now, &q5bLastCopy, &q5bMaxCopyGap);
    q5bCopyStartTick = now;
    q5bCopyCalls++;
}

static void Q5B_CopyEnd(void)
{
    uint32 elapsed = TickCount() - q5bCopyStartTick;
    if (elapsed > q5bMaxCopyCost)
        q5bMaxCopyCost = elapsed;
}

static void Q5B_ShowDiagnostics(WindowPtr window)
{
    uint32 now = TickCount();
    char text[240];
    Str255 title;
    size_t length;

    if (q5bSampleTick == 0) {
        q5bSampleTick = now;
        return;
    }

    if ((uint32)(now - q5bSampleTick) < 60)
        return;

    sprintf(
        text,
        "Q5B SG:%lu RG:%lu CG:%lu SC:%lu RC:%lu CC:%lu BUR:%lu S:%lu R:%lu C:%lu",
        (unsigned long)q5bMaxSimGap,
        (unsigned long)q5bMaxRenderGap,
        (unsigned long)q5bMaxCopyGap,
        (unsigned long)q5bMaxSimCost,
        (unsigned long)q5bMaxRenderCost,
        (unsigned long)q5bMaxCopyCost,
        (unsigned long)q5bSimBursts,
        (unsigned long)q5bSimCalls,
        (unsigned long)q5bRenderCalls,
        (unsigned long)q5bCopyCalls
    );

    length = strlen(text);
    if (length > 255) length = 255;
    title[0] = (unsigned char)length;
    memcpy(title + 1, text, length);
    SetWTitle(window, title);

    q5bMaxSimGap = 0;
    q5bMaxRenderGap = 0;
    q5bMaxCopyGap = 0;
    q5bMaxSimCost = 0;
    q5bMaxRenderCost = 0;
    q5bMaxCopyCost = 0;
    q5bSimBursts = 0;
    q5bSimCalls = 0;
    q5bRenderCalls = 0;
    q5bCopyCalls = 0;
    q5bSampleTick = now;
}

/* R3D.1: passive redraw-cause counters; no renderer or simulation changes. */
static uint32 r3dScheduled = 0;
static uint32 r3dPrepared = 0;
static uint32 r3dCopies = 0;
static uint32 r3dScrolls = 0;
static uint32 r3dPreviewChanges = 0;
static uint32 r3dSelectionInvalidations = 0;
static uint32 r3dLastReport = 0;

static uint32 r3d2UnitChangedFrames = 0;
static uint32 r3d2UnitStableFrames = 0;
static uint32 r3d2VisibleUnits = 0;

/* F12C11: passive source-pixel coverage audit; no blit modifications. */
static unsigned long f12c11ComparedFrames = 0;
static unsigned long f12c11MissedPixels = 0;
static unsigned long f12c11MissedFrames = 0;
static unsigned long f12c11ChangedPixels = 0;
static unsigned long f12c11SkippedFrames = 0;
static unsigned long f12c11CopyCount = 0;

static void R3D1_ShowCounters(WindowPtr window)
{
    uint32 now = TickCount();
    char message[180];
    Str255 title;
    size_t length;
    if (window == NULL) return;
    if (r3dLastReport == 0) { r3dLastReport = now; return; }
    if ((uint32)(now - r3dLastReport) < 60) return;
    sprintf(message, "R3D2 S:%lu B:%lu C:%lu Uchg:%lu Ustable:%lu Units:%lu Scr:%lu F11:%lu/%lu",
        (unsigned long)r3dScheduled, (unsigned long)r3dPrepared,
        (unsigned long)r3dCopies, (unsigned long)r3d2UnitChangedFrames,
        (unsigned long)r3d2UnitStableFrames, (unsigned long)r3d2VisibleUnits,
        (unsigned long)r3dScrolls,
        f12c11MissedFrames, f12c11ComparedFrames);
    length = strlen(message);
    if (length > 255) length = 255;
    title[0] = (unsigned char)length;
    memcpy(title + 1, message, length);
    SetWTitle(window, title);
    r3d2UnitChangedFrames = r3d2UnitStableFrames = 0;
    r3dScheduled = r3dPrepared = r3dCopies = 0;
    r3dScrolls = r3dPreviewChanges = r3dSelectionInvalidations = 0;
    r3dLastReport = now;
}

/* R3D.2: diagnostic-only unit-state probe. NOT a redraw-skip predicate.
 * Sample once per scheduled frame, never inside simulation loops.
 * Deliberately ignores animations, structures, effects, and selection. */
static Boolean r3d2UnitSnapshotValid = false;
static uint16 r3d2PrevX[UNIT_INDEX_MAX];
static uint16 r3d2PrevY[UNIT_INDEX_MAX];
static uint16 r3d2PrevType[UNIT_INDEX_MAX];
static uint8 r3d2PrevPresent[UNIT_INDEX_MAX];

/* R5A passive scheduler accounting; never changes drawing behavior. */
static unsigned long r5Frames=0, r5Sim=0, r5Quiet=0, r5Scroll=0;
static unsigned long r5Input=0, r5StableSim=0, r5Steps=0, r5LastSteps=0;
static uint32 r5LastScroll=0, r5LastSelection=0, r5LastPreview=0;
static int g_viewTileX;
static int g_viewTileY;
static int r5LastViewX=-1, r5LastViewY=-1;
static void R5A_Sample(void)
{
    Boolean sim = r5Steps != r5LastSteps;
    Boolean scroll = r5LastViewX != g_viewTileX ||
                     r5LastViewY != g_viewTileY ||
                     r5LastScroll != r3dScrolls;
    Boolean input = r5LastSelection != r3dSelectionInvalidations ||
                    r5LastPreview != r3dPreviewChanges;
    r5Frames++;
    if (sim) r5Sim++;
    if (scroll) r5Scroll++;
    if (input) r5Input++;
    if (!sim && !scroll && !input) r5Quiet++;
    r5LastSteps=r5Steps;
    r5LastScroll=r3dScrolls;
    r5LastSelection=r3dSelectionInvalidations;
    r5LastPreview=r3dPreviewChanges;
    r5LastViewX=g_viewTileX;
    r5LastViewY=g_viewTileY;
}
static void R3D2_SampleUnits(void)
{
    unsigned int i;
    uint32 visible = 0;
    Boolean changed = !r3d2UnitSnapshotValid;
    for (i = 0; i < UNIT_INDEX_MAX; i++) {
        Unit *u = Unit_Get_ByIndex((uint16)i);
        uint8 present = (u != NULL && u->o.flags.s.used &&
                         !u->o.flags.s.isNotOnMap) ? 1 : 0;
        uint16 x = present ? u->o.position.x : 0;
        uint16 y = present ? u->o.position.y : 0;
        uint16 type = present ? u->o.type : 0;
        if (present) visible++;
        if (r3d2UnitSnapshotValid &&
            (present != r3d2PrevPresent[i] ||
             x != r3d2PrevX[i] || y != r3d2PrevY[i] ||
             type != r3d2PrevType[i])) changed = true;
        r3d2PrevPresent[i] = present;
        r3d2PrevX[i] = x;
        r3d2PrevY[i] = y;
        r3d2PrevType[i] = type;
    }
    r3d2UnitSnapshotValid = true;
    r3d2VisibleUnits = visible;
    if (changed) r3d2UnitChangedFrames++;
    else {
        r3d2UnitStableFrames++;
        if (r5Steps != r5LastSteps) r5StableSim++;
    }
}

/* Stage 15O-Q.4: diagnostic counters only. */
static uint32 q4UnitCalls = 0;
static uint32 q4StructureCalls = 0;
static uint32 q4RenderCalls = 0;
static uint32 q4UnitTime = 0;
static uint32 q4StructureTime = 0;
static uint32 q4RenderTime = 0;
static uint32 q4BacklogPeak = 0;
static uint32 q4SampleTick = 0;
static uint32 q4PreviousSkipped = 0;

static void Q4_ShowDiagnostics(WindowPtr window)
{
    uint32 current = TickCount();
    uint32 skipped;
    char text[240];
    Str255 title;
    size_t length;

    if (q4SampleTick == 0) {
        q4SampleTick = current;
        q4PreviousSkipped = g_perfSkippedTicks;
        return;
    }

    if ((uint32)(current - q4SampleTick) < 60)
        return;

    skipped = g_perfSkippedTicks - q4PreviousSkipped;

    sprintf(text,
        "Q5A U:%lu S:%lu R:%lu B:%lu DROP:%lu CPU:%lu/%lu/%lu",
        (unsigned long)q4UnitCalls,
        (unsigned long)q4StructureCalls,
        (unsigned long)q4RenderCalls,
        (unsigned long)q4BacklogPeak,
        (unsigned long)skipped,
        (unsigned long)q4UnitTime,
        (unsigned long)q4StructureTime,
        (unsigned long)q4RenderTime);

    length = strlen(text);
    if (length > 255)
        length = 255;

    title[0] = (unsigned char)length;
    memcpy(title + 1, text, length);
    SetWTitle(window, title);

    q4UnitCalls = 0;
    q4StructureCalls = 0;
    q4RenderCalls = 0;
    q4UnitTime = 0;
    q4StructureTime = 0;
    q4RenderTime = 0;
    q4BacklogPeak = 0;
    q4PreviousSkipped = g_perfSkippedTicks;
    q4SampleTick = current;
}

/* Stage 15O-Q.2: house economy, without full house AI. */
/* Stage 15O-Q.3: isolated original house economy scheduler.
 * Runs outside bounded simulation catch-up, using game ticks.
 * Does not activate reinforcements, starports or house AI.
 */
static uint32 stage15OQ3NextEconomyTick = 0;
static uint32 stage15OQ3NextMaintenanceTick = 0;
static uint32 stage15OQ3EconomyCalls = 0;
static uint32 stage15OQ3MaintenanceCalls = 0;

static void Stage15OQ3_UpdateHouses(void)
{
    uint32 now;
    Boolean economyDue;
    Boolean maintenanceDue;
    struct PoolFindStruct find;
    House *h;

    if (g_missionTestResult != 1 || g_playerHouse == NULL)
        return;

    now = g_timerGame;

    economyDue =
        stage15OQ3NextEconomyTick == 0 ||
        (int32)(now - stage15OQ3NextEconomyTick) >= 0;

    maintenanceDue =
        stage15OQ3NextMaintenanceTick == 0 ||
        (int32)(now - stage15OQ3NextMaintenanceTick) >= 0;

    if (!economyDue && !maintenanceDue)
        return;

    if (economyDue) {
        stage15OQ3NextEconomyTick = now + 900;
        stage15OQ3EconomyCalls++;
    }

    if (maintenanceDue) {
        stage15OQ3NextMaintenanceTick = now + 10800;
        stage15OQ3MaintenanceCalls++;
    }

    find.houseID = HOUSE_INVALID;
    find.index = 0xFFFF;
    find.type = 0xFFFF;

    while ((h = House_Find(&find)) != NULL) {
        if (economyDue)
            House_CalculatePowerAndCredit(h);

        if (maintenanceDue) {
            uint16 cost = (h->powerUsage / 32) + 1;

            if (cost > h->credits)
                cost = h->credits;

            h->credits -= cost;
        }
    }
}




/*
 * Stage 10C:
 * Execute the original OpenDUNE Format80_Decode function.
 *
 * Format 80 command 0x84 copies four literal bytes.
 * Command 0x80 terminates the compressed stream.
 */
static Boolean RunOpenDUNETest(void)
{
    static const uint8 compressed[] = {
        0x84, 'D', 'U', 'N', 'E', 0x80
    };

    uint8 output[8] = {0};
    uint16 length;

    length = Format80_Decode(
        output,
        compressed,
        8
    );

    return (
        length == 4 &&
        output[0] == 'D' &&
        output[1] == 'U' &&
        output[2] == 'N' &&
        output[3] == 'E'
    );
}


static int RunPakTest(void)
{
    uint8 index;
    uint8 buffer[4];

    if (!File_Init())
        return 1;

    index = File_Open("GRAYRMAP.TBL", FILE_MODE_READ);

    if (index == FILE_INVALID) {
        File_Uninit();
        return 2;
    }

    if (File_GetSize(index) != 256) {
        File_Close(index);
        File_Uninit();
        return 3;
    }

    if (File_Read(index, buffer, sizeof(buffer)) != sizeof(buffer)) {
        File_Close(index);
        File_Uninit();
        return 4;
    }

    File_Close(index);
    File_Uninit();
    return 0;
}


/*
 * Stage 10E:
 * Decode the original DUNEMAP.CPS image from DUNE.PAK.
 *
 * Return codes:
 * 0 = success
 * 1 = file subsystem initialization failed
 * 2 = CPS resource missing
 * 3 = allocation failed
 * 4 = CPS header/read error
 * 5 = unsupported compression or palette
 * 6 = decoded length incorrect
 */
/* Original Dune II map: 320 x 200 indexed pixels. */
static uint8 *g_duneMapPixels = NULL;

static int RunCPSTest(void)
{
    uint8 index;
    uint8 header[10];
    uint8 *compressed = NULL;
    uint8 *decoded = NULL;
    uint32 fileSize;
    uint32 payloadSize;
    uint16 result;
    int status = 0;

    if (!File_Init())
        return 1;

    index = File_Open("DUNEMAP.CPS", FILE_MODE_READ);

    if (index == FILE_INVALID) {
        File_Uninit();
        return 2;
    }

    fileSize = File_GetSize(index);

    if (fileSize < 11 || fileSize > 65535UL) {
        status = 4;
        goto cleanup;
    }

    if (File_Read(index, header, 10) != 10) {
        status = 4;
        goto cleanup;
    }

    if (header[2] != 0x04 ||
        header[8] != 0 ||
        header[9] != 0) {
        status = 5;
        goto cleanup;
    }

    payloadSize = fileSize - 10;

    compressed = (uint8 *)malloc(payloadSize);
    decoded = (uint8 *)malloc(64000UL);

    if (compressed == NULL || decoded == NULL) {
        status = 3;
        goto cleanup;
    }

    if (File_Read(index, compressed, payloadSize) != payloadSize) {
        status = 4;
        goto cleanup;
    }

    result = Format80_Decode(decoded, compressed, 64000);

    if (result != 64000) {
        status = 6;
    } else {
        g_duneMapPixels = decoded;
        decoded = NULL;
    }

cleanup:
    free(compressed);
    free(decoded);
    File_Close(index);
    File_Uninit();

    return status;
}


/*
 * Stage 10F: create an 8-bit offscreen image containing
 * the original 320 x 200 Dune II pixels.
 */
static GWorldPtr g_duneMapWorld = NULL;
static Boolean g_duneMapWorldReady = false;
static short g_duneGWorldError = 0;


/* Original Dune II IBM.PAL: 256 colors, 3 bytes per color. */
static uint8 g_dunePalette[768];
static Boolean g_dunePaletteLoaded = false;

static Boolean LoadDunePalette(void)
{
    uint8 index;
    Boolean success = false;

    /* Stage 14G: Reuse the active OpenDUNE file session.
       Scripts and scenario have already initialized it. */

    index = File_Open("IBM.PAL", FILE_MODE_READ);

    if (index != FILE_INVALID) {
        if (File_GetSize(index) == 768 &&
            File_Read(index, g_dunePalette, 768) == 768) {
            success = true;
        }

        File_Close(index);
    }

    /* Keep resource indexes available for subsequent sprite loading. */

    return success;
}


/* Stage 10F: convert the original VGA palette to Macintosh RGB. */
static PaletteHandle g_duneMacPalette = NULL;
static CTabHandle g_duneColorTable = NULL;

/* R3D7P4-D: Animate the original Wind Trap palette index (223).
 * Source: OpenDUNE GUI_PaletteAnimate, using one RGB step per update.
 * Keep the offscreen PixMap CLUT synchronized with the window palette.
 * Do not change tile indices, gameplay, or the CopyBits transfer path. */
static volatile unsigned long f12c7PaletteUpdates = 0;
static uint32 p4dLastPaletteTick = 0;
static uint32 r4p1RenderInterval = 12;
static uint8 p4dTargetColour = 12;
static Boolean p4dPaletteInitialized = false;

static void P3A_PaletteChanged(void);
static void P4D_AnimateWindTrapPalette(WindowPtr window)
{
    unsigned int channel;
    unsigned int changed = 0;
    RGBColor color;
    PixMapHandle map;
    CTabHandle mapCT;
    ColorTable *table;
    uint32 now;

    if (window == NULL || g_duneMacPalette == NULL ||
        g_duneColorTable == NULL || g_duneMapWorld == NULL)
        return;

    now = g_timerGUI;
    if (!p4dPaletteInitialized) {
        p4dLastPaletteTick = now;
        p4dPaletteInitialized = true;
        return;
    }
    if ((uint32)(now - p4dLastPaletteTick) < 30UL)
        return;
    p4dLastPaletteTick = now;

    table = *g_duneColorTable;
    color = table->ctTable[223].rgb;
    {
        unsigned short *current[3];
        const RGBColor *target = &table->ctTable[p4dTargetColour].rgb;
        const unsigned short *desired[3];
        current[0] = &color.red;
        current[1] = &color.green;
        current[2] = &color.blue;
        desired[0] = &target->red;
        desired[1] = &target->green;
        desired[2] = &target->blue;
        for (channel = 0; channel < 3; ++channel) {
            unsigned int value = (unsigned int)(*current[channel]);
            unsigned int goal = (unsigned int)(*desired[channel]);
            if (value != goal) {
                unsigned int step = 1041U;
                changed = 1;
                if (value < goal)
                    value += ((goal - value) < step) ? (goal - value) : step;
                else
                    value -= ((value - goal) < step) ? (value - goal) : step;
                *current[channel] = (unsigned short)value;
            }
        }
    }
    if (!changed) {
        p4dTargetColour = (p4dTargetColour == 12) ? 10 : 12;
        return;
    }

    table->ctTable[223].rgb = color;
    CTabChanged(g_duneColorTable);

    map = GetGWorldPixMap(g_duneMapWorld);
    mapCT = (map != NULL) ? (**map).pmTable : NULL;
    if (mapCT != NULL && mapCT != g_duneColorTable &&
        (**mapCT).ctSize >= 223) {
        (**mapCT).ctTable[223].rgb = color;
        CTabChanged(mapCT);
    }

    AnimateEntry(window, 223, &color);
    f12c7PaletteUpdates++;
}

static Boolean CreateDuneMacPalette(void)
{
    int i;
    ColorTable *table;

    if (!g_dunePaletteLoaded)
        return false;

    g_duneColorTable = (CTabHandle)NewHandle(
        sizeof(ColorTable) + 255 * sizeof(ColorSpec)
    );

    if (g_duneColorTable == NULL)
        return false;

    HLock((Handle)g_duneColorTable);
    table = *g_duneColorTable;

    table->ctSeed = 0;
    table->ctFlags = 0;
    table->ctSize = 255;

    for (i = 0; i < 256; i++) {
        table->ctTable[i].value = i;
        table->ctTable[i].rgb.red =
            (uint16)((uint32)g_dunePalette[i * 3] * 65535UL / 63);
        table->ctTable[i].rgb.green =
            (uint16)((uint32)g_dunePalette[i * 3 + 1] * 65535UL / 63);
        table->ctTable[i].rgb.blue =
            (uint16)((uint32)g_dunePalette[i * 3 + 2] * 65535UL / 63);
    }

    HUnlock((Handle)g_duneColorTable);

    g_duneMacPalette = NewPalette(
        256, g_duneColorTable, 0, 0
    );

    return g_duneMacPalette != NULL;
}


/* Stage 10I.4: live mission terrain overview. */
#include "../OpenDUNE/src/map.h"

static Boolean CreateMissionTerrainWorld(void)
{
    PixMapHandle pixmap;
    unsigned char *destination;
    short stride;
    int x, y, dx, dy;
    uint16 landscape;
    uint16 colour;

    if (g_duneMapWorld == NULL || g_missionTestResult != 1)
        return false;

    pixmap = GetGWorldPixMap(g_duneMapWorld);

    if (pixmap == NULL || !LockPixels(pixmap))
        return false;

    stride = (**pixmap).rowBytes & 0x3FFF;
    destination = (unsigned char *)GetPixBaseAddr(pixmap);

    if (destination == NULL || stride < 320) {
        UnlockPixels(pixmap);
        return false;
    }

    for (y = 0; y < 64; y++) {
        for (x = 0; x < 64; x++) {
            landscape = Map_GetLandscapeType((uint16)(y * 64 + x));

            if (landscape >= LST_MAX)
                landscape = LST_NORMAL_SAND;

            colour = g_table_landscapeInfo[landscape].radarColour;

            if (colour > 255)
                colour = 0;

            for (dy = 0; dy < 3; dy++) {
                for (dx = 0; dx < 5; dx++) {
                    destination[(y * 3 + dy) * stride +
                                x * 5 + dx] = (unsigned char)colour;
                }
            }
        }
    }

    UnlockPixels(pixmap);
    return true;
}


/* Stage 10I.8: original ICON.ICN terrain tiles. */
#include "../OpenDUNE/src/scenario.h"
#include "../OpenDUNE/src/map.h"
#include "../OpenDUNE/src/sprites.h"

/* Stage 10I.11: scrolling viewport. */
static int g_viewTileX = 16;
static int g_viewTileY = 16;

/* Camera limits derived from the loaded mission's playable map. */
static void Mac_GetScrollBounds(int *minX, int *maxX,
                                int *minY, int *maxY)
{
    const MapInfo *info;

    /* Safe fallback before a mission is successfully loaded. */
    if (g_missionTestResult != 1 || g_scenario.mapScale >= 3) {
        *minX = 0;
        *maxX = 44;
        *minY = 0;
        *maxY = 52;
        return;
    }

    info = &g_mapInfos[g_scenario.mapScale];

    *minX = info->minX;
    *minY = info->minY;

    *maxX = info->minX + info->sizeX - 20;
    *maxY = info->minY + info->sizeY - 12;

    if (*maxX < *minX) *maxX = *minX;
    if (*maxY < *minY) *maxY = *minY;
}




/*
 * Stage 10J: Draw original OpenDUNE units into the Macintosh
 * 8-bit QuickDraw GWorld.
 *
 * Uses OpenDUNE's unit pool, sprite IDs and Format80 decoder.
 */

/* Count active units loaded from the original scenario. */

/* Show the actual number of loaded mission units. */

static unsigned int g_firstStructureType = 0;
static unsigned int g_firstStructureHouse = 0;
static unsigned int g_firstStructureX = 0;
static unsigned int g_firstStructureY = 0;

#include "../OpenDUNE/src/house.h"

extern House *g_playerHouse;

static Boolean stage15ON1MissionWon = false;
static Boolean stage15ON1MissionLost = false;

static void Mac_DrawUnitCountDiagnostic(void)
{
    char message[80];
    Str255 text;
    unsigned int i;

    sprintf(message, "UNITS: %u     STRUCTURES: %u",
            g_missionUnitCount, g_missionStructureCount);

    text[0] = (unsigned char)strlen(message);
    for (i = 0; i < text[0]; i++)
        text[i + 1] = (unsigned char)message[i];

    MoveTo(20, 245);
    DrawString(text);

    sprintf(message, "BUILDING TYPE: %u     HOUSE: %u",
            g_firstStructureType, g_firstStructureHouse);

    text[0] = (unsigned char)strlen(message);
    for (i = 0; i < text[0]; i++)
        text[i + 1] = (unsigned char)message[i];

    MoveTo(20, 265);
    DrawString(text);

    /* Stage 14C: Economy diagnostics.
       Display scenario credits and quota without changing simulation. */
    if (g_missionTestResult == 1 && g_playerHouse != NULL) {
        sprintf(message, "CREDITS: %u / %u",
                (unsigned int)g_playerHouse->credits,
                (unsigned int)g_playerHouse->creditsQuota);

        text[0] = (unsigned char)strlen(message);
        for (i = 0; i < text[0]; i++)
            text[i + 1] = (unsigned char)message[i];

        MoveTo(20, 285);
        DrawString(text);
    }

    sprintf(message, "SCRIPTS U:%d T:%d B:%d",
            g_moveScriptReady,
            g_teamScriptReady,
            g_structureScriptReady);

    text[0] = (unsigned char)strlen(message);
    for (i = 0; i < text[0]; i++)
        text[i + 1] = (unsigned char)message[i];

    MoveTo(20, 305);
    DrawString(text);

    

}


static unsigned int Mac_CountMissionUnits(void)
{
    unsigned int i;
    unsigned int count = 0;

    for (i = 0; i < 102; i++) {
        Unit *unit = Unit_Get_ByIndex(i);

        if (unit != NULL && unit->o.flags.s.used &&
            !unit->o.flags.s.isNotOnMap)
            count++;
    }

    return count;
}



static unsigned int Mac_CountMissionStructures(void)
{
    unsigned int index;
    unsigned int count = 0;

    g_firstStructureType = 0;
    g_firstStructureHouse = 0;
    g_firstStructureX = 0;
    g_firstStructureY = 0;

    for (index = 0; index < STRUCTURE_INDEX_MAX_SOFT; index++) {
        Structure *s = Structure_Get_ByIndex(index);

        if (s == NULL || !s->o.flags.s.used ||
            s->o.flags.s.isNotOnMap)
            continue;

        if (count == 0) {
            g_firstStructureType = s->o.type;
            g_firstStructureHouse = s->o.houseID;
            g_firstStructureX = s->o.position.x >> 8;
            g_firstStructureY = s->o.position.y >> 8;
        }

        count++;
    }

    return count;
}


/* Stage 11A: Mouse-based unit selection. */
static int g_selectedUnitIndex = -1;

/* Stage 15O-A: Building selection. */
static int g_selectedStructureIndex = -1;

/* Stage 15O-D: Explicit placement mode; preserve selection on bad clicks. */
static Boolean g_placingStructure = false;
static int g_placementYardIndex = -1;
static int g_placementResult = 0; /* 0 none, 1 invalid, 2 placed */

static Structure *Mac_PlacementYard(void);

/* Stage 15O-E: Mouse-aligned placement preview. */
static int g_previewTileX = -1;
static int g_previewTileY = -1;
static int g_previewValidity = 0;

static int Mac_ValidateStructureAt(int x, int y)
{
    Structure *yard = Mac_PlacementYard();
    Structure *building;
    const StructureInfo *info;
    int width, height;
    if (yard == NULL || yard->state != STRUCTURE_STATE_READY ||
        yard->o.linkedID == 0xFF) return 0;
    building = Structure_Get_ByIndex(yard->o.linkedID);
    if (building == NULL || !building->o.flags.s.used ||
        !building->o.flags.s.isNotOnMap ||
        building->o.type >= STRUCTURE_MAX ||
        building->o.houseID != yard->o.houseID) return 0;
    info = &g_table_structureInfo[building->o.type];
    width = g_table_structure_layoutSize[info->layout].width;
    height = g_table_structure_layoutSize[info->layout].height;
    if (x < 0 || y < 0 || x + width > 64 || y + height > 64)
        return 0;
    return Structure_IsValidBuildLocation(Tile_PackXY(x, y),
                                          building->o.type) != 0;
}

static void Mac_UpdatePlacementPreview(WindowPtr window)
{
    Point mouse;
    int x = -1, y = -1, valid = 0;
    Rect area;
    if (!g_placingStructure) {
        if (g_previewTileX == -1 && g_previewTileY == -1) return;
    } else if (FrontWindow() == window) {
        GetMouse(&mouse);
        if (mouse.h >= 20 && mouse.h < 340 &&
            mouse.v >= 20 && mouse.v < 212) {
            x = g_viewTileX + (mouse.h - 20) / 16;
            y = g_viewTileY + (mouse.v - 20) / 16;
            valid = Mac_ValidateStructureAt(x, y);
        }
    }
    if (x == g_previewTileX && y == g_previewTileY &&
        valid == g_previewValidity) return;
    g_previewTileX = x;
    g_previewTileY = y;
    g_previewValidity = valid;
    r3dPreviewChanges++;
    SetRect(&area, 20, 20, 340, 300);
    InvalRect(&area);
}

static void Mac_DrawPlacementPreview(void)
{
    const StructureInfo *info;
    Rect r;
    int width, height, sx, sy;
    if (!g_placingStructure || g_previewTileX < 0 ||
        g_previewTileY < 0) return;
    {
        Structure *yard = Mac_PlacementYard();
        Structure *building;
        if (yard == NULL || yard->o.linkedID == 0xFF)
            return;
        building = Structure_Get_ByIndex(yard->o.linkedID);
        if (building == NULL || !building->o.flags.s.used ||
            building->o.type >= STRUCTURE_MAX)
            return;
        info = &g_table_structureInfo[building->o.type];
    }
    width = g_table_structure_layoutSize[info->layout].width;
    height = g_table_structure_layoutSize[info->layout].height;
    sx = 20 + (g_previewTileX - g_viewTileX) * 16;
    sy = 20 + (g_previewTileY - g_viewTileY) * 16;
    SetRect(&r, sx, sy, sx + width * 16, sy + height * 16);
    {
        RgnHandle oldClip = NewRgn();
        Rect battlefield;
        GetClip(oldClip);
        SetRect(&battlefield, 20, 20, 340, 212);
        ClipRect(&battlefield);
        PenSize(2, 2);
        FrameRect(&r);
        PenSize(1, 1);
        SetClip(oldClip);
        DisposeRgn(oldClip);
    }
    PenSize(1, 1);
    MoveTo(20, 290);
    if (g_previewValidity)
        DrawString("\pSITE OK - CLICK TO PLACE");
    else
        DrawString("\pSITE BLOCKED - MOVE MOUSE");
}

static Structure *Mac_PlacementYard(void)
{
    Structure *yard;
    if (g_placementYardIndex < 0 ||
        g_placementYardIndex >= STRUCTURE_INDEX_MAX_SOFT)
        return NULL;
    yard = Structure_Get_ByIndex(g_placementYardIndex);
    if (yard == NULL || !yard->o.flags.s.used ||
        yard->o.flags.s.isNotOnMap ||
        yard->o.type != STRUCTURE_CONSTRUCTION_YARD ||
        g_playerHouse == NULL ||
        yard->o.houseID != g_playerHouse->index)
        return NULL;
    return yard;
}

static Boolean Mac_PlaceStructureAt(int x, int y)
{
    Structure *yard = Mac_PlacementYard();
    Structure *building;
    const StructureInfo *info;
    uint16 packed;
    int width, height;

    if (yard == NULL || yard->state != STRUCTURE_STATE_READY ||
        yard->o.linkedID == 0xFF) return false;
    building = Structure_Get_ByIndex(yard->o.linkedID);
    if (building == NULL || !building->o.flags.s.used ||
        !building->o.flags.s.isNotOnMap ||
        building->o.type >= STRUCTURE_MAX ||
        building->o.houseID != yard->o.houseID) return false;

    info = &g_table_structureInfo[building->o.type];
    width = g_table_structure_layoutSize[info->layout].width;
    height = g_table_structure_layoutSize[info->layout].height;
    /* Guard against layout wrapping across the 64x64 map. */
    if (x < 0 || y < 0 || x + width > 64 || y + height > 64)
        return false;
    packed = Tile_PackXY(x, y);
    if (!Mac_ValidateStructureAt(x, y)) return false;
    if (!Structure_Place(building, packed)) return false;

    /* The original GUI releases the linked object after placement. */
    yard->o.linkedID = 0xFF;
    yard->countDown = 0;
    Structure_SetState(yard, STRUCTURE_STATE_IDLE);
    g_placingStructure = false;
    g_previewTileX = -1;
    g_previewTileY = -1;
    g_placementYardIndex = -1;
    g_placementResult = 2;
    return true;
}


static Structure *Mac_SelectedStructure(void)
{
    Structure *s;

    if (g_selectedStructureIndex < 0 ||
        g_selectedStructureIndex >= STRUCTURE_INDEX_MAX_SOFT)
        return NULL;

    s = Structure_Get_ByIndex(g_selectedStructureIndex);

    if (s == NULL || !s->o.flags.s.used ||
        s->o.flags.s.isNotOnMap)
        return NULL;

    return s;
}


static Unit *Mac_SelectedUnit(void)
{
    Unit *u;

    if (g_selectedUnitIndex < 0 || g_selectedUnitIndex >= 102)
        return NULL;

    u = Unit_Get_ByIndex(g_selectedUnitIndex);

    if (u == NULL || !u->o.flags.s.used ||
        u->o.flags.s.isNotOnMap)
        return NULL;

    return u;
}

/* Stage 12A: Click a unit, then click terrain to move. */
static int g_moveDestinationX = -1;
static int g_moveDestinationY = -1;


static Boolean R3D6_IsDiscovered(int x, int y);
static Boolean R3D6_UnitVisible(const Unit *u);

static void Mac_SelectAt(Point mouse, Boolean shiftHeld, Boolean optionHeld)
{
    int x, y, best = -1;
    unsigned int i;
    Unit *selected;

    if (mouse.h < 20 || mouse.h >= 340 ||
        mouse.v < 20 || mouse.v >= 212)
        return;

    x = g_viewTileX + (mouse.h - 20) / 16;
    y = g_viewTileY + (mouse.v - 20) / 16;

    /* Placement takes precedence over selecting units or structures. */
    if (g_placingStructure) {
        g_placementResult = Mac_PlaceStructureAt(x, y) ? 2 : 1;
        return;
    }

    selected = Mac_SelectedUnit();

    /* R3D.3: Option+Shift+click issues a unit attack order.
     * The target must be an actual enemy unit under the pointer.
     * An invalid target leaves the selected unit and its orders intact.
     */
    if (shiftHeld && optionHeld) {
        int clickX = mouse.h - 20;
        int clickY = mouse.v - 20;
        int bestDistance = 999999;
        int enemyIndex = -1;

        if (selected == NULL || selected->o.houseID != g_playerHouseID)
            return;

        for (i = 0; i < 102; i++) {
            Unit *candidate = Unit_Get_ByIndex(i);
            int unitX, unitY, dx, dy, distance;

            if (candidate == NULL || !candidate->o.flags.s.used ||
                candidate->o.flags.s.isNotOnMap ||
                House_AreAllied(selected->o.houseID, candidate->o.houseID) ||
                !R3D6_UnitVisible(candidate))
                continue;

            unitX = ((int)candidate->o.position.x >> 4) - g_viewTileX * 16;
            unitY = ((int)candidate->o.position.y >> 4) - g_viewTileY * 16;
            dx = clickX - unitX;
            dy = clickY - unitY;
            if (dx < -11 || dx > 11 || dy < -11 || dy > 11)
                continue;
            distance = dx * dx + dy * dy;
            if (distance < bestDistance) {
                bestDistance = distance;
                enemyIndex = (int)i;
            }
        }

        if (enemyIndex >= 0 && g_moveScriptReady) {
            uint16 target = Tools_Index_Encode((uint16)enemyIndex, IT_UNIT);
            Unit_SetAction(selected, ACTION_ATTACK);
            Unit_SetTarget(selected, target);
            g_moveDestinationX = -1;
            g_moveDestinationY = -1;
        }
        return;
    }

    /* Select the nearest visible unit using pixel coordinates. */
    {
        int clickX = mouse.h - 20;
        int clickY = mouse.v - 20;
        int bestDistance = 999999;

        for (i = 0; i < 102; i++) {
            Unit *u = Unit_Get_ByIndex(i);
            int unitX, unitY, dx, dy, distance;
            int radius;

            if (u == NULL || !u->o.flags.s.used ||
                u->o.flags.s.isNotOnMap)
                continue;

            if (!R3D6_UnitVisible(u)) continue;

            unitX = ((int)u->o.position.x >> 4) -
                    g_viewTileX * 16;
            unitY = ((int)u->o.position.y >> 4) -
                    g_viewTileY * 16;

            dx = clickX - unitX;
            dy = clickY - unitY;

            radius = 11;

            if (dx < -radius || dx > radius ||
                dy < -radius || dy > radius)
                continue;

            distance = dx * dx + dy * dy;

            if (distance < bestDistance) {
                bestDistance = distance;
                best = (int)i;
            }
        }
    }

    if (best >= 0) {
        g_selectedStructureIndex = -1;

        if (best == g_selectedUnitIndex)
            g_selectedUnitIndex = -1;
        else
            g_selectedUnitIndex = best;

        g_moveDestinationX = -1;
        g_moveDestinationY = -1;
        return;
    }

    /* Select an existing building by any occupied map tile. */
    if (!shiftHeld && x >= 0 && x < 64 && y >= 0 && y < 64) {
        Structure *structure =
            Structure_Get_ByPackedTile(Tile_PackXY(x, y));

        if (structure != NULL &&
            structure->o.flags.s.used &&
            !structure->o.flags.s.isNotOnMap &&
            (structure->o.houseID == g_playerHouseID ||
             R3D6_IsDiscovered(x, y))) {

            int index = (int)structure->o.index;

            g_selectedStructureIndex =
                (g_selectedStructureIndex == index) ? -1 : index;

            g_selectedUnitIndex = -1;
            g_moveDestinationX = -1;
            g_moveDestinationY = -1;
            return;
        }
    }

    if (selected != NULL && g_moveScriptReady && shiftHeld) {
        uint16 packed = Tile_PackXY(x, y);
        uint16 destination = Tools_Index_Encode(packed, IT_TILE);

        Unit_SetAction(selected, ACTION_MOVE);
        Unit_SetDestination(selected, destination);

        g_moveDestinationX = x;
        g_moveDestinationY = y;
    } else {
        g_selectedUnitIndex = -1;
        g_selectedStructureIndex = -1;
    }
}


/* Stage 15O-P.4: temporary, always-visible diagnostic HUD. */

/* Stage 15O-P.6: read-only Harvester and mission debugger. */
static uint32 g_profSimCalls = 0;


/* Stage 15O-P.8: independent, read-only performance sampler. */
static uint32 stage15OP8LastSample = 0;
static uint32 stage15OP8LastCalls = 0;
static uint32 stage15OP8LastSkipped = 0;
static uint32 stage15OP8LastGame = 0;
static uint32 stage15OP8LastDiscarded = 0;

static uint32 stage15OP8SimRate = 0;
static uint32 stage15OP8SkipRate = 0;
static uint32 stage15OP8GameRate = 0;
static uint32 stage15OP8DiscardRate = 0;
static uint32 stage15OP8LastHUD = 0;

static void Stage15OP8_SamplePerformance(void)
{
    uint32 now = TickCount();
    uint32 elapsed;

    if (stage15OP8LastSample == 0) {
        stage15OP8LastSample = now;
        stage15OP8LastCalls = g_profSimCalls;
        stage15OP8LastSkipped = g_perfSkippedTicks;
        stage15OP8LastGame = g_timerGame;
        stage15OP8LastDiscarded = g_macTimerDiscardedTicks;
        return;
    }

    elapsed = now - stage15OP8LastSample;

    if (elapsed < 60)
        return;

    stage15OP8SimRate =
        (g_profSimCalls - stage15OP8LastCalls) * 60UL / elapsed;

    stage15OP8SkipRate =
        (g_perfSkippedTicks - stage15OP8LastSkipped) * 60UL / elapsed;

    stage15OP8GameRate =
        (g_timerGame - stage15OP8LastGame) * 60UL / elapsed;

    stage15OP8DiscardRate =
        (g_macTimerDiscardedTicks - stage15OP8LastDiscarded)
        * 60UL / elapsed;

    stage15OP8LastSample = now;
    stage15OP8LastCalls = g_profSimCalls;
    stage15OP8LastSkipped = g_perfSkippedTicks;
    stage15OP8LastGame = g_timerGame;
    stage15OP8LastDiscarded = g_macTimerDiscardedTicks;
}

/* H1C trace page: H cycles pages while Harvester is selected. */
static unsigned int s_h1cPage = 0;
extern uint16 H1C_TraceCount(void);
extern int H1C_TraceGet(uint16, uint16 *, uint16 *, uint16 *, uint16 *,
    uint32 *, uint16 *, uint16 *, uint16 *, uint16 *, uint16 *, uint16 *, uint16 *);

static void Stage15OP6_DrawHarvesterDiagnostics(void)
{
    Rect area;
    char message[128];
    Str255 text;
    Unit *harvester = NULL;
    unsigned int count = 0;
    unsigned int index;
    unsigned int line;
    unsigned int i;

    SetRect(&area, 20, 383, 440, 472);
    EraseRect(&area);

    TextFont(0);
    TextSize(9);
    TextFace(0);

    /* Inspect every active Harvester, regardless of ownership. */
    for (index = 0; index < 102; index++) {
        Unit *candidate = Unit_Get_ByIndex(index);

        if (candidate == NULL)
            continue;

        if (!candidate->o.flags.s.used)
            continue;

        if (candidate->o.type != UNIT_HARVESTER)
            continue;

        count++;

        if (harvester == NULL)
            harvester = candidate;
    }

    sprintf(message,
            "P8 SIM:%lu SKIP:%lu GAME:%lu LOST:%lu /s",
            (unsigned long)stage15OP8SimRate,
            (unsigned long)stage15OP8SkipRate,
            (unsigned long)stage15OP8GameRate,
            (unsigned long)stage15OP8DiscardRate);

    text[0] = (unsigned char)strlen(message);
    for (i = 0; i < text[0]; i++)
        text[i + 1] = (unsigned char)message[i];

    MoveTo(20, 397);
    DrawString(text);

    /* R3D8C: suppress Mission 1-only harvester diagnostics. */
    /* H1A: inspect the Harvester in Mission 2 as well. */


    if (harvester != NULL) {
        sprintf(message,
                "UNIT:%u HOUSE:%u MAP:%u LOAD:%u",
                (unsigned int)harvester->o.index,
                (unsigned int)harvester->o.houseID,
                harvester->o.flags.s.isNotOnMap ? 0U : 1U,
                (unsigned int)harvester->amount);

        text[0] = (unsigned char)strlen(message);
        for (i = 0; i < text[0]; i++)
            text[i + 1] = (unsigned char)message[i];

        MoveTo(20, 410);
        DrawString(text);

        sprintf(message,
                "ACT:%u NEXT:%u DEST:%u DELAY:%u",
                (unsigned int)harvester->actionID,
                (unsigned int)harvester->nextActionID,
                (unsigned int)harvester->targetMove,
                (unsigned int)harvester->o.script.delay);

        text[0] = (unsigned char)strlen(message);
        for (i = 0; i < text[0]; i++)
            text[i + 1] = (unsigned char)message[i];

        MoveTo(20, 423);
        DrawString(text);

        sprintf(message,
                "POS:%u,%u LAST:%u,%u",
                (unsigned int)(harvester->o.position.x >> 8),
                (unsigned int)(harvester->o.position.y >> 8),
                (unsigned int)(harvester->targetLast.x >> 8),
                (unsigned int)(harvester->targetLast.y >> 8));

    } else {
        sprintf(message, "NO ACTIVE HARVESTER FOUND");

        text[0] = (unsigned char)strlen(message);
        for (i = 0; i < text[0]; i++)
            text[i + 1] = (unsigned char)message[i];

        MoveTo(20, 410);
        DrawString(text);

        sprintf(message, "WAITING FOR HARVESTER DELIVERY");
    }

    text[0] = (unsigned char)strlen(message);
    for (i = 0; i < text[0]; i++)
        text[i + 1] = (unsigned char)message[i];

    MoveTo(20, 436);
    DrawString(text);

    sprintf(message,
            "WIN:%u FLAGS:%u TICK:%lu",
            stage15ON1MissionWon ? 1U : 0U,
            (unsigned int)g_scenario.winFlags,
            (unsigned long)g_timerGame);

    text[0] = (unsigned char)strlen(message);
    for (i = 0; i < text[0]; i++)
        text[i + 1] = (unsigned char)message[i];

    MoveTo(20, 449);
    DrawString(text);
}


/* R3D8D-B1.2: status is drawn AFTER the harvester panel clears y=300..375. */
static unsigned int g_b12FactoryKeyResult = 0;
static unsigned int g_c1BarracksKeyResult = 0;
static unsigned int g_c2OutpostKeyResult = 0;

static void Mac_DrawFactoryDiagnostics(void)
{
    Structure *yard = Mac_SelectedStructure();
    const StructureInfo *info = &g_table_structureInfo[STRUCTURE_LIGHT_VEHICLE];
    const StructureInfo *barracksInfo = &g_table_structureInfo[STRUCTURE_BARRACKS];
    Rect area;
    Str255 text;
    char message[128];
    uint32 mask;
    uint32 built;
    uint32 missing;
    unsigned int i;

    SetRect(&area, 20, 318, 620, 390);
    EraseRect(&area);
    if (yard == NULL || g_playerHouse == NULL ||
        yard->o.type != STRUCTURE_CONSTRUCTION_YARD ||
        yard->o.houseID != g_playerHouse->index) return;

    mask = Structure_GetBuildable(yard);
    built = House_Get_ByIndex(yard->o.houseID)->structuresBuilt;
    missing = info->o.structuresRequired & ~built;
    TextFont(0); TextSize(9); TextFace(0);
    sprintf(message, "B12 CAM:%u MASK:%08lX L:%u ST:%d KEY:%u",
            (unsigned int)g_campaignID, (unsigned long)mask,
            (unsigned int)((mask & FLAG_STRUCTURE_LIGHT_VEHICLE) != 0),
            (int)yard->state, g_b12FactoryKeyResult);
    text[0] = (unsigned char)strlen(message);
    for (i = 0; i < text[0]; i++) text[i + 1] = (unsigned char)message[i];
    MoveTo(20, 331); DrawString(text);

    sprintf(message, "FACTORY REQ:%08lX MISS:%08lX UP:%u/%u HOUSE:%u",
            (unsigned long)info->o.structuresRequired, (unsigned long)missing,
            (unsigned int)yard->upgradeLevel,
            (unsigned int)info->o.upgradeLevelRequired,
            (unsigned int)((info->o.availableHouse & (1 << yard->o.houseID)) != 0));
    text[0] = (unsigned char)strlen(message);
    for (i = 0; i < text[0]; i++) text[i + 1] = (unsigned char)message[i];
    MoveTo(20, 348); DrawString(text);

    sprintf(message, "C1 BARRACKS B:%u KEY:%u MISS:%08lX UP:%u/%u",
            (unsigned int)((mask & (1UL << STRUCTURE_BARRACKS)) != 0),
            g_c1BarracksKeyResult,
            (unsigned long)(barracksInfo->o.structuresRequired & ~built),
            (unsigned int)yard->upgradeLevel,
            (unsigned int)barracksInfo->o.upgradeLevelRequired);
    text[0] = (unsigned char)strlen(message);
    for (i = 0; i < text[0]; i++) text[i + 1] = (unsigned char)message[i];
    MoveTo(20, 365); DrawString(text);

    sprintf(message, "C2 OUTPOST A:%u KEY:%u MISS:%08lX",
            (unsigned int)((mask & (1UL << STRUCTURE_OUTPOST)) != 0),
            g_c2OutpostKeyResult,
            (unsigned long)(g_table_structureInfo[STRUCTURE_OUTPOST].o.structuresRequired & ~built));
    text[0] = (unsigned char)strlen(message);
    for (i = 0; i < text[0]; i++) text[i + 1] = (unsigned char)message[i];
    MoveTo(20, 382); DrawString(text);
}

/* D1: Barracks training and deployment. Keep the linked unit until an
 * unoccupied tile is available, then release it using original engine APIs. */
static unsigned int g_d1SoldierKeyResult = 0;
static unsigned int g_d1DeployedCount = 0;
static unsigned int g_d1BlockedExit = 0;
static unsigned int g_g2TrikeResult = 0;
static unsigned int g_g2TrikesDeployed = 0;
static unsigned int g_g2BlockedExit = 0;

static void Mac_D1DeployInfantry(void)
{
    Structure *s;
    PoolFindStruct find;
    g_d1BlockedExit = 0;
    find.houseID = g_playerHouseID;
    find.index = 0xFFFF;
    find.type = STRUCTURE_BARRACKS;
    while ((s = Structure_Find(&find)) != NULL) {
        Unit *u;
        uint16 position;
        tile32 tile;
        if (s->state != STRUCTURE_STATE_READY ||
            s->o.linkedID == 0xFF || s->countDown != 0) continue;
        u = Unit_Get_ByIndex(s->o.linkedID);
        if (u == NULL) continue;
        position = Structure_FindFreePosition(s, false);
        if (position == 0) {
            g_d1BlockedExit = 1;
            continue;
        }
        tile = Tile_Center(Tile_UnpackTile(position));
        if (!Unit_SetPosition(u, tile)) {
            g_d1BlockedExit = 1;
            continue;
        }
        u->o.seenByHouses |= s->o.seenByHouses;
        s->o.linkedID = u->o.linkedID;
        u->o.linkedID = 0xFF;
        Unit_SetOrientation(u,
            Tile_GetDirection(s->o.position, u->o.position) & 0xE0,
            true, 0);
        Unit_SetOrientation(u, u->orientation[0].current, true, 1);
        if (s->o.linkedID == 0xFF)
            Structure_SetState(s, STRUCTURE_STATE_IDLE);
        g_d1DeployedCount++;
    }
}

/* G2: Deploy completed Light Factory vehicles using the existing
 * linked-unit lifecycle, leaving production READY if the exit is blocked. */
static void Mac_G2DeployTrikes(void)
{
    Structure *s;
    PoolFindStruct find;
    g_g2BlockedExit = 0;
    find.houseID = g_playerHouseID;
    find.index = 0xFFFF;
    find.type = STRUCTURE_LIGHT_VEHICLE;
    while ((s = Structure_Find(&find)) != NULL) {
        Unit *u;
        uint16 position;
        tile32 tile;
        if (s->state != STRUCTURE_STATE_READY ||
            s->o.linkedID == 0xFF || s->countDown != 0) continue;
        u = Unit_Get_ByIndex(s->o.linkedID);
        if (u == NULL || u->o.type != UNIT_TRIKE) continue;
        position = Structure_FindFreePosition(s, false);
        if (position == 0) {
            g_g2BlockedExit = 1;
            continue;
        }
        tile = Tile_Center(Tile_UnpackTile(position));
        if (!Unit_SetPosition(u, tile)) {
            g_g2BlockedExit = 1;
            continue;
        }
        u->o.seenByHouses |= s->o.seenByHouses;
        s->o.linkedID = u->o.linkedID;
        u->o.linkedID = 0xFF;
        Unit_SetOrientation(u,
            Tile_GetDirection(s->o.position, u->o.position) & 0xE0,
            true, 0);
        Unit_SetOrientation(u, u->orientation[0].current, true, 1);
        if (s->o.linkedID == 0xFF)
            Structure_SetState(s, STRUCTURE_STATE_IDLE);
        g_g2TrikesDeployed++;
    }
}

/* UI1: Read-only, low-cost sidebar. No additional CopyBits and no gameplay
 * mutations. The fixed 85px panel fits the existing 440px window. */
static void UI1_DrawLine(int y, const char *message)
{
    Str255 pascalText;
    unsigned int i, n = (unsigned int)strlen(message);
    if (n > 255) n = 255;
    pascalText[0] = (unsigned char)n;
    for (i = 0; i < n; i++) pascalText[i + 1] = (unsigned char)message[i];
    MoveTo(354, y);
    DrawString(pascalText);
}

/* R4-G1-UI2: compact mouse actions; no additional frame buffer. */
static void G1_DrawAt(int x, int y, const char *message)
{
    Str255 p;
    unsigned int i, n = (unsigned int)strlen(message);
    if (n > 255) n = 255;
    p[0] = (unsigned char)n;
    for (i = 0; i < n; ++i) p[i + 1] = (unsigned char)message[i];
    MoveTo(x, y);
    DrawString(p);
}
static void G1_DrawCommands(Structure *s)
{
    static const char *yardLabels[8] = {
        "WIND", "SLAB", "REF", "BARR", "SILO", "FACT", "PLACE", "CANCEL"
    };
    unsigned int i;
    if (s == NULL) return;
    if (s->o.type == STRUCTURE_CONSTRUCTION_YARD) {
        for (i = 0; i < 8; ++i)
            G1_DrawAt((i & 1) ? 397 : 354, 151 + (int)(i / 2) * 16, yardLabels[i]);
    } else if (s->o.type == STRUCTURE_BARRACKS) {
        G1_DrawAt(354, 151, "TRAIN SOLDIER");
    } else if (s->o.type == STRUCTURE_LIGHT_VEHICLE) {
        G1_DrawAt(354, 151, "BUILD TRIKE");
        G1_DrawAt(354, 170, "G2 TRIKE UI");
    }
}
static Boolean G1_HandleSidebarClick(Point mouse)
{
    Structure *s;
    unsigned int index;
    uint8 type;
    if (mouse.h < 348 || mouse.h >= 438 || mouse.v < 20 || mouse.v >= 219)
        return false;
    s = Mac_SelectedStructure();
    if (s == NULL || g_playerHouse == NULL ||
        s->o.houseID != g_playerHouse->index) return true;
    if (s->o.type == STRUCTURE_BARRACKS) {
        if (mouse.v >= 141 && mouse.v < 158 && s->state == STRUCTURE_STATE_IDLE &&
            s->o.linkedID == 0xFF &&
            (Structure_GetBuildable(s) & FLAG_UNIT_SOLDIER) != 0)
            g_d1SoldierKeyResult = Structure_BuildObject(s, UNIT_SOLDIER) ? 5 : 4;
        return true;
    }
    if (s->o.type == STRUCTURE_LIGHT_VEHICLE) {
        if (mouse.v >= 141 && mouse.v < 158) {
            if (s->state != STRUCTURE_STATE_IDLE || s->o.linkedID != 0xFF)
                g_g2TrikeResult = 2;
            else if ((Structure_GetBuildable(s) & (1UL << UNIT_TRIKE)) == 0)
                g_g2TrikeResult = 3;
            else
                g_g2TrikeResult = Structure_BuildObject(s, UNIT_TRIKE) ? 5 : 4;
        }
        return true;
    }
    if (s->o.type != STRUCTURE_CONSTRUCTION_YARD ||
        mouse.v < 141 || mouse.v >= 205) return true;
    index = (unsigned int)((mouse.v - 141) / 16) * 2 + (mouse.h >= 394 ? 1 : 0);
    if (index == 6) {
        if (s->state == STRUCTURE_STATE_READY && s->o.linkedID != 0xFF) {
            g_placingStructure = true;
            g_placementYardIndex = s->o.index;
            g_placementResult = 0;
        }
    } else if (index == 7) {
        (void)Mac_CancelYardProduction(s);
    } else if (index < 6 && s->state == STRUCTURE_STATE_IDLE) {
        static const uint8 types[6] = {
            STRUCTURE_WINDTRAP, STRUCTURE_SLAB_1x1,
            STRUCTURE_REFINERY, STRUCTURE_BARRACKS,
            STRUCTURE_SILO, STRUCTURE_LIGHT_VEHICLE
        };
        type = types[index];
        if ((Structure_GetBuildable(s) & (1UL << type)) != 0)
            (void)Structure_BuildObject(s, type);
    }
    return true;
}

static void UI1_DrawSidebar(void)
{
    Rect panel, rule;
    Structure *s = Mac_SelectedStructure();
    Unit *u = Mac_SelectedUnit();
    char line[64];

    SetRect(&panel, 348, 20, 438, 219);
    EraseRect(&panel);
    FrameRect(&panel);
    TextFont(0);
    TextSize(9);
    TextFace(bold);
    UI1_DrawLine(33, "ATREIDES");
    TextFace(0);
    SetRect(&rule, 351, 38, 435, 39);
    PaintRect(&rule);
    if (g_playerHouse != NULL) {
        sprintf(line, "CR %u/%u", (unsigned int)g_playerHouse->credits,
                (unsigned int)g_playerHouse->creditsStorage);
        UI1_DrawLine(52, line);
        sprintf(line, "PWR %u/%u", (unsigned int)g_playerHouse->powerProduction,
                (unsigned int)g_playerHouse->powerUsage);
        UI1_DrawLine(65, line);
    }
    SetRect(&rule, 351, 72, 435, 73);
    PaintRect(&rule);
    if (s != NULL) {
        switch (s->o.type) {
            case STRUCTURE_CONSTRUCTION_YARD: UI1_DrawLine(87, "CONSTR YARD"); break;
            case STRUCTURE_BARRACKS: UI1_DrawLine(87, "BARRACKS"); break;
            case STRUCTURE_LIGHT_VEHICLE: UI1_DrawLine(87, "LIGHT FACTORY"); break;
            case STRUCTURE_OUTPOST: UI1_DrawLine(87, "OUTPOST"); break;
            default:
                sprintf(line, "BUILDING %u", (unsigned int)s->o.type);
                UI1_DrawLine(87, line);
                break;
        }
        sprintf(line, "HP %u/%u", (unsigned int)s->o.hitpoints,
                (unsigned int)s->hitpointsMax);
        UI1_DrawLine(101, line);
        if (s->state == STRUCTURE_STATE_BUSY) {
            UI1_DrawLine(118, s->o.flags.s.onHold ? "NEED CREDITS" : "PRODUCING");
            sprintf(line, "TIMER %u", (unsigned int)s->countDown);
            UI1_DrawLine(131, line);
        } else if (s->state == STRUCTURE_STATE_READY) {
            UI1_DrawLine(118, "READY");
        } else {
            UI1_DrawLine(118, "IDLE");
        }
        G1_DrawCommands(s);
    } else if (u != NULL) {
        UI1_DrawLine(87, "UNIT SELECTED");
        sprintf(line, "TYPE %u", (unsigned int)u->o.type);
        UI1_DrawLine(101, line);
        sprintf(line, "HP %u", (unsigned int)u->o.hitpoints);
        UI1_DrawLine(115, line);
        /* H1C: normal state page, or 3 trace events per page. */
        if (u->o.type == UNIT_HARVESTER) {
            if (s_h1cPage == 0) {
                sprintf(line, "ID %u LOAD %u", (unsigned int)u->o.index,
                        (unsigned int)u->amount);
                UI1_DrawLine(135, line);
                sprintf(line, "ACT %u NEXT %u", (unsigned int)u->actionID,
                        (unsigned int)u->nextActionID);
                UI1_DrawLine(148, line);
                sprintf(line, "TARGET %u", (unsigned int)u->targetMove);
                UI1_DrawLine(161, line);
                sprintf(line, "DXY %u,%u", (unsigned int)u->currentDestination.x,
                        (unsigned int)u->currentDestination.y);
                UI1_DrawLine(174, line);
                sprintf(line, "D%u T%lu", (unsigned int)u->o.script.delay,
                        (unsigned long)g_timerGame);
                UI1_DrawLine(187, line);
            } else {
                unsigned int j;
                unsigned int baseAge = (s_h1cPage - 1) * 2;
                sprintf(line, "H LOG %u/8", s_h1cPage);
                UI1_DrawLine(126, line);
                for (j = 0; j < 2; j++) {
                    uint16 id, oldAction, newAction, reason, cargo, target, dx, dy, delay, terrain, packed;
                    uint32 tick;
                    if (!H1C_TraceGet(baseAge + j, &id, &oldAction,
                        &newAction, &reason, &tick, &cargo, &target, &dx, &dy, &delay, &terrain, &packed)) break;
                    sprintf(line, "%u:%u>%u R%u", (unsigned int)id,
                        (unsigned int)oldAction, (unsigned int)newAction, (unsigned int)reason);
                    UI1_DrawLine(140 + j * 23, line);
                    sprintf(line, "T%lu L%u G%u", (unsigned long)tick,
                        (unsigned int)cargo, (unsigned int)terrain);
                    UI1_DrawLine(151 + j * 23, line);
                }
            }
        }
    } else {
        UI1_DrawLine(87, "NO SELECTION");
    }
    if (s != NULL && s->o.type == STRUCTURE_LIGHT_VEHICLE) {
        char g2Line[48];
        sprintf(g2Line, "TRIKE:%u OUT:%u", g_g2TrikeResult, g_g2TrikesDeployed);
        UI1_DrawLine(191, g_g2BlockedExit ? "EXIT BLOCKED" : g2Line);
    }
    UI1_DrawLine(209, "G2 MOUSE UI");
}

static void Mac_DrawSelectedStructure(void)
{
    Structure *s = Mac_SelectedStructure();
    const StructureInfo *si;
    Rect r;
    int x, y, width, height;
    char message[100];
    Str255 text;
    unsigned int i;

    if (s == NULL)
        return;

    si = &g_table_structureInfo[s->o.type];

    x = (int)(s->o.position.x >> 8) - g_viewTileX;
    y = (int)(s->o.position.y >> 8) - g_viewTileY;

    width = g_table_structure_layoutSize[si->layout].width;
    height = g_table_structure_layoutSize[si->layout].height;

    SetRect(&r,
            20 + x * 16,
            20 + y * 16,
            20 + (x + width) * 16,
            20 + (y + height) * 16);

    /* R.2B: structure outline is composed offscreen. */

    TextFont(0);
    TextSize(10);
    TextFace(0);

    sprintf(message,
            "BUILDING TYPE:%u  HOUSE:%u  HP:%u/%u  STATE:%d",
            (unsigned int)s->o.type,
            (unsigned int)s->o.houseID,
            (unsigned int)s->o.hitpoints,
            (unsigned int)s->hitpointsMax,
            (int)s->state);

    text[0] = (unsigned char)strlen(message);

    for (i = 0; i < text[0]; i++)
        text[i + 1] = (unsigned char)message[i];

    MoveTo(20, 235);
    DrawString(text);

    /* Live economy: visible even when older diagnostics are hidden. */
    if (g_playerHouse != NULL) {
        sprintf(message, "CR:%u/%u PWR:%u/%u",
                (unsigned int)g_playerHouse->credits,
                (unsigned int)g_playerHouse->creditsQuota,
                (unsigned int)g_playerHouse->powerProduction,
                (unsigned int)g_playerHouse->powerUsage);
        text[0] = (unsigned char)strlen(message);
        for (i = 0; i < text[0]; i++)
            text[i + 1] = (unsigned char)message[i];
        MoveTo(20, 255);
        DrawString(text);
    }

    /* Stage 15O-C: Original Construction Yard production status. */
    if (s->o.type == STRUCTURE_CONSTRUCTION_YARD) {
        if (s->state == STRUCTURE_STATE_READY) {
            sprintf(message, g_placingStructure ?
                    "PLACE BUILDING: CLICK MAP" :
                    "PRODUCTION: READY - PRESS P");
        } else if (s->state == STRUCTURE_STATE_BUSY) {
            sprintf(message, s->o.flags.s.onHold ?
                    "PRODUCTION: WAITING FOR CREDITS  TIMER:%u  X CANCEL" :
                    "PRODUCTION: BUILDING  TIMER:%u  X CANCEL",
                    (unsigned int)s->countDown);
        } else {
            sprintf(message, "IDLE: W WIND C SLAB F REF L FACT B BARR A RAD");
        }

        text[0] = (unsigned char)strlen(message);
        for (i = 0; i < text[0]; i++)
            text[i + 1] = (unsigned char)message[i];

        MoveTo(20, 275);
        DrawString(text);
    } else if (s->o.type == STRUCTURE_BARRACKS &&
               g_playerHouse != NULL &&
               s->o.houseID == g_playerHouse->index) {
        if (s->state == STRUCTURE_STATE_READY && s->o.linkedID != 0xFF)
            sprintf(message, "D1 SOLDIER READY - %s  DEP:%u KEY:%u",
                    g_d1BlockedExit ? "WAITING FOR EXIT" : "DEPLOYING",
                    g_d1DeployedCount, g_d1SoldierKeyResult);
        else if (s->state == STRUCTURE_STATE_BUSY)
            sprintf(message, "D1 SOLDIER %s TIMER:%u  I TRAIN KEY:%u",
                    s->o.flags.s.onHold ? "WAITING FOR CREDITS" : "TRAINING",
                    (unsigned int)s->countDown, g_d1SoldierKeyResult);
        else
            sprintf(message, "D1 BARRACKS IDLE - I TRAIN SOLDIER  KEY:%u DEP:%u",
                    g_d1SoldierKeyResult, g_d1DeployedCount);
        text[0] = (unsigned char)strlen(message);
        for (i = 0; i < text[0]; i++) text[i + 1] = (unsigned char)message[i];
        MoveTo(20, 275);
        DrawString(text);
    } else if (g_playerHouse != NULL &&
               s->o.houseID == g_playerHouse->index) {
        unsigned long mask = (unsigned long)Structure_GetBuildable(s);
        sprintf(message, "R3D8D TECH:%u MASK:%08lX ST:%d CD:%u",
                (unsigned int)g_campaignID, mask,
                (int)s->state, (unsigned int)s->countDown);
        text[0] = (unsigned char)strlen(message);
        for (i = 0; i < text[0]; i++)
            text[i + 1] = (unsigned char)message[i];
        MoveTo(20, 275);
        DrawString(text);
    }

        if (g_placingStructure) return;
    if (g_placementResult == 1) {
        MoveTo(20, 290);
        DrawString("\pINVALID SITE - TRY AGAIN");
    } else if (g_placementResult == 2) {
        MoveTo(20, 290);
        DrawString("\pCONSTRUCTION PLACED");
    }
}

static void Mac_DrawSelectedUnit(void)
{
    Unit *u = Mac_SelectedUnit();
    Rect r;
    int x, y;
    char message[64];
    Str255 text;
    unsigned int i;

    if (u == NULL)
        return;

    if (g_moveDestinationX >= 0 && g_moveDestinationY >= 0) {
        int dx = g_moveDestinationX - g_viewTileX;
        int dy = g_moveDestinationY - g_viewTileY;

        if (dx >= 0 && dx < 20 && dy >= 0 && dy < 12) {
            SetRect(&r,
                    23 + dx * 16, 23 + dy * 16,
                    33 + dx * 16, 33 + dy * 16);
            /* R.2B: outline composed offscreen. */
        }
    }

    x = ((int)u->o.position.x >> 4) -
        g_viewTileX * 16;
    y = ((int)u->o.position.y >> 4) -
        g_viewTileY * 16;

    if (x >= -16 && x < 320 &&
        y >= -16 && y < 192) {
        SetRect(&r,
                12 + x, 12 + y,
                28 + x, 28 + y);
        /* R.2B: outline composed offscreen. */
    }

    /* Selection graphics complete; skip diagnostic text. */
    return;
    TextSize(12);

    sprintf(message, "UNIT: TYPE %u HOUSE %u  TILE: %u,%u",
            (unsigned int)u->o.type,
            (unsigned int)Unit_GetHouseID(u),
            (unsigned int)(u->o.position.x >> 8),
            (unsigned int)(u->o.position.y >> 8));

    text[0] = (unsigned char)strlen(message);
    for (i = 0; i < text[0]; i++)
        text[i + 1] = (unsigned char)message[i];

    MoveTo(20, 235);
    DrawString(text);

    if (g_moveDestinationX >= 0 && g_moveDestinationY >= 0) {
        sprintf(message, "LAST ORDER: X %d  Y %d",
                g_moveDestinationX, g_moveDestinationY);
    } else {
        sprintf(message, "LAST ORDER: NONE");
    }

    text[0] = (unsigned char)strlen(message);
    for (i = 0; i < text[0]; i++)
        text[i + 1] = (unsigned char)message[i];

    MoveTo(20, 252);
    DrawString(text);

    sprintf(message, "VIEWPORT: X %d  Y %d",
            g_viewTileX, g_viewTileY);

    text[0] = (unsigned char)strlen(message);
    for (i = 0; i < text[0]; i++)
        text[i + 1] = (unsigned char)message[i];

    MoveTo(20, 269);
    DrawString(text);

}

/* Stage 15O-I.2: cache Format80-decoded unit sprites by sprite ID.
 * The sprite collection is loaded once for the current mission.
 * Keep allocation bounded to 355 entries, each at most 4096 bytes.
 * If allocation fails, fall back to the original scratch buffer. */
#define MAC_UNIT_SPRITE_COUNT 355
static unsigned char *g_cachedUnitSpritePixels[MAC_UNIT_SPRITE_COUNT];
static const unsigned char *g_cachedUnitSpriteSource[MAC_UNIT_SPRITE_COUNT];
static unsigned int g_cachedUnitSpriteSize[MAC_UNIT_SPRITE_COUNT];

/* R3D.6-A: permanent OpenDUNE exploration state. */
static Boolean r3d6ShroudEnabled = true;
static uint16 r3d6LastUnitTile[102];
static Boolean r3d6ExplorationStarted = false;

static Boolean R3D6_IsDiscovered(int x, int y)
{
    if (!r3d6ShroudEnabled) return true;
    if (x < 0 || x >= 64 || y < 0 || y >= 64) return false;
    return g_map[y * 64 + x].isUnveiled != 0;
}

static Boolean R3D6_UnitVisible(const Unit *u)
{
    int x, y;
    if (u == NULL) return false;
    if (!r3d6ShroudEnabled) return true;
    if (House_AreAllied(u->o.houseID, g_playerHouseID)) return true;
    x = (int)u->o.position.x >> 8;
    y = (int)u->o.position.y >> 8;
    return R3D6_IsDiscovered(x, y);
}

static void R3D8C_SetInitialCamera(void)
{
    int x = -1, y = -1;
    int minX, maxX, minY, maxY;
    unsigned int i;
    for (i = 0; i < 80; ++i) {
        Structure *st = Structure_Get_ByIndex(i);
        if (st != NULL && st->o.flags.s.used &&
            !st->o.flags.s.isNotOnMap &&
            st->o.houseID == g_playerHouseID) {
            x = (int)(st->o.position.x >> 8);
            y = (int)(st->o.position.y >> 8);
            break;
        }
    }
    if (x < 0) {
        for (i = 0; i < 102; ++i) {
            Unit *u = Unit_Get_ByIndex(i);
            if (u != NULL && u->o.flags.s.used &&
                !u->o.flags.s.isNotOnMap &&
                u->o.houseID == g_playerHouseID) {
                x = (int)(u->o.position.x >> 8);
                y = (int)(u->o.position.y >> 8);
                break;
            }
        }
    }
    if (x < 0) return;
    Mac_GetScrollBounds(&minX, &maxX, &minY, &maxY);
    x -= 10;
    y -= 6;
    if (x < minX) x = minX;
    if (x > maxX) x = maxX;
    if (y < minY) y = minY;
    if (y > maxY) y = maxY;
    g_viewTileX = x;
    g_viewTileY = y;
}

static void R3D6_UpdateExploration(void)
{
    unsigned int i;
    if (g_missionTestResult != 1) return;
    if (!r3d6ExplorationStarted) {
        for (i = 0; i < 102; ++i) r3d6LastUnitTile[i] = 0xFFFF;
        for (i = 0; i < 80; ++i) {
            Structure *st = Structure_Get_ByIndex(i);
            if (st != NULL && st->o.flags.s.used &&
                !st->o.flags.s.isNotOnMap &&
                st->o.houseID == g_playerHouseID)
                Structure_RemoveFog(st);
        }
        r3d6ExplorationStarted = true;
    }
    for (i = 0; i < 102; ++i) {
        Unit *u = Unit_Get_ByIndex(i);
        uint16 packed = 0xFFFF;
        if (u != NULL && u->o.flags.s.used &&
            !u->o.flags.s.isNotOnMap &&
            House_AreAllied(u->o.houseID, g_playerHouseID)) {
            int x = (int)u->o.position.x >> 8;
            int y = (int)u->o.position.y >> 8;
            if (x >= 0 && x < 64 && y >= 0 && y < 64)
                packed = (uint16)(y * 64 + x);
        }
        if (packed != r3d6LastUnitTile[i]) {
            r3d6LastUnitTile[i] = packed;
            if (packed != 0xFFFF) Unit_RemoveFog(u);
        }
    }
}


/* R3D7C2.1: Sprite bounds for future dirty-rectangle composition.
 * Metadata only. The proven full-frame renderer remains unchanged.
 */
typedef struct R3D7C2_Bounds {
    short left, top, right, bottom;
    Boolean valid;
    unsigned short spriteID;
    unsigned char mirror;
    unsigned char houseID;
} R3D7C2_Bounds;

#include "dirty_closure.h"
static Boolean b2ScanMode = false;
static Boolean b2ForceFull = true;
static unsigned char b2Redraw[102];
static unsigned char b2Dirty[12][20];
static unsigned long b2DrawnUnits = 0;
static unsigned long b2SkippedUnits = 0;
static R3D7C2_Bounds r3d7c2Previous[102];
static R3D7C2_Bounds r3d7c2Current[102];
static unsigned long r3d7c2ChangedSlots = 0;
static unsigned long r3d7c2Frames = 0;
/* R3D7C2.2 diagnostic only: number of frames with changes and
 * approximate bounding-box coverage; no partial blits yet. */
static unsigned long r3d7c2DirtyFrames = 0;
static unsigned long r3d7c2DirtyPixels = 0;
static unsigned long r3d7c2AnimationChanges = 0;
static int r3d7c2ViewX = -1, r3d7c2ViewY = -1;
/* R3D7C2.3 coarse sprite-only coverage; NOT a total dirty-region measure.
 * Terrain, fog, selection, exposure and UI changes are excluded.
 */
static unsigned char r3d7c23Cells[12][20];
static unsigned long r3d7c23CoveredCells = 0;
static unsigned long r3d7c23ScrollFrames = 0;
static unsigned long r3d7c23ChangedFrames = 0;
static unsigned long r3d7c23FrameCount = 0;
static void R3D7C23_MarkRect(int left, int top, int right, int bottom)
{
    int x, y, x0, x1, y0, y1;
    if (right <= left || bottom <= top) return;
    /* Clip before division; avoid negative-index and out-of-bounds cases. */
    if (right <= 0 || bottom <= 0 || left >= 320 || top >= 192) return;
    if (left < 0) left = 0;
    if (top < 0) top = 0;
    if (right > 320) right = 320;
    if (bottom > 192) bottom = 192;
    x0 = left / 16; x1 = (right - 1) / 16;
    y0 = top / 16; y1 = (bottom - 1) / 16;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > 19) x1 = 19;
    if (y1 > 11) y1 = 11;
    for (y = y0; y <= y1; ++y)
        for (x = x0; x <= x1; ++x)
            r3d7c23Cells[y][x] = 1;
}

static void R3D7C2_TrackBounds(
    unsigned int slot, int x, int y,
    unsigned int width, unsigned int height,
    unsigned int spriteID, int mirror, unsigned int houseID)
{
    R3D7C2_Bounds *b;
    int right = x + (int)width;
    int bottom = y + (int)height;

    if (slot >= 102 || right <= 0 || bottom <= 0 ||
        x >= 320 || y >= 192)
        return;

    b = &r3d7c2Current[slot];
    b->left = (short)(x < 0 ? 0 : x);
    b->top = (short)(y < 0 ? 0 : y);
    b->right = (short)(right > 320 ? 320 : right);
    b->bottom = (short)(bottom > 192 ? 192 : bottom);
    b->valid = true;
    b->spriteID = (unsigned short)spriteID;
    b->mirror = (unsigned char)(mirror != 0);
    b->houseID = (unsigned char)houseID;
}

static void R3D7C2_BeginFrame(void)
{
    memset(r3d7c2Current, 0, sizeof(r3d7c2Current));
}

/* F12C4: diagnostic-only terrain cache invalidation, 20x12 tiles.
 * A cache miss marks a terrain cell; a viewport change marks all cells.
 * This records cache updates, NOT a complete list of framebuffer changes.
 * Existing full CopyBits path remains mandatory. */
static unsigned char f12c4TerrainCells[12][20];
static Boolean f12c4TerrainScrolled = true;
static volatile unsigned long f12c4TerrainChangedCells = 0;
static volatile unsigned long f12c4TerrainChangedFrames = 0;
static volatile unsigned long f12c4TerrainScrollFrames = 0;
static volatile unsigned long f12c4CandidateUnionCells = 0;

/* F12C3: conservative 20x13 candidate map. No screen-copy changes.
 * This is NOT a complete invalidation tracker. The final 8 pixel rows,
 * terrain/fog, overlays, structures, effects, selection, palette changes,
 * and window exposure are not proven covered by sprite diagnostics.
 * Therefore a full transfer remains mandatory until coverage is complete.
 */
#define F12C3_COLS 20
#define F12C3_ROWS 13
static unsigned char f12c3Candidate[F12C3_ROWS][F12C3_COLS];
static Boolean f12c3MustCopyFull = true;
/* R4-M3B presentation coverage is only valid after a completed composition.
 * Updates/exposures and periodic refreshes must use the full fallback. */
static unsigned char r4m3bPresentCells[13][20];
static unsigned long r4m3bComposedSerial = 0;
static unsigned long r4m3bPresentedSerial = 0;
static unsigned int r4m3bFullRefreshCounter = 0;
static Boolean r4m3bForceExposure = true;


static volatile unsigned long f12c3ObservedFrames = 0;
static volatile unsigned long f12c3CandidateCells = 0;
static volatile unsigned long f12c3ForcedFullFrames = 0;

static void F12C3_ObserveSpriteCoverage(Boolean scrolled)
{
    unsigned int x, y;
    unsigned long unionCount = 0;
    memset(f12c3Candidate, 0, sizeof(f12c3Candidate));

    /* Sprite coverage is not sufficient for selection outlines,
     * explosions, palette animation, or external window exposure.
     * These untracked writers continue to require a full blit. */
    for (y = 0; y < 12; ++y) {
        for (x = 0; x < F12C3_COLS; ++x) {
            f12c3Candidate[y][x] =
                (r3d7c23Cells[y][x] || f12c4TerrainCells[y][x]) ? 1 : 0;
        }
    }
    /* The last eight rows are not covered by the terrain/sprite grids. */
    for (x = 0; x < F12C3_COLS; ++x)
        f12c3Candidate[12][x] = 1;

    if (scrolled || f12c4TerrainScrolled)
        memset(f12c3Candidate, 1, sizeof(f12c3Candidate));

    for (y = 0; y < F12C3_ROWS; ++y)
        for (x = 0; x < F12C3_COLS; ++x)
            unionCount += f12c3Candidate[y][x] != 0;

    /* Instrumentation only: the renderer must still copy all 320x200. */
    f12c3MustCopyFull = true;
    f12c3ObservedFrames++;
    f12c3CandidateCells += unionCount;
    f12c4CandidateUnionCells += unionCount;
    f12c3ForcedFullFrames++;
}

/* F12C11: compare ACTUAL offscreen pixel indices against the candidate
 * tile map from the most recent prepared frame. This is observational only.
 * The normal full CopyBits is always executed. Palette-only changes are
 * invisible to this comparison and require separate handling.
 * Every screen copy is sampled to preserve consecutive-frame comparisons.
 */
static unsigned char f12c11PreviousPixels[320 * 200];
static Boolean f12c11PreviousValid = false;
static void F12C11_CompareOffscreen(PixMapHandle source)
{
    unsigned char *pixels;
    unsigned long misses = 0, changes = 0;
    int stride, x, y;
    if (source == NULL) return;
    stride = (**source).rowBytes & 0x3FFF;
    pixels = (unsigned char *)GetPixBaseAddr(source);
    if (pixels == NULL || stride < 320) { f12c11SkippedFrames++; return; }
    /* Maintain a consecutive snapshot: skipping comparisons would create
     * differences that the latest dirty map could not legitimately cover. */
    for (y = 0; y < 200; ++y) {
        const unsigned char *row = pixels + y * stride;
        unsigned char *old = f12c11PreviousPixels + y * 320;
        for (x = 0; x < 320; ++x) {
            if (f12c11PreviousValid && row[x] != old[x]) {
                changes++;
                if (!f12c3Candidate[y / 16][x / 16]) misses++;
            }
            old[x] = row[x];
        }
    }
    if (f12c11PreviousValid) {
        f12c11ComparedFrames++;
        f12c11ChangedPixels += changes;
        f12c11MissedPixels += misses;
        if (misses) f12c11MissedFrames++;
    }
    f12c11PreviousValid = true;
}

static void R3D7C2_EndFrame(void)
{
    unsigned int i;

    unsigned long framePixels = 0;
    Boolean changed = false;
    Boolean scrolled = (r3d7c2ViewX != g_viewTileX ||
                        r3d7c2ViewY != g_viewTileY);
    unsigned int x, y;
    memset(r3d7c23Cells, 0, sizeof(r3d7c23Cells));
    for (i = 0; i < 102; i++) {
        const R3D7C2_Bounds *a = &r3d7c2Previous[i];
        const R3D7C2_Bounds *b = &r3d7c2Current[i];

        if (a->valid != b->valid ||
            (a->valid &&
             (a->left != b->left ||
              a->top != b->top ||
              a->right != b->right ||
              a->bottom != b->bottom ||
              a->spriteID != b->spriteID ||
              a->mirror != b->mirror ||
              a->houseID != b->houseID))) {
            int left = a->valid ? a->left : b->left;
            int top = a->valid ? a->top : b->top;
            int right = a->valid ? a->right : b->right;
            int bottom = a->valid ? a->bottom : b->bottom;
            if (b->valid) {
                if (b->left < left) left = b->left;
                if (b->top < top) top = b->top;
                if (b->right > right) right = b->right;
                if (b->bottom > bottom) bottom = b->bottom;
            }
            if (right > left && bottom > top) {
                framePixels += (unsigned long)(right-left)*(bottom-top);
                /* Coverage is the UNION of old and new sprite cells,
                 * not the bounding box spanning the two positions. */
                if (a->valid)
                    R3D7C23_MarkRect(a->left, a->top, a->right, a->bottom);
                if (b->valid)
                    R3D7C23_MarkRect(b->left, b->top, b->right, b->bottom);
            }
            if (a->valid && b->valid &&
                (a->spriteID != b->spriteID || a->mirror != b->mirror))
                r3d7c2AnimationChanges++;
            r3d7c2ChangedSlots++;
            changed = true;
        }
    }

    if (scrolled) {
        framePixels = 320UL * 192UL;
        r3d7c23ScrollFrames++;
    }
    if (changed) r3d7c23ChangedFrames++;
    if (scrolled) {
        r3d7c23CoveredCells += 240UL;
    } else {
        for (y = 0; y < 12; ++y)
            for (x = 0; x < 20; ++x)
                r3d7c23CoveredCells += r3d7c23Cells[y][x];
    }
    F12C3_ObserveSpriteCoverage(scrolled);
    r3d7c23FrameCount++;
    if (framePixels > 320UL * 192UL) framePixels = 320UL * 192UL;
    if (changed || scrolled) r3d7c2DirtyFrames++;
    r3d7c2DirtyPixels += framePixels;
    r3d7c2ViewX = g_viewTileX;
    r3d7c2ViewY = g_viewTileY;
    memcpy(r3d7c2Previous, r3d7c2Current,
           sizeof(r3d7c2Previous));
    r3d7c2Frames++;
}

static unsigned char g_cachedTerrain[320 * 192];

static void Mac_DrawMissionUnits(unsigned char *destination, short stride)

{
    static const unsigned char vehicleFrames[8] =
        {0, 1, 2, 3, 4, 3, 2, 1};
    static const unsigned char infantryDirections[8] =
        {0, 1, 1, 1, 2, 1, 1, 1};
    static const unsigned char infantryMirror[8] =
        {0, 0, 0, 0, 0, 1, 1, 1};
    static const unsigned char infantry3Frames[4] =
        {0, 1, 0, 2};

    unsigned int i;

    if (g_sprites == NULL || destination == NULL)
        return;

    for (i = 0; i < 102; i++) {
        Unit *unit = Unit_Get_ByIndex(i);
        const UnitInfo *info;
        const unsigned char *sprite;
        const unsigned char *pixels;
        const unsigned char *spritePalette = NULL;
        unsigned int spriteID;
        unsigned int flags;
        unsigned int width, height, decodedSize;
        unsigned int x, y, row, column, consumed;
        unsigned int orientation;
        unsigned int houseID;
        unsigned int frame;
        int mirror = 0;
        int screenX, screenY;
        static unsigned char decoded[4096];

        if (unit == NULL || !unit->o.flags.s.used ||
            unit->o.flags.s.isNotOnMap)
            continue;

        if (!R3D6_UnitVisible(unit)) continue;

        info = &g_table_unitInfo[unit->o.type];
        spriteID = info->groundSpriteID;

        orientation = ((unsigned char)unit->orientation[0].current + 16) >> 5;
        orientation &= 7;

        if (unit->spriteOffset < 0 && info->destroyedSpriteID != 0) {
            spriteID = info->destroyedSpriteID -
                       unit->spriteOffset - 1;
        } else {
            switch (info->displayMode) {
                case DISPLAYMODE_UNIT:
                case DISPLAYMODE_ROCKET:
                    if (info->movementType != MOVEMENT_SLITHER) {
                        spriteID += vehicleFrames[orientation];
                        mirror = orientation >= 5;
                    }
                    break;

                case DISPLAYMODE_INFANTRY_3_FRAMES:
                    frame = infantry3Frames[
                        ((unsigned int)unit->spriteOffset) & 3
                    ];
                    spriteID += infantryDirections[orientation] * 3;
                    spriteID += frame;
                    mirror = infantryMirror[orientation];
                    break;

                case DISPLAYMODE_INFANTRY_4_FRAMES:
                    frame = ((unsigned int)unit->spriteOffset) & 3;
                    spriteID += infantryDirections[orientation] * 4;
                    spriteID += frame;
                    mirror = infantryMirror[orientation];
                    break;

                default:
                    break;
            }
        }

        if (spriteID >= 355)
            continue;


        sprite = g_sprites[spriteID];

        if (sprite == NULL)
            continue;

        flags = sprite[0] | ((unsigned int)sprite[1] << 8);
        height = sprite[2];
        width = sprite[3] | ((unsigned int)sprite[4] << 8);
        decodedSize = sprite[8] | ((unsigned int)sprite[9] << 8);

        if (width == 0 || height == 0 ||
            width > 128 || height > 128 ||
            decodedSize == 0 || decodedSize > sizeof(decoded))
            continue;

        /* B2: collect unit bounds without decoding or drawing pixels. */
        if (b2ScanMode) {
            if (unit->o.type == UNIT_SANDWORM) {
                b2ForceFull = true;
                continue;
            }
            houseID = (unsigned int)Unit_GetHouseID(unit);
            x = unit->o.position.x;
            y = unit->o.position.y;
            screenX = ((int)(x >> 4) - g_viewTileX * 16) - (int)width / 2;
            screenY = ((int)(y >> 4) - g_viewTileY * 16) - (int)height / 2;
            R3D7C2_TrackBounds(i, screenX, screenY, width, height,
                               spriteID, mirror, houseID);
            continue;
        }
        if (!b2ForceFull && !b2Redraw[i]) {
            ++b2SkippedUnits;
            continue;
        }
        ++b2DrawnUnits;
        pixels = sprite + 10;

        if (flags & 1) {
            spritePalette = pixels;
            pixels += 16;
        }

        if (!(flags & 2)) {
            if (g_cachedUnitSpritePixels[spriteID] != NULL &&
                g_cachedUnitSpriteSource[spriteID] == sprite &&
                g_cachedUnitSpriteSize[spriteID] == decodedSize) {
                pixels = g_cachedUnitSpritePixels[spriteID];
            } else {
                unsigned char *cache;
                /* A changed sprite source must not reuse stale pixels. */
                if (g_cachedUnitSpritePixels[spriteID] != NULL) {
                    free(g_cachedUnitSpritePixels[spriteID]);
                    g_cachedUnitSpritePixels[spriteID] = NULL;
                    g_cachedUnitSpriteSource[spriteID] = NULL;
                    g_cachedUnitSpriteSize[spriteID] = 0;
                }
                cache = (unsigned char *)malloc(decodedSize);
                if (cache != NULL) {
                    Format80_Decode(cache, pixels, decodedSize);
                    g_cachedUnitSpritePixels[spriteID] = cache;
                    g_cachedUnitSpriteSource[spriteID] = sprite;
                    g_cachedUnitSpriteSize[spriteID] = decodedSize;
                    pixels = cache;
                } else {
                    Format80_Decode(decoded, pixels, decodedSize);
                    pixels = decoded;
                }
            }
        }


        /* R3D8D-W4: Original framebuffer sandworm blur.
         *
         * Reuse the already decoded original sprite as a mask.
         * Nontransparent pixels copy nearby terrain pixels,
         * following the original eight-step blur offsets.
         *
         * Render current and previous positions, matching
         * OpenDUNE's dedicated sandworm viewport path.
         *
         * No additional sprite decoding, allocation,
         * floating point, or QuickDraw calls.
         */
        if (unit->o.type == UNIT_SANDWORM) {
            static const unsigned char blurOffsets[8] =
                {1, 3, 2, 5, 4, 3, 2, 1};
            tile32 positions[3];
            int left = 320, top = 192;
            int right = 0, bottom = 0;
            unsigned int pass;
            unsigned int phase =
                (unsigned int)((g_timerGame >> 2) & 7);

            positions[0] = unit->o.position;
            positions[1] = unit->targetLast;
            positions[2] = unit->targetPreLast;

            for (pass = 0; pass < 3; pass++) {
                int cx, cy, sx, sy;
                unsigned int r = 0, c = 0, used = 0;
                const unsigned char *mask = pixels;
                unsigned int offset =
                    blurOffsets[(phase + pass) & 7];

                /* Avoid drawing uninitialized trail positions. */
                if (positions[pass].x == 0 &&
                    positions[pass].y == 0)
                    continue;

                cx = ((int)(positions[pass].x >> 4) -
                      g_viewTileX * 16);
                cy = ((int)(positions[pass].y >> 4) -
                      g_viewTileY * 16);

                sx = cx - (int)width / 2;
                sy = cy - (int)height / 2;

                if (sx >= 320 || sy >= 192 ||
                    sx + (int)width <= 0 ||
                    sy + (int)height <= 0)
                    continue;

                if (sx < left) left = sx;
                if (sy < top) top = sy;
                if (sx + (int)width > right)
                    right = sx + (int)width;
                if (sy + (int)height > bottom)
                    bottom = sy + (int)height;

                while (r < height && used < decodedSize) {
                    unsigned int v = *mask++;
                    used++;

                    if (v == 0) {
                        unsigned int skip;

                        if (used >= decodedSize) break;

                        skip = *mask++;
                        used++;

                        while (skip-- && r < height) {
                            c++;
                            if (c >= width) {
                                c = 0;
                                r++;
                            }
                        }
                    } else {
                        int px = sx + (int)c;
                        int py = sy + (int)r;

                        if (px >= 0 &&
                            px + (int)offset < 320 &&
                            py >= 0 && py < 192) {
                            unsigned int tileX =
                                (unsigned int)g_viewTileX +
                                (unsigned int)px / 16;
                            unsigned int tileY =
                                (unsigned int)g_viewTileY +
                                (unsigned int)py / 16;
                            unsigned int sourceTileX =
                                (unsigned int)g_viewTileX +
                                (unsigned int)(px + (int)offset) / 16;
                            unsigned int packedTile = tileY * 64 + tileX;
                            unsigned int sourcePacked = tileY * 64 + sourceTileX;
                            if (tileX < 64 && tileY < 64 &&
                                sourceTileX < 64 &&
                                g_table_landscapeInfo[
                                    Map_GetLandscapeType(packedTile)].isSand &&
                                g_table_landscapeInfo[
                                    Map_GetLandscapeType(sourcePacked)].isSand &&
                                Structure_Get_ByPackedTile(packedTile) == NULL &&
                                Structure_Get_ByPackedTile(sourcePacked) == NULL) {
                                destination[(unsigned int)py * stride +
                                            (unsigned int)px] =
                                    g_cachedTerrain[(unsigned int)py * 320 +
                                                    (unsigned int)px + offset];
                            }
                        }

                        c++;
                        if (c >= width) {
                            c = 0;
                            r++;
                        }
                    }
                }
            }

            if (right > left && bottom > top) {
                R3D7C2_TrackBounds(
                    i, left, top,
                    (unsigned int)(right - left),
                    (unsigned int)(bottom - top),
                    161 + phase, 0, (unsigned int)Unit_GetHouseID(unit));
            } else {
                R3D7C2_TrackBounds(
                    i, -32, -32, 0, 0, 161, 0,
                    (unsigned int)Unit_GetHouseID(unit));
            }

            continue;
        }

        houseID = (unsigned int)Unit_GetHouseID(unit);

        /* Pixel-accurate positioning: 256 units per tile. */
        x = unit->o.position.x;
        y = unit->o.position.y;

        screenX = ((int)(x >> 4) - g_viewTileX * 16) -
                  (int)width / 2;
        screenY = ((int)(y >> 4) - g_viewTileY * 16) -
                  (int)height / 2;

        R3D7C2_TrackBounds(
            i, screenX, screenY, width, height, spriteID, mirror, houseID);

        /* R4-M5A: offscreen units still update their tracked bounds,
         * but need no pixel decoding or framebuffer writes. */
        if (screenX >= 320 || screenY >= 192 ||
            screenX + (int)width <= 0 ||
            screenY + (int)height <= 0)
            continue;

        row = 0;
        column = 0;
        consumed = 0;

        while (row < height && consumed < decodedSize) {
            unsigned int value = *pixels++;
            consumed++;

            if (value == 0) {
                unsigned int skip;

                if (consumed >= decodedSize)
                    break;

                skip = *pixels++;
                consumed++;

                /* R4-M5A: advance transparent run in constant time.
                 * Clamp to the sprite boundary, matching the old loop. */
                {
                    /* R4-M5C-B1: avoid two variable integer divisions
                     * for the common transparent-run case. A run that
                     * crosses rows uses a bounded subtract loop; decoded
                     * skip is at most 255, and width is nonzero. */
                    unsigned int remaining = (height - row - 1) * width
                                           + (width - column);
                    unsigned int advance = skip < remaining ? skip : remaining;
                    unsigned int toEdge = width - column;
                    if (advance < toEdge) {
                        column += advance;
                    } else {
                        advance -= toEdge;
                        ++row;
                        column = 0;
                        while (advance >= width && row < height) {
                            advance -= width;
                            ++row;
                        }
                        if (row < height) column = advance;
                    }
                }
            } else {
                int px = screenX +
                         (mirror ? (int)width - 1 - (int)column
                                 : (int)column);
                int py = screenY + (int)row;

                if (spritePalette != NULL && value < 16) {
                    value = spritePalette[value];

                    if (value >= 0x90 && value <= 0x98)
                        value += houseID << 4;
                }

                if (px >= 0 && px < 320 &&
                    py >= 0 && py < 192)
                    destination[py * stride + px] =
                        (unsigned char)value;

                column++;

                if (column >= width) {
                    column = 0;
                    row++;
                }
            }
        }

        /* H1E: inexpensive two-frame dust/scoop indicator. Drawn into the
         * indexed battlefield buffer only while actually harvesting spice.
         * No QuickDraw calls, allocation, or extra sprite decoding. */
        if (unit->o.type == UNIT_HARVESTER &&
            unit->actionID == ACTION_HARVEST &&
            (Map_GetLandscapeType(Tile_PackTile(unit->o.position)) == LST_SPICE ||
             Map_GetLandscapeType(Tile_PackTile(unit->o.position)) == LST_THICK_SPICE)) {
            int cx = ((int)(unit->o.position.x >> 4) - g_viewTileX * 16);
            int cy = ((int)(unit->o.position.y >> 4) - g_viewTileY * 16);
            unsigned int phase = (unsigned int)((g_timerGame / 8) & 1);
            int ox = phase ? -7 : 6;
            int oy = phase ? 5 : 6;
            int px = cx + ox;
            int py = cy + oy;
            if (px >= 1 && px < 319 && py >= 1 && py < 191) {
                destination[py * stride + px] = 0x58;
                destination[py * stride + px + 1] = 0x58;
                destination[(py - 1) * stride + px] = 0x57;
            }
        }
    }
}

/* Stage 15O-F: cache original ground graphics by visible tile ID.
 * Structure_UpdateMap and Animation_Tick modify g_map[].groundTileID.
 * Compare these IDs each render to catch placement and animation changes
 * without redrawing all 240 tiles on every unit-movement frame. */
static uint32 g_cachedTileIDs[20 * 12];
static uint8 g_cachedHouseIDs[20 * 12];
static int g_cachedViewX = -1;
static int g_cachedViewY = -1;
static Boolean g_cachedTerrainValid = false;


/* R.2B: compose selection overlays into the 8-bit framebuffer.
 * Coordinates are relative to the 320x192 battlefield.
 * This avoids a separate window-based QuickDraw outline pass.
 */
static void R2B_Outline(
    unsigned char *pixels,
    short stride,
    int left, int top, int right, int bottom)
{
    int x, y;

    if (right <= 0 || bottom <= 0 ||
        left >= 320 || top >= 192)
        return;

    if (left < 0) left = 0;
    if (top < 0) top = 0;
    if (right > 320) right = 320;
    if (bottom > 192) bottom = 192;

    if (left >= right || top >= bottom)
        return;

    for (x = left; x < right; x++) {
        pixels[top * stride + x] = 0;
        if (bottom - 1 != top)
            pixels[(bottom - 1) * stride + x] = 0;
    }

    for (y = top + 1; y < bottom - 1; y++) {
        pixels[y * stride + left] = 0;
        if (right - 1 != left)
            pixels[y * stride + right - 1] = 0;
    }
}

/* F12C6: diagnostic overlay bounds. Four selection outlines at most:
 * attack target, move destination, selected unit, selected structure.
 * Previous/current rectangles are tracked separately from sprite slots.
 * Full CopyBits remains mandatory; this is NOT a partial-blit switch. */
#define F12C6_OVERLAY_SLOTS 4
static R3D7C2_Bounds f12c6Previous[F12C6_OVERLAY_SLOTS];
static R3D7C2_Bounds f12c6Current[F12C6_OVERLAY_SLOTS];
static unsigned int f12c6OverlaySlot;
static volatile unsigned long f12c6OverlayChangedFrames;
static volatile unsigned long f12c6OverlayChangedCells;
static volatile unsigned long f12c6OverlayOverflowFrames;

static void F12C6_Outline(unsigned char *pixels, short stride,
                          int left, int top, int right, int bottom)
{
    R3D7C2_Bounds *b;
    R2B_Outline(pixels, stride, left, top, right, bottom);
    if (right <= 0 || bottom <= 0 || left >= 320 || top >= 192)
        return;
    if (left < 0) left = 0;
    if (top < 0) top = 0;
    if (right > 320) right = 320;
    if (bottom > 192) bottom = 192;
    if (right <= left || bottom <= top) return;
    if (f12c6OverlaySlot >= F12C6_OVERLAY_SLOTS) {
        f12c6OverlayOverflowFrames++;
        f12c3MustCopyFull = true;
        return;
    }
    b = &f12c6Current[f12c6OverlaySlot++];
    b->left = (short)left;
    b->top = (short)top;
    b->right = (short)right;
    b->bottom = (short)bottom;
    b->valid = true;
}

static void F12C6_EndOverlays(void)
{
    unsigned int i, x, y;
    unsigned long added = 0;
    Boolean changed = false;
    for (i = 0; i < F12C6_OVERLAY_SLOTS; ++i) {
        const R3D7C2_Bounds *a = &f12c6Previous[i];
        const R3D7C2_Bounds *b = &f12c6Current[i];
        if (a->valid != b->valid ||
            (a->valid && (a->left != b->left || a->top != b->top ||
                          a->right != b->right || a->bottom != b->bottom))) {
            changed = true;
            if (a->valid) {
                for (y = (unsigned int)a->top / 16; y <= (unsigned int)(a->bottom - 1) / 16; ++y)
                    for (x = (unsigned int)a->left / 16; x <= (unsigned int)(a->right - 1) / 16; ++x)
                        f12c3Candidate[y][x] = 1;
            }
            if (b->valid) {
                for (y = (unsigned int)b->top / 16; y <= (unsigned int)(b->bottom - 1) / 16; ++y)
                    for (x = (unsigned int)b->left / 16; x <= (unsigned int)(b->right - 1) / 16; ++x)
                        f12c3Candidate[y][x] = 1;
            }
        }
    }
    for (y = 0; y < F12C3_ROWS; ++y)
        for (x = 0; x < F12C3_COLS; ++x)
            added += f12c3Candidate[y][x] != 0;
    /* Previous F12C3 count was recorded before overlay composition.
     * This diagnostic counts post-overlay coverage separately. */
    f12c6OverlayChangedCells += added;
    if (changed) f12c6OverlayChangedFrames++;
    memcpy(f12c6Previous, f12c6Current, sizeof(f12c6Previous));
}

static void R2B_DrawSelection(
    unsigned char *pixels, short stride)
{
    Unit *u = Mac_SelectedUnit();
    Structure *s = Mac_SelectedStructure();

    /* R3D.4: Show the selected unit's live engine attack target.
     * This is intentionally not a remembered mouse click: moving or
     * destroying the target clears the indicator through engine state.
     * Draw into the existing offscreen framebuffer; no extra CopyBits. */
    /* A valid Shift-click movement order overrides attack confirmation.
     * OpenDUNE may retain targetAttack while transitioning to movement;
     * do not mutate engine state merely to clear a visual indicator.
     * A new attack order resets g_moveDestinationX to -1. */
    if (u != NULL && g_moveDestinationX < 0 &&
        u->targetAttack != 0 &&
        Tools_Index_GetType(u->targetAttack) == IT_UNIT) {
        uint16 index = Tools_Index_Decode(u->targetAttack);
        if (index < 102) {
            Unit *enemy = Unit_Get_ByIndex(index);
            if (enemy != NULL && enemy->o.flags.s.used &&
                !enemy->o.flags.s.isNotOnMap &&
                !House_AreAllied(u->o.houseID, enemy->o.houseID) &&
                R3D6_UnitVisible(enemy)) {
                int tx = ((int)enemy->o.position.x >> 4) -
                         g_viewTileX * 16;
                int ty = ((int)enemy->o.position.y >> 4) -
                         g_viewTileY * 16;
                F12C6_Outline(pixels, stride,
                    tx - 10, ty - 10, tx + 10, ty + 10);
            }
        }
    }

    if (u != NULL) {
        int x, y;

        if (g_moveDestinationX >= 0 &&
            g_moveDestinationY >= 0) {
            int dx = g_moveDestinationX - g_viewTileX;
            int dy = g_moveDestinationY - g_viewTileY;

            if (dx >= 0 && dx < 20 &&
                dy >= 0 && dy < 12) {
                F12C6_Outline(pixels, stride,
                    3 + dx * 16, 3 + dy * 16,
                    13 + dx * 16, 13 + dy * 16);
            }
        }

        x = ((int)u->o.position.x >> 4) -
            g_viewTileX * 16;
        y = ((int)u->o.position.y >> 4) -
            g_viewTileY * 16;

        if (x >= -16 && x < 320 &&
            y >= -16 && y < 192) {
            F12C6_Outline(pixels, stride,
                x - 8, y - 8,
                x + 8, y + 8);
        }
    }

    if (s != NULL) {
        const StructureInfo *si =
            &g_table_structureInfo[s->o.type];

        int x = (int)(s->o.position.x >> 8) -
            g_viewTileX;
        int y = (int)(s->o.position.y >> 8) -
            g_viewTileY;

        int width =
            g_table_structure_layoutSize[si->layout].width;
        int height =
            g_table_structure_layoutSize[si->layout].height;

        F12C6_Outline(pixels, stride,
            x * 16, y * 16,
            (x + width) * 16,
            (y + height) * 16);
    }
}

/* R2D: terrain preparation counters.
 * These are accumulated, not timed per individual call.
 */
static uint32 r2dTerrainCalls = 0;
static uint32 r2dFullRefreshes = 0;
static uint32 r2dDecodedTiles = 0;
static uint32 r2dCopyCalls = 0;

/* R3B.2: accumulated TickCount measurements. */
static uint32 r3bRestoreTicks = 0;
static uint32 r3bSpritesTicks = 0;

/* F12C7: read-only explosion coverage; no change to sprite decoding. */
static R3D7C2_Bounds f12c7EffectPrevious[EXPLOSION_MAX];
static R3D7C2_Bounds f12c7EffectCurrent[EXPLOSION_MAX];
static volatile unsigned long f12c7EffectChangedFrames;
static volatile unsigned long f12c7EffectChangedCells;
static volatile unsigned long f12c7ExposureEvents;

static void F12C7_MarkBounds(const R3D7C2_Bounds *b)
{
    unsigned int x, y;
    if (!b->valid || b->right <= b->left || b->bottom <= b->top) return;
    for (y = (unsigned int)b->top / 16; y <= (unsigned int)(b->bottom - 1) / 16; ++y)
        for (x = (unsigned int)b->left / 16; x <= (unsigned int)(b->right - 1) / 16; ++x)
            f12c3Candidate[y][x] = 1;
}

static void F12C7_EndEffects(void)
{
    unsigned int i, x, y;
    Boolean changed = false;
    unsigned long cells = 0;
    for (i = 0; i < EXPLOSION_MAX; ++i) {
        const R3D7C2_Bounds *a = &f12c7EffectPrevious[i];
        const R3D7C2_Bounds *b = &f12c7EffectCurrent[i];
        if (a->valid != b->valid ||
            (a->valid && (a->left != b->left || a->top != b->top ||
                          a->right != b->right || a->bottom != b->bottom ||
                          a->spriteID != b->spriteID))) {
            changed = true;
            F12C7_MarkBounds(a);
            F12C7_MarkBounds(b);
        }
    }
    if (changed) f12c7EffectChangedFrames++;
    for (y = 0; y < F12C3_ROWS; ++y)
        for (x = 0; x < F12C3_COLS; ++x)
            cells += f12c3Candidate[y][x] != 0;
    f12c7EffectChangedCells += cells;
    memcpy(f12c7EffectPrevious, f12c7EffectCurrent,
           sizeof(f12c7EffectPrevious));
}

/* W4.1: draw original active explosion sprite frames over battlefield. */
static void W41_DrawSwallowEffects(unsigned char *destination, short stride)
{
    unsigned int i;
    if (g_sprites == NULL) return;
    for (i = 0; i < EXPLOSION_MAX; ++i) {
        Explosion *e = Explosion_Get_ByIndex((int)i);
        unsigned int id, flags, width, height, decodedSize;
        const unsigned char *sprite, *pixels, *palette = NULL;
        unsigned char decoded[4096];
        unsigned int row = 0, column = 0, used = 0;
        int sx, sy;
        if (e == NULL || e->commands == NULL) continue;
        id = e->spriteID;
        if (id < 218 || id > 222) continue;
        if (!R3D6_IsDiscovered((int)(e->position.x >> 8),
                               (int)(e->position.y >> 8))) continue;
        sprite = g_sprites[id];
        if (sprite == NULL) continue;
        flags = sprite[0] | ((unsigned int)sprite[1] << 8);
        height = sprite[2];
        width = sprite[3] | ((unsigned int)sprite[4] << 8);
        decodedSize = sprite[8] | ((unsigned int)sprite[9] << 8);
        if (width == 0 || height == 0 || width > 128 || height > 128 ||
            decodedSize == 0 || decodedSize > sizeof(decoded)) continue;
        pixels = sprite + 10;
        if (flags & 1) { palette = pixels; pixels += 16; }
        if (!(flags & 2)) {
            Format80_Decode(decoded, pixels, decodedSize);
            pixels = decoded;
        }
        sx = (int)(e->position.x >> 4) - g_viewTileX * 16 - (int)width / 2;
        sy = (int)(e->position.y >> 4) - g_viewTileY * 16 - (int)height / 2;
        /* Diagnostic only: capture the clipped bounding box of this effect. */
        if (sx < 320 && sy < 192 && sx + (int)width > 0 &&
            sy + (int)height > 0) {
            R3D7C2_Bounds *b = &f12c7EffectCurrent[i];
            b->left = (short)(sx < 0 ? 0 : sx);
            b->top = (short)(sy < 0 ? 0 : sy);
            b->right = (short)(sx + (int)width > 320 ? 320 : sx + (int)width);
            b->bottom = (short)(sy + (int)height > 192 ? 192 : sy + (int)height);
            b->spriteID = (unsigned short)id;
            b->valid = true;
        }
        while (row < height && used < decodedSize) {
            unsigned int value = *pixels++;
            ++used;
            if (value == 0) {
                unsigned int skip;
                if (used >= decodedSize) break;
                skip = *pixels++;
                ++used;
                while (skip-- && row < height) {
                    if (++column >= width) { column = 0; ++row; }
                }
            } else {
                int px = sx + (int)column;
                int py = sy + (int)row;
                if (palette != NULL && value < 16) value = palette[value];
                if (px >= 0 && px < 320 && py >= 0 && py < 192)
                    destination[(unsigned int)py * stride + (unsigned int)px] =
                        (unsigned char)value;
                if (++column >= width) { column = 0; ++row; }
            }
        }
    }
}

/* P1 diagnostic: each key disables CPU work, not just visibility. */
static Boolean p1SkipTerrain = false;
static Boolean p1SkipSprites = false;
static Boolean p1SkipBlit = false;
static Boolean p1SkipHUD = false;
static unsigned long p1TerrainUs = 0, p1TerrainCalls = 0;
static unsigned long p1RestoreUs = 0, p1RestoreCalls = 0;
static unsigned long p1SpritesUs = 0, p1SpritesCalls = 0;

/* R4-M2: composition stages remain in this translation unit while their
 * shared state is being disentangled. Framebuffer ownership is explicit. */
static void R4_MarkRestoreBounds(unsigned char dirty[12][20],
                                  const R3D7C2_Bounds *b)
{
    int x, y, l, t, r, bottom;
    if (!b->valid) return;
    l = b->left < 0 ? 0 : b->left;
    t = b->top < 0 ? 0 : b->top;
    r = b->right > 320 ? 320 : b->right;
    bottom = b->bottom > 192 ? 192 : b->bottom;
    if (r <= l || bottom <= t) return;
    for (y = t / 16; y <= (bottom - 1) / 16; ++y)
        for (x = l / 16; x <= (r - 1) / 16; ++x)
            dirty[y][x] = 1;
}

/* B2: pre-render scan and overlap closure. No pixel writes here.
 * The full path remains mandatory for effects, selection, scrolling and
 * unusually large dirty regions. */
static void B2_PrepareFrame(unsigned char *destination, short stride)
{
    unsigned int i, x, y, count = 0;
    B2Bounds bounds[B2_SLOTS];
    memset(b2Dirty, 0, sizeof(b2Dirty));
    memset(b2Redraw, 0, sizeof(b2Redraw));
    b2ForceFull = f12c4TerrainScrolled || p1SkipTerrain || p1SkipSprites;
    R3D7C2_BeginFrame();
    b2ScanMode = true;
    Mac_DrawMissionUnits(destination, stride);
    b2ScanMode = false;
    for (y = 0; y < 12; ++y)
        for (x = 0; x < 20; ++x)
            b2Dirty[y][x] = f12c4TerrainCells[y][x];
    for (i = 0; i < 102; ++i) {
        const R3D7C2_Bounds *a = &r3d7c2Previous[i];
        const R3D7C2_Bounds *b = &r3d7c2Current[i];
        bounds[i].left = b->left;
        bounds[i].top = b->top;
        bounds[i].right = b->right;
        bounds[i].bottom = b->bottom;
        bounds[i].valid = b->valid;
        if (a->valid != b->valid ||
            (a->valid && (a->left != b->left || a->top != b->top ||
                          a->right != b->right || a->bottom != b->bottom ||
                          a->spriteID != b->spriteID || a->mirror != b->mirror ||
                          a->houseID != b->houseID))) {
            if (a->valid) R4_MarkRestoreBounds(b2Dirty, a);
            if (b->valid) R4_MarkRestoreBounds(b2Dirty, b);
        }
    }
    for (i = 0; i < F12C6_OVERLAY_SLOTS; ++i)
        if (f12c6Previous[i].valid) b2ForceFull = true;
    for (i = 0; i < EXPLOSION_MAX; ++i)
        if (f12c7EffectPrevious[i].valid) b2ForceFull = true;
    if (!b2ForceFull) {
        B2_ExpandDirty(b2Dirty, bounds, b2Redraw);
        for (y = 0; y < 12; ++y)
            for (x = 0; x < 20; ++x)
                count += b2Dirty[y][x] != 0;
        if (count > 160) b2ForceFull = true;
    }
    if (b2ForceFull) {
        memset(b2Dirty, 1, sizeof(b2Dirty));
        memset(b2Redraw, 1, sizeof(b2Redraw));
    }

    /* R4-P2: unit visibility correctness.
     * Restore terrain beneath every currently visible unit,
     * then redraw each unit on every scheduled presentation.
     * Preserve R4-P1's reduced presentation cadence.
     */
    for (i = 0; i < 102; ++i) {
        if (r3d7c2Current[i].valid) {
            R4_MarkRestoreBounds(
                b2Dirty, &r3d7c2Current[i]);
            b2Redraw[i] = 1;
        }
    }
}

/* R4-M3A: restore only cells that could contain stale foreground pixels.
 * Every previous sprite/effect/selection bound is included, even stationary
 * objects; terrain-cache changes are included separately. Screen transfer
 * remains the original full CopyBits path. */
static void R4_RestoreTerrainStage(unsigned char *destination, short stride)
{
    unsigned long restoreStart = P1_Now();
    if (p1SkipTerrain) {
        unsigned int y;
        for (y = 0; y < 192; ++y)
            memset(destination + y * stride, 0, 320);
    } else {
        R4_RestoreTerrainCells(destination, stride, g_cachedTerrain,
                               b2Dirty, b2ForceFull);
    }
    p1RestoreUs += P1_Now() - restoreStart;
    p1RestoreCalls++;
}

static void R4_ComposeSpriteStage(unsigned char *destination, short stride)
{
        if (!p1SkipSprites) {
            unsigned long spritesStart = P1_Now();
            memset(f12c7EffectCurrent, 0, sizeof(f12c7EffectCurrent));
            Mac_DrawMissionUnits(destination, stride);
            W41_DrawSwallowEffects(destination, stride);
            R3D7C2_EndFrame();
            memset(f12c6Current, 0, sizeof(f12c6Current));
            f12c6OverlaySlot = 0;
            R2B_DrawSelection(destination, stride);
            F12C6_EndOverlays();
            F12C7_EndEffects();
            memcpy(r4m3bPresentCells, f12c3Candidate,
                   sizeof(r4m3bPresentCells));
            ++r4m3bComposedSerial;
            p1SpritesUs += P1_Now() - spritesStart;
            p1SpritesCalls++;
        }
}

static Boolean CreateOriginalTerrainWorld(void)
{
    PixMapHandle pixmap;
    R4_Framebuffer frame;
    unsigned char *destination;
    short stride;
    int tileX, tileY, x, y;
    uint16 tileID;
    uint8 paletteIndex;
    uint8 *tilePixels;
    uint8 *tilePalette;
    unsigned char packed;
    Boolean fullRefresh;

    if (g_duneMapWorld == NULL || g_missionTestResult != 1)
        return false;

    if (g_tilesPixels == NULL ||
        g_iconRTBL == NULL ||
        g_iconRPAL == NULL)
        return false;

    if (!R4_FramebufferAcquire(g_duneMapWorld, 320, &frame))
        return false;
    pixmap = frame.pixmap;
    stride = frame.stride;
    destination = frame.pixels;

    if (!p1SkipTerrain) {
    unsigned long terrainStart = P1_Now();
    r2dTerrainCalls++;

    fullRefresh = !g_cachedTerrainValid ||
                  g_cachedViewX != g_viewTileX ||
                  g_cachedViewY != g_viewTileY;

    memset(f12c4TerrainCells, 0, sizeof(f12c4TerrainCells));
    f12c4TerrainScrolled = (g_cachedViewX != g_viewTileX ||
                            g_cachedViewY != g_viewTileY ||
                            !g_cachedTerrainValid);
    if (f12c4TerrainScrolled) {
        memset(f12c4TerrainCells, 1, sizeof(f12c4TerrainCells));
        f12c4TerrainScrollFrames++;
    }

    /* R.2C: reuse overlapping cached terrain on a one-tile
     * cardinal viewport movement. Only the newly exposed
     * row or column needs terrain decoding.
     *
     * Larger or diagonal jumps retain the full-refresh path.
     */
    if (g_cachedTerrainValid) {
        int dx = g_viewTileX - g_cachedViewX;
        int dy = g_viewTileY - g_cachedViewY;

        if ((dx == 1 || dx == -1) && dy == 0) {
            int row;

            for (row = 0; row < 192; row++) {
                unsigned char *p =
                    g_cachedTerrain + row * 320;

                if (dx == 1)
                    memmove(p, p + 16, 304);
                else
                    memmove(p + 16, p, 304);
            }

            for (row = 0; row < 12; row++) {
                uint32 *ids = g_cachedTileIDs + row * 20;

                if (dx == 1) {
                    memmove(ids, ids + 1,
                            19 * sizeof(uint32));
                    ids[19] = 0xFFFF;
                } else {
                    memmove(ids + 1, ids,
                            19 * sizeof(uint32));
                    ids[0] = 0xFFFF;
                }
                {
                    uint8 *houses = g_cachedHouseIDs + row * 20;
                    if (dx == 1)
                        memmove(houses, houses + 1, 19);
                    else
                        memmove(houses + 1, houses, 19);
                }
            }

            fullRefresh = false;

        } else if (dx == 0 && (dy == 1 || dy == -1)) {
            if (dy == 1) {
                memmove(g_cachedTerrain,
                        g_cachedTerrain + 16 * 320,
                        176 * 320);

                memmove(g_cachedTileIDs,
                        g_cachedTileIDs + 20,
                        11 * 20 * sizeof(uint32));
                memmove(g_cachedHouseIDs,
                        g_cachedHouseIDs + 20, 11 * 20);

                for (x = 0; x < 20; x++)
                    g_cachedTileIDs[11 * 20 + x] = 0xFFFFFFFFUL;

            } else {
                memmove(g_cachedTerrain + 16 * 320,
                        g_cachedTerrain,
                        176 * 320);

                memmove(g_cachedTileIDs + 20,
                        g_cachedTileIDs,
                        11 * 20 * sizeof(uint32));
                memmove(g_cachedHouseIDs + 20,
                        g_cachedHouseIDs, 11 * 20);

                for (x = 0; x < 20; x++)
                    g_cachedTileIDs[x] = 0xFFFFFFFFUL;
            }

            fullRefresh = false;
        }
    }

    if (fullRefresh)
        r2dFullRefreshes++;

    for (tileY = 0; tileY < 12; tileY++) {
        for (tileX = 0; tileX < 20; tileX++) {
            int index = tileY * 20 + tileX;
            tileID = g_map[(tileY + g_viewTileY) * 64 +
                           (tileX + g_viewTileX)].groundTileID;
            {
                uint16 overlay = g_map[(tileY + g_viewTileY) * 64 +
                                       (tileX + g_viewTileX)].overlayTileID;
                uint8 houseID = g_map[(tileY + g_viewTileY) * 64 +
                                      (tileX + g_viewTileX)].houseID;
                Boolean discovered = R3D6_IsDiscovered(
                    tileX + g_viewTileX, tileY + g_viewTileY);
                Boolean fogEdge = r3d6ShroudEnabled && discovered &&
                    overlay >= (uint16)(g_veiledTileID - 15) &&
                    overlay <= g_veiledTileID;
                uint32 cacheKey = (uint32)tileID |
                    ((uint32)(fogEdge ? overlay : 0xFFFF) << 16);
                if (!discovered) cacheKey = 0xFFFFFFFEUL;
                if (!fullRefresh && g_cachedTileIDs[index] == cacheKey &&
                    g_cachedHouseIDs[index] == houseID)
                    continue;
                f12c4TerrainCells[tileY][tileX] = 1;
                g_cachedTileIDs[index] = cacheKey;
                g_cachedHouseIDs[index] = houseID;
                if (!discovered) {
                    for (y = 0; y < 16; ++y)
                        memset(g_cachedTerrain +
                               (tileY * 16 + y) * 320 + tileX * 16,
                               0, 16);
                    r2dDecodedTiles++;
                    continue;
                }
            }

            r2dDecodedTiles++;
            tilePixels = g_tilesPixels + tileID * 128;
            paletteIndex = g_iconRTBL[tileID];
            tilePalette = g_iconRPAL + paletteIndex * 16;
            {
                uint8 houseID = g_map[(tileY + g_viewTileY) * 64 +
                                      (tileX + g_viewTileX)].houseID;
                uint8 localPalette[16];
                if (houseID != 0 && houseID < HOUSE_MAX) {
                    int k;
                    for (k = 0; k < 16; ++k) {
                        uint8 colour = tilePalette[k];
                        if ((colour & 0xF0) == 0x90 && colour <= 0x96)
                            colour = (uint8)(colour + (houseID << 4));
                        localPalette[k] = colour;
                    }
                    tilePalette = localPalette;
                }

            for (y = 0; y < 16; y++) {
                unsigned char *cacheRow = g_cachedTerrain +
                    (tileY * 16 + y) * 320 + tileX * 16;
                for (x = 0; x < 8; x++) {
                    packed = tilePixels[y * 8 + x];
                    cacheRow[x * 2] = tilePalette[packed >> 4];
                    cacheRow[x * 2 + 1] = tilePalette[packed & 15];
                }
            }
            }
            /* R3D6-B: original fog-edge pixels, composited only when
             * OpenDUNE marks this tile with a veiling overlay. The zero
             * nibble is transparent; no extra QuickDraw or screen copy. */
            if (r3d6ShroudEnabled) {
                uint16 overlay = g_map[(tileY + g_viewTileY) * 64 +
                                       (tileX + g_viewTileX)].overlayTileID;
                if (overlay >= (uint16)(g_veiledTileID - 15) &&
                    overlay <= g_veiledTileID) {
                    const uint8 *fogPixels = g_tilesPixels + overlay * 128;
                    const uint8 *fogPalette = g_iconRPAL +
                        (unsigned int)g_iconRTBL[overlay] * 16;
                    for (y = 0; y < 16; y++) {
                        unsigned char *cacheRow = g_cachedTerrain +
                            (tileY * 16 + y) * 320 + tileX * 16;
                        for (x = 0; x < 8; x++) {
                            uint8 fog = fogPixels[y * 8 + x];
                            uint8 hi = fog >> 4;
                            uint8 lo = fog & 15;
                            if (hi != 0) cacheRow[x * 2] = fogPalette[hi];
                            if (lo != 0) cacheRow[x * 2 + 1] = fogPalette[lo];
                        }
                    }
                }
            }
        }
    }

    {
        unsigned int tx, ty;
        unsigned long changed = 0;
        for (ty = 0; ty < 12; ++ty)
            for (tx = 0; tx < 20; ++tx)
                changed += f12c4TerrainCells[ty][tx] != 0;
        f12c4TerrainChangedCells += changed;
        if (changed != 0) f12c4TerrainChangedFrames++;
    }
    g_cachedViewX = g_viewTileX;
    g_cachedViewY = g_viewTileY;
    g_cachedTerrainValid = true;
    p1TerrainUs += P1_Now() - terrainStart;
    p1TerrainCalls++;
    } /* terrain processing */

    /* R3B.2: profile framebuffer composition. */
    {
        uint32 start = TickCount();
        uint32 afterRestore;

        B2_PrepareFrame(destination, stride);
        R4_RestoreTerrainStage(destination, stride);
        afterRestore = TickCount();
        r3bRestoreTicks += afterRestore - start;

        R4_ComposeSpriteStage(destination, stride);

        r3bSpritesTicks += TickCount() - afterRestore;
    }

    R4_FramebufferRelease(&frame);
    return true;
}

static Boolean CreateDuneMapWorld(void)
{
    Rect bounds;
    PixMapHandle pixmap;
    unsigned char *destination;
    short stride;
    int y;

    if (g_duneMapPixels == NULL) {
        g_duneGWorldError = -100;
        return false;
    }

    SetRect(&bounds, 0, 0, 320, 200);

    {
        OSErr err = NewGWorld(&g_duneMapWorld, 8, &bounds,
                             g_duneColorTable, NULL, 0);
        if (err != noErr) {
            g_duneGWorldError = err;
            return false;
        }
    }

    pixmap = GetGWorldPixMap(g_duneMapWorld);

    if (pixmap == NULL || !LockPixels(pixmap)) {
        g_duneGWorldError = -101;
        DisposeGWorld(g_duneMapWorld);
        g_duneMapWorld = NULL;
        return false;
    }

    stride = (**pixmap).rowBytes & 0x3FFF;
    destination = (unsigned char *)GetPixBaseAddr(pixmap);

    if (destination == NULL || stride < 320) {
        UnlockPixels(pixmap);
        DisposeGWorld(g_duneMapWorld);
        g_duneMapWorld = NULL;
        return false;
    }

    for (y = 0; y < 200; y++) {
        memcpy(destination + y * stride,
               g_duneMapPixels + y * 320,
               320);
    }

    UnlockPixels(pixmap);
    return true;
}


/*
 * Copy the live OpenDUNE framebuffer into the existing
 * Macintosh 8-bit QuickDraw GWorld.
 *
 * This does not change the current static-map display.
 */
extern const uint8 *MacVideo_GetPixels(void);

static Boolean UpdateOpenDUNEWorld(void)
{
    PixMapHandle pixmap;
    unsigned char *destination;
    const uint8 *source;
    short stride;
    int y;

    if (g_duneMapWorld == NULL)
        return false;

    source = MacVideo_GetPixels();
    if (source == NULL)
        return false;

    pixmap = GetGWorldPixMap(g_duneMapWorld);
    if (pixmap == NULL || !LockPixels(pixmap))
        return false;

    stride = (**pixmap).rowBytes & 0x3FFF;
    destination = (unsigned char *)GetPixBaseAddr(pixmap);

    if (destination == NULL || stride < 320) {
        UnlockPixels(pixmap);
        return false;
    }

    for (y = 0; y < 200; y++) {
        memcpy(destination + y * stride,
               source + y * 320,
               320);
    }

    UnlockPixels(pixmap);
    return true;
}

/* Draw the original 320 x 200 Dune II map. */
/* Display skipped simulation ticks once per second. */


static uint32 g_profSimTime = 0;
static uint32 g_profBuildTime = 0;
static uint32 g_profCopyTime = 0;


/* Stage 15H: Live Construction Yard state diagnostic. */
static void Mac_DrawStructureLiveDiagnostic(void)
{
    unsigned int index;
    unsigned int count = 0;
    Structure *yard = NULL;
    char message[120];
    Str255 text;
    unsigned int i;
    unsigned int used = 0;
    unsigned int onMap = 0;
    unsigned int tileOccupied = 0;
    unsigned int hp = 0;
    unsigned int state = 0;

    if (g_missionTestResult != 1)
        return;

    for (index = 0; index < STRUCTURE_INDEX_MAX_SOFT; index++) {
        Structure *s = Structure_Get_ByIndex(index);

        if (s == NULL || !s->o.flags.s.used)
            continue;

        if (s->o.type == STRUCTURE_CONSTRUCTION_YARD)
            yard = s;

        if (!s->o.flags.s.isNotOnMap)
            count++;
    }

    if (yard != NULL) {
        unsigned int x = yard->o.position.x >> 8;
        unsigned int y = yard->o.position.y >> 8;

        used = yard->o.flags.s.used ? 1 : 0;
        onMap = yard->o.flags.s.isNotOnMap ? 0 : 1;
        hp = yard->o.hitpoints;
        state = yard->state;

        if (onMap && x < 64 && y < 64)
            tileOccupied = g_map[y * 64 + x].hasStructure ? 1 : 0;
    }

    TextFont(0);
    TextSize(10);
    TextFace(0);

    sprintf(message,
            "LIVE STR:%u YARD:%u MAP:%u TILE:%u",
            count, used, onMap, tileOccupied);

    text[0] = (unsigned char)strlen(message);
    for (i = 0; i < text[0]; i++)
        text[i + 1] = (unsigned char)message[i];

    MoveTo(20, 260);
    DrawString(text);

    sprintf(message, "YARD HP:%u STATE:%u",
            hp, state);

    text[0] = (unsigned char)strlen(message);
    for (i = 0; i < text[0]; i++)
        text[i + 1] = (unsigned char)message[i];

    MoveTo(20, 275);
    DrawString(text);

    /* Stage 15K: Inspect the four Construction Yard tiles. */
    if (yard != NULL && onMap) {
        unsigned int x = yard->o.position.x >> 8;
        unsigned int y = yard->o.position.y >> 8;

        if (x < 63 && y < 63) {
            unsigned int p = y * 64 + x;

            sprintf(message, "YARD TILES: %u %u %u %u",
                (unsigned int)g_map[p].groundTileID,
                (unsigned int)g_map[p + 1].groundTileID,
                (unsigned int)g_map[p + 64].groundTileID,
                (unsigned int)g_map[p + 65].groundTileID);

            text[0] = (unsigned char)strlen(message);
            for (i = 0; i < text[0]; i++)
                text[i + 1] = (unsigned char)message[i];

            MoveTo(20, 290);
            DrawString(text);
        }
    }
}

static uint32 r3b3bCopiedPixels = 0;
static uint32 r3b3bCopyRects = 0;
static uint32 r3b3bComparedTicks = 0;
static uint32 r3b3bProbeOK = 0;
static uint32 r3b3bProbeNull = 0;
static uint32 r3b3bProbeStride = 0;

/* R3C.2-D1: Read-only QuickDraw configuration diagnostics. */
static unsigned int r3c2SrcDepth = 0;
static unsigned int r3c2DstDepth = 0;
static unsigned int r3c2SrcStride = 0;
static unsigned int r3c2DstStride = 0;
static int r3c2SrcLeft = 0;
static int r3c2SrcTop = 0;
static int r3c2DstLeft = 0;
static int r3c2DstTop = 0;
static unsigned int r3c2Samples = 0;


/* R3D7P4-B: read-only Wind Trap tile palette audit. */
static void P4B_WindTrapPaletteDiagnostic(char *out)
{
    unsigned long histogram[256];
    unsigned int i, j, k, best[3], eligible = 0, extended = 0;
    unsigned int house = 255, tiles = 0;
    Structure *wind = NULL;
    const StructureInfo *info;
    const uint16 *layout;
    unsigned int count;

    memset(histogram, 0, sizeof(histogram));
    best[0] = best[1] = best[2] = 0;
    strcpy(out, "P4B WT: NONE");
    if (g_tilesPixels == NULL || g_iconRTBL == NULL || g_iconRPAL == NULL)
        return;

    for (i = 0; i < STRUCTURE_INDEX_MAX_SOFT; i++) {
        Structure *s = Structure_Get_ByIndex(i);
        if (s != NULL && s->o.flags.s.used &&
            !s->o.flags.s.isNotOnMap && s->o.type == STRUCTURE_WINDTRAP) {
            wind = s;
            break;
        }
    }
    if (wind == NULL) return;
    info = &g_table_structureInfo[wind->o.type];
    layout = g_table_structure_layoutTiles[info->layout];
    count = g_table_structure_layoutTileCount[info->layout];

    for (j = 0; j < count; j++) {
        unsigned int packed = (unsigned int)Tile_PackTile(wind->o.position) + layout[j];
        uint16 tileID;
        const uint8 *palette, *pixels;
        if (packed >= 4096U) continue;
        tileID = g_map[packed].groundTileID;
        house = g_map[packed].houseID;
        palette = g_iconRPAL + (unsigned int)g_iconRTBL[tileID] * 16U;
        pixels = g_tilesPixels + (unsigned long)tileID * 128UL;
        tiles++;
        for (k = 0; k < 128; k++) {
            unsigned int v1 = palette[pixels[k] >> 4];
            unsigned int v2 = palette[pixels[k] & 15];
            histogram[v1]++;
            histogram[v2]++;
            if (v1 >= 0x90 && v1 <= 0x96) eligible++;
            if (v2 >= 0x90 && v2 <= 0x96) eligible++;
            if (v1 >= 0x97 && v1 <= 0x9F) extended++;
            if (v2 >= 0x97 && v2 <= 0x9F) extended++;
        }
    }
    for (i = 0; i < 256; i++) {
        if (histogram[i] > histogram[best[0]]) {
            best[2] = best[1]; best[1] = best[0]; best[0] = i;
        } else if (i != best[0] && histogram[i] > histogram[best[1]]) {
            best[2] = best[1]; best[1] = i;
        } else if (i != best[0] && i != best[1] &&
                   histogram[i] > histogram[best[2]]) {
            best[2] = i;
        }
    }
    sprintf(out, "P4B WT H:%u T:%u 90:%u 97:%u TOP:%02X,%02X,%02X",
            house, tiles, eligible, extended, best[0], best[1], best[2]);
}

/* R3D7P4-C: read-only palette-index and RGB analysis for one Wind Trap.
 * RAW and FINAL use the same tile pixels, with/without the existing
 * house remapping. Counts include every pixel of the four tiles. */
static void P4C_WindTrapPixelDiagnostic(char *out, Boolean finalColors)
{
    unsigned long hist[256];
    unsigned long pinkPixels = 0;
    unsigned int top[3] = {0, 0, 0};
    unsigned int pinkTop[2] = {0, 0};
    unsigned int i, j, k, tiles = 0, house = 255;
    Structure *wind = NULL;
    const StructureInfo *info;
    const uint16 *layout;
    unsigned int count;
    ColorTable *ct;

    memset(hist, 0, sizeof(hist));
    strcpy(out, "P4C WT: NONE");
    if (!g_tilesPixels || !g_iconRTBL || !g_iconRPAL || !g_duneColorTable)
        return;
    for (i = 0; i < STRUCTURE_INDEX_MAX_SOFT; i++) {
        Structure *candidate = Structure_Get_ByIndex(i);
        if (candidate && candidate->o.flags.s.used &&
            !candidate->o.flags.s.isNotOnMap &&
            candidate->o.type == STRUCTURE_WINDTRAP) {
            wind = candidate;
            break;
        }
    }
    if (!wind) return;
    info = &g_table_structureInfo[wind->o.type];
    layout = g_table_structure_layoutTiles[info->layout];
    count = g_table_structure_layoutTileCount[info->layout];
    ct = *g_duneColorTable;
    for (j = 0; j < count; j++) {
        unsigned int packed = (unsigned int)Tile_PackTile(wind->o.position) + layout[j];
        uint16 tileID;
        const uint8 *palette, *pixels;
        if (packed >= 4096U) continue;
        tileID = g_map[packed].groundTileID;
        house = g_map[packed].houseID;
        palette = g_iconRPAL + (unsigned int)g_iconRTBL[tileID] * 16U;
        pixels = g_tilesPixels + (unsigned long)tileID * 128UL;
        tiles++;
        for (k = 0; k < 256; k++) {
            unsigned int nibble = (k & 1) ? (pixels[k / 2] & 15) : (pixels[k / 2] >> 4);
            unsigned int index = palette[nibble];
            if (finalColors && house != 0 && house < HOUSE_MAX &&
                index >= 0x90 && index <= 0x96)
                index = (index + (house << 4)) & 255;
            hist[index]++;
        }
    }
    for (i = 0; i < 256; i++) {
        const RGBColor *rgb = &ct->ctTable[i].rgb;
        unsigned long r = rgb->red, g = rgb->green, b = rgb->blue;
        Boolean pink = (r > 22000UL && b > 22000UL &&
                        r > g + 9000UL && b > g + 9000UL);
        if (hist[i] > hist[top[0]]) {
            top[2] = top[1]; top[1] = top[0]; top[0] = i;
        } else if (i != top[0] && hist[i] > hist[top[1]]) {
            top[2] = top[1]; top[1] = i;
        } else if (i != top[0] && i != top[1] && hist[i] > hist[top[2]]) {
            top[2] = i;
        }
        if (pink && hist[i]) {
            pinkPixels += hist[i];
            if (hist[i] > hist[pinkTop[0]]) {
                pinkTop[1] = pinkTop[0]; pinkTop[0] = i;
            } else if (i != pinkTop[0] && hist[i] > hist[pinkTop[1]]) {
                pinkTop[1] = i;
            }
        }
    }
    sprintf(out, "P4C %s H:%u T:%u P:%lu I:%02X,%02X TOP:%02X,%02X,%02X",
            finalColors ? "FIN" : "RAW", house, tiles, pinkPixels,
            pinkTop[0], pinkTop[1], top[0], top[1], top[2]);
}

static void Mac_DrawPerformanceDiagnostics(void)
{
    static uint32 lastTime = 0;
    static uint32 lastGameClock = 0;
    static uint32 lastSimCalls = 0;
    static uint32 lastSimTime = 0;
    static uint32 lastBuildTime = 0;
    static uint32 lastCopyTime = 0;
    static uint32 lastTerrainCalls = 0;
    static uint32 lastCopyCalls = 0;
    static uint32 lastFullRefreshes = 0;
    static uint32 lastDecodedTiles = 0;
    static uint32 lastRestoreTicks = 0;
    static uint32 lastSpritesTicks = 0;
    static uint32 lastPixels = 0;
    static uint32 lastRects = 0;
    static uint32 lastCompare = 0;
    static uint32 r2dDisplayCycle = 0;
    static unsigned long lastCells = 0;
    static unsigned long lastProbeFrames = 0;
    static unsigned long lastScrollFrames = 0;
    static unsigned long lastChangedFrames = 0;
    static unsigned long lastAnimationChanges = 0;
    static char message[128] = "PROFILER: WAITING";
    uint32 now = TickCount();
    Str255 text;
    Rect area;
    unsigned int i;

    if (lastTime == 0) {
        lastTime = now;
        lastGameClock = g_timerGame;
    }

    P1_Snapshot();
    if ((uint32)(now - lastTime) >= 60) {
        uint32 elapsed = now - lastTime;
        uint32 calls = g_profSimCalls - lastSimCalls;
        uint32 sim = g_profSimTime - lastSimTime;
        uint32 build = g_profBuildTime - lastBuildTime;
        uint32 copy = g_profCopyTime - lastCopyTime;
        uint32 clock = g_timerGame - lastGameClock;

        /* R3B.2: cycle through three diagnostic pages.
         * Measurements are 60 Hz ticks per second.
         */
        {
            uint32 page;
            if (p2DisplayMode == 1) page = 6;
            else if (p2DisplayMode == 2) page = 7;
            else if (p2DisplayMode == 3) page = 8;
            else if (p2DisplayMode == 4) page = 9;
            else if (p2DisplayMode == 5) page = 10;
            else if (p2DisplayMode == 6) page = 11;
            else if (p2DisplayMode == 7) page = 12;
            else if (p2DisplayMode == 8) page = 13;
            else { page = r2dDisplayCycle % 9; r2dDisplayCycle++; }

            if (page == 12) {
                unsigned long avgUs = p2BlitSnapCalls ?
                    p2BlitSnapUs / p2BlitSnapCalls : 0UL;

                sprintf(message,
                        "P2 BLIT N:%lu AVG:%luus MAX:%luus",
                        p2BlitSnapCalls,
                        avgUs,
                        p2BlitSnapMaxUs);

            } else if (page == 13) {
                sprintf(message,
                        "R5A F:%lu S:%lu Q:%lu V:%lu I:%lu U:%lu",
                        r5Frames, r5Sim, r5Quiet, r5Scroll,
                        r5Input, r5StableSim);

            } else if (page == 10 || page == 11) {
                P4C_WindTrapPixelDiagnostic(message, page == 11);
            } else if (page == 9) {
                P4B_WindTrapPaletteDiagnostic(message);
            } else if (page == 8) {
                sprintf(message, "P3 PAL DIFF:%u SRC:%u DST:%u N:%u",
                        p3PaletteMismatch, p3PaletteSourceCount,
                        p3PaletteDestCount, p3PaletteSamples);
            } else if (page == 6) {
                sprintf(message, "P2 %% S:%lu B:%lu DRAW:%lu BLIT:%lu",
                        P1_Pct(P1_SIM), P1_Pct(P1_BUILD),
                        P1_Pct(P1_COPY),
                        p1WallUs ? ((p2BlitSnapUs / 1000UL) * 100UL /
                                    (p1WallUs / 1000UL ? p1WallUs / 1000UL : 1UL)) : 0UL);
            } else if (page == 7) {
                sprintf(message, "P2 ms D:%lu Q:%lu N:%lu MAX:%lu",
                        p1Snapshot[P1_COPY] / 1000UL,
                        p2BlitSnapUs / 1000UL,
                        p2BlitSnapCalls,
                        p2BlitSnapMaxUs / 1000UL);
            } else if (page == 0) {
                sprintf(message,
                        "SIM:%lu CLK:%lu S:%lu B:%lu C:%lu",
                        (unsigned long)(calls * 60 / elapsed),
                        (unsigned long)(clock * 60 / elapsed),
                        (unsigned long)(sim * 60 / elapsed),
                        (unsigned long)(build * 60 / elapsed),
                        (unsigned long)(copy * 60 / elapsed));
            } else if (page == 1) {
                sprintf(message,
                        "R:%lu CP:%lu F:%lu T:%lu",
                        (unsigned long)((r2dTerrainCalls -
                            lastTerrainCalls) * 60 / elapsed),
                        (unsigned long)((r2dCopyCalls -
                            lastCopyCalls) * 60 / elapsed),
                        (unsigned long)((r2dFullRefreshes -
                            lastFullRefreshes) * 60 / elapsed),
                        (unsigned long)((r2dDecodedTiles -
                            lastDecodedTiles) * 60 / elapsed));
            } else if (page == 2) {
                sprintf(message,
                        "R3B REST:%lu SPR:%lu COPY:%lu",
                        (unsigned long)((r3bRestoreTicks -
                            lastRestoreTicks) * 60 / elapsed),
                        (unsigned long)((r3bSpritesTicks -
                            lastSpritesTicks) * 60 / elapsed),
                        (unsigned long)(copy * 60 / elapsed));
            } else if (page == 3) {
                sprintf(message,
                        "A2 OK:%lu NULL:%lu STR:%lu",
                        (unsigned long)r3b3bProbeOK,
                        (unsigned long)r3b3bProbeNull,
                        (unsigned long)r3b3bProbeStride);
            } else if (page == 4) {
                sprintf(message,
                        "D1 S:%u/%u D:%u/%u XY:%d,%d/%d,%d N:%u",
                        r3c2SrcDepth, r3c2SrcStride,
                        r3c2DstDepth, r3c2DstStride,
                        r3c2SrcLeft, r3c2SrcTop,
                        r3c2DstLeft, r3c2DstTop,
                        r3c2Samples);
            } else {
                unsigned long frames = r3d7c23FrameCount - lastProbeFrames;
                unsigned long cells = r3d7c23CoveredCells - lastCells;
                unsigned long scrolls = r3d7c23ScrollFrames - lastScrollFrames;
                unsigned long changedFrames = r3d7c23ChangedFrames - lastChangedFrames;
                unsigned long animations = r3d7c2AnimationChanges - lastAnimationChanges;
                /* One decimal place: 1/240 cells/frame is only 0.4%.
                 * Raw cell total makes the measurement auditable. */
                unsigned long tenths = frames ?
                    (cells * 1000UL / (frames * 240UL)) : 0;
                sprintf(message, "C24 S:%lu.%lu%% C:%lu F:%lu SC:%lu CH:%lu AN:%lu",
                        tenths / 10UL, tenths % 10UL, cells, frames,
                        scrolls, changedFrames, animations);
            }
        }
        lastCells = r3d7c23CoveredCells;
        lastProbeFrames = r3d7c23FrameCount;
        lastScrollFrames = r3d7c23ScrollFrames;
        lastChangedFrames = r3d7c23ChangedFrames;
        lastAnimationChanges = r3d7c2AnimationChanges;

        lastPixels = r3b3bCopiedPixels;
        lastRects = r3b3bCopyRects;
        lastCompare = r3b3bComparedTicks;
        lastRestoreTicks = r3bRestoreTicks;
        lastSpritesTicks = r3bSpritesTicks;

        lastTerrainCalls = r2dTerrainCalls;
        lastCopyCalls = r2dCopyCalls;
        lastFullRefreshes = r2dFullRefreshes;
        lastDecodedTiles = r2dDecodedTiles;

        lastSimCalls = g_profSimCalls;
        lastSimTime = g_profSimTime;
        lastBuildTime = g_profBuildTime;
        lastCopyTime = g_profCopyTime;
        lastGameClock = g_timerGame;
        lastTime = now;
    }

    /* R2D.1: visible profiler below harvester diagnostics. */
    SetRect(&area, 20, 365, 440, 380);
    EraseRect(&area);

    text[0] = (unsigned char)strlen(message);
    for (i = 0; i < text[0]; i++)
        text[i + 1] = (unsigned char)message[i];

    TextSize(9);
    MoveTo(20, 377);
    DrawString(text);
}

/* R3B.3b: 8-pixel horizontal bands, fixed 64 KB history.
 * No allocation or decoding in the screen-transfer loop.
 * We deliberately fall back to a full blit for scrolling, exposure,
 * and when more than half the bands changed.
 */
#define R3B3B_BAND_HEIGHT 8
#define R3B3B_BAND_COUNT 25
static unsigned char r3b3bPrevious[320 * 200];
static Boolean r3b3bHistoryValid = false;
static Boolean r3b3bScheduledUpdate = false;
static int r3b3bPreviousViewX = -1;
static int r3b3bPreviousViewY = -1;

static void DrawDuneMap(WindowPtr window)
{
    PixMapHandle source;
    Rect sourceRect;
    Rect destinationRect;

    if (g_duneMapWorld == NULL || window == NULL || p1SkipBlit)
        return;

    source = GetGWorldPixMap(g_duneMapWorld);
    if (source == NULL)
        return;

    SetRect(&sourceRect, 0, 0, 320, 200);
    SetRect(&destinationRect, 20, 20, 340, 220);

    if (!LockPixels(source))
        return;

    /* F12C11 is diagnostic: compare before the existing full-screen copy. */
    /* F12C12 isolation: pixel scanner disabled, full CopyBits unchanged. */
    f12c11CopyCount++;
    Q5B_CopyStart();
    {
    unsigned long p2StartUs = P1_Now();
    unsigned long p2DurationUs;
    {
        Boolean forceFull = true; /* R4-M4B full-frame presentation control */
        R4_PresentCandidate(source, window, r4m3bPresentCells, forceFull);
        r4m3bPresentedSerial = r4m3bComposedSerial;
        r4m3bForceExposure = false;
        if (forceFull) r4m3bFullRefreshCounter = 0;
    }
    p2DurationUs = P1_Now() - p2StartUs;
    p2BlitTotalUs += p2DurationUs;
    p2BlitCalls++;
    if (p2DurationUs > p2BlitMaxUs) p2BlitMaxUs = p2DurationUs;
    }

    Q5B_CopyEnd();
    r3dCopies++;
    r2dCopyCalls++;

    /* R3D7P3: inspect palettes AFTER CopyBits, never inside timing.
     * Only compare existing PixMap color tables; no pixel writes.
     * 999 means a table is unavailable or has an unexpected size. */
    if (p3PaletteSamples == 0U) {
        PixMapHandle dst = ((CGrafPtr)window)->portPixMap;
        CTabHandle srcCT = (**source).pmTable;
        CTabHandle dstCT = (dst != NULL) ? (**dst).pmTable : NULL;
        p3PaletteMismatch = 999;
        p3PaletteSourceCount = srcCT ? (unsigned int)((**srcCT).ctSize + 1) : 0;
        p3PaletteDestCount = dstCT ? (unsigned int)((**dstCT).ctSize + 1) : 0;
        if (srcCT && dstCT && p3PaletteSourceCount == 256 &&
            p3PaletteDestCount == 256) {
            unsigned int mismatches = 0;
            unsigned int index;
            for (index = 0; index < 256; ++index) {
                const ColorSpec *a = &(**srcCT).ctTable[index];
                const ColorSpec *b = &(**dstCT).ctTable[index];
                if (a->rgb.red != b->rgb.red ||
                    a->rgb.green != b->rgb.green ||
                    a->rgb.blue != b->rgb.blue) mismatches++;
            }
            p3PaletteMismatch = mismatches;
        }
        p3PaletteSamples++;
    }

    /* R3C.2-D1: Inspect QuickDraw pixel-map metadata only.
       Do not change drawing, clipping, or buffer allocation. */
    if (r3c2Samples == 0U) {
        PixMapHandle destination =
            ((CGrafPtr)window)->portPixMap;

        if (destination != NULL) {
            r3c2SrcDepth = (unsigned int)(**source).pixelSize;
            r3c2DstDepth = (unsigned int)(**destination).pixelSize;
            r3c2SrcStride =
                (unsigned int)((**source).rowBytes & 0x3FFF);
            r3c2DstStride =
                (unsigned int)((**destination).rowBytes & 0x3FFF);
            r3c2SrcLeft = (**source).bounds.left;
            r3c2SrcTop = (**source).bounds.top;
            r3c2DstLeft = (**destination).bounds.left;
            r3c2DstTop = (**destination).bounds.top;
            r3c2Samples++;
        }
    }

    /* A2: Probe framebuffer assumptions without
       preventing the known-good full-screen blit. */
    {
        short a2Stride = (**source).rowBytes & 0x3FFF;
        unsigned char *a2Pixels =
            (unsigned char *)GetPixBaseAddr(source);

        r3b3bCopyRects++;
        r3b3bCopiedPixels += 64000UL;

        if (a2Pixels == NULL)
            r3b3bProbeNull++;
        else if (a2Stride < 320)
            r3b3bProbeStride++;
        else
            r3b3bProbeOK++;
    }
    UnlockPixels(source);
}

static void DrawTestResult(Boolean passed, int pakResult, int cpsResult)
{
    MoveTo(20, 35);
    DrawString("\pOpenDUNE - Macintosh 68k");

    MoveTo(20, 65);
    DrawString("\pOriginal Format 80 decoder");

    MoveTo(20, 95);

    if (passed) {
        DrawString("\pPASS: decoded DUNE correctly");
    } else {
        DrawString("\pFAIL: decoder result incorrect");
    }

    MoveTo(20, 130);
    DrawString("\pClose window or press ESC to quit.");

    MoveTo(20, 160);
    switch (pakResult) {
        case 0:
            DrawString("\pPASS: GRAYRMAP.TBL loaded");
            break;
        case 1:
            DrawString("\pPAK FAIL: File_Init");
            break;
        case 2:
            DrawString("\pPAK FAIL: resource not found");
            break;
        case 3:
            DrawString("\pPAK FAIL: unexpected size");
            break;
        case 4:
            DrawString("\pPAK FAIL: read error");
            break;
    }

    MoveTo(20, 190);

    switch (cpsResult) {
        case 0:
            DrawString("\pPASS: DUNEMAP.CPS decoded (64000 bytes)");
            break;
        case 1:
            DrawString("\pCPS FAIL: File_Init");
            break;
        case 2:
            DrawString("\pCPS FAIL: resource not found");
            break;
        case 3:
            DrawString("\pCPS FAIL: memory allocation");
            break;
        case 4:
            DrawString("\pCPS FAIL: header or read error");
            break;
        case 5:
            DrawString("\pCPS FAIL: unsupported format");
            break;
        case 6:
            DrawString("\pCPS FAIL: decoded size incorrect");
            break;
    }

    MoveTo(20, 220);

    if (cpsResult != 0) {
        DrawString("\pGWORLD: skipped (CPS decode failed)");
    } else if (g_duneMapWorldReady) {
        DrawString("\pGWORLD PASS: 320 x 200, 8-bit buffer");
    } else {
        {
            char message[80];
            Str255 text;
            unsigned int i;

            sprintf(message, "GWORLD FAIL: error %d",
                    (int)g_duneGWorldError);

            text[0] = (unsigned char)strlen(message);
            for (i = 0; i < text[0]; i++)
                text[i + 1] = (unsigned char)message[i];

            DrawString(text);
        }
    }

}


/*
 * DOS-style mouse edge scrolling.
 * Check the pointer without requiring a mouse click.
 */
static void Mac_UpdateEdgeScrolling(WindowPtr window)
{
    static unsigned long lastScrollTick = 0;
    unsigned long now;
    Point mouse;
    Rect redrawRect;
    Boolean moved = false;
    int minX, maxX, minY, maxY;

    Mac_GetScrollBounds(&minX, &maxX, &minY, &maxY);

    if (FrontWindow() != window || !g_duneMapWorldReady)
        return;

    GetMouse(&mouse);

    /* Only scroll while the pointer is inside the battlefield. */
    if (mouse.h < 20 || mouse.h >= 340 ||
        mouse.v < 20 || mouse.v >= 212)
        return;

    now = TickCount();

    if ((unsigned long)(now - lastScrollTick) < 6)
        return;

    if (mouse.h < 32 && g_viewTileX > minX) {
        g_viewTileX--;
        moved = true;
    } else if (mouse.h >= 328 && g_viewTileX < maxX) {
        g_viewTileX++;
        moved = true;
    }

    if (mouse.v < 32 && g_viewTileY > minY) {
        g_viewTileY--;
        moved = true;
    } else if (mouse.v >= 200 && g_viewTileY < maxY) {
        g_viewTileY++;
        moved = true;
    }

    if (!moved)
        return;

    lastScrollTick = now;
    r3dScrolls++;

    /* R3B.1: viewport changed.
     * The regular render scheduler will compose the next
     * frame. Avoid a second immediate terrain rebuild.
     */
}


/* Stage 15O-N.1: original Mission 1 credit objective. */

static uint32 stage15ON1MissionStartTick = 0;

/* R3D.5: Resource-free Macintosh Mission 1 completion screen.
 * Called only after the existing scenario credit victory flag becomes true.
 * While this function waits for acknowledgement, the normal simulation loop
 * does not run. No DOS UI resources or extra frame buffers are required. */
static void R3D5_ShowVictory(WindowPtr window)
{
    Rect area;
    EventRecord event;
    char message[128];
    Str255 line;
    unsigned int i;
    unsigned int length;

    if (window == NULL) return;

    SetPort(window);
    SetRect(&area, 0, 0, 700, 500);
    EraseRect(&area);
    TextFont(0);
    TextSize(18);
    TextFace(bold);
    MoveTo(65, 115);
    DrawString("\pMISSION COMPLETE");

    TextFace(0);
    TextSize(12);
    sprintf(message, "Mission successful. Credits: %u / %u",
        (unsigned int)g_playerHouse->credits,
        (unsigned int)g_playerHouse->creditsQuota);
    length = strlen(message);
    if (length > 255) length = 255;
    line[0] = (unsigned char)length;
    for (i = 0; i < length; i++) line[i + 1] = (unsigned char)message[i];
    MoveTo(65, 155);
    DrawString(line);
    MoveTo(65, 195);
    DrawString("\pPress any key or click to exit.");

    FlushEvents(keyDownMask | keyUpMask | mDownMask | mUpMask, 0);
    while (true) {
        if (!WaitNextEvent(everyEvent, &event, 30, NULL)) continue;
        if (event.what == keyDown || event.what == mouseDown) break;
        if (event.what == updateEvt) {
            BeginUpdate(window);
            EraseRect(&area);
            TextFont(0);
            TextSize(18);
            TextFace(bold);
            MoveTo(65, 115);
            DrawString("\pMISSION COMPLETE");
            TextFace(0);
            TextSize(12);
            MoveTo(65, 155);
            DrawString(line);
            MoveTo(65, 195);
            DrawString("\pPress any key or click to exit.");
            EndUpdate(window);
        }
    }
}


static void R4G3B3_ShowDefeat(WindowPtr window)
{
    Rect area;
    EventRecord event;
    char message[128];
    Str255 line;
    unsigned int i;
    unsigned int length;

    if (window == NULL) return;

    SetPort(window);
    SetRect(&area, 0, 0, 700, 500);
    EraseRect(&area);
    TextFont(0);
    TextSize(18);
    TextFace(bold);
    MoveTo(65, 115);
    DrawString("\pMISSION FAILED");

    TextFace(0);
    TextSize(12);
    sprintf(message, "Mission unsuccessful. Credits: %u / %u",
        (unsigned int)g_playerHouse->credits,
        (unsigned int)g_playerHouse->creditsQuota);
    length = strlen(message);
    if (length > 255) length = 255;
    line[0] = (unsigned char)length;
    for (i = 0; i < length; i++) line[i + 1] = (unsigned char)message[i];
    MoveTo(65, 155);
    DrawString(line);
    MoveTo(65, 195);
    DrawString("\pPress any key or click to exit.");

    FlushEvents(keyDownMask | keyUpMask | mDownMask | mUpMask, 0);
    while (true) {
        if (!WaitNextEvent(everyEvent, &event, 30, NULL)) continue;
        if (event.what == keyDown || event.what == mouseDown) break;
        if (event.what == updateEvt) {
            BeginUpdate(window);
            EraseRect(&area);
            TextFont(0);
            TextSize(18);
            TextFace(bold);
            MoveTo(65, 115);
            DrawString("\pMISSION FAILED");
            TextFace(0);
            TextSize(12);
            MoveTo(65, 155);
            DrawString(line);
            MoveTo(65, 195);
            DrawString("\pPress any key or click to exit.");
            EndUpdate(window);
        }
    }
}

static void Stage15ON1_UpdateMission(void)
{
    struct PoolFindStruct find;
    Structure *s;
    uint16 friendly = 0;
    uint16 enemy = 0;
    Boolean quotaReached = false;
    Boolean finished = false;
    Boolean won = false;

    if (stage15ON1MissionWon || stage15ON1MissionLost)
        return;

    if (g_missionTestResult != 1 || g_playerHouse == NULL)
        return;

    if ((uint32)(g_timerGame -
                 stage15ON1MissionStartTick) < 7200)
        return;

    find.houseID = HOUSE_INVALID;
    find.index = 0xFFFF;
    find.type = 0xFFFF;

    while ((s = Structure_Find(&find)) != NULL) {
        if (s->o.type == STRUCTURE_SLAB_1x1 ||
            s->o.type == STRUCTURE_SLAB_2x2 ||
            s->o.type == STRUCTURE_WALL ||
            s->o.type == STRUCTURE_TURRET ||
            s->o.type == STRUCTURE_ROCKET_TURRET)
            continue;

        if (s->o.houseID == g_playerHouseID)
            friendly++;
        else
            enemy++;
    }

    quotaReached =
        g_playerHouse->credits >= g_playerHouse->creditsQuota;

    if ((g_scenario.winFlags & 0x1) && enemy == 0)
        finished = true;

    if ((g_scenario.winFlags & 0x2) && friendly == 0)
        finished = true;

    if ((g_scenario.winFlags & 0x4) && quotaReached)
        finished = true;

    if (!finished)
        return;

    if ((g_scenario.loseFlags & 0x1) && enemy == 0)
        won = true;

    if ((g_scenario.loseFlags & 0x4) && quotaReached)
        won = true;

    if (won)
        stage15ON1MissionWon = true;
    else
        stage15ON1MissionLost = true;
}


static void Stage15ON1_ShowObjective(WindowPtr window)
{
    char message[240];
    Str255 macMessage;
    unsigned int i;
    unsigned int length;

    if (window == NULL)
        return;

    if (g_missionTestResult != 1 ||
        g_playerHouse == NULL) {
        strcpy(message, "Dune II - Mission not loaded");
    } else {
        sprintf(
            message,
            "Dune II M2 - Credits %u/%u - %s",
            (unsigned int)g_playerHouse->credits,
            (unsigned int)g_playerHouse->creditsQuota,
            stage15ON1MissionWon ?
                "Objective reached" :
                ((g_scenario.winFlags & 4) ?
                    "Collect spice" : "Mission active")
        );
    }

    length = strlen(message);

    if (length > 255)
        length = 255;

    macMessage[0] = (unsigned char)length;

    for (i = 0; i < length; i++)
        macMessage[i + 1] = message[i];

    SetWTitle(window, macMessage);
}

int main(void)
{
    WindowPtr window;
    WindowPtr clickedWindow;
    EventRecord event;
    Rect bounds;
    Rect dragBounds;
    short part;
    Boolean running = true;
    Boolean passed;
    int pakResult;
    int cpsResult;

    InitGraf(&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitDialogs(NULL);
    InitCursor();

    /* R3D7S1: preload original GUNSHOT sample once; gameplay remains usable if absent. */
    DuneGameSound_Init();

    /* Initialize the original OpenDUNE engine pools. */
    MacMission_InitializePools();

    /* Stage 15M: Initialize the original animation pool
       before loading mission structures. */
    Animation_Init();
    Explosion_Init();

    /* Initialize OpenDUNE's shared INI parsing buffer. */
    g_readBufferSize = 12000;
    g_readBuffer = calloc(1, g_readBufferSize);

    if (g_readBuffer == NULL) {
        g_readBufferSize = 0;
        return 1;
    }

    /* Initialize original graphics buffers. */
    GFX_Init();


    passed = RunOpenDUNETest();
    pakResult = RunPakTest();
    cpsResult = RunCPSTest();

    /* Stage 12A: Initialize original unit scripting. */
    if (File_Init()) {
        g_moveScriptReady =
            Script_LoadFromFile("UNIT.EMC",
                                g_scriptUnit,
                                g_scriptFunctionsUnit,
                                NULL) != 0;

            /* Stage 13: Load original team AI scripts before
               Scenario_Load creates any teams. */
            g_teamScriptReady =
                Script_LoadFromFile("TEAM.EMC",
                                    g_scriptTeam,
                                    g_scriptFunctionsTeam,
                                    NULL) != 0;

            /* Stage 14: Initialize original building scripts.
               Structure simulation remains disabled for now. */
            g_structureScriptReady =
                Script_LoadFromFile("BUILD.EMC",
                                    g_scriptStructure,
                                    g_scriptFunctionsStructure,
                                    NULL) != 0;
    }

    /* Stage 14F: Reuse the file system initialized for scripts.
       Do not rebuild its resource indexes before loading the mission. */
    if (!File_Exists("SCENA002.INI"))
        g_missionTestResult = 2;
    else
        g_missionTestResult = (g_campaignID = 2, Scenario_Load(2, 1) ? 1 : 0);

    if (g_missionTestResult == 1)
        R3D8C_SetInitialCamera();

    if (g_missionTestResult == 1) {
        g_missionUnitCount = Mac_CountMissionUnits();
        g_missionStructureCount = Mac_CountMissionStructures();
    }

    g_dunePaletteLoaded = LoadDunePalette();

    if (g_dunePaletteLoaded)
        CreateDuneMacPalette();

    if (cpsResult == 0 && g_duneColorTable != NULL) {
        g_duneMapWorldReady = CreateDuneMapWorld();

    /* Stage 10J: Load original Dune II unit graphics. */
    if (g_missionTestResult == 1) {
        /* Stage 14H: The palette loader now preserves the active
           file session. Reuse it for original Dune II sprites. */
        Sprites_Init();

        if (g_sprites != NULL &&
            g_sprites[111] != NULL &&
            g_sprites[151] != NULL &&
            g_sprites[238] != NULL) {
            g_unitSpriteTestResult = 1;
        } else {
            g_unitSpriteTestResult = 2;
        }
    }

        if (g_duneMapWorldReady && g_missionTestResult == 1) {
            R3D6_UpdateExploration();
            if (!CreateOriginalTerrainWorld())
                CreateMissionTerrainWorld();
        }
    }

    SetRect(&bounds, 60, 60, 500, 440);

    window = NewCWindow(
        NULL,
        &bounds,
        "\pOpenDUNE 68k - Shift Click",
        true,
        documentProc,
        (WindowPtr)-1,
        true,
        0
    );

    if (window == NULL)
        return 1;

    /* Associate the original Dune II palette with this window. */
    if (g_duneMacPalette != NULL) {
        NSetPalette(window, g_duneMacPalette, 0);
        ActivatePalette(window);
    }

    SetRect(&dragBounds, 4, 24, 636, 476);

    /* Activate the clocks used by the original game simulation. */


    /* R4-G3A: Do not create a diagnostic Sandworm.
     * Scenario-defined units remain unchanged.
     */

    Timer_SetTimer(TIMER_GAME, true);
    Timer_SetTimer(TIMER_GUI, true);
    stage15ON1MissionStartTick = g_timerGame;

    /* Stage 15O-J.1: T toggles automatic redraws; R takes a snapshot. */
    Boolean stage15OJFreezeRedraw = false;
    Boolean stage15OKUseCatchup = true;
    uint32 stage15OKLastSimulationTick = 0;

    while (running) {
        /* R3D7S2: sound playback outside engine simulation. */
        { unsigned long p1 = P1_Now();
          DuneGameSound_Update();
          P1_Add(P1_SOUND, p1); }
        /* Advance the original OpenDUNE clocks. */
        MacTimer_Update();
        Stage15OQ3_UpdateHouses();
        Stage15ON1_UpdateMission();
        R3D6_UpdateExploration();
        if (stage15ON1MissionWon || stage15ON1MissionLost) {
            if (stage15ON1MissionWon)
                R3D5_ShowVictory(window);
            else
                R4G3B3_ShowDefeat(window);
            running = false;
            break;
        }

        /* Stage 15O-P.1: original Harvester delivery check.
         * Keep this independent of the unported house simulation.
         */
        if (g_missionTestResult == 1 &&
            g_moveScriptReady &&
            g_playerHouse != NULL) {
            static uint32 nextHarvesterCheck = 0;

            if (g_timerGame >= nextHarvesterCheck) {
                nextHarvesterCheck = g_timerGame + 900;
                {
                    struct PoolFindStruct houseFind;
                    House *economyHouse;

                    houseFind.houseID = HOUSE_INVALID;
                    houseFind.index = 0xFFFF;
                    houseFind.type = 0xFFFF;

                    while ((economyHouse = House_Find(&houseFind)) != NULL) {
                        Mac_House_EnsureHarvesterAvailable(
                            (uint8)economyHouse->index);
                    }
                }
            }
        }

        if (g_moveScriptReady && g_missionTestResult == 1) {
            static uint32 lastRenderTick = 0;
            uint32 lastSimulationTick = stage15OKLastSimulationTick;
            uint32 now = g_timerGame;

#if STAGE15OK_BOUNDED_CATCHUP
            if (stage15OKUseCatchup) {
            /* Bounded catch-up using original engine routines. */
            if (lastSimulationTick != 0) {
                uint32 backlog = now - lastSimulationTick;
                if (backlog > q4BacklogPeak)
                    q4BacklogPeak = backlog;
            }

            if (lastSimulationTick == 0)
                lastSimulationTick = now;

            if ((uint32)(now - lastSimulationTick) > 24) {
                g_perfSkippedTicks +=
                    (uint32)(now - lastSimulationTick) - 24;
                lastSimulationTick = now - 24;
            }

            {
                unsigned int steps = 0;

                while ((uint32)(now - lastSimulationTick) >= 3 &&
                       steps < 2) {
                    uint32 startTime = TickCount();
                    unsigned long p1 = P1_Now();

                    lastSimulationTick += 3;
                    g_timerGame = lastSimulationTick;

                    {
                        uint32 q4Start = TickCount();
                        Q5B_SimStart();
                        if (g_teamScriptReady) GameLoop_Team();
                        GameLoop_Unit();
                        Q5D_RecordMovement();
                        q4UnitTime += TickCount() - q4Start;
                        q4UnitCalls++;
                    }

                    if (g_structureScriptReady) {
                        uint32 q4Start = TickCount();
                        GameLoop_Structure();
                        Mac_D1DeployInfantry();
                        Mac_G2DeployTrikes();
                        q4StructureTime += TickCount() - q4Start;
                        q4StructureCalls++;
                    }

                    Animation_Tick();
                    Explosion_Tick();
                    r5Steps++;
                    Q5B_SimEnd();

                    g_timerGame = now;

                    g_profSimTime += TickCount() - startTime;
                    g_profSimCalls++;
                    P1_Add(P1_SIM, p1);
                    steps++;
                }
            }
            } else
#endif
            if (now != lastSimulationTick) {
                if (lastSimulationTick != 0 &&
                    (uint32)(now - lastSimulationTick) > 1)
                    g_perfSkippedTicks +=
                        (uint32)(now - lastSimulationTick) - 1;

                {
                    uint32 startTime = TickCount();
                    unsigned long p1 = P1_Now();

                    {
                        uint32 q4Start = TickCount();
                        Q5B_SimStart();
                        if (g_teamScriptReady) GameLoop_Team();
                        GameLoop_Unit();
                        Q5D_RecordMovement();
                        q4UnitTime += TickCount() - q4Start;
                        q4UnitCalls++;
                    }

                    if (g_structureScriptReady) {
                        uint32 q4Start = TickCount();
                        GameLoop_Structure();
                        Mac_D1DeployInfantry();
                        Mac_G2DeployTrikes();
                        q4StructureTime += TickCount() - q4Start;
                        q4StructureCalls++;
                    }

                    Animation_Tick();
                    Explosion_Tick();
                    r5Steps++;
                    Q5B_SimEnd();

                    g_profSimTime += TickCount() - startTime;
                    g_profSimCalls++;
                    P1_Add(P1_SIM, p1);
                }

                lastSimulationTick = now;
            }

            stage15OKLastSimulationTick = lastSimulationTick;

            /* R4-P1: adjustable 5/10/4 FPS presentation cadence.
             * Simulation and input timing are unchanged. */
            if (!stage15OJFreezeRedraw &&
                (uint32)(now - lastRenderTick) >= r4p1RenderInterval) {
                Rect redrawRect;
                Boolean prepared = false;
                uint32 startTime = TickCount();
                unsigned long p1 = P1_Now();

                lastRenderTick = now;
                r3dScheduled++;
                R3D2_SampleUnits();
                R5A_Sample();
                Q5B_RenderStart();

                if (g_duneMapWorldReady)
                    prepared = CreateOriginalTerrainWorld();

                Q5B_RenderEnd();

                {
                    uint32 q4Elapsed = TickCount() - startTime;
                    g_profBuildTime += q4Elapsed;
                    q4RenderTime += q4Elapsed;
                    q4RenderCalls++;
                }
                P1_Add(P1_BUILD, p1);

                if (prepared) {
                    r3dPrepared++;
                    /* R.2A: Only invalidate the battlefield.
                     * The status panel has its own update schedule.
                     */
                    SetRect(&redrawRect, 20, 20, 340, 220);
                    InvalRect(&redrawRect);
                    r3b3bScheduledUpdate = true;
                }
            }
        }

        P4D_AnimateWindTrapPalette(window);

        /* Q.4: update title diagnostics once per second. */
        Q4_ShowDiagnostics(window);
        Q5B_ShowDiagnostics(window);
        Q5D_ShowMovement(window);
        R3D1_ShowCounters(window);

        /* P.8: sample independently of updateEvt. */
        Stage15OP8_SamplePerformance();

        /* Refresh only the diagnostic panel once per second. */
        {
            uint32 hudNow = TickCount();

            if ((uint32)(hudNow - stage15OP8LastHUD) >= 120) {
                Rect hudRect;

                GrafPtr previousPort;

                stage15OP8LastHUD = hudNow;
                /* R2D.3: include construction and credit status
                 * in the existing one-second HUD refresh.
                 */
                SetRect(&hudRect, 20, 220, 440, 472);

                GetPort(&previousPort);
                SetPort(window);
                InvalRect(&hudRect);
                /* UI1.1: refresh credits, power and production in sidebar once/sec. */
                SetRect(&hudRect, 348, 20, 438, 219);
                InvalRect(&hudRect);
                SetPort(previousPort);
            }
        }

        Mac_UpdateEdgeScrolling(window);
        Mac_UpdatePlacementPreview(window);

        if (WaitNextEvent(
            everyEvent,
            &event,
            1,
            NULL
        )) {
            switch (event.what) {
                case mouseDown:
                    part = FindWindow(
                        event.where,
                        &clickedWindow
                    );

                    switch (part) {
                        case inDrag:
                            DragWindow(
                                clickedWindow,
                                event.where,
                                &dragBounds
                            );
                            break;

                        case inGoAway:
                            if (TrackGoAway(
                                clickedWindow,
                                event.where
                            )) {
                                running = false;
                            }
                            break;

                        case inContent:
                                if (clickedWindow != FrontWindow()) {
                                    SelectWindow(clickedWindow);
                                } else {
                                    Point localMouse = event.where;
                                    Rect redrawRect;

                                    SetPort(window);
                                    GlobalToLocal(&localMouse);
                                    if (!G1_HandleSidebarClick(localMouse))
                                        Mac_SelectAt(localMouse, (event.modifiers & shiftKey) != 0, (event.modifiers & optionKey) != 0);

                                    SetRect(&redrawRect, 20, 20, 340, 300);
                                    r3dSelectionInvalidations++;
                                    r3b3bScheduledUpdate = false;
                                    InvalRect(&redrawRect);
                                    /* UI1.1: selection changes must invalidate the sidebar too. */
                                    SetRect(&redrawRect, 348, 20, 438, 219);
                                    InvalRect(&redrawRect);
                                }
                                break;
                    }
                    break;

                case updateEvt:
                    if ((WindowPtr)event.message == window) {
                        /* F12C7: exposure diagnostic, no blit behavior change. */
                        f12c7ExposureEvents++;
                        /* R3B.3a: Inspect pending update region
                         * before BeginUpdate clears it.
                         * WindowPeek provides updateRgn.
                         */
                        Rect r3bBattleRect;
                        Boolean r3bBattleNeedsCopy;
                        Boolean r4m4aScheduledOnly = false;

                        SetRect(&r3bBattleRect,
                                20, 20, 340, 220);

                        r3bBattleNeedsCopy = RectInRgn(
                            &r3bBattleRect,
                            ((WindowPeek)window)->updateRgn);
                        {
                            Rect pendingBounds = (**((WindowPeek)window)->updateRgn).rgnBBox;
                            r4m4aScheduledOnly = r3b3bScheduledUpdate &&
                                EqualRect(&pendingBounds, &r3bBattleRect);
                        }

                        r3b3bScheduledUpdate = false;
                        BeginUpdate(window);
                        SetPort(window);
                        /* Avoid erasing the battlefield before CopyBits.
                         * On a real Macintosh IIci, the erase becomes visible
                         * as flickering during repeated updates.
                         */
                        if (!g_duneMapWorldReady) {
                            EraseRect(&window->portRect);
                        }
                        if (g_duneMapWorldReady) {
                            if (r3bBattleNeedsCopy) {
                                uint32 startTime = TickCount();
                                unsigned long p1 = P1_Now();
                                r4m3bForceExposure = !r4m4aScheduledOnly;
                                DrawDuneMap(window);
                                P1_Add(P1_COPY, p1);
                                r3b3bScheduledUpdate = false;
                                g_profCopyTime += TickCount() - startTime;
                            }

                            MoveTo(20, 245);
                            if (!g_dunePaletteLoaded)
                                DrawString("\pIBM.PAL FAIL: palette not loaded");
                            else if (g_duneMacPalette == NULL)
                                DrawString("\pPALETTE FAIL: NewPalette failed");
                        } else {
                            DrawTestResult(passed, pakResult, cpsResult);
                        /* Stage 14E: Initialization status */
                        {
                            char message[100];
                            Str255 macText;
                            unsigned int i;

                            TextFont(0);
                            TextSize(10);
                            TextFace(0);

                            sprintf(message,
                                    "MISSION:%d PAL:%d CTAB:%d",
                                    g_missionTestResult,
                                    g_dunePaletteLoaded ? 1 : 0,
                                    g_duneColorTable != NULL ? 1 : 0);

                            macText[0] = (unsigned char)strlen(message);
                            for (i = 0; i < macText[0]; i++)
                                macText[i + 1] = (unsigned char)message[i];

                            MoveTo(20, 295);
                            DrawString(macText);

                            sprintf(message,
                                    "SCRIPTS U:%d T:%d B:%d",
                                    g_moveScriptReady,
                                    g_teamScriptReady,
                                    g_structureScriptReady);

                            macText[0] = (unsigned char)strlen(message);
                            for (i = 0; i < macText[0]; i++)
                                macText[i + 1] = (unsigned char)message[i];

                            MoveTo(20, 315);
                            DrawString(macText);
                        }

                        }
                        
                        /* Stage 10M.2: Compact diagnostic display */
                        /* Unit-count diagnostic hidden for movement testing. */
                        /* Stage 15O-C.2:
                         * Clear only the status panel, never the battlefield.
                         * Prevent old production text from remaining visible.
                         */
                        {
                            Rect statusRect;
                            SetRect(&statusRect, 20, 220, 620, 335);
                            EraseRect(&statusRect);
                        }

                        UI1_DrawSidebar(); /* UI1 read-only right panel */
                        Mac_DrawSelectedUnit(); /* Selection graphics only */
                        Mac_DrawSelectedStructure();
                        Mac_DrawPlacementPreview();
                        Mac_DrawFactoryDiagnostics();
                        /* R2D.1: visible profiler. */
                        if (!p1SkipHUD) Mac_DrawPerformanceDiagnostics();
                        /* H1A.1: Harvester diagnostics are in the visible sidebar. */

                        EndUpdate(window);
                    }
                    break;

                case keyDown:
                case autoKey:
                {
                    unsigned char keyCode =
                        (unsigned char)((event.message >> 8) & 0xFF);
                    Boolean moved = false;
                    int minX, maxX, minY, maxY;
                    Mac_GetScrollBounds(&minX, &maxX, &minY, &maxY);

                    if (event.what == keyDown &&
                        ((event.message & charCodeMask) == 'h' ||
                         (event.message & charCodeMask) == 'H')) {
                        Unit *h1cSelected = Mac_SelectedUnit();
                        if (h1cSelected != NULL && h1cSelected->o.type == UNIT_HARVESTER) {
                            Rect h1cRect;
                            s_h1cPage = (s_h1cPage + 1) % 9;
                            SetRect(&h1cRect, 348, 20, 438, 219);
                            InvalRect(&h1cRect);
                            break;
                        }
                    }

                    if ((event.message & charCodeMask) == 27) {
                        running = false;
                        break;
                    }

                    /* P1: isolated live tests. KeyDown only; auto-repeat ignored.
                     * 1 terrain cache/decode, 2 sprite composition,
                     * 3 battlefield CopyBits, 7 performance HUD, 0 restore.
                     * 4/5/6 intentionally not assigned: simulation isolation
                     * requires a deterministic workload first. */
                    if (event.what == keyDown) {
                        unsigned char diagnosticKey =
                            (unsigned char)(event.message & charCodeMask);
                        Boolean handled = true;
                        switch (diagnosticKey) {
                            case '1':
                                p1SkipTerrain = !p1SkipTerrain;
                                g_cachedTerrainValid = false;
                                break;
                            case '2': p1SkipSprites = !p1SkipSprites; break;
                            case '3': p1SkipBlit = !p1SkipBlit; break;
                            case '4': r4p1RenderInterval = 15; break;
                            case '5': r4p1RenderInterval = 12; break;
                            case '6': r4p1RenderInterval = 6; break;
                            case '7': p1SkipHUD = !p1SkipHUD; break;
                            case '0':
                                p1SkipTerrain = p1SkipSprites =
                                    p1SkipBlit = p1SkipHUD = false;
                                g_cachedTerrainValid = false;
                                break;
                            default: handled = false; break;
                        }
                        if (handled) {
                            /* Make mode changes visible immediately.
                             * A redraw is scheduled; normal rate resumes. */
                            Rect diagnosticRect;
                            SetRect(&diagnosticRect, 20, 20, 340, 220);
                            InvalRect(&diagnosticRect);
                            break;
                        }
                    }

                    /* R3D7P3: Command-Shift-D cycles profiler pages.
                     * Do not intercept plain P: gameplay uses it for placement.
                     * Check modifiers as well as the character code so ordinary
                     * D and Shift-D are unaffected. */
                    if (event.what == keyDown &&
                        (event.modifiers & (cmdKey | shiftKey)) == (cmdKey | shiftKey) &&
                        ((event.message & charCodeMask) == 'd' ||
                         (event.message & charCodeMask) == 'D')) {
                        p2DisplayMode = (unsigned char)((p2DisplayMode + 1) % 9);
                        InvalRect(&window->portRect);
                        break;
                    }


                    /* R3D7S1: press G to test preloaded original GUNSHOT sound. */
                    if (event.what == keyDown &&
                        ((event.message & charCodeMask) == 'g' ||
                         (event.message & charCodeMask) == 'G')) {
                        DuneGameSound_PlayTest();
                        break;
                    }

                    /* R3D7S2: M toggles combat audio; S is not used. */
                    if (event.what == keyDown &&
                        ((event.message & charCodeMask) == 'm' ||
                         (event.message & charCodeMask) == 'M')) {
                        DuneGameSound_Toggle();
                        break;
                    }

                    /* R3D.6-A.2: V toggles shroud for visual comparison. */
                    if (event.what == keyDown &&
                        ((event.message & charCodeMask) == 'v' ||
                         (event.message & charCodeMask) == 'V')) {
                        r3d6ShroudEnabled = !r3d6ShroudEnabled;
                        g_cachedTerrainValid = false;
                        break;
                    }

                    /* Stage 15O-N.2: Mission objective status. */
                    if (event.what == keyDown &&
                        ((event.message & charCodeMask) == 'o' ||
                         (event.message & charCodeMask) == 'O')) {
                        Stage15ON1_ShowObjective(window);
                        break;
                    }

                    /* R3D8D-B1.3: reserve L for Light Vehicle Factory.
                     * Timer diagnostic snapshot moved to Shift+D. */
                    if (event.what == keyDown &&
                        ((event.message & charCodeMask) == 'D')) {
                        char report[256];
                        short refNum;
                        long length;
                        OSErr err;

                        sprintf(report,
                            "Stage 15O-L.2 Timer Diagnostics\\r"
                            "Cap hits: %lu\\r"
                            "Discarded Mac ticks: %lu\\r"
                            "Maximum elapsed: %lu\\r",
                            (unsigned long)g_macTimerCapHits,
                            (unsigned long)g_macTimerDiscardedTicks,
                            (unsigned long)g_macTimerMaximumElapsed);

                        err = Create("\pDuneTimerLog.txt", 0,
                                     '????', 'TEXT');

                        if (err == noErr || err == dupFNErr) {
                            err = FSOpen("\pDuneTimerLog.txt", 0,
                                         &refNum);
                            if (err == noErr) {
                                length = strlen(report);
                                err = SetFPos(refNum, fsFromStart, 0);
                                if (err == noErr)
                                    err = FSWrite(refNum, &length, report);
                                if (err == noErr)
                                    err = SetEOF(refNum, length);
                                FSClose(refNum);
                            }
                        }

                        if (err != noErr)
                            SysBeep(3);
                        break;
                    }

                    /* Stage 15O-K.2: toggle simulation scheduler. */
                    if (event.what == keyDown &&
                        ((event.message & charCodeMask) == 'k' ||
                         (event.message & charCodeMask) == 'K')) {
                        stage15OKUseCatchup = !stage15OKUseCatchup;
                        stage15OKLastSimulationTick = g_timerGame;
                        SysBeep(stage15OKUseCatchup ? 1 : 3);
                        break;
                    }

                    /* Stage 15O-J.1 diagnostic controls.
                     * T: suspend periodic battlefield composition and redraw.
                     * R: rebuild the current battlefield once for inspection.
                     * Game simulation continues unchanged in both modes.
                     */
                    if (event.what == keyDown &&
                        ((event.message & charCodeMask) == 't' ||
                         (event.message & charCodeMask) == 'T')) {
                        stage15OJFreezeRedraw = !stage15OJFreezeRedraw;
                        break;
                    }
                    if (event.what == keyDown &&
                        ((event.message & charCodeMask) == 'r' ||
                         (event.message & charCodeMask) == 'R')) {
                        if (g_duneMapWorldReady && CreateOriginalTerrainWorld()) {
                            Rect redrawRect;
                            SetRect(&redrawRect, 20, 20, 340, 300);
                            InvalRect(&redrawRect);
                        }
                        break;
                    }

                    /* Stage 15O-D: P enters placement for a completed Wind Trap. */
                    if (event.what == keyDown &&
                        ((event.message & charCodeMask) == 'p' ||
                         (event.message & charCodeMask) == 'P')) {
                        Structure *yard = Mac_SelectedStructure();
                        if (yard != NULL && g_playerHouse != NULL &&
                            yard->o.houseID == g_playerHouse->index &&
                            yard->o.type == STRUCTURE_CONSTRUCTION_YARD &&
                            yard->state == STRUCTURE_STATE_READY &&
                            yard->o.linkedID != 0xFF) {
                            g_placingStructure = true;
                            g_placementYardIndex = yard->o.index;
                            g_placementResult = 0;
                        }
                        {
                            Rect redrawRect;
                            SetRect(&redrawRect, 20, 220, 340, 300);
                            InvalRect(&redrawRect);
                        }
                        break;
                    }

                    /* C3: X cancels a busy player Construction Yard job,
                     * using OpenDUNE's original cancellation/refund routine. */
                    if (event.what == keyDown &&
                        ((event.message & charCodeMask) == 'x' ||
                         (event.message & charCodeMask) == 'X')) {
                        Structure *yard = Mac_SelectedStructure();
                        if (Mac_CancelYardProduction(yard)) {
                            Rect redrawRect;
                            SetRect(&redrawRect, 20, 20, 620, 400);
                            InvalRect(&redrawRect);
                        }
                        break;
                    }

                    /* D1: I trains a Soldier only when a player Barracks is selected. */
                    if (event.what == keyDown &&
                        ((event.message & charCodeMask) == 'i' ||
                         (event.message & charCodeMask) == 'I')) {
                        Structure *barracks = Mac_SelectedStructure();
                        if (barracks == NULL || g_playerHouse == NULL ||
                            barracks->o.houseID != g_playerHouse->index ||
                            barracks->o.type != STRUCTURE_BARRACKS)
                            g_d1SoldierKeyResult = 1;
                        else if (barracks->state != STRUCTURE_STATE_IDLE ||
                                 barracks->o.linkedID != 0xFF)
                            g_d1SoldierKeyResult = 2;
                        else if ((Structure_GetBuildable(barracks) & FLAG_UNIT_SOLDIER) == 0)
                            g_d1SoldierKeyResult = 3;
                        else
                            g_d1SoldierKeyResult =
                                Structure_BuildObject(barracks, UNIT_SOLDIER) ? 5 : 4;
                        {
                            Rect redrawRect;
                            SetRect(&redrawRect, 20, 220, 620, 390);
                            InvalRect(&redrawRect);
                        }
                        break;
                    }

                    /* R3D8D-C2: A builds Outpost/Radar using OpenDUNE tech rules.
                     * KEY: 1=no player yard, 2=busy, 3=tech locked,
                     * 4=engine rejected, 5=engine accepted. */
                    if (event.what == keyDown &&
                        ((event.message & charCodeMask) == 'a' ||
                         (event.message & charCodeMask) == 'A')) {
                        Structure *yard = Mac_SelectedStructure();
                        if (yard == NULL || g_playerHouse == NULL ||
                            yard->o.houseID != g_playerHouse->index ||
                            yard->o.type != STRUCTURE_CONSTRUCTION_YARD) {
                            g_c2OutpostKeyResult = 1;
                        } else if (yard->state != STRUCTURE_STATE_IDLE) {
                            g_c2OutpostKeyResult = 2;
                        } else if ((Structure_GetBuildable(yard) &
                                    (1UL << STRUCTURE_OUTPOST)) == 0) {
                            g_c2OutpostKeyResult = 3;
                        } else {
                            g_c2OutpostKeyResult =
                                Structure_BuildObject(yard, STRUCTURE_OUTPOST)
                                ? 5 : 4;
                        }
                        {
                            Rect redrawRect;
                            SetRect(&redrawRect, 20, 20, 620, 400);
                            InvalRect(&redrawRect);
                        }
                        break;
                    }

                    /* R3D8D-C1: Barracks construction through original tech rules.
                     * KEY: 1=no player yard, 2=busy, 3=tech locked,
                     * 4=engine rejected, 5=engine accepted. */
                    if (event.what == keyDown &&
                        ((event.message & charCodeMask) == 'b' ||
                         (event.message & charCodeMask) == 'B')) {
                        Structure *yard = Mac_SelectedStructure();
                        if (yard == NULL || g_playerHouse == NULL ||
                            yard->o.houseID != g_playerHouse->index ||
                            yard->o.type != STRUCTURE_CONSTRUCTION_YARD) {
                            g_c1BarracksKeyResult = 1;
                        } else if (yard->state != STRUCTURE_STATE_IDLE) {
                            g_c1BarracksKeyResult = 2;
                        } else if ((Structure_GetBuildable(yard) &
                                    (1UL << STRUCTURE_BARRACKS)) == 0) {
                            g_c1BarracksKeyResult = 3;
                        } else {
                            g_c1BarracksKeyResult =
                                Structure_BuildObject(yard, STRUCTURE_BARRACKS)
                                ? 5 : 4;
                        }
                        {
                            Rect redrawRect;
                            SetRect(&redrawRect, 20, 20, 620, 380);
                            InvalRect(&redrawRect);
                        }
                        break;
                    }

                    /* R3D8D-B1: Light Vehicle Factory, using original tech rules. */
                    if (event.what == keyDown &&
                        ((event.message & charCodeMask) == 'l' ||
                         (event.message & charCodeMask) == 'L')) {
                        Structure *yard = Mac_SelectedStructure();
                        /* KEY: 1=no player yard, 2=busy, 3=tech locked,
                         * 4=engine rejected, 5=engine accepted. */
                        if (yard == NULL || g_playerHouse == NULL ||
                            yard->o.houseID != g_playerHouse->index ||
                            yard->o.type != STRUCTURE_CONSTRUCTION_YARD) {
                            g_b12FactoryKeyResult = 1;
                        } else if (yard->state != STRUCTURE_STATE_IDLE) {
                            g_b12FactoryKeyResult = 2;
                        } else if ((Structure_GetBuildable(yard) &
                                    FLAG_STRUCTURE_LIGHT_VEHICLE) == 0) {
                            g_b12FactoryKeyResult = 3;
                        } else {
                            g_b12FactoryKeyResult =
                                Structure_BuildObject(yard, STRUCTURE_LIGHT_VEHICLE)
                                ? 5 : 4;
                        }
                        {
                            Rect redrawRect;
                            SetRect(&redrawRect, 20, 20, 340, 320);
                            InvalRect(&redrawRect);
                        }
                        break;
                    }

                    /* R4-G4B: K builds Spice Silo via original tech rules. */
                    if (event.what == keyDown &&
                        ((event.message & charCodeMask) == 'k' ||
                         (event.message & charCodeMask) == 'K')) {
                        Structure *yard = Mac_SelectedStructure();
                        if (yard != NULL && g_playerHouse != NULL &&
                            yard->o.houseID == g_playerHouse->index &&
                            yard->o.type == STRUCTURE_CONSTRUCTION_YARD &&
                            yard->state == STRUCTURE_STATE_IDLE &&
                            (Structure_GetBuildable(yard) &
                             (1UL << STRUCTURE_SILO)) != 0) {
                            (void)Structure_BuildObject(yard, STRUCTURE_SILO);
                        }
                        {
                            Rect redrawRect;
                            SetRect(&redrawRect, 20, 20, 620, 400);
                            InvalRect(&redrawRect);
                        }
                        break;
                    }

                    /* Stage 15O-O.1: Concrete and Refinery production. */
                    if (event.what == keyDown &&
                        ((event.message & charCodeMask) == 'c' ||
                         (event.message & charCodeMask) == 'C' ||
                         (event.message & charCodeMask) == 'f' ||
                         (event.message & charCodeMask) == 'F')) {
                        Structure *yard = Mac_SelectedStructure();
                        uint8 requestedType =
                            ((event.message & charCodeMask) == 'c' ||
                             (event.message & charCodeMask) == 'C')
                            ? STRUCTURE_SLAB_1x1 : STRUCTURE_REFINERY;

                        if (yard != NULL &&
                            g_playerHouse != NULL &&
                            yard->o.houseID == g_playerHouse->index &&
                            yard->o.type == STRUCTURE_CONSTRUCTION_YARD &&
                            yard->state == STRUCTURE_STATE_IDLE &&
                            (Structure_GetBuildable(yard) &
                             (1UL << requestedType)) != 0) {
                            Structure_BuildObject(yard, requestedType);
                        }

                        {
                            Rect redrawRect;
                            SetRect(&redrawRect, 20, 20, 340, 300);
                            InvalRect(&redrawRect);
                        }
                        break;
                    }

                    /* Stage 15O-C: Start original Wind Trap production. */
                    if (event.what == keyDown &&
                        ((event.message & charCodeMask) == 'w' ||
                         (event.message & charCodeMask) == 'W')) {
                        Structure *yard = Mac_SelectedStructure();

                        if (yard != NULL &&
                            g_playerHouse != NULL &&
                            yard->o.houseID == g_playerHouse->index &&
                            yard->o.type == STRUCTURE_CONSTRUCTION_YARD &&
                            yard->state == STRUCTURE_STATE_IDLE &&
                            (Structure_GetBuildable(yard) &
                             FLAG_STRUCTURE_WINDTRAP) != 0) {
                            Structure_BuildObject(
                                yard, STRUCTURE_WINDTRAP);
                        }

                        {
                            Rect redrawRect;
                            SetRect(&redrawRect, 20, 20, 340, 300);
                            InvalRect(&redrawRect);
                        }
                        break;
                    }

                    /* Macintosh arrow-key virtual key codes. */
                    switch (keyCode) {
                        case 0x7B: /* Left */
                            if (g_viewTileX > minX) {
                                g_viewTileX--;
                                moved = true;
                            }
                            break;

                        case 0x7C: /* Right */
                            if (g_viewTileX < maxX) {
                                g_viewTileX++;
                                moved = true;
                            }
                            break;

                        case 0x7E: /* Up */
                            if (g_viewTileY > minY) {
                                g_viewTileY--;
                                moved = true;
                            }
                            break;

                        case 0x7D: /* Down */
                            if (g_viewTileY < maxY) {
                                g_viewTileY++;
                                moved = true;
                            }
                            break;
                    }

                    /* R3B.1: arrow-key scrolling changes
                     * viewport coordinates only. The normal
                     * renderer handles the next screen update.
                     */
                    (void)moved;

                    break;
                }
            }
        }
    }

    DuneGameSound_Shutdown();

    if (g_duneMapWorld != NULL)
        DisposeGWorld(g_duneMapWorld);

    DisposeWindow(window);

    if (g_duneMacPalette != NULL)
        DisposePalette(g_duneMacPalette);

    if (g_duneColorTable != NULL)
        DisposeHandle((Handle)g_duneColorTable);
    free(g_duneMapPixels);
    g_duneMapPixels = NULL;
    return passed ? 0 : 1;
}
