#include <stdio.h>

void __aeabi_assert (const char * expr, const char * file, int line)
    {
    printf ("*** assertion failed: %s, file %s, line %d\n", expr, file, line);
    while (1);
    }

