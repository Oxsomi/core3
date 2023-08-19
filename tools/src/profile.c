/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "types/buffer.h"
#include "types/string.h"
#include "types/time.h"
#include "types/flp.h"
#include "platforms/log.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/errorx.h"
#include "cli.h"

typedef Error (*ProfileOperation)(ParsedArgs, Buffer);

bool CLI_profileData(ParsedArgs args, ProfileOperation op) {

	U64 bufferSize = GIBI;

	Buffer dat = Buffer_createNull();
	Error err = Error_none();

	_gotoIfError(clean, Buffer_createUninitializedBytesx(bufferSize, &dat));

	if(!Buffer_csprng(dat))
		_gotoIfError(clean, Error_invalidState(0));

	_gotoIfError(clean, op(args, dat));

clean:

	if(err.genericError)
		Error_printx(err, ELogLevel_Error, ELogOptions_Default);

	Buffer_freex(&dat);
	return !err.genericError;
}

typedef enum EProfileCastStep {

	EProfileCastStep_Regular,
	EProfileCastStep_Zero,
	EProfileCastStep_NaN,
	EProfileCastStep_Inf,
	EProfileCastStep_DeN

} EProfileCastStep;

U64 _CLI_profileCastStep(U64 l, U64 k, U64 j, const U8 *ptr, U64 i) {

	const EFloatType types[] = {
		EFloatType_F64,
		EFloatType_F32,
		EFloatType_F16
	};

	EFloatType inputType = types[k];
	EFloatType outputType = types[j];

	//Grab value

	U64 v = 0;

	switch (inputType) {

		case EFloatType_F64:
			v = ((const U64*)ptr)[i];
			break;
			
		case EFloatType_F32:
			v = ((const U32*)ptr)[i];
			break;
			
		case EFloatType_F16:
			v = ((const U16*)ptr)[i];
			break;
	}

	//Convert to ensure we're dealing with the right type

	switch (l) {

		case EProfileCastStep_Regular:

			if(EFloatType_isDeN(inputType, v))
				v += (U64)1 << EFloatType_exponentShift(inputType);

			else if(!EFloatType_isFinite(inputType, v))
				v -= (U64)1 << EFloatType_exponentShift(inputType);

			break;

		case EProfileCastStep_Zero:
			v &=~ EFloatType_signMask(inputType);
			break;

		case EProfileCastStep_NaN:

			if(!EFloatType_mantissa(inputType, v))
				v |= (U64)1 << EFloatType_mantissaShift(inputType);

			v |= EFloatType_exponentMask(inputType) << EFloatType_exponentShift(inputType);
			break;

		case EProfileCastStep_Inf:
			v &=~ EFloatType_signMask(inputType);
			v |= EFloatType_exponentMask(inputType) << EFloatType_exponentShift(inputType);
			break;

		case EProfileCastStep_DeN:
		default:
			v &=~ (EFloatType_exponentMask(inputType) << EFloatType_exponentShift(inputType));
			break;

	}

	return EFloatType_convert(inputType, v, outputType);
}

Error _CLI_profileCast(ParsedArgs args, Buffer buf) {

	args;
	Error err = Error_none();

	if(Buffer_length(buf) < GIBI)
		_gotoIfError(clean, Error_invalidParameter(1, 0));

	const U64 number = GIBI / sizeof(F64) / 32;
	const C8 *iterationNames[] = { "Non denormalized", "(Un)Signed zero", "NaN", "Inf", "DeN" };
	const C8 *floatTypeNames[] = { "F64", "F32", "F16" };

	const U64 itCount = sizeof(iterationNames) / sizeof(iterationNames[0]);
	const U64 floatTypes = sizeof(floatTypeNames) / sizeof(floatTypeNames[0]);

	for(U64 l = 0; l < itCount; ++l) {

		int dbg1 = 0;
		dbg1;

		for(U64 k = 0; k < floatTypes; ++k)
			for(U64 j = 0; j < floatTypes; ++j) {

				if(j == k)		//No profile needed
					continue;

				Ns then = Time_now();
				U64 temp = 0;

				for (U64 i = 0; i < number; ++i)
					temp += _CLI_profileCastStep(l, k, j, buf.ptr, i);

				Ns now = Time_now();

				Log_debugLn(
					"%s: %llux %s -> %s within %fs (%fns/op). (Operation hash: %llu)", 
					iterationNames[l], number, 
					floatTypeNames[k], floatTypeNames[j], 
					(F64)(now - then) / SECOND,
					(F64)(now - then) / number,
					temp
				);
			}

		int dbg = 0;
		dbg;
	}

clean:
	return err;
}

Bool CLI_profileCast(ParsedArgs args) {
	return CLI_profileData(args, _CLI_profileCast);
}

Bool CLI_profileRNG(ParsedArgs args) {
	return CLI_profileData(args, _CLI_profileCast); //_CLI_profileRNG); TODO:
}

Bool CLI_profileCRC32C(ParsedArgs args) {
	return CLI_profileData(args, _CLI_profileCast); //_CLI_profileCRC32C); TODO:
}

Bool CLI_profileSHA256(ParsedArgs args) {
	return CLI_profileData(args, _CLI_profileCast); //_CLI_profileSHA256); TODO:
}

Bool CLI_profileAES256(ParsedArgs args) {
	return CLI_profileData(args, _CLI_profileCast); //_CLI_profileAES256); TODO:
}