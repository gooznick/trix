/*
 * trix_sdt.h — minimal SDT (Statically Defined Tracing) probe implementation.
 *
 * Generates SystemTap-compatible probes with zero external dependencies.
 * Probes compile to a NOP instruction. perf/SystemTap patch it at runtime.
 *
 * To use with perf:
 *   perf buildid-cache --add /path/to/libtrix.so   # register the library
 *   perf record -e 'sdt_trix:algo_begin' ./myapp
 *   perf script
 *
 * Compatible with: x86_64, i386, aarch64.
 */

#ifndef TRIX_SDT_H
#define TRIX_SDT_H

#if defined(__x86_64__) || defined(__aarch64__)
#  define _TRIX_SDT_ADDR ".8byte"
#else
#  define _TRIX_SDT_ADDR ".4byte"
#endif

/* Emits the .stapsdt.base anchor section required by the SDT ELF note format. */
#define _TRIX_SDT_BASE                                                          \
    ".ifndef _.stapsdt.base\n\t"                                                \
    ".pushsection .stapsdt.base,\"aG\",\"progbits\",.stapsdt.base,comdat\n\t"  \
    "_.stapsdt.base: .space 1\n\t"                                              \
    ".size _.stapsdt.base, 1\n\t"                                               \
    ".popsection\n\t"                                                           \
    ".endif\n\t"

/*
 * _TRIX_SDT_PROBE(provider, name, arg_descriptor, constraints, ...)
 *
 * arg_descriptor: SDT argument string, e.g. "8@%rdi" or "" for no args.
 * constraints/...: asm input operands for the arguments.
 */
#define _TRIX_SDT_PROBE(provider, name, arg_descriptor, ...)                   \
    asm volatile(                                                               \
        "990: nop\n\t"                                                          \
        ".pushsection .note.stapsdt,\"?\",\"note\"\n\t"                        \
        "  .balign 4\n\t"                                                       \
        "  .4byte 992f-991f, 994f-993f, 3\n\t"                                 \
        "991: .asciz \"stapsdt\"\n\t"                                           \
        "992: .balign 4\n\t"                                                    \
        "993: " _TRIX_SDT_ADDR " 990b\n\t"                                     \
        "     " _TRIX_SDT_ADDR " _.stapsdt.base\n\t"                           \
        "     " _TRIX_SDT_ADDR " 0\n\t"                                        \
        "  .asciz \"" #provider "\"\n\t"                                        \
        "  .asciz \"" #name "\"\n\t"                                            \
        "  .asciz \"" arg_descriptor "\"\n\t"                                   \
        "994: .balign 4\n\t"                                                    \
        ".popsection\n\t"                                                       \
        _TRIX_SDT_BASE                                                          \
        : : __VA_ARGS__)

/* Convenience macros for 0–2 arguments */
#define TRIX_PROBE0(provider, name) \
    _TRIX_SDT_PROBE(provider, name, "")

#define TRIX_PROBE1(provider, name, arg1) \
    _TRIX_SDT_PROBE(provider, name, "-8@%0", "nor" ((long)(arg1)))

#define TRIX_PROBE2(provider, name, arg1, arg2) \
    _TRIX_SDT_PROBE(provider, name, "-8@%0 -8@%1", \
        "nor" ((long)(arg1)), "nor" ((long)(arg2)))

#endif /* TRIX_SDT_H */
