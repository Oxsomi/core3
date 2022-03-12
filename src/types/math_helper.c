#include "math_helper.h"
#include <math.h>

f32 Math_powf(f32 v, f32 exp) { return powf(v, exp); }
f32 Math_expf(f32 v) { return powf(Math_e, v); }
f32 Math_exp2f(f32 v) { return powf(2, v); }

f32 Math_sqrtf(f32 v) { return sqrtf(v); }

f32 Math_log10f(f32 v) { return log10f(v); }
f32 Math_logef(f32 v) { return logf(v); }
f32 Math_log2f(f32 v) { return log2f(v); }

f32 Math_asin(f32 v) { return asinf(v); }
f32 Math_sin(f32 v) { return sinf(v); }
f32 Math_cos(f32 v) { return cosf(v); }
f32 Math_acos(f32 v) { return acosf(v); }
f32 Math_tan(f32 v) { return tanf(v); }
f32 Math_atan(f32 v) { return atanf(v); }
f32 Math_atan2(f32 y, f32 x) { return atan2f(y, x); }

f32 Math_round(f32 v) { return roundf(v); }
f32 Math_ceil(f32 v) { return ceilf(v); }
f32 Math_floor(f32 v) { return floorf(v); }

f32 Math_mod(f32 v, f32 mod) { return fmodf(v, mod); }

bool Math_isnan(f32 v) { return isnan(v); }
bool Math_isinf(f32 v) { return isinf(v); }