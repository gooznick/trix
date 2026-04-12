/*
 * test_nop.c — verifies the header compiles to pure no-ops with no link dependency.
 * TRIX_ENABLED is intentionally NOT defined.
 */

#include <stdint.h>
#include <stdio.h>

#include <trix/trix.h>

int main(void) {
    printf("trix nop test (no library linked)\n");

    trix_frame_begin(0);
    trix_algo_begin("test");
    trix_data_int("x", 42);
    trix_data_float("y", 1.5f);
    trix_data_string("z", "hello");
    trix_algo_end("test");
    trix_frame_end(0);

    TRIX_FRAME_BEGIN(1);
    TRIX_ALGO_BEGIN("macro_test");
    TRIX_DATA_INT("a", 1);
    TRIX_ALGO_END("macro_test");
    TRIX_FRAME_END(1);

    printf("done\n");
    return 0;
}
