/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2026 Oxsomi / Nielsbishere (Niels Brunekreef)
*
*  This program is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program. If not, see https://github.com/Oxsomi/core3/blob/main/LICENSE.
*  Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.
*  To prevent this a separate license will have to be requested at contact@osomi.net for a premium;
*  This is called dual licensing.
*/

//tools/oxc3_cli/numeric.c
//Numeric utilities: float-format conversion / inspection and time conversion.

#include "tools/oxc3_cli/cli.h"
#include "platforms/platform.h"
#include "platforms/logx.h"
#include "types/container/string.h"
#include "types/base/string_read.h"
#include "types/base/string_read_helper.h"
#include "types/math/flp.h"
#include "types/base/fixed_point.h"
#include "types/base/time.h"
#include "types/base/error.h"
#include "types/base/mathi.h"
#include "types/base/constants.h"

//Reinterpret helpers (avoid strict-aliasing pointer casts)

static U64 CLI_f64ToBits(F64 v) { union { F64 f; U64 u; } x; x.f = v; return x.u; }
static F64 CLI_bitsToF64(U64 u) { union { F64 f; U64 u; } x; x.u = u; return x.f; }

static Bool CLI_parseFloatType(const CharString *s, EFloatType *out) {

	static const struct { const C8 *name; EFloatType type; } types[] = {
		{ "F8",    EFloatType_F8    }, { "F16", EFloatType_F16 }, { "F32", EFloatType_F32 }, { "F64", EFloatType_F64 },
		{ "BF16",  EFloatType_BF16  }, { "TF19", EFloatType_TF19 }, { "PXR24", EFloatType_PXR24 }, { "FP24", EFloatType_FP24 }
	};

	for(U64 i = 0; i < sizeof(types) / sizeof(types[0]); ++i)
		if(CharString_equalsCStringInsensitive(s, types[i].name)) {
			*out = types[i].type;
			return true;
		}

	return false;
}

//Parse the -input as either a raw 0x... bit pattern or a decimal value; returns the raw bits in float format `type`.

static Bool CLI_parseFloatInput(const CharString *in, EFloatType type, U64 *bitsOut) {

	const CharString ox = CharString_createRefCStrConst("0x");

	if(CharString_startsWithStringInsensitive(in, &ox, 0)) {        //Raw bits of `type`
		U64 raw = 0;
		if(!CharString_parseHex(*in, &raw))
			return false;
		*bitsOut = raw;
		return true;
	}

	F64 v = 0;                                                      //Decimal value -> F64 -> convert to `type`
	if(!CharString_parseDouble(*in, &v))
		return false;

	*bitsOut = EFloatType_convert(EFloatType_F64, CLI_f64ToBits(v), type);
	return true;
}

//float convert

Bool CLI_floatConvert(const ParsedArgs *args) {

	if(!args) return false;

	CharString input = CharString_createNull();
	if(!ParsedArgs_getArg(args, EOperationHasParameter_InputShift, &input, NULL)) {
		Log_errorLnx("float convert requires -input <value>.");
		return false;
	}

	F64 v = 0;
	if(!CharString_parseDouble(input, &v)) {
		Log_errorLnx("float convert -input must be a decimal value.");
		return false;
	}

	//--fixed: show the value in both supported fixed-point formats

	if(args->flags & EOperationFlags_Fixed) {

		const FP37f4 fp37 = FP37f4_fromDouble(v);
		const FP46f6 fp46 = FP46f6_fromDouble(v);

		Log_debugLnx(
			"%f as fixed point:\n"
			"\tFP37f4 (37i.4f): 0x%016"PRIx64" (represents %f)\n"
			"\tFP46f6 (46i.6f): 0x%016"PRIx64" (represents %f)",
			v,
			(U64) fp37, FP37f4_toDouble(fp37),
			(U64) fp46, FP46f6_toDouble(fp46)
		);

		return true;
	}

	//Otherwise convert to the -type float format

	EFloatType dst = EFloatType_F32;
	CharString typeStr = CharString_createNull();

	if(ParsedArgs_getArg(args, EOperationHasParameter_TypeShift, &typeStr, NULL)) {
		if(!CLI_parseFloatType(&typeStr, &dst)) {
			Log_errorLnx("Unknown -type. Use one of F8, F16, F32, F64, BF16, TF19, PXR24, FP24.");
			return false;
		}
	}

	const U64 dstBits = EFloatType_convert(EFloatType_F64, CLI_f64ToBits(v), dst);
	const F64 roundtrip = CLI_bitsToF64(EFloatType_convert(dst, dstBits, EFloatType_F64));
	const U8 bytes = EFloatType_bytes(dst);

	const Bool hasType = CharString_length(typeStr) != 0;
	const int typeLen = hasType ? (int) CharString_length(typeStr) : 3;
	const C8 *typeName = hasType ? typeStr.ptr : "F32";

	Log_debugLnx(
		"%f as %.*s = 0x%0*"PRIx64" (represents %f)",
		v, typeLen, typeName, bytes * 2, dstBits, roundtrip
	);

	return true;
}

//float dissect

Bool CLI_floatDissect(const ParsedArgs *args) {

	if(!args) return false;

	CharString input = CharString_createNull();
	if(!ParsedArgs_getArg(args, EOperationHasParameter_InputShift, &input, NULL)) {
		Log_errorLnx("float dissect requires -input <value or 0xbits>.");
		return false;
	}

	EFloatType type = EFloatType_F32;
	CharString typeStr = CharString_createNull();

	if(ParsedArgs_getArg(args, EOperationHasParameter_TypeShift, &typeStr, NULL)) {
		if(!CLI_parseFloatType(&typeStr, &type)) {
			Log_errorLnx("Unknown -type. Use one of F8, F16, F32, F64, BF16, TF19, PXR24, FP24.");
			return false;
		}
	}

	U64 bits = 0;
	if(!CLI_parseFloatInput(&input, type, &bits)) {
		Log_errorLnx("float dissect -input must be a decimal value or a 0x... bit pattern.");
		return false;
	}

	const C8 *kind =
		EFloatType_isNaN(type, bits) ? "NaN" : (
			EFloatType_isInf(type, bits) ? "Inf" : (
				EFloatType_isZero(type, bits) ? "Zero" : (EFloatType_isDeN(type, bits) ? "Denormal" : "Normal")
			)
		);

	const U8 bytes = EFloatType_bytes(type);
	const U64 exp = EFloatType_exponent(type, bits);
	const I64 bias = ((I64)1 << (EFloatType_exponentBits(type) - 1)) - 1;

	Log_debugLnx(
		"%.*s value 0x%0*"PRIx64"\n"
		"\tClass:     %s\n"
		"\tSign:      %"PRIu64"\n"
		"\tExponent:  %"PRIu64" (raw), %"PRIi64" (unbiased, bias %"PRIi64")\n"
		"\tMantissa:  0x%"PRIx64" (%u bits)\n"
		"\tValue:     %f",
		CharString_length(typeStr) ? (int) CharString_length(typeStr) : 3, CharString_length(typeStr) ? typeStr.ptr : "F32",
		bytes * 2, bits,
		kind,
		(U64) (EFloatType_sign(type, bits) ? 1 : 0),
		exp, (I64) exp - bias, bias,
		EFloatType_mantissa(type, bits), EFloatType_mantissaBits(type),
		CLI_bitsToF64(EFloatType_convert(type, bits, EFloatType_F64))
	);

	return true;
}

//time now

Bool CLI_timeNow(const ParsedArgs *args) {

	if(!args) return false;

	const Ns now = Time_now();
	TimeFormat tf = { 0 };
	Time_format(now, tf, false);

	Log_debugLnx("%s (%"PRIu64" ns since Unix epoch)", tf, now);
	return true;
}

//time convert (epoch ns <-> ISO 8601)

Bool CLI_timeConvert(const ParsedArgs *args) {

	if(!args) return false;

	CharString input = CharString_createNull();
	if(!ParsedArgs_getArg(args, EOperationHasParameter_InputShift, &input, NULL)) {
		Log_errorLnx("time convert requires -input <epoch-ns or ISO-8601>.");
		return false;
	}

	//All-digit input -> treat as epoch nanoseconds; otherwise treat as an ISO 8601 string

	if(CharString_isDec(input)) {

		U64 ns = 0;
		if(!CharString_parseU64(input, &ns)) {
			Log_errorLnx("time convert: couldn't parse epoch nanoseconds.");
			return false;
		}

		TimeFormat tf = { 0 };
		Time_format((Ns) ns, tf, false);
		Log_debugLnx("%"PRIu64" ns  ->  %s", ns, tf);
		return true;
	}

	TimeFormat tf = { 0 };
	const U64 n = U64_min(CharString_length(input), (U64) SHORTSTRING_LEN - 1);

	for(U64 i = 0; i < n; ++i)
		tf[i] = input.ptr[i];

	Ns ns = 0;
	if(!Time_parseFormat(&ns, tf, false)) {
		Log_errorLnx("time convert: couldn't parse ISO 8601 (expected e.g. 2026-07-26T00:00:00.000000000Z).");
		return false;
	}

	Log_debugLnx("%.*s  ->  %"PRIu64" ns since Unix epoch", (int) CharString_length(input), input.ptr, ns);
	return true;
}
