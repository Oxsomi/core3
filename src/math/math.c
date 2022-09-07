#include "math/math.h"
#include <math.h>

struct Error f32_pow(f32 v, f32 exp, f32 *res) { 

	f32 r = powf(v, exp); 

	if(f32_isnan(r) || f32_isinf(r))
		return Error_base(
			GenericError_Overflow, 0,
			0, 0, 
			*(const u32*) &v, *(const u32*) &exp
		);

	*res = r;
	return Error_none();
}

//TODO: Error check this

f32 f32_expe(f32 v) { return powf(f32_e, v); }
f32 f32_exp2(f32 v) { return powf(2, v); }
f32 f32_exp10(f32 v) { return powf(10, v); }

f32 f32_sqrt(f32 v) { return sqrtf(v); }

f32 f32_log10(f32 v) { return log10f(v); }
f32 f32_loge(f32 v) { return logf(v); }
f32 f32_log2(f32 v) { return log2f(v); }

f32 f32_asin(f32 v) { return asinf(v); }
f32 f32_sin(f32 v) { return sinf(v); }
f32 f32_cos(f32 v) { return cosf(v); }
f32 f32_acos(f32 v) { return acosf(v); }
f32 f32_tan(f32 v) { return tanf(v); }
f32 f32_atan(f32 v) { return atanf(v); }
f32 f32_atan2(f32 y, f32 x) { return atan2f(y, x); }

f32 f32_round(f32 v) { return roundf(v); }
f32 f32_ceil(f32 v) { return ceilf(v); }
f32 f32_floor(f32 v) { return floorf(v); }

struct Error f32_mod(f32 v, f32 mod, f32 *res) { 

	if(!mod)
		return Error_base(
			GenericError_DivideByZero,
			0, 0, 0,
			*(const u32*) &v, *(const u32*) &mod
		);

	f32 r = fmodf(v, mod); 

	if(f32_isinf(r) || f32_isnan(r))
		return Error_base(
			GenericError_Overflow,
			0, 0, 0,
			*(const u32*) &v, *(const u32*) &mod
		);

	*res = r;
	return Error_none();
}

bool f32_isnan(f32 v) { return isnan(v); }
bool f32_isinf(f32 v) { return isinf(v); }
