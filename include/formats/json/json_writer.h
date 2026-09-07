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

//formats/json/json_writer.h

#pragma once
#include "types/base/string_base.h"
#include "types/base/error.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct Allocator Allocator;

//Appends one JSON document to a CharString. Containers track their own separators, keys and values are
//checked against the container they land in, strings escape what JSON requires, and pretty mode lays the
//document out with a tab per level. A document is consumed whole by whoever reads it, a page or a file, so
//this materializes into the string rather than streaming.
//Nesting is bounded at 64 levels, one bit per level in the masks below.

typedef struct JsonWriter {

	CharString *out;
	const Allocator *alloc;

	U64 firstMask;              //Per depth: the next element there is the first, so no separator precedes it
	U64 objectMask;             //Per depth: the container there is an object, so a key precedes every value
	U64 keyMask;                //Per depth: a key was written and its value is still due

	U8 depth;
	Bool pretty;
	U8 pad[6];

} JsonWriter;

JsonWriter JsonWriter_create(CharString *out, Bool pretty, const Allocator *alloc);

Bool JsonWriter_beginObject(JsonWriter *w, Error *e_rr);
Bool JsonWriter_endObject(JsonWriter *w, Error *e_rr);
Bool JsonWriter_beginArray(JsonWriter *w, Error *e_rr);
Bool JsonWriter_endArray(JsonWriter *w, Error *e_rr);

//A key is only legal inside an object, and exactly one value follows it

Bool JsonWriter_key(JsonWriter *w, const C8 *key, Error *e_rr);
Bool JsonWriter_keyString(JsonWriter *w, CharString key, Error *e_rr);

//Values. A NULL C string is the empty string, the way a null CharString is; null is written on purpose only.

Bool JsonWriter_str(JsonWriter *w, CharString v, Error *e_rr);
Bool JsonWriter_cstr(JsonWriter *w, const C8 *v, Error *e_rr);
Bool JsonWriter_bool(JsonWriter *w, Bool v, Error *e_rr);
Bool JsonWriter_null(JsonWriter *w, Error *e_rr);
Bool JsonWriter_u64(JsonWriter *w, U64 v, Error *e_rr);
Bool JsonWriter_i64(JsonWriter *w, I64 v, Error *e_rr);
Bool JsonWriter_f64(JsonWriter *w, F64 v, Error *e_rr);

//A value the caller spelled itself, appended verbatim in value position: a number in a particular notation,
// or a fragment another writer produced. The caller is responsible for it being one JSON value.

Bool JsonWriter_raw(JsonWriter *w, const C8 *v, Error *e_rr);
Bool JsonWriter_fmt(JsonWriter *w, Error *e_rr, const C8 *format, ...);

//A key and its value in one call, which is how most of a document reads: keyObject and keyArray open the
// container the key names, the rest write the value outright.

Bool JsonWriter_keyObject(JsonWriter *w, const C8 *key, Error *e_rr);
Bool JsonWriter_keyArray(JsonWriter *w, const C8 *key, Error *e_rr);
Bool JsonWriter_keyStr(JsonWriter *w, const C8 *key, CharString v, Error *e_rr);
Bool JsonWriter_keyCstr(JsonWriter *w, const C8 *key, const C8 *v, Error *e_rr);
Bool JsonWriter_keyBool(JsonWriter *w, const C8 *key, Bool v, Error *e_rr);
Bool JsonWriter_keyNull(JsonWriter *w, const C8 *key, Error *e_rr);
Bool JsonWriter_keyU64(JsonWriter *w, const C8 *key, U64 v, Error *e_rr);
Bool JsonWriter_keyI64(JsonWriter *w, const C8 *key, I64 v, Error *e_rr);
Bool JsonWriter_keyF64(JsonWriter *w, const C8 *key, F64 v, Error *e_rr);
Bool JsonWriter_keyRaw(JsonWriter *w, const C8 *key, const C8 *v, Error *e_rr);

//True once every container opened has been closed, which is what makes the string a document

Bool JsonWriter_isComplete(const JsonWriter *w);

#ifdef __cplusplus
	}
#endif
