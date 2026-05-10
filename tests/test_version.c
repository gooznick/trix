/*
 * test_version.c — verify version macros and trix_version() runtime function.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <trix/trix.h>

int main(void) {
    /* Compile-time macro checks */
    assert(TRIX_VERSION_MAJOR == 1);
    assert(TRIX_VERSION_MINOR == 0);
    assert(TRIX_VERSION_PATCH == 0);
    assert(strcmp(TRIX_VERSION_STRING, "1.0.0") == 0);
    assert(TRIX_VERSION_NUMBER == 10000);
    assert(TRIX_VERSION_AT_LEAST(1, 0, 0));
    assert(!TRIX_VERSION_AT_LEAST(2, 0, 0));

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
