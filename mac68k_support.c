/*
 * Stage 10D Macintosh compatibility support.
 *
 * Temporary configuration and error handlers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "types.h"

void Warning(const char *format, ...)
{
    (void)format;
}

void Error(const char *format, ...)
{
    (void)format;
}

/*
 * Temporary classic Macintosh mkdir compatibility.
 *
 * Stage 10D only needs an existing directory.
 * Actual directory creation will be implemented later.
 */
#include <sys/stat.h>
#include <errno.h>

int mkdir(const char *path, mode_t mode)
{
    struct stat st;

    (void)mode;

    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode))
            return 0;

        errno = EEXIST;
        return -1;
    }

    errno = ENOSYS;
    return -1;
}
