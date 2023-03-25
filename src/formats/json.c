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

#include "formats/json.h"
#include "types/buffer.h"
#include "types/buffer_layout.h"
#include "types/allocator.h"
#include "types/error.h"

Bool JSON_isPlainEscaped(C8 c) {					//When only \ has to be added
	return c == '"' || c == '\\' || c == '/';
}

Bool JSON_isEscaped(C8 c) {							//When it requires \ and might need special attention
	return c == '\t' || c == '\r' || c == '\n' || JSON_isPlainEscaped(c);
}

typedef struct JSONSerializeForeach {

	CharString *output;

	Allocator alloc;
	U64 indentOffset;

	Buffer data;

	Bool prettify, useSpaces, notFirst, prependComma, disableIndent, isObject;
	U8 indentPerStep;

} JSONSerializeForeach;

Error JSON_serializeForeach(
	BufferLayout layout, 
	LayoutPathInfo info, 
	CharString path, 
	Buffer data, 
	JSONSerializeForeach *output
) {

	Error err = Error_none();
	CharString tmp0 = CharString_createNull();
	CharString tmp1 = CharString_createNull();
	CharString tmp2 = CharString_createNull();

	//Prepare prepend

	if(output->prettify)
		_gotoIfError(
			clean, 
			CharString_create(
				output->useSpaces ? ' ' : '\t', 
				(output->indentOffset + 1) * output->indentPerStep,
				output->alloc,
				&tmp0
			)
		);

	if(CharString_isEmpty(tmp0))
		tmp0 = CharString_createRefSized("\0", 0, true);

	//Prepend comma

	if (!output->notFirst)
		output->notFirst = true;

	else if (output->prependComma)
		_gotoIfError(clean, CharString_appendString(
			output->output, 
			CharString_createConstRefUnsafe(
				!output->disableIndent && output->prettify ? ",\n" : 
				(output->disableIndent && output->prettify ? ", " : ",")
			),
			output->alloc
		));

	//Prepend 

	if (output->isObject) {		//Ignoring objects that are treated as arrays.

		_gotoIfError(clean, CharString_format(
			output->alloc, &tmp1, 
			output->prettify ? "%s\"%.*s\": " : "%s\"%.*s\":",
			tmp0.ptr,
			(int) CharString_length(info.memberName), info.memberName.ptr
		));

		_gotoIfError(clean, CharString_appendString(output->output, tmp1, output->alloc));
		CharString_free(&tmp1, output->alloc);
	}

	else if(!output->disableIndent)
		_gotoIfError(clean, CharString_appendString(output->output, tmp0, output->alloc));

	//Serialize string

	if (info.typeId == ETypeId_C8 && info.leftoverArray.length == 1) {

		U64 escapedChars = 0, escapedU = 0;		//escapedChars: \(char), escapedU: \u(4 hex)
		U64 totalChars = 0;

		U64 total = ((const U32*)info.leftoverArray.ptr)[0];
		const C8 *ptr = (const C8*) data.ptr;

		for (U64 i = 0; i < total; ++i) {

			C8 c = ptr[i];

			if(c == '\n' || c == '\t' || c == '\r' || c == '/' || c == '\\' || c == '"')
				++escapedChars;

			else if(c >= 0x20 && c < 0x7F)
				++totalChars;

			else ++escapedU;
		}

		_gotoIfError(clean, CharString_reserve(
			output->output,
			CharString_length(*output->output) + 2 + totalChars + escapedChars * 2 + escapedU * 6,
			output->alloc
		));

		_gotoIfError(clean, CharString_append(output->output, '"', output->alloc));

		//Quick stringify

		if(totalChars == total)
			_gotoIfError(clean, CharString_appendString(
				output->output, 
				CharString_createConstRefSized(ptr, total, false), 
				output->alloc
			))

		//We have to escape 

		else {

			for (U64 i = 0; i < total; ++i) {

				C8 c = ptr[i];

				if (c == '\n' || c == '\t' || c == '\r' || c == '/' || c == '\\' || c == '"') {

					_gotoIfError(clean, CharString_append(output->output, '\\', output->alloc));

					if(c == '\n')		c = 'n';
					else if(c == '\r')	c = 'r';
					else if(c == '\t')	c = 't';

					_gotoIfError(clean, CharString_append(output->output, c, output->alloc));
				}

				else if(c >= 0x20 && c < 0x7F)
					_gotoIfError(clean, CharString_append(output->output, c, output->alloc))

				else {

					_gotoIfError(clean, CharString_appendString(
						output->output, 
						CharString_createConstRefUnsafe("\\u00"),
						output->alloc
					));

					C8 c1 = C8_createHex((U8)c >> 4);
					C8 c0 = C8_createHex((U8)c & 0xF);

					_gotoIfError(clean, CharString_append(output->output, c1, output->alloc));
					_gotoIfError(clean, CharString_append(output->output, c0, output->alloc));
				}
			}
		}

		_gotoIfError(clean, CharString_append(output->output, '"', output->alloc));
		goto clean;
	}

	//Serialize array

	Bool isArray = info.leftoverArray.length;
	Bool isVecOrMat = info.typeId != ETypeId_Undefined && ETypeId_getElements(info.typeId) > 1;

	//TODO: Check if array elements are linear

	if(isArray || isVecOrMat) {

		//Serialize all elements

		//Prepare for foreach and store on stack to be able to restore

		if(output->prettify)
			output->indentOffset += output->indentPerStep;

		Bool prep = output->prependComma;
		CharString *outStr = output->output;
		Bool isObj = output->isObject;
		Bool notFirst = output->notFirst;

		output->output = &tmp1;
		output->prependComma = true;
		output->isObject = output->notFirst = false;

		//Handle children

		if(isArray)
			_gotoIfError(clean, BufferLayout_foreachData(
				output->data,
				layout,
				path,
				(BufferLayoutForeachDataFunc) JSON_serializeForeach,
				output,
				false,
				output->alloc
			))

		else {

			ETypeId tid = info.typeId;

			_gotoIfError(clean, ETypeId_getPureType(info.typeId, &info.typeId));

			U64 len = ETypeId_getDataTypeBytes(info.typeId);
			const U8 *ptr = data.ptr;

			Bool indent = output->disableIndent;
			output->disableIndent = true;

			for(U8 j = 0, k = ETypeId_getHeight(tid); j < k; ++j) {

				if (j)
					_gotoIfError(clean, CharString_appendString(
						output->output, 
						CharString_createConstRefUnsafe(output->prettify ? ", " : ","),
						output->alloc
					));

				if (k > 1)
					_gotoIfError(clean, CharString_appendString(
						output->output, 
						CharString_createConstRefUnsafe(output->prettify ? "[ " : "["),
						output->alloc
					));

				output->notFirst = false;

				for(U8 i = 0; i < ETypeId_getWidth(tid); ++i) {

					_gotoIfError(clean, JSON_serializeForeach(
						layout,
						info,
						path,
						Buffer_createConstRef(ptr, len),
						output
					));

					ptr += len;
				}

				if (k > 1)
					_gotoIfError(clean, CharString_appendString(
						output->output, 
						CharString_createConstRefUnsafe(output->prettify ? " ]" : "]"),
						output->alloc
					));
			}

			output->disableIndent = indent;
		}

		//Restore

		output->output = outStr;
		output->prependComma = prep;
		output->isObject = isObj;
		output->notFirst = notFirst;

		if(output->prettify)
			output->indentOffset -= output->indentPerStep;

		//Output

		_gotoIfError(clean, CharString_format(
			output->alloc,
			&tmp2, 
			output->prettify && !isVecOrMat ? "%s[%s\n%s\n%s]" : "%s[%s%s%s]", 
			output->isObject ? "" : tmp0.ptr,
			isVecOrMat && output->prettify ? " " : tmp0.ptr,
			tmp1.ptr,
			isVecOrMat && output->prettify ? " " : tmp0.ptr
		));

		_gotoIfError(clean, CharString_appendString(output->output, tmp2, output->alloc));
		goto clean;
	}

	//Serialize json object

	if (info.structId != U32_MAX) {

		//Serialize all elements

		//Prepare for foreach and store on stack to be able to restore

		if(output->prettify)
			output->indentOffset += output->indentPerStep;

		Bool prep = output->prependComma;
		CharString *outStr = output->output;
		Bool isObj = output->isObject;
		Bool notFirst = output->notFirst;

		output->output = &tmp1;
		output->prependComma = true;
		output->isObject = true;
		output->notFirst = false;
		
		//Go over childs

		_gotoIfError(clean, BufferLayout_foreachData(
			output->data,
			layout,
			path,
			(BufferLayoutForeachDataFunc) JSON_serializeForeach,
			output,
			false,
			output->alloc
		))

		//Restore

		output->output = outStr;
		output->prependComma = prep;
		output->isObject = isObj;
		output->notFirst = notFirst;

		if(output->prettify)
			output->indentOffset -= output->indentPerStep;

		_gotoIfError(clean, CharString_format(
			output->alloc,
			&tmp2, 
			output->prettify ? "{\n%s\n%s}" : "{%s%s}", 
			tmp1.ptr,
			tmp0.ptr
		));

		_gotoIfError(clean, CharString_appendString(output->output, tmp2, output->alloc));
		goto clean;
	}

	//Serialize value

	switch(info.typeId) {

		case ETypeId_Bool: {

			U8 b = *data.ptr;

			if(b > 1)
				_gotoIfError(clean, Error_outOfBounds(3, b, 2));

			_gotoIfError(clean, CharString_appendString(
				output->output, CharString_createConstRefUnsafe(b ? "true" : "false"), output->alloc
			));
			
			goto clean;
		}

		case ETypeId_C8: {

			C8 c = *(const C8*) data.ptr;

			if(!C8_isValidAscii(c))
				_gotoIfError(clean, CharString_format(
					output->alloc, &tmp1, "\"\\u00%02x\"", (int)c
				))

			//Escape characters

			else if (JSON_isEscaped(c)) {

				switch (c) {
					case '\t':	c = 't';	break;
					case '\n':	c = 'n';	break;
					case '\r':	c = 'r';	break;
				}

				_gotoIfError(clean, CharString_format(output->alloc, &tmp1, "\"\\%c\"", c));
			}

			//Regular

			else _gotoIfError(clean, CharString_format(output->alloc, &tmp1, "\"%c\"", c));

			_gotoIfError(clean, CharString_appendString(output->output, tmp1, output->alloc));
			goto clean;
		}

		//"Number" aka F64

		//UInt conversions

		case ETypeId_U8:	case ETypeId_U16:
		case ETypeId_U32:	case ETypeId_U64: {

			U64 v = 0;

			switch (info.typeId) {
				case ETypeId_U8:	v = *data.ptr;					break;
				case ETypeId_U16:	v = *(const U16*) data.ptr;		break;
				case ETypeId_U32:	v = *(const U32*) data.ptr;		break;
				case ETypeId_U64:	v = *(const U64*) data.ptr;		break;
			}

			if((U64)v >> 52)
				_gotoIfError(clean, Error_outOfBounds(3, v, (U64)1 << 52));

			_gotoIfError(clean, CharString_format(output->alloc, &tmp1, "%"PRIu64, v));
			_gotoIfError(clean, CharString_appendString(output->output, tmp1, output->alloc));
			goto clean;
		}

		//Int conversions

		case ETypeId_I8:	case ETypeId_I16:
		case ETypeId_I32:	case ETypeId_I64: {

			I64 v = 0;

			switch (info.typeId) {
				case ETypeId_I8:	v = *(const I8*)  data.ptr;		break;
				case ETypeId_I16:	v = *(const I16*) data.ptr;		break;
				case ETypeId_I32:	v = *(const I32*) data.ptr;		break;
				case ETypeId_I64:	v = *(const I64*) data.ptr;		break;
			}

			//>> isn't defined on int so safely handle.

			if(v > 0 && ((U64)v >> 52))
				_gotoIfError(clean, Error_outOfBounds(3, (U64)v, (U64)1 << 52))

			else if(v < 0 && ((U64)(-v) >> 52))
				_gotoIfError(clean, Error_outOfBounds(3, (U64)(-v), (U64)1 << 52));

			_gotoIfError(clean, CharString_format(output->alloc, &tmp1, "%"PRIi64, v));
			_gotoIfError(clean, CharString_appendString(output->output, tmp1, output->alloc));
			goto clean;
		}

		//Float conversions

		case ETypeId_F16:
		case ETypeId_F32:
		case ETypeId_F64: {

			F64 v = 0;

			switch (info.typeId) {

				case ETypeId_F64:	v = *(const F64*) data.ptr;		break;
				case ETypeId_F32:	v = *(const F32*) data.ptr;		break;

				//TODO: F64_fromHalf (convert sign, exponent, mantissa)
				default:
					_gotoIfError(clean, Error_unimplemented(0));
			}

			_gotoIfError(clean, CharString_format(output->alloc, &tmp1, "%f", v));
			_gotoIfError(clean, CharString_appendString(output->output, tmp1, output->alloc));
			goto clean;
		}
	}

	_gotoIfError(clean, Error_unsupportedOperation(0));

clean:
	CharString_free(&tmp2, output->alloc);
	CharString_free(&tmp1, output->alloc);
	CharString_free(&tmp0, output->alloc);
	return err;
}

Error JSON_serialize(
	Buffer buffer,
	BufferLayout layout,
	CharString path,
	Bool prettify,
	Bool useSpaces,
	U8 indentPerStage,
	Allocator alloc,
	CharString *serialized
) {

	if(!serialized)
		return Error_nullPointer(3);

	if(serialized->ptr)
		return Error_invalidParameter(3, 0);

	Error err = Error_none();
	CharString tmp0 = CharString_createNull();

	LayoutPathInfo info = (LayoutPathInfo) { 0 };
	_gotoIfError(clean, BufferLayout_resolveLayout(layout, path, &info, NULL, alloc));

	//Value

	if (info.typeId != ETypeId_Undefined) {

		JSONSerializeForeach output = (JSONSerializeForeach) {

			.output = serialized,
			.alloc = alloc,

			.data = buffer,

			.prettify = prettify,
			.useSpaces = useSpaces
		};

		Buffer data = Buffer_createConstRef(buffer.ptr + info.offset, info.length);

		_gotoIfError(clean, JSON_serializeForeach(
			layout, 
			info, 
			path, 
			data, 
			&output
		));
	}

	//Array

	else if (info.leftoverArray.length) {

		JSONSerializeForeach foreach = (JSONSerializeForeach) {
			.alloc = alloc,
			.output = &tmp0,
			.prettify = prettify,
			.useSpaces = useSpaces,
			.indentPerStep = indentPerStage,
			.data = buffer,
			.prependComma = true
		};

		_gotoIfError(clean, BufferLayout_foreachData(
			buffer,
			layout,
			path,
			(BufferLayoutForeachDataFunc) JSON_serializeForeach,
			&foreach,
			false,
			alloc
		));

		_gotoIfError(clean, CharString_format(
			alloc,
			serialized, 
			prettify ? "[\n%s\n]" : "[%s]", 
			tmp0.ptr
		));
	}

	//Object

	else {

		JSONSerializeForeach foreach = (JSONSerializeForeach) {
			.alloc = alloc,
			.output = &tmp0,
			.prettify = prettify,
			.useSpaces = useSpaces,
			.indentPerStep = indentPerStage,
			.data = buffer,
			.isObject = true,
			.prependComma = true
		};

		_gotoIfError(clean, BufferLayout_foreachData(
			buffer,
			layout,
			path,
			(BufferLayoutForeachDataFunc) JSON_serializeForeach,
			&foreach,
			false,
			alloc
		));

		_gotoIfError(clean, CharString_format(
			alloc,
			serialized, 
			prettify ? "{\n%s\n}" : "{%s}", 
			tmp0.ptr
		));
	}

clean:
	CharString_free(&tmp0, alloc);
	return err;
}
