#pragma once

//Cast

inline f32x4 f32x4_fromI32x4(i32x4 a) { return (f32x4) { .v = { (f32) i32x4_x(a), (f32) i32x4_y(a), (f32) i32x4_z(a), (f32) i32x4_w(a) } }; }
inline f32x2 f32x2_fromI32x2(i32x2 a) { return (f32x2) { .v = { (f32) i32x2_x(a), (f32) i32x2_y(a) } }; }
inline i32x4 i32x4_fromF32x4(f32x4 a) { return (i32x4) { .v = { (i32) f32x4_x(a), (i32) f32x4_y(a), (i32) f32x4_z(a), (i32) f32x4_w(a) } }; }
inline i32x2 i32x2_fromF32x2(f32x2 a) { return (i32x2) { .v = { (i32) f32x2_x(a), (i32) f32x2_y(a) } }; }

//Arithmetic

inline i32x4 i32x4_add(i32x4 a, i32x4 b) _NONE_OP4I(a.v[i] + b.v[i])
inline i32x2 i32x2_add(i32x2 a, i32x2 b) _NONE_OP2I(a.v[i] + b.v[i])
inline f32x4 f32x4_add(f32x4 a, f32x4 b) _NONE_OP4F(a.v[i] + b.v[i])
inline f32x2 f32x2_add(f32x2 a, f32x2 b) _NONE_OP2F(a.v[i] + b.v[i])

inline i32x4 i32x4_sub(i32x4 a, i32x4 b) _NONE_OP4I(a.v[i] - b.v[i])
inline i32x2 i32x2_sub(i32x2 a, i32x2 b) _NONE_OP2I(a.v[i] - b.v[i])
inline f32x4 f32x4_sub(f32x4 a, f32x4 b) _NONE_OP4F(a.v[i] - b.v[i])
inline f32x2 f32x2_sub(f32x2 a, f32x2 b) _NONE_OP2F(a.v[i] - b.v[i])

inline i32x4 i32x4_mul(i32x4 a, i32x4 b) _NONE_OP4I(a.v[i] * b.v[i])
inline i32x2 i32x2_mul(i32x2 a, i32x2 b) _NONE_OP2I(a.v[i] * b.v[i])
inline f32x4 f32x4_mul(f32x4 a, f32x4 b) _NONE_OP4F(a.v[i] * b.v[i])
inline f32x2 f32x2_mul(f32x2 a, f32x2 b) _NONE_OP2F(a.v[i] * b.v[i])

inline i32x4 i32x4_div(i32x4 a, i32x4 b) _NONE_OP4I(a.v[i] / b.v[i])
inline i32x2 i32x2_div(i32x2 a, i32x2 b) _NONE_OP2I(a.v[i] / b.v[i])
inline f32x4 f32x4_div(f32x4 a, f32x4 b) _NONE_OP4F(a.v[i] / b.v[i])
inline f32x2 f32x2_div(f32x2 a, f32x2 b) _NONE_OP2F(a.v[i] / b.v[i])

//Swizzle

inline i32x4 i32x4_trunc2(i32x4 a) { return i32x4_init2(i32x4_x(a), i32x4_y(a)); }
inline f32x4 f32x4_trunc2(f32x4 a) { return f32x4_init2(f32x4_x(a), f32x4_y(a)); }

inline i32x4 i32x4_trunc3(i32x4 a) { return i32x4_init3(i32x4_x(a), i32x4_y(a), i32x4_z(a)); }
inline f32x4 f32x4_trunc3(f32x4 a) { return f32x4_init3(f32x4_x(a), f32x4_y(a), f32x4_z(a)); }

inline i32 i32x4_x(i32x4 a) { return a.v[0]; }
inline i32 i32x2_x(i32x2 a) { return a.v[0]; }
inline f32 f32x4_x(f32x4 a) { return a.v[0]; }
inline f32 f32x2_x(f32x2 a) { return a.v[0]; }

inline i32 i32x4_y(i32x4 a) { return a.v[1]; }
inline i32 i32x2_y(i32x2 a) { return a.v[1]; }
inline f32 f32x4_y(f32x4 a) { return a.v[1]; }
inline f32 f32x2_y(f32x2 a) { return a.v[1]; }

inline i32 i32x4_z(i32x4 a) { return a.v[2]; }
inline f32 f32x4_z(f32x4 a) { return a.v[2]; }

inline i32 i32x4_w(i32x4 a) { return a.v[3]; }
inline f32 f32x4_w(f32x4 a) { return a.v[3]; }

inline i32x4 i32x4_init4(i32 x, i32 y, i32 z, i32 w) { return (i32x4) { .v = { x, y, z, w } }; }
inline f32x4 f32x4_init4(f32 x, f32 y, f32 z, f32 w) { return (f32x4) { .v = { x, y, z, w } }; }

inline i32x4 i32x4_init3(i32 x, i32 y, i32 z) { return i32x4_init4(x, y, z, 0); }
inline f32x4 f32x4_init3(f32 x, f32 y, f32 z) { return f32x4_init4(x, y, z, 0); }

inline i32x4 i32x4_init2(i32 x, i32 y) { return i32x4_init4(x, y, 0, 0); }
inline f32x4 f32x4_init2(f32 x, f32 y) { return f32x4_init4(x, y, 0, 0); }
inline i32x2 i32x2_init2(i32 x, i32 y) { return (i32x2) { .v = { x, y } }; }
inline f32x2 f32x2_init2(f32 x, f32 y) { return (f32x2) { .v = { x, y } }; }

inline i32x4 i32x4_init1(i32 x) { return i32x4_init4(x, 0, 0, 0); }
inline f32x4 f32x4_init1(f32 x) { return f32x4_init4(x, 0, 0, 0); }
inline i32x2 i32x2_init1(i32 x) { return i32x2_init2(x, 0); }
inline f32x2 f32x2_init1(f32 x) { return f32x2_init2(x, 0); }

inline i32x4 i32x4_xxxx4(i32 x) { return i32x4_init4(x, x, x, x); }
inline f32x4 f32x4_xxxx4(f32 x) { return f32x4_init4(x, x, x, x); }
inline i32x2 i32x2_xx2(i32 x) { return i32x2_init2(x, x); }
inline f32x2 f32x2_xx2(f32 x) { return f32x2_init2(x, x); }

inline i32x4 i32x4_zero() { return i32x4_xxxx4(0); }
inline i32x2 i32x2_zero() { return i32x2_xx2(0); }
inline f32x4 f32x4_zero() { return f32x4_xxxx4(0); }
inline f32x2 f32x2_zero() { return f32x2_xx2(0); }

//Comparison

inline i32x4 i32x4_eq(i32x4 a, i32x4 b)  _NONE_OP4I(a.v[i] == b.v[i])
inline i32x2 i32x2_eq(i32x2 a, i32x2 b)  _NONE_OP2I(a.v[i] == b.v[i])
inline f32x4 f32x4_eq(f32x4 a, f32x4 b)  _NONE_OP4F((f32)(a.v[i] == b.v[i]))
inline f32x2 f32x2_eq(f32x2 a, f32x2 b)  _NONE_OP2F((f32)(a.v[i] == b.v[i]))

inline i32x4 i32x4_neq(i32x4 a, i32x4 b) _NONE_OP4I(a.v[i] != b.v[i])
inline i32x2 i32x2_neq(i32x2 a, i32x2 b) _NONE_OP2I(a.v[i] != b.v[i])
inline f32x4 f32x4_neq(f32x4 a, f32x4 b)  _NONE_OP4F((f32)(a.v[i] != b.v[i]))
inline f32x2 f32x2_neq(f32x2 a, f32x2 b)  _NONE_OP2F((f32)(a.v[i] != b.v[i]))

inline i32x4 i32x4_geq(i32x4 a, i32x4 b) _NONE_OP4I(a.v[i] >= b.v[i])
inline i32x2 i32x2_geq(i32x2 a, i32x2 b) _NONE_OP2I(a.v[i] >= b.v[i])
inline f32x4 f32x4_geq(f32x4 a, f32x4 b)  _NONE_OP4F((f32)(a.v[i] >= b.v[i]))
inline f32x2 f32x2_geq(f32x2 a, f32x2 b)  _NONE_OP2F((f32)(a.v[i] >= b.v[i]))

inline i32x4 i32x4_gt(i32x4 a, i32x4 b)  _NONE_OP4I(a.v[i] > b.v[i])
inline i32x2 i32x2_gt(i32x2 a, i32x2 b)  _NONE_OP2I(a.v[i] > b.v[i])
inline f32x4 f32x4_gt(f32x4 a, f32x4 b)  _NONE_OP4F((f32)(a.v[i] > b.v[i]))
inline f32x2 f32x2_gt(f32x2 a, f32x2 b)  _NONE_OP2F((f32)(a.v[i] > b.v[i]))

inline i32x4 i32x4_leq(i32x4 a, i32x4 b) _NONE_OP4I(a.v[i] <= b.v[i])
inline i32x2 i32x2_leq(i32x2 a, i32x2 b) _NONE_OP2I(a.v[i] <= b.v[i])
inline f32x4 f32x4_leq(f32x4 a, f32x4 b)  _NONE_OP4F((f32)(a.v[i] <= b.v[i]))
inline f32x2 f32x2_leq(f32x2 a, f32x2 b)  _NONE_OP2F((f32)(a.v[i] <= b.v[i]))

inline i32x4 i32x4_lt(i32x4 a, i32x4 b)  _NONE_OP4I(a.v[i] < b.v[i])
inline i32x2 i32x2_lt(i32x2 a, i32x2 b)  _NONE_OP2I(a.v[i] < b.v[i])
inline f32x4 f32x4_lt(f32x4 a, f32x4 b)  _NONE_OP4F((f32)(a.v[i] < b.v[i]))
inline f32x2 f32x2_lt(f32x2 a, f32x2 b)  _NONE_OP2F((f32)(a.v[i] < b.v[i]))

//Bitwise

inline i32x4 i32x4_or(i32x4 a, i32x4 b)  _NONE_OP4I(a.v[i] | b.v[i])
inline i32x2 i32x2_or(i32x2 a, i32x2 b)  _NONE_OP2I(a.v[i] | b.v[i])

inline i32x4 i32x4_and(i32x4 a, i32x4 b) _NONE_OP4I(a.v[i] & b.v[i])
inline i32x2 i32x2_and(i32x2 a, i32x2 b) _NONE_OP2I(a.v[i] & b.v[i])

inline i32x4 i32x4_xor(i32x4 a, i32x4 b) _NONE_OP4I(a.v[i] ^ b.v[i])
inline i32x2 i32x2_xor(i32x2 a, i32x2 b) _NONE_OP2I(a.v[i] ^ b.v[i])

//Min/max

inline i32x4 i32x4_min(i32x4 a, i32x4 b) _NONE_OP4I((i32) Math_mini(a.v[i], b.v[i]))
inline i32x2 i32x2_min(i32x2 a, i32x2 b) _NONE_OP2I((i32) Math_mini(a.v[i], b.v[i]))
inline f32x4 f32x4_min(f32x4 a, f32x4 b) _NONE_OP4F(Math_minf(a.v[i], b.v[i]))
inline f32x2 f32x2_min(f32x2 a, f32x2 b) _NONE_OP2F(Math_minf(a.v[i], b.v[i]))

inline i32x4 i32x4_max(i32x4 a, i32x4 b) _NONE_OP4I((i32) Math_maxi(a.v[i], b.v[i]))
inline i32x2 i32x2_max(i32x2 a, i32x2 b) _NONE_OP2I((i32) Math_maxi(a.v[i], b.v[i]))
inline f32x4 f32x4_max(f32x4 a, f32x4 b) _NONE_OP4F(Math_maxf(a.v[i], b.v[i]))
inline f32x2 f32x2_max(f32x2 a, f32x2 b) _NONE_OP2F(Math_maxf(a.v[i], b.v[i]))

//Reduce

inline i32 i32x4_reduce(i32x4 a) { return i32x4_x(a) + i32x4_y(a) + i32x4_z(a) + i32x4_w(a); }
inline f32 f32x4_reduce(f32x4 a) { return f32x4_x(a) + f32x4_y(a) + f32x4_z(a) + f32x4_w(a); }

inline i32 i32x2_reduce(i32x2 a) { return i32x2_x(a) + i32x2_y(a); }
inline f32 f32x2_reduce(f32x2 a) { return f32x2_x(a) + f32x2_y(a); }

//Simple swizzle

inline i32x2 i32x2_xx(i32x2 a) { return i32x2_xx2(i32x2_x(a)); }
inline f32x2 f32x2_xx(f32x2 a) { return f32x2_xx2(f32x2_x(a)); }

inline i32x2 i32x2_xy(i32x2 a) { return i32x2_init2(i32x2_y(a), i32x2_x(a)); }
inline f32x2 f32x2_xy(f32x2 a) { return f32x2_init2(f32x2_y(a), f32x2_x(a)); }

inline f32x2 f32x2_yy(f32x2 a) { return f32x2_xx2(f32x2_y(a)); }
inline i32x2 i32x2_yy(i32x2 a) { return i32x2_xx2(i32x2_y(a)); }

//Float arithmetic

inline f32x4 f32x4_ceil(f32x4 a) _NONE_OP4F(Math_ceil(a.v[i]))
inline f32x2 f32x2_ceil(f32x2 a)  _NONE_OP2F(Math_ceil(a.v[i]))

inline f32x4 f32x4_floor(f32x4 a) _NONE_OP4F(Math_floor(a.v[i]))
inline f32x2 f32x2_floor(f32x2 a)  _NONE_OP2F(Math_floor(a.v[i]))

inline f32x4 f32x4_round(f32x4 a) _NONE_OP4F(Math_round(a.v[i]))
inline f32x2 f32x2_round(f32x2 a) _NONE_OP2F(Math_round(a.v[i]))

inline f32x4 f32x4_sqrt(f32x4 a) _NONE_OP4F(Math_sqrtf(a.v[i]))
inline f32x2 f32x2_sqrt(f32x2 a) _NONE_OP2F(Math_sqrtf(a.v[i]))

inline f32x4 f32x4_rsqrt(f32x4 a) _NONE_OP4F(1 / Math_sqrtf(a.v[i]))
inline f32x2 f32x2_rsqrt(f32x2 a) _NONE_OP2F(1 / Math_sqrtf(a.v[i]))

inline f32x4 f32x4_acos(f32x4 a) _NONE_OP4F(Math_acos(a.v[i]))
inline f32x2 f32x2_acos(f32x2 a) _NONE_OP2F(Math_acos(a.v[i]))

inline f32x4 f32x4_cos(f32x4 a) _NONE_OP4F(Math_cos(a.v[i]))
inline f32x2 f32x2_cos(f32x2 a) _NONE_OP2F(Math_cos(a.v[i]))

inline f32x4 f32x4_asin(f32x4 a) _NONE_OP4F(Math_asin(a.v[i]))
inline f32x2 f32x2_asin(f32x2 a) _NONE_OP2F(Math_asin(a.v[i]))

inline f32x4 f32x4_sin(f32x4 a) _NONE_OP4F(Math_sin(a.v[i]))
inline f32x2 f32x2_sin(f32x2 a) _NONE_OP2F(Math_sin(a.v[i]))

inline f32x4 f32x4_atan(f32x4 a) _NONE_OP4F(Math_atan(a.v[i]))
inline f32x2 f32x2_atan(f32x2 a) _NONE_OP2F(Math_atan(a.v[i]))

inline f32x4 f32x4_atan2(f32x4 a, f32x4 x) _NONE_OP4F(Math_atan2(a.v[i], x.v[i]))
inline f32x2 f32x2_atan2(f32x2 a, f32x2 x) _NONE_OP2F(Math_atan2(a.v[i], x.v[i]))

inline f32x4 f32x4_tan(f32x4 a)  _NONE_OP4F(Math_tan(a.v[i]))
inline f32x2 f32x2_tan(f32x2 a)  _NONE_OP2F(Math_tan(a.v[i]))

inline f32x4 f32x4_loge(f32x4 a)  _NONE_OP4F(Math_logef(a.v[i]))
inline f32x2 f32x2_loge(f32x2 a)  _NONE_OP2F(Math_logef(a.v[i]))

inline f32x4 f32x4_log10(f32x4 a)  _NONE_OP4F(Math_log10f(a.v[i]))
inline f32x2 f32x2_log10(f32x2 a)  _NONE_OP2F(Math_log10f(a.v[i]))

inline f32x4 f32x4_log2(f32x4 a)  _NONE_OP4F(Math_log2f(a.v[i]))
inline f32x2 f32x2_log2(f32x2 a)  _NONE_OP2F(Math_log2f(a.v[i]))

inline f32x4 f32x4_exp(f32x4 a) _NONE_OP4F(Math_expf(a.v[i]))
inline f32x2 f32x2_exp(f32x2 a) _NONE_OP2F(Math_expf(a.v[i]))

inline f32x4 f32x4_exp10(f32x4 a) _NONE_OP4F(Math_exp10f(a.v[i]))
inline f32x2 f32x2_exp10(f32x2 a) _NONE_OP2F(Math_exp10f(a.v[i]))

inline f32x2 f32x2_exp2(f32x2 a) _NONE_OP2F(Math_exp2f(a.v[i]))

//Dot products

inline f32 f32x4_dot2(f32x4 a, f32x4 b) { return f32x4_x(a) * f32x4_x(b) + f32x4_y(a) * f32x4_y(b); }
inline f32 f32x4_dot3(f32x4 a, f32x4 b) { return f32x4_dot2(a, b) + f32x4_z(a) * f32x4_z(b); }
inline f32 f32x4_dot4(f32x4 a, f32x4 b) { return f32x4_dot3(a, b) + f32x4_w(a) * f32x4_w(b); }

inline f32 f32x2_dot(f32x2 a, f32x2 b) { return f32x4_dot2(f32x4_fromF32x2(a), f32x4_fromF32x2(b)); }
