/* trix_version.h — trix version information */

#ifndef TRIX_VERSION_H
#define TRIX_VERSION_H

#define TRIX_VERSION_MAJOR 1
#define TRIX_VERSION_MINOR 1
#define TRIX_VERSION_PATCH 2
#define TRIX_VERSION_STRING "1.1.2"

/* Single integer: 10000*major + 100*minor + patch */
#define TRIX_VERSION_NUMBER \
    (TRIX_VERSION_MAJOR * 10000 + TRIX_VERSION_MINOR * 100 + TRIX_VERSION_PATCH)

/* Compile-time compatibility check */
#define TRIX_VERSION_AT_LEAST(maj, min, patch) \
    (TRIX_VERSION_NUMBER >= ((maj)*10000 + (min)*100 + (patch)))

#endif /* TRIX_VERSION_H */
