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

//shader_compiler/compiler_annotate.cpp

#include "types/container/list_impl.h"
#include "types/container/list_basic_types.h"
#include "types/container/string.h"
#include "types/container/string_unicode.h"
#include "types/container/log.h"
#include "types/container/buffer.h"
#include "types/container/ref_ptr.h"
#include "types/base/string_read_helper.h"
#include "types/base/string_mut_helper.h"
#include "types/base/allocator.h"
#include "types/base/c8.h"
#include "types/base/mathi.h"
#include "types/math/flp.h"
#include "types/base/constants.h"
#include "platforms/file.h"
#include "types/container/file_base.h"
#include "platforms/platform.h"
#include "shader_compiler/compiler.h"

#if _PLATFORM_TYPE == PLATFORM_WINDOWS
	#define UNICODE
	#define WIN32_LEAN_AND_MEAN
	#define NOMINMAX
	#include <Windows.h>
	#include <Unknwn.h>
#endif

#define ENABLE_DXC_STATIC_LINKING
#include "dxcompiler/dxcapi.h"
#include "dxcompiler/dxcreflect.h"
#include <exception>
#include "compiler_private.hpp"

//Call HLSL reflection API to acquire functions + annotations

ESHPipelineStage Compiler_parseStage(CharString stageName) {

	Buffer buf = CharString_bufferConst(stageName);
	U64 stageNameLen = CharString_length(stageName);
	U32 c8x4 = Buffer_readU32(buf, 0, NULL, NULL);

	switch (c8x4) {

		default:
			break;

		case C8x4('v', 'e', 'r', 't'):        //vertex

			if(stageNameLen == 6 && Buffer_readU16(buf, 4, NULL, NULL) == C8x2('e', 'x'))
				return ESHPipelineStage_Vertex;

			break;

		case C8x4('d', 'o', 'm', 'a'):        //domain

			if(stageNameLen == 6 && Buffer_readU16(buf, 4, NULL, NULL) == C8x2('i', 'n'))
				return ESHPipelineStage_Domain;

			break;

		case C8x4('p', 'i', 'x', 'e'):        //pixel

			if(stageNameLen == 5 && stageName.ptr[4] == 'l')
				return ESHPipelineStage_Pixel;

			break;

		case C8x4('g', 'e', 'o', 'm'):        //geometry

			if(stageNameLen == 8 && Buffer_readU32(buf, 4, NULL, NULL) == C8x4('e', 't', 'r', 'y'))
				return ESHPipelineStage_GeometryExt;

			break;

		case C8x4('c', 'o', 'm', 'p'):        //compute

			if(stageNameLen == 7 && Buffer_readU32(buf, 3, NULL, NULL) == C8x4('p', 'u', 't', 'e'))
				return ESHPipelineStage_Compute;

			break;

		case C8x4('n', 'o', 'd', 'e'):        //node
			if(stageNameLen == 4)            return ESHPipelineStage_WorkgraphExt;
			break;

		case C8x4('m', 'e', 's', 'h'):        //mesh
			if(stageNameLen == 4)            return ESHPipelineStage_MeshExt;
			break;

		case C8x4('t', 'a', 's', 'k'):        //task
			if(stageNameLen == 4)            return ESHPipelineStage_TaskExt;
			break;

		case C8x4('h', 'u', 'l', 'l'):        //hull
			if(stageNameLen == 4)            return ESHPipelineStage_Hull;
			break;

		//Raytracing

		case C8x4('m', 'i', 's', 's'):        //miss
			if(stageNameLen == 4)            return ESHPipelineStage_MissExt;
			break;

		case C8x4('a', 'n', 'y', 'h'):        //anyhit

			if(stageNameLen == 6 && Buffer_readU16(buf, 4, NULL, NULL) == C8x2('i', 't'))
				return ESHPipelineStage_AnyHitExt;

			break;

		case C8x4('c', 'l', 'o', 's'):        //closesthit

			if(stageNameLen == 10 && Buffer_readU64(buf, 2, NULL, NULL) == C8x8('o', 's', 'e', 's', 't', 'h', 'i', 't'))
				return ESHPipelineStage_ClosestHitExt;

			break;

		case C8x4('c', 'a', 'l', 'l'):        //callable

			if(stageNameLen == 8 && Buffer_readU32(buf, 4, NULL, NULL) == C8x4('a', 'b', 'l', 'e'))
				return ESHPipelineStage_CallableExt;

			break;

		case C8x4('r', 'a', 'y', 'g'):        //raygeneration

			if(
				stageNameLen == 13 &&
				Buffer_readU64(buf, 4, NULL, NULL) == C8x8('e', 'n', 'e', 'r', 'a', 't', 'i', 'o') &&
				stageName.ptr[12] == 'n'
			)
				return ESHPipelineStage_RaygenExt;

			break;

		case C8x4('i', 'n', 't', 'e'):        //intersection

			if(stageNameLen == 12 && Buffer_readU64(buf, 4, NULL, NULL) == C8x8('r', 's', 'e', 'c', 't', 'i', 'o', 'n'))
				return ESHPipelineStage_IntersectionExt;

			break;
	}

	return ESHPipelineStage_Count;
}

ESHVendor Compiler_parseVendor(CharString vendor) {

	Buffer buf = CharString_bufferConst(vendor);
	U64 len = CharString_length(vendor);

	switch(len) {

		default:
			return ESHVendor_Count;

		case 2:        //NV
			return Buffer_readU16(buf, 0, NULL, NULL) == C8x2('N', 'V') ? ESHVendor_NV : ESHVendor_Count;

		case 3:        //AMD, ARM
			switch (Buffer_readU16(buf, 0, NULL, NULL)) {
				default:                return ESHVendor_Count;
				case C8x2('A', 'M'):    return vendor.ptr[2] == 'D' ? ESHVendor_AMD : ESHVendor_Count;
				case C8x2('A', 'R'):    return vendor.ptr[2] == 'M' ? ESHVendor_ARM : ESHVendor_Count;
			}

		case 4:        //QCOM, INTC, IMGT, MSFT, APPL, SMSG, HWEI
			switch (Buffer_readU32(buf, 0, NULL, NULL)) {
				default:                            return ESHVendor_Count;
				case C8x4('Q', 'C', 'O', 'M'):        return ESHVendor_QCOM;
				case C8x4('I', 'N', 'T', 'C'):        return ESHVendor_INTC;
				case C8x4('I', 'M', 'G', 'T'):        return ESHVendor_IMGT;
				case C8x4('M', 'S', 'F', 'T'):        return ESHVendor_MSFT;
				case C8x4('A', 'P', 'P', 'L'):        return ESHVendor_APPL;
				case C8x4('S', 'M', 'S', 'G'):        return ESHVendor_SMSG;
				case C8x4('H', 'W', 'E', 'I'):        return ESHVendor_HWEI;
			}
	}
}

ESHExtension Compiler_parseExtension(CharString extensionName) {

	Buffer buf = CharString_bufferConst(extensionName);
	U64 stageNameLen = CharString_length(extensionName);
	U16 c8x2 = Buffer_readU16(buf, 0, NULL, NULL);

	switch (c8x2) {

		case C8x2('F', '6'):    //F64
			if(stageNameLen == 3 && extensionName.ptr[2] == '4')    return ESHExtension_F64;
			break;

		case C8x2('I', '6'):    //I64
			if(stageNameLen == 3 && extensionName.ptr[2] == '4')    return ESHExtension_I64;
			break;

		case C8x2('P', 'A'):    //PAQ
			if(stageNameLen == 3 && extensionName.ptr[2] == 'Q')    return ESHExtension_PAQ;
			break;

		case C8x2('1', '6'):    //16BitTypes

			if(stageNameLen == 10 && Buffer_readU64(buf, 2, NULL, NULL) == C8x8('B', 'i', 't', 'T', 'y', 'p', 'e', 's'))
				return ESHExtension_16BitTypes;

			break;

		case C8x2('M', 'u'):    //Multiview

			if(stageNameLen == 9 && Buffer_readU64(buf, 1, NULL, NULL) == C8x8('u', 'l', 't', 'i', 'v', 'i', 'e', 'w'))
				return ESHExtension_Multiview;

			break;

		case C8x2('C', 'o'):    //ComputeDeriv, CoopVec, CoopMat

			if(
				stageNameLen == 12 &&
				Buffer_readU64(buf,  2, NULL, NULL) == C8x8('m', 'p', 'u', 't', 'e', 'D', 'e', 'r') &&
				Buffer_readU16(buf, 10, NULL, NULL) == C8x2('i', 'v')
			)
				return ESHExtension_ComputeDeriv;

			if(stageNameLen == 7 && Buffer_readU16(buf, 2, NULL, NULL) == C8x2('o', 'p')) {

				const U32 tail = Buffer_readU32(buf, 3, NULL, NULL);      //buf[3..6]: "pVec" / "pMat" / "pFP8"

				if(tail == C8x4('p', 'V', 'e', 'c'))
					return ESHExtension_CoopVec;

				if(tail == C8x4('p', 'M', 'a', 't'))
					return ESHExtension_CoopMat;

				if(tail == C8x4('p', 'F', 'P', '8'))
					return ESHExtension_CoopFP8;
			}

			//CoopVecTraining (len 15): "CoopVecTraining"
			if(
				stageNameLen == 15 &&
				Buffer_readU32(buf, 3, NULL, NULL) == C8x4('p', 'V', 'e', 'c') &&
				Buffer_readU64(buf, 7, NULL, NULL) == C8x8('T', 'r', 'a', 'i', 'n', 'i', 'n', 'g')
			)
				return ESHExtension_CoopVecTraining;

			break;

		case C8x2('W', 'r'):

			if (
				stageNameLen == 14 &&
				Buffer_readU64(buf,  2, NULL, NULL) == C8x8('i', 't', 'e', 'M', 'S', 'T', 'e', 'x') &&
				Buffer_readU32(buf, 10, NULL, NULL) == C8x4('t', 'u', 'r', 'e')
			)
				return ESHExtension_WriteMSTexture;

			break;

		case C8x2('D', 'e'):    //DescriptorHeap

			if (
				stageNameLen == 14 &&
				Buffer_readU64(buf,  2, NULL, NULL) == C8x8('s', 'c', 'r', 'i', 'p', 't', 'o', 'r') &&
				Buffer_readU32(buf, 10, NULL, NULL) == C8x4('H', 'e', 'a', 'p')
			)
				return ESHExtension_DescriptorHeap;

			break;

		case C8x2('M', 'e'):

			if (
				stageNameLen == 16 &&
				Buffer_readU64(buf,  2, NULL, NULL) == C8x8('s', 'h', 'T', 'a', 's', 'k', 'T', 'e') &&
				Buffer_readU32(buf, 10, NULL, NULL) == C8x4('x', 'D', 'e', 'r') &&
				Buffer_readU16(buf, 14, NULL, NULL) == C8x2('i', 'v')
			)
				return ESHExtension_MeshTaskTexDeriv;

			break;

		case C8x2('A', 't'):    //AtomicI64, AtomicF32, AtomicF64

			if(stageNameLen == 9)
				switch (Buffer_readU64(buf, 1, NULL, NULL)) {
					case C8x8('t', 'o', 'm', 'i', 'c', 'I', '6', '4'):        return ESHExtension_AtomicI64;
					case C8x8('t', 'o', 'm', 'i', 'c', 'F', '3', '2'):        return ESHExtension_AtomicF32;
					case C8x8('t', 'o', 'm', 'i', 'c', 'F', '6', '4'):        return ESHExtension_AtomicF64;
				}

			break;

		case C8x2('S', 'u'):    //SubgroupArithmetic, SubgroupShuffle, SubgroupOperations

			if(stageNameLen == 15) {            //SubgroupShuffle
				if(
					Buffer_readU64(buf, 0, NULL, NULL) == C8x8('S', 'u', 'b', 'g', 'r', 'o', 'u', 'p') &&
					Buffer_readU64(buf, 7, NULL, NULL) == C8x8('p', 'S', 'h', 'u', 'f', 'f', 'l', 'e')
				)
					return ESHExtension_SubgroupShuffle;
			}

			else if (stageNameLen == 18) {        //SubgroupArithmetic, SubgroupOperations

				if(
					Buffer_readU64(buf,  2, NULL, NULL) == C8x8('b', 'g', 'r', 'o', 'u', 'p', 'A', 'r') &&
					Buffer_readU64(buf, 10, NULL, NULL) == C8x8('i', 't', 'h', 'm', 'e', 't', 'i', 'c')
				)
					return ESHExtension_SubgroupArithmetic;
					
				else if(
					Buffer_readU64(buf,  2, NULL, NULL) == C8x8('b', 'g', 'r', 'o', 'u', 'p', 'O', 'p') &&
					Buffer_readU64(buf, 10, NULL, NULL) == C8x8('e', 'r', 'a', 't', 'i', 'o', 'n', 's')
				)
					return ESHExtension_SubgroupOperations;

			}

			break;

		case C8x2('R', 'a'):    //RayQuery, RayMicromapOpacity, RayMotionBlur, RayReorder

			if(stageNameLen == 8 && Buffer_readU64(buf, 0, NULL, NULL) == C8x8('R', 'a', 'y', 'Q', 'u', 'e', 'r', 'y'))
				return ESHExtension_RayQuery;

			else if(stageNameLen >= 10)
				switch (Buffer_readU64(buf, 2, NULL, NULL)) {

					case C8x8('y', 'M', 'i', 'c', 'r', 'o', 'm', 'a'):        //RayMicromapOpacity

						if(
							stageNameLen == 18 && Buffer_readU64(buf, 10, NULL, NULL) == C8x8('p', 'O', 'p', 'a', 'c', 'i', 't', 'y')
						)
							return ESHExtension_RayMicromapOpacity;

						break;

					case C8x8('y', 'M', 'o', 't', 'i', 'o', 'n', 'B'):        //RayMotionBlur

						if(stageNameLen == 13 && Buffer_readU32(buf, 10, NULL, NULL) == C8x4('B', 'l', 'u', 'r'))
							return ESHExtension_RayMotionBlur;

						break;

					case C8x8('y', 'R', 'e', 'o', 'r', 'd', 'e', 'r'):        //RayReorder

						if(stageNameLen == 10)
							return ESHExtension_RayReorder;

						break;

					case C8x8('y', 'T', 'r', 'i', 'P', 'o', 's', 'i'):        //RayTriPosition

						if(stageNameLen == 14 && Buffer_readU32(buf, 10, NULL, NULL) == C8x4('t', 'i', 'o', 'n'))
							return ESHExtension_RayTriPosition;

						break;
				}

			break;
	}

	return (ESHExtension)(1u << ESHExtension_Count);
}

Bool Compiler_registerExtension(U32 *extensions, CharString extensionName, Error *e_rr) {

	Bool s_uccess = true;

	ESHExtension extension = Compiler_parseExtension(extensionName);

	if(extension >> ESHExtension_Count)
		retError(clean, Error_invalidParameter(
			0, 5, "Compiler_registerExtension() unrecognized extension in extension annotation"
		));

	if(*extensions & extension)
		retError(clean, Error_invalidParameter(
			0, 6, "Compiler_registerExtension() duplicate extension found"
		));

	*extensions |= extension;

clean:
	return s_uccess;
}

Bool Compiler_registerExtensions(ListU32 *extensionsRegistered, U32 extensions, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	if(ListU32_contains(*extensionsRegistered, extensions, 0, NULL))
		retError(clean, Error_alreadyDefined(0, "Compiler_registerExtensions() extensions already defined"));

	gotoIfError3(clean, ListU32_pushBack(extensionsRegistered, extensions, alloc, e_rr));

clean:
	return s_uccess;
}

Bool Compiler_registerVendor(U16 *vendors, CharString vendorName, Error *e_rr) {

	Bool s_uccess = true;

	//Find vendor

	ESHVendor vendor = Compiler_parseVendor(vendorName);

	if(vendor == ESHVendor_Count)
		retError(clean, Error_invalidParameter(
			0, 1, "Compiler_registerVendor() unrecognized vendor in vendor annotation"
		));

	if((*vendors >> vendor) & 1)
		retError(clean, Error_invalidParameter(
			0, 2, "Compiler_registerVendor() duplicate vendor found"
		));

	*vendors |= (U16)(1 << vendor);

clean:
	return s_uccess;
}

Bool Compiler_registerModel(ListU16 *vendors, const C8 *strStart, const C8 *strEnd, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	U16 version = 0;
	U8 maj = 0, mi = 0;

	//Find model version

	if (!C8_isDec(strStart[0]))
		retError(clean, Error_invalidParameter(0, 1, "Compiler_registerModel() expected maj.min"));

	maj = C8_dec(strStart[0]);

	if (maj < 6)
		retError(clean, Error_invalidParameter(
			0, 1, "Compiler_registerModel() model maj version is too low to be supported (must be >=6)"
		));

	++strStart;

	if (strStart >= strEnd || strStart[0] != '.')
		retError(clean, Error_invalidParameter(0, 1, "Compiler_registerModel() expected maj.min"));

	++strStart;

	if (strStart >= strEnd || !C8_isDec(strStart[0]))
		retError(clean, Error_invalidParameter(0, 1, "Compiler_registerModel() expected min (maj.min)"));

	mi = C8_dec(strStart[0]);
	++strStart;

	if (strStart < strEnd) {

		if(!C8_isDec(strStart[0]))
			retError(clean, Error_invalidParameter(0, 1, "Compiler_registerModel() expected min (maj.min)"));

		mi = mi * 10 + C8_dec(strStart[0]);
		++strStart;

		if(strStart != strEnd)
			retError(clean, Error_invalidParameter(0, 1, "Compiler_registerModel() expected min (max 2 digits) (maj.min)"));
	}

	if(maj == 6 && mi < 5)
		retError(clean, Error_invalidParameter(
			0, 1, "Compiler_registerModel() model maj.min version is too low to be supported (must be >=6.5)"
		));

	if (maj > 6 || mi > 10)
		retError(clean, Error_invalidParameter(
			0, 1, "Compiler_registerModel() model version is too high, only supported up to 6.10"
		));

	version = OISH_SHADER_MODEL(maj, mi);

	if(ListU16_contains(*vendors, version, 0, NULL))
		retError(clean, Error_invalidParameter(
			0, 1, "Compiler_registerModel() model version was referenced multiple times"
		));

	gotoIfError3(clean, ListU16_pushBack(vendors, version, alloc, e_rr));

clean:
	return s_uccess;
}

Bool Compiler_skipWhitespace(const C8 *&ptr) {

	while (C8_isWhitespace(*ptr))
		++ptr;

	return *ptr;
}

Bool Compiler_skipAlphaNumeric(const C8 *&ptr) {

	while (C8_isAlphaNumeric(*ptr))
		++ptr;

	return *ptr;
}

Bool Compiler_skipNumericDot(const C8 *&ptr) {

	while (C8_isDec(*ptr) || *ptr == '.')
		++ptr;

	return *ptr;
}

Bool Compiler_skipRndBracket(const C8 *&str, Bool isLeft, Error *e_rr) {

	Bool s_uccess = true;

	if (!Compiler_skipWhitespace(str))
		retError(clean, Error_invalidParameter(0, 0, "Compiler_skipRndBracket() reached end of token"));

	if (str[0] != (isLeft ? '(' : ')'))
		retError(clean, Error_invalidParameter(0, 0, "Compiler_skipRndBracket() expected ()"));

	++str;

clean:
	return s_uccess;
}

Bool Compiler_skipDblQuot(const C8 *&str, Bool isLeft, Error *e_rr) {

	Bool s_uccess = true;

	if (isLeft && !Compiler_skipWhitespace(str))
		retError(clean, Error_invalidParameter(0, 0, "Compiler_skipDblQuot() reached end of token"));

	if (str[0] != '"')
		retError(clean, Error_invalidParameter(0, 0, "Compiler_skipDblQuot() expected ()"));

	++str;

clean:
	return s_uccess;
}

Bool Compiler_consumeString(const C8 *&str, const C8 *&strStart, const C8 *&strEnd, Error *e_rr) {

	Bool s_uccess = true;

	// "test"
	//^^

	gotoIfError3(clean, Compiler_skipDblQuot(str, true, e_rr));

	// "test"
	//  ^^^^

	strEnd = strStart = str;

	//Still a loop like this because at some point we might add escaping anyways.

	while (true) {

		C8 c = *strEnd;
		++strEnd;

		if(!c)
			retError(clean, Error_invalidParameter(0, 0, "Compiler_consumeString() unexpected end of token"));

		//Because we reference a literal string when using defines/uniforms/etc.
		// Rather than a copied/processed string.

		if (c == '\\')
			retError(clean, Error_invalidParameter(0, 0, "Compiler_consumeString() escaping characters is unsupported"));

		// "test"
		// "test"
		//      ^

		if (c == '"') {
			--strEnd;
			break;
		}
	}

	str = strEnd + 1;

clean:
	return s_uccess;
}

Bool Compiler_consumeIdentifier(const C8 *&str, const C8 *&start, Error *e_rr) {

	Bool s_uccess = true;

	if (!Compiler_skipWhitespace(str))
		retError(clean, Error_invalidState(0, "Compiler_consumeIdentifier() ended unexpectedly"));

	start = str;
	C8 c;

	while ((c = *str) != '\0' && !C8_isWhitespace(c) && !C8_isSymbol(c))
		++str;

clean:
	return s_uccess;
}

Bool Compiler_parseShaderStageAnnot(
	const C8 *&str, SHEntryRuntime &entry, CharString functionName, const Allocator *alloc, Error *e_rr
) {
	
	const C8 *annotStart = NULL;
	const C8 *annotEnd = NULL;
	ESHPipelineStage stage = ESHPipelineStage_Count;

	Bool s_uccess = true;

	if (entry.entry.stage != ESHPipelineStage_Count)
		retError(clean, Error_invalidParameter(
			0, 0, "Compiler_parseShaderStageAnnot() shader already had shader or stage annotation"
		));
	
	//shader     ( "test" )
	//oxc::stage ( "test" )
	//          ^^^^^^^^^^^^

	gotoIfError3(clean, Compiler_skipRndBracket(str, true, e_rr));
	gotoIfError3(clean, Compiler_consumeString(str, annotStart, annotEnd, e_rr));
	gotoIfError3(clean, Compiler_skipRndBracket(str, false, e_rr));

	//Ensure nothing is leftover

	if (Compiler_skipWhitespace(str))
		retError(clean, Error_invalidState(0, "Compiler_parseShaderStageAnnot() expected end of token"));

	//Parse stage

	stage = Compiler_parseStage(CharString_createRefSizedConst(annotStart, annotEnd - annotStart, false));

	if (stage == ESHPipelineStage_Count)
		retError(clean, Error_invalidParameter(
			0, 1, "Compiler_parseShaderStageAnnot() unrecognized stage in shader annotation"
		));

	gotoIfError3(clean, CharString_createCopy(functionName, alloc, &entry.entry.name, e_rr));

	entry.entry.stage = SHPipelineStage(stage);

	//isRt = RT stage; containsGfxOrComp = not RT and not workgraph (folded into runtimeFlags)

	entry.runtimeFlags &= (U8)~(ESHEntryRuntimeFlag_IsRt | ESHEntryRuntimeFlag_ContainsGfxOrComp);

	if (stage >= ESHPipelineStage_RtStartExt && stage <= ESHPipelineStage_RtEndExt)
		entry.runtimeFlags |= (U8)ESHEntryRuntimeFlag_IsRt;

	else if (stage != ESHPipelineStage_WorkgraphExt)
		entry.runtimeFlags |= (U8)ESHEntryRuntimeFlag_ContainsGfxOrComp;

clean:
	return s_uccess;
}

Bool Compiler_parseShaderAnnot(
	const D3D12_HLSL_ANNOTATION &annot,
	CharString functionName,
	const C8 *&str,
	U64 annotLen,
	SHEntryRuntime &entry,
	const Allocator *alloc,
	Error *e_rr
) {

	Buffer buf = Buffer_createRefConst(str, annotLen);
	U32 c8x4 = 0;

	Bool s_uccess = true;

	if (!annot.IsBuiltin)    //not [shader("")]
		goto clean;

	c8x4 = Buffer_readU32(buf, 0, NULL, NULL);

	//[shader("vertex")]
	// ^

	if (c8x4 != C8x4('s', 'h', 'a', 'd') || Buffer_readU16(buf, 4, NULL, NULL) != C8x2('e', 'r'))
		goto clean;

	str += annotLen;

	//[shader("vertex")]
	//       ^

	gotoIfError3(clean, Compiler_parseShaderStageAnnot(str, entry, functionName, alloc, e_rr));
	entry.isShaderAnnotation = true;

clean:
	return s_uccess;
}

Bool Compiler_parseStageAnnot(
	SHEntryRuntime &entry, CharString functionName, const C8 *&str, const Allocator *alloc, Error *e_rr
) {

	Bool s_uccess = true;

	//[[oxc::stage("vertex")]]
	//              ^

	gotoIfError3(clean, Compiler_parseShaderStageAnnot(str, entry, functionName, alloc, e_rr));

	entry.isShaderAnnotation = false;
	entry.runtimeFlags |= ESHEntryRuntimeFlag_ContainsGfxOrComp;

clean:
	return s_uccess;
}

Bool Compiler_parseModelAnnot(SHEntryRuntime &entry, const C8 *&str, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	const C8 *strStart = nullptr;
	const C8 *strEnd = nullptr;
	
	//oxc::model ( "6.5" )
	//          ^^^^^^^^^^

	gotoIfError3(clean, Compiler_skipRndBracket(str, true, e_rr));
	gotoIfError3(clean, Compiler_consumeString(str, strStart, strEnd, e_rr));
	gotoIfError3(clean, Compiler_skipRndBracket(str, false, e_rr));
		
	//Ensure nothing is leftover

	if (Compiler_skipWhitespace(str))
		retError(clean, Error_invalidState(0, "Compiler_parseModelAnnot() expected end of token"));

	gotoIfError3(clean, Compiler_registerModel(&entry.shaderVersions, strStart, strEnd, alloc, e_rr));

clean:
	return s_uccess;
}

Bool Compiler_parseVendorAnnot(SHEntryRuntime &entry, const C8 *&str, Error *e_rr) {

	Bool s_uccess = true;

	const C8 *annotStart = NULL;
	const C8 *annotEnd = NULL;

	//oxc::vendor ( "NV", "AMD" )
	//           ^^

	gotoIfError3(clean, Compiler_skipRndBracket(str, true, e_rr));

	//oxc::vendor ( "NV" , "AMD" )
	//             ^^^^^

	gotoIfError3(clean, Compiler_consumeString(str, annotStart, annotEnd, e_rr));

	gotoIfError3(clean, Compiler_registerVendor(
		&entry.vendorMask, CharString_createRefSizedConst(annotStart, annotEnd - annotStart, false), e_rr
	));

	while (true) {

		//oxc::vendor ( "NV" , "AMD" )
		//                  ^       ^

		if (!Compiler_skipWhitespace(str))
			retError(clean, Error_invalidState(0, "Compiler_parseVendorAnnot() vendor annotation ended unexpectedly"));

		//oxc::vendor ( "NV" , "AMD" )
		//                           ^

		if (*str == ')')
			break;

		//oxc::vendor ( "NV" , "AMD" )
		//                   ^

		if(*str != ',')
			retError(clean, Error_invalidState(0, "Compiler_parseVendorAnnot() vendor annotation expected ,"));

		++str;

		//oxc::vendor ( "NV" , "AMD" )
		//                    ^^^^^^

		gotoIfError3(clean, Compiler_consumeString(str, annotStart, annotEnd, e_rr));

		gotoIfError3(clean, Compiler_registerVendor(
			&entry.vendorMask, CharString_createRefSizedConst(annotStart, annotEnd - annotStart, false), e_rr
		));
	}

	//oxc::vendor ( "NV" , "AMD" )
	//                           ^

	gotoIfError3(clean, Compiler_skipRndBracket(str, false, e_rr));

	//Ensure nothing is leftover

	if (Compiler_skipWhitespace(str))
		retError(clean, Error_invalidState(0, "Compiler_parseVendorAnnot() expected end of token"));

clean:
	return s_uccess;
}

Bool Compiler_registerBinaryType(U8 *binaryTypes, CharString name, Error *e_rr) {

	Bool s_uccess = true;
	ESHBinaryType type = ESHBinaryType_Count;

	//"spv" / "spirv" -> SPIRV, "dxil" -> DXIL (case insensitive)

	if(CharString_equalsCStringInsensitive(&name, "spv") || CharString_equalsCStringInsensitive(&name, "spirv"))
		type = ESHBinaryType_SPIRV;

	else if(CharString_equalsCStringInsensitive(&name, "dxil"))
		type = ESHBinaryType_DXIL;

	else retError(clean, Error_invalidParameter(
		0, 1, "Compiler_registerBinaryType() unrecognized binary type (expected \"spv\"/\"spirv\" or \"dxil\")"
	));

	if((*binaryTypes >> type) & 1)
		retError(clean, Error_invalidParameter(
			0, 2, "Compiler_registerBinaryType() duplicate binary type found"
		));

	*binaryTypes |= (U8)(1 << type);

clean:
	return s_uccess;
}

Bool Compiler_parseBinaryAnnot(SHEntryRuntime &entry, const C8 *&str, Error *e_rr) {

	Bool s_uccess = true;

	const C8 *annotStart = NULL;
	const C8 *annotEnd = NULL;
	U8 binaryTypes = 0;

	//oxc::binary ( "spv" , "dxil" )
	//           ^^

	gotoIfError3(clean, Compiler_skipRndBracket(str, true, e_rr));

	//oxc::binary ( "spv" , "dxil" )
	//             ^^^^^

	gotoIfError3(clean, Compiler_consumeString(str, annotStart, annotEnd, e_rr));

	gotoIfError3(clean, Compiler_registerBinaryType(
		&binaryTypes, CharString_createRefSizedConst(annotStart, annotEnd - annotStart, false), e_rr
	));

	while (true) {

		//oxc::binary ( "spv" , "dxil" )
		//                   ^        ^

		if (!Compiler_skipWhitespace(str))
			retError(clean, Error_invalidState(0, "Compiler_parseBinaryAnnot() binary annotation ended unexpectedly"));

		//oxc::binary ( "spv" , "dxil" )
		//                             ^

		if (*str == ')')
			break;

		//oxc::binary ( "spv" , "dxil" )
		//                    ^

		if(*str != ',')
			retError(clean, Error_invalidState(0, "Compiler_parseBinaryAnnot() binary annotation expected ,"));

		++str;

		//oxc::binary ( "spv" , "dxil" )
		//                      ^^^^^^

		gotoIfError3(clean, Compiler_consumeString(str, annotStart, annotEnd, e_rr));

		gotoIfError3(clean, Compiler_registerBinaryType(
			&binaryTypes, CharString_createRefSizedConst(annotStart, annotEnd - annotStart, false), e_rr
		));
	}

	//oxc::binary ( "spv" , "dxil" )
	//                             ^

	gotoIfError3(clean, Compiler_skipRndBracket(str, false, e_rr));

	//Ensure nothing is leftover

	if (Compiler_skipWhitespace(str))
		retError(clean, Error_invalidState(0, "Compiler_parseBinaryAnnot() expected end of token"));

	entry.binaryTypes = binaryTypes;

clean:
	return s_uccess;
}

Bool Compiler_parseExtensionAnnot(SHEntryRuntime &entry, const C8 *&str, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	const C8 *annotStart = NULL;
	const C8 *annotEnd = NULL;
	U32 extensions = 0;

	//oxc::extension ( "16BitTypes", "I64" )
	//              ^^

	gotoIfError3(clean, Compiler_skipRndBracket(str, true, e_rr));

	//oxc::extension ( "16BitTypes", "I64" )
	//oxc::extension ( )
	//                ^

	if (!Compiler_skipWhitespace(str))
		retError(clean, Error_invalidState(0, "Compiler_parseExtensionAnnot() extension annotation ended unexpectedly"));

	if (str[0] == ')') {

		++str;

		if (Compiler_skipWhitespace(str))
			retError(clean, Error_invalidState(
				0, "Compiler_parseExtensionAnnot() extension annotation had unknown syntax appended"
			));

		goto clean;
	}

	//oxc::extension ( "16BitTypes", "I64" )
	//                 ^^^^^^^^^^^^

	gotoIfError3(clean, Compiler_consumeString(str, annotStart, annotEnd, e_rr));

	gotoIfError3(clean, Compiler_registerExtension(
		&extensions, CharString_createRefSizedConst(annotStart, annotEnd - annotStart, false), e_rr
	));

	while (true) {

		//oxc::extension ( "16BitTypes" , "I64" )
		//                             ^       ^

		if (!Compiler_skipWhitespace(str))
			retError(clean, Error_invalidState(0, "Compiler_parseExtensionAnnot() extension annotation ended unexpectedly"));

		//oxc::extension ( "16BitTypes" , "I64" )
		//                                      ^

		if (*str == ')')
			break;

		//oxc::extension ( "16BitTypes" , "I64" )
		//                              ^

		if(*str != ',')
			retError(clean, Error_invalidState(0, "Compiler_parseExtensionAnnot() extension annotation expected ,"));

		++str;

		//oxc::extension ( "16BitTypes" , "I64" )
		//                               ^^^^^^

		gotoIfError3(clean, Compiler_consumeString(str, annotStart, annotEnd, e_rr));

		gotoIfError3(clean, Compiler_registerExtension(
			&extensions, CharString_createRefSizedConst(annotStart, annotEnd - annotStart, false), e_rr
		));
	}

	//oxc::extension ( "16BitTypes" , "I64" )
	//                                      ^

	gotoIfError3(clean, Compiler_skipRndBracket(str, false, e_rr));
		
	//Ensure nothing is leftover

	if (Compiler_skipWhitespace(str))
		retError(clean, Error_invalidState(0, "Compiler_parseExtensionAnnot() expected end of token"));

clean:
	if(s_uccess)
		gotoIfError3(clean1, Compiler_registerExtensions(
			&entry.extensions, extensions, alloc, e_rr
		));

clean1:
	return s_uccess;
}

Bool Compiler_parseDefine(SHEntryRuntime &entry, const C8 *&str, U8 &defineCount, const Allocator *alloc, Error *e_rr) {

	const C8 *strStart = NULL;
	const C8 *strEnd = NULL;

	Bool s_uccess = true;

	CharString defineName = CharString_createNull();
	CharString defineValue = CharString_createNull();

	//"X" = "12345"
	//"Y"
	//^

	gotoIfError3(clean, Compiler_consumeString(str, strStart, strEnd, e_rr));

	//"X" = "12345"
	//"Y"
	//    ^
	
	if(!Compiler_skipWhitespace(str))
		retError(clean, Error_invalidParameter(0, 0, "Compiler_parseDefine() reached unexpected end of token"));

	defineName = CharString_createRefSizedConst(strStart, strEnd - strStart, false);

	for(U64 i = 0; i < CharString_length(defineName); ++i)
		if((C8_isSymbol(defineName.ptr[i]) && defineName.ptr[i] != '_') || C8_isWhitespace(defineName.ptr[i]))
			retError(clean, Error_alreadyDefined(0, "Compiler_parseDefine() can't contain symbols or whitespace"));
			
	//Scan last strings for the same define

	for(
		U64 i = (entry.defineNameValues.length >> 1) - 1;
		i != (entry.defineNameValues.length >> 1) - defineCount - 1;
		--i
	)
		if (CharString_equalsStringSensitive(&defineName, &entry.defineNameValues.ptr[i << 1]))
			retError(clean, Error_alreadyDefined(0, "Compiler_parseDefine() already contains define"));

	//Let caller handle invalid syntax or ,).
	//And push current define / with empty value to allow , to work.

	gotoIfError3(clean, ListCharString_pushBack(&entry.defineNameValues, defineName, alloc, e_rr));

	if (*str != '=') {
		gotoIfError3(clean, ListCharString_pushBack(&entry.defineNameValues, defineValue, alloc, e_rr));
		goto clean;
	}

	++str;

	//"X" = "12345"
	//     ^

	gotoIfError3(clean, Compiler_consumeString(str, strStart, strEnd, e_rr));
	defineValue = CharString_createRefSizedConst(strStart, strEnd - strStart, false);

	gotoIfError3(clean, ListCharString_pushBack(&entry.defineNameValues, defineValue, alloc, e_rr));

clean:
	return s_uccess;
}

Bool Compiler_parseDefinesAnnot(SHEntryRuntime &entry, const C8 *&str, const Allocator *alloc, Error *e_rr) {
	
	Bool s_uccess = true;

	U8 defineCount = 0;

	//oxc::defines ( "X" = "Y" )
	//oxc::defines ( "X" )
	//oxc::defines ( )
	//            ^^

	gotoIfError3(clean, Compiler_skipRndBracket(str, true, e_rr));

	//oxc::defines ( "X" = "Y" )
	//oxc::defines ( "X" )
	//oxc::defines ( )
	//              ^

	if (!Compiler_skipWhitespace(str))
		retError(clean, Error_invalidState(0, "Compiler_parseDefinesAnnot() defines annotation ended unexpectedly"));

	if (str[0] == ')') {

		++str;

		if (Compiler_skipWhitespace(str))
			retError(clean, Error_invalidState(
				0, "Compiler_parseDefinesAnnot() defines annotation had unknown syntax appended"
			));

		goto clean;
	}

	//oxc::defines ( "X" = "Y" )
	//oxc::defines ( "X" )
	//               ^

	gotoIfError3(clean, Compiler_parseDefine(entry, str, defineCount, alloc, e_rr));
	++defineCount;

	while (true) {

		//oxc::defines ( "X" = "Y" )
		//oxc::defines ( "X" = "Y" , "Z" )
		//oxc::defines ( "X" )
		//                  ^     ^     ^

		if (!Compiler_skipWhitespace(str))
			retError(clean, Error_invalidState(0, "Compiler_parseDefinesAnnot() defines annotation ended unexpectedly"));

		//oxc::defines ( "X" = "Y" )
		//oxc::defines ( "X" = "Y" , "Z" )
		//oxc::defines ( "X" )
		//                   ^     ^     ^

		if (*str == ')')
			break;

		//oxc::defines ( "X" = "Y" , "Z" )
		//                         ^

		if(*str != ',')
			retError(clean, Error_invalidState(0, "Compiler_parseDefinesAnnot() defines annotation expected ,"));

		++str;

		//oxc::defines ( "X" = "Y" , "Z" )
		//                          ^^^^

		gotoIfError3(clean, Compiler_parseDefine(entry, str, defineCount, alloc, e_rr));
		++defineCount;
	}

	//oxc::defines ( "X" = "Y" , "Z" )
	//                               ^

	gotoIfError3(clean, Compiler_skipRndBracket(str, false, e_rr));
		
	//Ensure nothing is leftover

	if (Compiler_skipWhitespace(str))
		retError(clean, Error_invalidState(0, "Compiler_parseDefinesAnnot() expected end of token"));

clean:

	if (s_uccess)
		gotoIfError3(clean, ListU8_pushBack(&entry.definesPerCompilation, defineCount, alloc, e_rr));

	goto clean1;

clean1:
	return s_uccess;
}

Bool Compiler_parseOxcAnnot(
	CharString functionName,
	const C8 *&str,
	U64 annotLen,
	SHEntryRuntime &entry,
	const Allocator *alloc,
	Error *e_rr
) {

	Buffer buf = Buffer_createRefConst(str, annotLen);
	Bool s_uccess = true;
	U32 c8x4 = 0;
	const C8 *annotEnd = str + annotLen;

	//oxc
	//^

	if (Buffer_readU16(buf, 0, NULL, NULL) != C8x2('o', 'x') || buf.ptr[2] != 'c')
		goto clean;

	//oxc::X
	//   ^

	str = annotEnd;

	if (!Compiler_skipWhitespace(str))
		goto clean;

	if (*str != ':' || str[1] != ':')
		goto clean;

	str += 2;

	if (!Compiler_skipWhitespace(str))
		goto clean;

	annotEnd = str;
	if (!Compiler_skipAlphaNumeric(annotEnd))
		goto clean;

	annotLen = annotEnd - str;

	if (annotLen < 5 || annotLen > 9)    //Skip unknown
		goto clean;

	buf = Buffer_createRefConst(str, annotLen);
	c8x4 = Buffer_readU32(buf, 0, NULL, NULL);

	switch (c8x4) {

	case C8x4('s', 't', 'a', 'g'):        //oxc::stage()

		//[[oxc::stage("vertex")]]
		//       ^
		if (annotLen == 5 && str[4] == 'e') {
			str += 5;
			gotoIfError3(clean, Compiler_parseStageAnnot(entry, functionName, str, alloc, e_rr));
		}

		goto clean;

	case C8x4('m', 'o', 'd', 'e'):        //oxc::model()

		//[[oxc::model("6.8")]]
		//       ^
		if (annotLen == 5 && str[4] == 'l') {
			str += 5;
			gotoIfError3(clean, Compiler_parseModelAnnot(entry, str, alloc, e_rr));
		}

		break;

	case C8x4('v', 'e', 'n', 'd'):        //oxc::vendor()

		//[[oxc::vendor("NV", "AMD")]]
		//       ^
		if (annotLen == 6 && Buffer_readU16(buf, 4, NULL, NULL) == C8x2('o', 'r')) {
			str += 6;
			gotoIfError3(clean, Compiler_parseVendorAnnot(entry, str, e_rr));
		}

		break;

	case C8x4('b', 'i', 'n', 'a'):        //oxc::binary()

		//[[oxc::binary("spv", "dxil")]]
		//       ^
		if (annotLen == 6 && Buffer_readU16(buf, 4, NULL, NULL) == C8x2('r', 'y')) {
			str += 6;
			gotoIfError3(clean, Compiler_parseBinaryAnnot(entry, str, e_rr));
		}

		break;

	case C8x4('d', 'e', 'f', 'i'):        //oxc::defines()

		//[[oxc::defines("X", "Y", "Z")]]
		//[[oxc::defines("X" = "123", "Y" = "ABC")]]
		//         ^
		if (annotLen == 7 && Buffer_readU16(buf, 4, NULL, NULL) == C8x2('n', 'e') && buf.ptr[6] == 's') {
			str += 7;
			gotoIfError3(clean, Compiler_parseDefinesAnnot(entry, str, alloc, e_rr));
		}

		break;

	case C8x4('u', 'n', 'i', 'f'):        //oxc::uniforms()

		//[[oxc::uniforms(U8x4 x = (1, 2, 3, 4))]]
		//[[oxc::uniforms(B1 b = true)]]
		//         ^
		if (annotLen == 8 && Buffer_readU32(buf, 4, NULL, NULL) == C8x4('o', 'r', 'm', 's')) {
			str += 8;
			gotoIfError3(clean, Compiler_parseUniformsAnnot(entry, str, alloc, e_rr));
		}

		break;

	case C8x4('e', 'x', 't', 'e'):        //oxc::extension()

		//[[oxc::extension()]]
		//       ^
		if (
			annotLen == 9 &&
			Buffer_readU64(buf, 1, NULL, NULL) == C8x8('x', 't', 'e', 'n', 's', 'i', 'o', 'n')
		) {
			str += 9;
			gotoIfError3(clean, Compiler_parseExtensionAnnot(entry, str, alloc, e_rr));
		}

		break;
	}

clean:
	return s_uccess;
}

Bool Compiler_parseAnnot(
	const D3D12_HLSL_ANNOTATION &annot,
	CharString funcName,
	SHEntryRuntime &entry,
	const Allocator *alloc,
	Error *e_rr
) {

	//Skip invalid annotations and find length

	const C8 *str = annot.Name;
	const C8 *annotEnd = NULL;
	U64 annotLen = 0;

	Bool s_uccess = true;

	if (!Compiler_skipWhitespace(str))
		goto clean;

	annotEnd = str;
	if (!Compiler_skipAlphaNumeric(annotEnd))
		goto clean;

	annotLen = annotEnd - str;

	if (annotLen != 3 && annotLen != 6)    //Ignore non oxc and shader annots
		goto clean;

	//[shader()]
	// ^

	if (annotLen == 6) {
		gotoIfError3(clean, Compiler_parseShaderAnnot(annot, funcName, str, annotLen, entry, alloc, e_rr));
		goto clean;
	}

	if (annot.IsBuiltin)    //not [[oxc::]]
		goto clean;

	//[[oxc::defines()]]
	//[[oxc::extension()]]
	//[[oxc::vendor()]]
	//[[oxc::model()]]
	//[[oxc::stage()]]
	//[[oxc::uniforms()]]
	//    ^

	gotoIfError3(clean, Compiler_parseOxcAnnot(funcName, str, annotLen, entry, alloc, e_rr));

clean:
	return s_uccess;
}
