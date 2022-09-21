#include "math/math.h"
#include <math.h>

struct Error F32_pow(F32 v, F32 exp, F32 *res) { 

	F32 r = powf(v, exp); 

	if(F32_isnan(r) || F32_isinf(r))
		return Error_base(
			GenericError_Overflow, 0,
			0, 0, 
			*(const U32*) &v, *(const U32*) &exp
		);

	*res = r;
	return Error_none();
}

//TODO: Error check this

F32 F32_expe(F32 v) { return powf(F32_e, v); }
F32 F32_exp2(F32 v) { return powf(2, v); }
F32 F32_exp10(F32 v) { return powf(10, v); }

F32 F32_sqrt(F32 v) { return sqrtf(v); }

F32 F32_log10(F32 v) { return log10f(v); }
F32 F32_loge(F32 v) { return logf(v); }
F32 F32_log2(F32 v) { return log2f(v); }

F32 F32_asin(F32 v) { return asinf(v); }
F32 F32_sin(F32 v) { return sinf(v); }
F32 F32_cos(F32 v) { return cosf(v); }
F32 F32_acos(F32 v) { return acosf(v); }
F32 F32_tan(F32 v) { return tanf(v); }
F32 F32_atan(F32 v) { return atanf(v); }
F32 F32_atan2(F32 y, F32 x) { return atan2f(y, x); }

F32 F32_round(F32 v) { return roundf(v); }
F32 F32_ceil(F32 v) { return ceilf(v); }
F32 F32_floor(F32 v) { return floorf(v); }

struct Error F32_mod(F32 v, F32 mod, F32 *res) { 

	if(!mod)
		return Error_base(
			GenericError_DivideByZero,
			0, 0, 0,
			*(const U32*) &v, *(const U32*) &mod
		);

	F32 r = fmodf(v, mod); 

	if(F32_isinf(r) || F32_isnan(r))
		return Error_base(
			GenericError_Overflow,
			0, 0, 0,
			*(const U32*) &v, *(const U32*) &mod
		);

	*res = r;
	return Error_none();
}

Bool F32_isnan(F32 v) { return isnan(v); }
Bool F32_isinf(F32 v) { return isinf(v); }
