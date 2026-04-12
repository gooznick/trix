/*
 * test_basic.c — smoke test for the C API.
 * Run with TRIX_BACKEND unset (nop) or a specific backend.
 */

#include <stdint.h>
#include <stdio.h>

#include <trix/trix.h>

int main(void) {
    printf("trix C API smoke test\n");

    trix_frame_begin(0);

    trix_algo_begin("encode");
    trix_data_int("width", 1920);
    trix_data_int("height", 1080);
    trix_data_float("fps", 29.97f);
    trix_data_string("codec", "h264");
    trix_algo_end("encode");

    trix_frame_end(0);

    trix_frame_begin(1);
    trix_algo_begin("decode");
    trix_algo_end("decode");
    trix_frame_end(1);

    /* Macro forms */
    TRIX_FRAME_BEGIN(2);
    TRIX_ALGO_BEGIN("render");
    TRIX_DATA_INT("triangles", 42000);
    TRIX_ALGO_END("render");
    TRIX_FRAME_END(2);

    printf("done\n");
    return 0;
}
