#pragma once
#include "math/math.h"
#include "cfg/config.h"
#include <stdalign.h>

//Helper function to insert a simple non SIMD operation
//Useful if there's no SIMD function that's faster than native

#define _NONE_OP_SELF_T(T, N, ...) {        \
                                            \
    T res = (T) { 0 };                      \
                                            \
    for (u8 i = 0; i < N; ++i)              \
        res.v[i] = (__VA_ARGS__);           \
                                            \
    return res;                             \
}

#define _NONE_OP4I(...) _NONE_OP_SELF_T(i32x4, 4, __VA_ARGS__)
#define _NONE_OP4F(...) _NONE_OP_SELF_T(f32x4, 4, __VA_ARGS__)
#define _NONE_OP2I(...) _NONE_OP_SELF_T(i32x2, 2, __VA_ARGS__)
#define _NONE_OP2F(...) _NONE_OP_SELF_T(f32x2, 2, __VA_ARGS__)

//

#if _SIMD == SIMD_NEON

    #error Compiling NEON isn't supported

    //TODO:

#elif _SIMD == SIMD_SSE

    #include <immintrin.h>
    #include <xmmintrin.h>
    #include <emmintrin.h>
    #include <smmintrin.h>

    //vec3 and vec4 can be represented using 4-element vectors,
    //These are a lot faster than just doing them manually.
    
    typedef __m128i i32x4;
    typedef __m128  f32x4;

    //vec2s are just regular memory to us, because XMM is legacy and needs a clear everytime vec ops are finished.
    //That's not very user friendly. 
    //Unpacking 2-element vectors to 4-element vectors, doing operation and packing them again is slower than normal.
    //This is because the latency of vector operations is bigger than 2x the normal operation. 
    //And 2x normal operation might be pipelined better.
    //The only exceptions are expensive operations like trig operations, rounding and sqrt & rsqrt, etc..

    typedef struct i32x2_t {
        alignas(8) i32 v[2];
    } i32x2;

    typedef struct f32x2_t {
        alignas(8) f32 v[2];
    } f32x2;

    //
    
    #define _shufflei(a, x, y, z, w) _mm_shuffle_epi32(a, _MM_SHUFFLE(w, z, y, x))
    #define _shufflef(a, x, y, z, w) _mm_shuffle_ps(a, a, _MM_SHUFFLE(w, z, y, x))

#else

    typedef struct i32x4_t {
        alignas(16) i32 v[4];
    } i32x4;
    
    typedef struct f32x4_t {
        alignas(16) f32 v[4];
    } f32x4;

    typedef struct i32x2_t {
        alignas(8) i32 v[2];
    } i32x2;
    
    typedef struct f32x2_t {
        alignas(8) f32 v[2];
    } f32x2;
    
    #define _shufflei(a, x, y, z, w) (i32x4){ { a.v[x], a.v[y], a.v[z], a.v[w] } }
    #define _shufflef(a, x, y, z, w) (f32x4){ { a.v[x], a.v[y], a.v[z], a.v[w] } }

#endif

#include "vec4i.h"
#include "vec4f.h"
#include "vec2i.h"
#include "vec2f.h"
    
inline i32x2 i32x2_fromI32x4(i32x4 a) { return i32x2_load2((const i32*) &a); }
inline f32x2 f32x2_fromF32x4(f32x4 a) { return f32x2_load2((const f32*) &a); }
inline i32x4 i32x4_fromI32x2(i32x2 a) { return i32x4_load2((const i32*) &a); }
inline f32x4 f32x4_fromF32x2(f32x2 a) { return f32x4_load2((const f32*) &a); }
    
//Platform/arch dependent

#if _SIMD == SIMD_NEON
    #include "arch/neon.h"
#elif _SIMD == SIMD_SSE
    #include "arch/sse.h"
#else
    #include "arch/none.h"
#endif
