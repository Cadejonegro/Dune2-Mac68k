#ifndef MAC68K_COMPAT_H
#define MAC68K_COMPAT_H

#include <stddef.h>

/*
 * Classic Macintosh / Retro68 compatibility.
 * ASCII case-insensitive filename comparison.
 */
static int mac68k_strcasecmp(const char *a, const char *b)
{
    unsigned char ca, cb;

    while (*a && *b) {
        ca = (unsigned char)*a++;
        cb = (unsigned char)*b++;

        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;

        if (ca != cb) return (int)ca - (int)cb;
    }

    ca = (unsigned char)*a;
    cb = (unsigned char)*b;

    if (ca >= 'A' && ca <= 'Z') ca += 32;
    if (cb >= 'A' && cb <= 'Z') cb += 32;

    return (int)ca - (int)cb;
}

#define strcasecmp mac68k_strcasecmp

#endif

/* Classic Macintosh compatibility for case-insensitive comparisons. */
#include <stddef.h>
#include <ctype.h>

static inline int mac68k_strncasecmp(const char *a, const char *b, size_t n)
{
    while (n-- != 0) {
        unsigned char ca = (unsigned char)*a++;
        unsigned char cb = (unsigned char)*b++;
        int da = tolower(ca);
        int db = tolower(cb);

        if (da != db) return da - db;
        if (ca == '\0') return 0;
    }
    return 0;
}

#define strncasecmp mac68k_strncasecmp
