/*
 * trix.h — Unified tracing API
 *
 * Usage:
 *   - Include this header in all translation units.
 *   - If TRIX_ENABLED is not defined, all calls compile to no-ops with zero overhead.
 *   - If TRIX_ENABLED is defined, link against libtrix.so (Linux) or trix.dll (Windows).
 *   - At runtime, set TRIX_BACKEND=ftrace|perf|itt|etw to select a backend.
 */

#ifndef TRIX_H
#define TRIX_H

#include <stdint.h>

/* --- DLL import/export --- */
#if defined(_WIN32)
#  if defined(TRIX_BUILDING_DLL)
#    define TRIX_API __declspec(dllexport)
#  else
#    define TRIX_API __declspec(dllimport)
#  endif
#else
#  define TRIX_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef TRIX_ENABLED

/* --- Declarations (implementations in libtrix.so / trix.dll) --- */

TRIX_API void trix_frame_begin(uint64_t frame_num);
TRIX_API void trix_frame_end(uint64_t frame_num);
TRIX_API void trix_algo_begin(const char* name);
TRIX_API void trix_algo_end(const char* name);
TRIX_API void trix_data_int(const char* key, uint64_t value);
TRIX_API void trix_data_float(const char* key, float value);
TRIX_API void trix_data_string(const char* key, const char* value);

#define TRIX_FRAME_BEGIN(n)    trix_frame_begin(n)
#define TRIX_FRAME_END(n)      trix_frame_end(n)
#define TRIX_ALGO_BEGIN(name)  trix_algo_begin(name)
#define TRIX_ALGO_END(name)    trix_algo_end(name)
#define TRIX_DATA_INT(k, v)    trix_data_int(k, v)
#define TRIX_DATA_FLOAT(k, v)  trix_data_float(k, v)
#define TRIX_DATA_STRING(k, v) trix_data_string(k, v)

#else /* TRIX_ENABLED not defined — all no-ops */

static inline void trix_frame_begin(uint64_t frame_num)                   { (void)frame_num; }
static inline void trix_frame_end(uint64_t frame_num)                     { (void)frame_num; }
static inline void trix_algo_begin(const char* name)                      { (void)name; }
static inline void trix_algo_end(const char* name)                        { (void)name; }
static inline void trix_data_int(const char* key, uint64_t value)         { (void)key; (void)value; }
static inline void trix_data_float(const char* key, float value)          { (void)key; (void)value; }
static inline void trix_data_string(const char* key, const char* value)   { (void)key; (void)value; }

#define TRIX_FRAME_BEGIN(n)    ((void)(n))
#define TRIX_FRAME_END(n)      ((void)(n))
#define TRIX_ALGO_BEGIN(name)  ((void)(name))
#define TRIX_ALGO_END(name)    ((void)(name))
#define TRIX_DATA_INT(k, v)    ((void)(k), (void)(v))
#define TRIX_DATA_FLOAT(k, v)  ((void)(k), (void)(v))
#define TRIX_DATA_STRING(k, v) ((void)(k), (void)(v))

#endif /* TRIX_ENABLED */

#ifdef __cplusplus
} /* extern "C" */

/* --- C++ RAII wrappers --- */

namespace trix {

/*
 * Scoped<T, Begin, End> — calls Begin(arg) on construction, End(arg) on destruction.
 *
 * Examples:
 *   trix::ScopedAlgo  scope("sort");  // calls trix_algo_begin/end
 *   trix::ScopedFrame scope(42);      // calls trix_frame_begin/end
 */
template<typename T, void(*Begin)(T), void(*End)(T)>
class Scoped {
public:
    explicit Scoped(T arg) : arg_(arg) { Begin(arg_); }
    ~Scoped() { End(arg_); }
    Scoped(const Scoped&) = delete;
    Scoped& operator=(const Scoped&) = delete;
private:
    T arg_;
};

using ScopedAlgo  = Scoped<const char*, trix_algo_begin,  trix_algo_end>;
using ScopedFrame = Scoped<uint64_t,    trix_frame_begin, trix_frame_end>;

} /* namespace trix */

#ifdef TRIX_ENABLED
#define TRIX_ALGO_SCOPE(name)  trix::ScopedAlgo  _trix_algo_scope_##__LINE__(name)
#define TRIX_FRAME_SCOPE(n)    trix::ScopedFrame _trix_frame_scope_##__LINE__(n)
#else
#define TRIX_ALGO_SCOPE(name)  ((void)(name))
#define TRIX_FRAME_SCOPE(n)    ((void)(n))
#endif

#endif /* __cplusplus */

#endif /* TRIX_H */
