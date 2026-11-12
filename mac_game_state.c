/*
 * Stage 10G: Macintosh-specific OpenDUNE game state.
 * Definitions and initial values follow original OpenDUNE.
 * This module must not be linked together with the original
 * modules that define the same globals.
 */

#include "../../OpenDUNE/include/types.h"
#include "../../OpenDUNE/src/opendune.h"
#include "../../OpenDUNE/src/gui/gui.h"
#include "../../OpenDUNE/src/input/mouse.h"

bool g_dune2_enhanced = true;
bool g_unpackSHPonLoad = true;

uint16 g_campaignID = 0;
uint16 g_scenarioID = 1;
uint16 g_activeAction = 0xFFFF;
uint32 g_tickScenarioStart = 0;

bool g_debugGame = false;
bool g_debugScenario = false;

void *g_readBuffer = NULL;
uint32 g_readBufferSize = 0;

uint16 g_validateStrictIfZero = 0;
uint16 g_selectionType = 0;
bool g_viewport_forceRedraw = false;
int16 g_musicInBattle = 0;

