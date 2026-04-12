/*
 * test_cpp.cpp — smoke test for the C++ RAII wrappers.
 */

#include <cstdint>
#include <cstdio>

#include <trix/trix.h>

static void process_frame(uint64_t frame_num) {
    TRIX_FRAME_SCOPE(frame_num);

    {
        TRIX_ALGO_SCOPE("decode");
        trix_data_string("format", "yuv420p");
    } /* trix_algo_end("decode") called here */

    {
        trix::ScopedAlgo render("render");
        trix_data_int("triangles", 12000);
    } /* trix_algo_end("render") called here */
}

int main(void) {
    printf("trix C++ RAII test\n");

    for (uint64_t i = 0; i < 3; i++) {
        process_frame(i);
    }

    /* Direct template instantiation */
    {
        trix::Scoped<const char*, trix_algo_begin, trix_algo_end> scope("custom");
        trix_data_float("score", 0.95f);
    }

    printf("done\n");
    return 0;
}
