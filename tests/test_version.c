/*
 * test_version.c — verify version macros and trix_version() runtime function.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <trix/trix.h>

int main(void) {
    /* Compile-time macro checks — version-agnostic */
    assert(TRIX_VERSION_MAJOR >= 0);
    assert(TRIX_VERSION_MINOR >= 0);
    assert(TRIX_VERSION_PATCH >= 0);
    /* TRIX_VERSION_STRING must contain exactly two dots (X.Y.Z format) */
    const char* s = TRIX_VERSION_STRING;
    int dots = 0;
    for (; *s; s++) if (*s == '.') dots++;
    assert(dots == 2);
    /* Numeric macro must be consistent with the three components */
    assert(TRIX_VERSION_NUMBER ==
           TRIX_VERSION_MAJOR * 10000 + TRIX_VERSION_MINOR * 100 + TRIX_VERSION_PATCH);
    assert(TRIX_VERSION_AT_LEAST(TRIX_VERSION_MAJOR, TRIX_VERSION_MINOR, TRIX_VERSION_PATCH));
    assert(!TRIX_VERSION_AT_LEAST(TRIX_VERSION_MAJOR + 1, 0, 0));

    /* Runtime function */
    const char* ver = trix_version();
    assert(ver != NULL);
    assert(ver[0] != '\0');
    assert(strcmp(ver, TRIX_VERSION_STRING) == 0);

    printf("trix version: %s (major=%d minor=%d patch=%d number=%d)\n",
           ver, TRIX_VERSION_MAJOR, TRIX_VERSION_MINOR, TRIX_VERSION_PATCH,
           TRIX_VERSION_NUMBER);
    return 0;
}
