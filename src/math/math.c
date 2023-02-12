/* MIT License
*   
*  Copyright (c) 2022 Oxsomi, Nielsbishere (Niels Brunekreef)
*  
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*  
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.
*  
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE. 
*/

#include "math/math.h"
#include "types/error.h"

#include <math.h>

Error F32_pow2(F32 v, F32 *res) { 

	if(!res)
		return Error_nullPointer(1, 0);

	*res = v * v; 
	return !F32_isValid(*res) ? Error_overflow(0, 0, *(const U32*)&v, *(const U32*)res) : 
		Error_none();
}

Error F32_pow3(F32 v, F32 *res) { 

	if(!res)
		return Error_nullPointer(1, 0);

	*res = v * v * v;
	return !F32_isValid(*res) ? Error_overflow(0, 0, *(const U32*)&v, *(const U32*)res) : 
		Error_none();
}

Error F32_pow4(F32 v, F32 *res) { 

	if(!res)
		return Error_nullPointer(1, 0);

	*res = v * v;
	*res *= *res;

	return !F32_isValid(*res) ? Error_overflow(0, 0, *(const U32*)&v, *(const U32*)res) : 
		Error_none();
}

Error F32_pow5(F32 v, F32 *res) { 

	if(!res)
		return Error_nullPointer(1, 0);

	*res = v * v; 
	*res *= *res; 
	*res *= v;

	return !F32_isValid(*res) ? Error_overflow(0, 0, *(const U32*)&v, *(const U32*)res) : 
		Error_none();
}

Error F32_pow(F32 v, F32 exp, F32 *res) { 

	F32 r = powf(v, exp); 

	if(!F32_isValid(r))
		return Error_overflow(0, 0, *(const U32*)&v, *(const U32*)&r);

	*res = r;
	return Error_none();
}

Error F32_expe(F32 v, F32 *res) { return F32_pow(F32_E, v, res); }
Error F32_exp2(F32 v, F32 *res) { return F32_pow(2, v, res); }
Error F32_exp10(F32 v, F32 *res) { return F32_pow(10, v, res); }

U64 U64_min(U64 v0, U64 v1) { return v0 <= v1 ? v0 : v1; }
U64 U64_max(U64 v0, U64 v1) { return v0 >= v1 ? v0 : v1; }
U64 U64_clamp(U64 v, U64 mi, U64 ma) { return U64_max(mi, U64_min(ma, v)); }

U64 U64_pow2(U64 v) {
	if(!v) return 0;
	U64 res = v * v; 
	return res / v != v ? U64_MAX : res;
}

U64 U64_pow3(U64 v) {
	if(!v) return 0;
	U64 res = U64_pow2(v), res2 = res * v;
	return res == U64_MAX || res2 / v != res ? U64_MAX : res2;
}

U64 U64_pow4(U64 v) {
	if(!v) return 0;
	U64 res = U64_pow2(v), res2 = U64_pow2(res);
	return res == U64_MAX || res2 == U64_MAX ? U64_MAX : res2;
}

U64 U64_pow5(U64 v) {
	if(!v) return 0;
	U64 res = U64_pow4(v), res2 = res * v;
	return res == U64_MAX || res2 / v != res ? U64_MAX : res2;
}

const U64 U64_POW10[] = {
	1,
	10,
	100,
	1'000,
	10'000,
	100'000,
	1'000'000,
	10'000'000,
	100'000'000,
	1'000'000'000,
	10'000'000'000,
	100'000'000'000,
	1'000'000'000'000,
	10'000'000'000'000,
	100'000'000'000'000,
	1'000'000'000'000'000,
	10'000'000'000'000'000,
	100'000'000'000'000'000,
	1'000'000'000'000'000'000,
	10'000'000'000'000'000'000
};

U64 U64_10pow(U64 v) {

	if(v >= sizeof(U64_POW10) / sizeof(U64_POW10[0]))
		return U64_MAX;

	return U64_POW10[v];
}

I64 I64_min(I64 v0, I64 v1) { return v0 <= v1 ? v0 : v1; }
I64 I64_max(I64 v0, I64 v1) { return v0 >= v1 ? v0 : v1; }
I64 I64_clamp(I64 v, I64 mi, I64 ma) { return I64_max(mi, I64_min(ma, v)); }

I64 I64_abs(I64 v) { return v < 0 ? -v : v; }

//TODO: Properly check this?

I64 I64_pow2(I64 v) { 
	I64 res = v * v; 
	return res < I64_abs(v) ? I64_MAX : res;
}

I64 I64_pow3(I64 v) { 
	I64 res = v * v * v; 
	return res < I64_abs(v) ? I64_MAX : res;
}

I64 I64_pow4(I64 v) { 
	I64 res = v * v; res *= res;
	return res < I64_abs(v) ? I64_MAX : res;
}

I64 I64_pow5(I64 v) { 
	I64 res = v * v; res *= res * v;
	return res < I64_abs(v) ? I64_MAX : res;
}

F32 F32_min(F32 v0, F32 v1) { return v0 <= v1 ? v0 : v1; }
F32 F32_max(F32 v0, F32 v1) { return v0 >= v1 ? v0 : v1; }
F32 F32_clamp(F32 v, F32 mi, F32 ma) { return F32_max(mi, F32_min(ma, v)); }
F32 F32_saturate(F32 v) { return F32_clamp(v, 0, 1); }

F32 F32_lerp(F32 a, F32 b, F32 perc) { return a + (b - a) * perc; }
F32 F32_abs(F32 v) { return v < 0 ? -v : v; }

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

Error F32_mod(F32 v, F32 mod, F32 *res) { 

	if(!res)
		return Error_nullPointer(1, 0);

	if(!mod)
		return Error_divideByZero(0, *(const U32*) &v, 0);

	F32 r = fmodf(v, mod); 

	if(!F32_isValid(r))
		return Error_NaN(0);

	*res = r;
	return Error_none();
}

Bool F32_isNaN(F32 v) { return isnan(v); }
Bool F32_isInf(F32 v) { return isinf(v); }
Bool F32_isValid(F32 v) { return isfinite(v); }

F32 F32_fract(F32 v) { return v - F32_floor(v); }

F32 F32_sign(F32 v) { return v < 0 ? -1.f : (v > 0 ? 1.f : 0.f); }
F32 F32_signInc(F32 v) { return v < 0 ? -1.f : 1.f; }
