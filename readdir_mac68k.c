/*
 * Stage 10G.66: Macintosh 68k file discovery adapter.
 *
 * Register the original Dune II archive and the first
 * Atreides mission from the application's working directory.
 *
 * Original OpenDUNE file management remains unchanged.
 */

#include <stdio.h>
#include "types.h"
#include "../OpenDUNE/src/os/readdir.h"

bool ReadDir_ProcessAllFiles(
    const char *dirpath,
    bool (*callback)(
        const char *name,
        const char *path,
        uint32 size
    )
)
{
    static const char *files[] = {
        "DUNE.PAK",
        "SCENA001.INI",
        "SCENA002.INI",
        "ICON.ICN",
        "ICON.MAP",
        "UNIT.EMC",
        "TEAM.EMC",
        "BUILD.EMC"
    };

    unsigned int i;
    bool foundArchive = false;

    (void)dirpath;

    for (i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
        FILE *fp;
        long size;

        fp = fopen(files[i], "rb");

        if (fp == NULL)
            continue;

        if (fseek(fp, 0, SEEK_END) != 0) {
            fclose(fp);
            continue;
        }

        size = ftell(fp);
        fclose(fp);

        if (size <= 0)
            continue;

        if (!callback(files[i], files[i], (uint32)size))
            return false;

        if (i == 0)
            foundArchive = true;
    }

    return foundArchive;
}
