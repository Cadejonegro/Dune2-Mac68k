/*
 * Stage 10H.4 - Original OpenDUNE engine initialization.
 *
 * Uses original OpenDUNE pool initialization functions.
 * No custom gameplay implementation.
 */

#include "types.h"
#include "../../../OpenDUNE/src/pool/unit.h"
#include "../../../OpenDUNE/src/pool/structure.h"
#include "../../../OpenDUNE/src/pool/house.h"
#include "../../../OpenDUNE/src/pool/team.h"

int MacMission_InitializePools(void)
{
    Unit_Init();
    Structure_Init();
    House_Init();
    Team_Init();

    return 0;
}
