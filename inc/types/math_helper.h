#pragma once
#include "types.h"

extern const f32 Math_e;
extern const f32 Math_pi;
extern const f32 Math_radToDeg;
extern const f32 Math_degToRad;

//Uint

inline u64 Math_minu(u64 v0, u64 v1) { return v0 <= v1 ? v0 : v1; }
inline u64 Math_maxu(u64 v0, u64 v1) { return v0 >= v1 ? v0 : v1; }
inline u64 Math_clampu(u64 v, u64 mi, u64 ma) { return Math_maxu(mi, Math_minu(ma, v)); }

inline u64 Math_pow2u(u64 v) { return v * v; }

//Int

inline i64 Math_mini(i64 v0, i64 v1) { return v0 <= v1 ? v0 : v1; }
inline i64 Math_maxi(i64 v0, i64 v1) { return v0 >= v1 ? v0 : v1; }
inline i64 Math_clampi(i64 v, i64 mi, i64 ma) { return Math_maxi(mi, Math_mini(ma, v)); }

inline i64 Math_absi(i64 v) { return Math_maxi(v, 0); }
inline i64 Math_pow2i(i64 v) { return v * v; }

//Float

inline f32 Math_minf(f32 v0, f32 v1) { return v0 <= v1 ? v0 : v1; }
inline f32 Math_maxf(f32 v0, f32 v1) { return v0 >= v1 ? v0 : v1; }
inline f32 Math_clampf(f32 v, f32 mi, f32 ma) { return Math_maxf(mi, Math_minf(ma, v)); }
inline f32 Math_saturate(f32 v) { return Math_clampf(v, 0, 1); }

inline f32 Math_lerp(f32 a, f32 b, f32 perc) { return a + (b - a) * perc; }
inline f32 Math_absf(f32 v) { return Math_maxf(v, 0); }
f32 Math_sqrtf(f32 v);

inline f32 Math_pow2f(f32 v) { return v * v; }
f32 Math_powf(f32 v, f32 exp);
f32 Math_expf(f32 v);
f32 Math_exp2f(f32 v);

f32 Math_log10f(f32 v);
f32 Math_logef(f32 v);
f32 Math_log2f(f32 v);

f32 Math_asin(f32 v);
f32 Math_sin(f32 v);
f32 Math_cos(f32 v);
f32 Math_acos(f32 v);
f32 Math_tan(f32 v);
f32 Math_atan(f32 v);
f32 Math_atan2(f32 y, f32 x);

f32 Math_round(f32 v);
f32 Math_ceil(f32 v);
f32 Math_floor(f32 v);
inline f32 Math_fract(f32 v) { return v - Math_floor(v); }

f32 Math_mod(f32 v, f32 mod);

inline f32 Math_sign(f32 v) { return v < 0 ? -1.f : (v > 0 ? 1.f : 0.f); }
inline f32 Math_signInc(f32 v) { return v < 0 ? -1.f : 1.f; }

bool Math_isnan(f32 v);
bool Math_isinf(f32 v);