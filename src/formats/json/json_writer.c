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

//formats/json/json_writer.c

#include "formats/json/json_writer.h"
#include "types/container/string.h"
#include "types/base/error.h"
#include <stdarg.h>
#include <inttypes.h>

#define JsonWriter_MAX_DEPTH 64

JsonWriter JsonWriter_create(CharString *out, Bool pretty, const Allocator *alloc) {
	return (JsonWriter) { .out = out, .alloc = alloc, .pretty = pretty };
}

Bool JsonWriter_isComplete(const JsonWriter *w) {
	return w && w->out && !w->depth;
}

static Bool JsonWriter_appendRaw(JsonWriter *w, const C8 *v, Error *e_rr) {
	const CharString ref = CharString_createRefCStrConst(v);
	return CharString_appendString(w->out, &ref, w->alloc, e_rr);
}

//Pretty mode breaks the line before every element and closing bracket, one tab per level

static Bool JsonWriter_indent(JsonWriter *w, U8 depth, Error *e_rr) {

	Bool s_uccess = true;

	if(!w->pretty)
		goto clean;

	//One resize covers the newline and the tabs: it grows the string by depth tabs plus one, and the first new
	// byte becomes the line break

	const U64 length = CharString_length(*w->out);
	gotoIfError3(clean, CharString_resize(w->out, length + depth + 1, '\t', w->alloc, e_rr));

	if(!CharString_setAt(*w->out, length, '\n'))
		retError(clean, Error_invalidState(0, "JsonWriter_indent() couldn't place the line break"));

clean:
	return s_uccess;
}

//Escapes what JSON requires and nothing else.
//Everything below 0x20 becomes \u00XX rather than being dropped, since a name or a diagnostic can carry one and
// a raw control byte would make the whole document unparseable on the other side.

static Bool JsonWriter_appendEscaped(JsonWriter *w, CharString v, Error *e_rr) {

	Bool s_uccess = true;
	CharString tmp = CharString_createNull();

	gotoIfError3(clean, CharString_append(w->out, '"', w->alloc, e_rr));

	const U64 length = CharString_length(v);

	for (U64 i = 0; i < length; ++i) {

		const C8 c = v.ptr[i];

		switch (c) {

			case '"':  gotoIfError3(clean, JsonWriter_appendRaw(w, "\\\"", e_rr));  break;
			case '\\': gotoIfError3(clean, JsonWriter_appendRaw(w, "\\\\", e_rr));  break;
			case '\b': gotoIfError3(clean, JsonWriter_appendRaw(w, "\\b", e_rr));   break;
			case '\f': gotoIfError3(clean, JsonWriter_appendRaw(w, "\\f", e_rr));   break;
			case '\n': gotoIfError3(clean, JsonWriter_appendRaw(w, "\\n", e_rr));   break;
			case '\r': gotoIfError3(clean, JsonWriter_appendRaw(w, "\\r", e_rr));   break;
			case '\t': gotoIfError3(clean, JsonWriter_appendRaw(w, "\\t", e_rr));   break;

			default:

				if((U8) c < 0x20) {
					CharString_free(&tmp, w->alloc);
					gotoIfError3(clean, CharString_format(w->alloc, &tmp, e_rr, "\\u%04x", (U32)(U8) c));
					gotoIfError3(clean, CharString_appendString(w->out, &tmp, w->alloc, e_rr));
					break;
				}

				gotoIfError3(clean, CharString_append(w->out, c, w->alloc, e_rr));
				break;
		}
	}

	gotoIfError3(clean, CharString_append(w->out, '"', w->alloc, e_rr));

clean:
	CharString_free(&tmp, w->alloc);
	return s_uccess;
}

//What precedes a value: inside an object the key already wrote the separator and is now consumed, inside an
// array a separator follows every element but the first.

static Bool JsonWriter_valuePrefix(JsonWriter *w, Error *e_rr) {

	Bool s_uccess = true;

	if(!w || !w->out)
		retError(clean, Error_nullPointer(0, "JsonWriter_valuePrefix()::w and w->out are required"));

	if(!w->depth)
		goto clean;

	const U64 bit = (U64)1 << (w->depth - 1);

	if (w->objectMask & bit) {

		if(!(w->keyMask & bit))
			retError(clean, Error_invalidState(0, "JsonWriter_valuePrefix() a value inside an object needs a key first"));

		w->keyMask &= ~bit;
		goto clean;
	}

	if(!(w->firstMask & bit))
		gotoIfError3(clean, CharString_append(w->out, ',', w->alloc, e_rr));

	w->firstMask &= ~bit;
	gotoIfError3(clean, JsonWriter_indent(w, w->depth, e_rr));

clean:
	return s_uccess;
}

static Bool JsonWriter_keyPrefix(JsonWriter *w, Error *e_rr) {

	Bool s_uccess = true;

	if(!w || !w->out)
		retError(clean, Error_nullPointer(0, "JsonWriter_keyPrefix()::w and w->out are required"));

	if(!w->depth || !(w->objectMask & ((U64)1 << (w->depth - 1))))
		retError(clean, Error_invalidState(0, "JsonWriter_keyPrefix() a key is only legal inside an object"));

	const U64 bit = (U64)1 << (w->depth - 1);

	if(w->keyMask & bit)
		retError(clean, Error_invalidState(1, "JsonWriter_keyPrefix() the previous key's value is still due"));

	if(!(w->firstMask & bit))
		gotoIfError3(clean, CharString_append(w->out, ',', w->alloc, e_rr));

	w->firstMask &= ~bit;
	w->keyMask |= bit;
	gotoIfError3(clean, JsonWriter_indent(w, w->depth, e_rr));

clean:
	return s_uccess;
}

static Bool JsonWriter_begin(JsonWriter *w, Bool isObject, Error *e_rr) {

	Bool s_uccess = true;

	gotoIfError3(clean, JsonWriter_valuePrefix(w, e_rr));

	if(w->depth >= JsonWriter_MAX_DEPTH)
		retError(clean, Error_outOfBounds(
			0, w->depth, JsonWriter_MAX_DEPTH, "JsonWriter_begin() nests deeper than a document may"
		));

	gotoIfError3(clean, CharString_append(w->out, isObject ? '{' : '[', w->alloc, e_rr));

	++w->depth;

	const U64 bit = (U64)1 << (w->depth - 1);
	w->firstMask |= bit;
	w->keyMask &= ~bit;

	if(isObject)
		w->objectMask |= bit;

	else w->objectMask &= ~bit;

clean:
	return s_uccess;
}

static Bool JsonWriter_end(JsonWriter *w, Bool isObject, Error *e_rr) {

	Bool s_uccess = true;

	if(!w || !w->out)
		retError(clean, Error_nullPointer(0, "JsonWriter_end()::w and w->out are required"));

	if(!w->depth)
		retError(clean, Error_invalidState(0, "JsonWriter_end() nothing is open"));

	const U64 bit = (U64)1 << (w->depth - 1);

	if(((w->objectMask & bit) != 0) != isObject)
		retError(clean, Error_invalidState(1, "JsonWriter_end() closes a container of the other kind"));

	if(w->keyMask & bit)
		retError(clean, Error_invalidState(2, "JsonWriter_end() a key's value is still due"));

	//An empty container closes on the same line; one with elements closes on a line of its own

	if(!(w->firstMask & bit))
		gotoIfError3(clean, JsonWriter_indent(w, (U8)(w->depth - 1), e_rr));

	gotoIfError3(clean, CharString_append(w->out, isObject ? '}' : ']', w->alloc, e_rr));
	--w->depth;

clean:
	return s_uccess;
}

Bool JsonWriter_beginObject(JsonWriter *w, Error *e_rr) { return JsonWriter_begin(w, true, e_rr); }
Bool JsonWriter_endObject(JsonWriter *w, Error *e_rr) { return JsonWriter_end(w, true, e_rr); }
Bool JsonWriter_beginArray(JsonWriter *w, Error *e_rr) { return JsonWriter_begin(w, false, e_rr); }
Bool JsonWriter_endArray(JsonWriter *w, Error *e_rr) { return JsonWriter_end(w, false, e_rr); }

Bool JsonWriter_keyString(JsonWriter *w, CharString key, Error *e_rr) {

	Bool s_uccess = true;

	gotoIfError3(clean, JsonWriter_keyPrefix(w, e_rr));
	gotoIfError3(clean, JsonWriter_appendEscaped(w, key, e_rr));
	gotoIfError3(clean, JsonWriter_appendRaw(w, w->pretty ? ": " : ":", e_rr));

clean:
	return s_uccess;
}

Bool JsonWriter_key(JsonWriter *w, const C8 *key, Error *e_rr) {

	Bool s_uccess = true;

	if(!key)
		retError(clean, Error_nullPointer(1, "JsonWriter_key()::key is required"));

	gotoIfError3(clean, JsonWriter_keyString(w, CharString_createRefCStrConst(key), e_rr));

clean:
	return s_uccess;
}

Bool JsonWriter_str(JsonWriter *w, CharString v, Error *e_rr) {

	Bool s_uccess = true;

	gotoIfError3(clean, JsonWriter_valuePrefix(w, e_rr));
	gotoIfError3(clean, JsonWriter_appendEscaped(w, v, e_rr));

clean:
	return s_uccess;
}

Bool JsonWriter_cstr(JsonWriter *w, const C8 *v, Error *e_rr) {
	return JsonWriter_str(w, v ? CharString_createRefCStrConst(v) : CharString_createNull(), e_rr);
}

Bool JsonWriter_hex(JsonWriter *w, Buffer v, Error *e_rr) {

	Bool s_uccess = true;

	gotoIfError3(clean, JsonWriter_valuePrefix(w, e_rr));

	//One resize for the whole run, since a hex string is exactly two characters per byte plus its quotes

	const U64 length = Buffer_length(v);
	const U64 at = CharString_length(*w->out);

	gotoIfError3(clean, CharString_resize(w->out, at + (length << 1) + 2, '"', w->alloc, e_rr));

	C8 *out = w->out->ptrNonConst + at + 1;

	for (U64 i = 0; i < length; ++i) {
		static const C8 digits[] = "0123456789abcdef";
		out[i << 1] = digits[v.ptr[i] >> 4];
		out[(i << 1) | 1] = digits[v.ptr[i] & 0xF];
	}

clean:
	return s_uccess;
}

Bool JsonWriter_raw(JsonWriter *w, const C8 *v, Error *e_rr) {

	Bool s_uccess = true;

	if(!v)
		retError(clean, Error_nullPointer(1, "JsonWriter_raw()::v is required"));

	gotoIfError3(clean, JsonWriter_valuePrefix(w, e_rr));
	gotoIfError3(clean, JsonWriter_appendRaw(w, v, e_rr));

clean:
	return s_uccess;
}

Bool JsonWriter_bool(JsonWriter *w, Bool v, Error *e_rr) { return JsonWriter_raw(w, v ? "true" : "false", e_rr); }
Bool JsonWriter_null(JsonWriter *w, Error *e_rr) { return JsonWriter_raw(w, "null", e_rr); }

Bool JsonWriter_fmt(JsonWriter *w, Error *e_rr, const C8 *format, ...) {

	Bool s_uccess = true;
	CharString tmp = CharString_createNull();

	if(!w || !format)
		retError(clean, Error_nullPointer(!w ? 0 : 2, "JsonWriter_fmt()::w and format are required"));

	va_list args;
	va_start(args, format);
	const Bool formatted = CharString_formatVariadic(w->alloc, &tmp, e_rr, format, args);
	va_end(args);

	if(!formatted)
		retError(clean, Error_invalidState(0, "JsonWriter_fmt() couldn't format"));

	gotoIfError3(clean, JsonWriter_valuePrefix(w, e_rr));
	gotoIfError3(clean, CharString_appendString(w->out, &tmp, w->alloc, e_rr));

clean:
	CharString_free(&tmp, w ? w->alloc : NULL);
	return s_uccess;
}

Bool JsonWriter_u64(JsonWriter *w, U64 v, Error *e_rr) { return JsonWriter_fmt(w, e_rr, "%"PRIu64, v); }
Bool JsonWriter_i64(JsonWriter *w, I64 v, Error *e_rr) { return JsonWriter_fmt(w, e_rr, "%"PRIi64, v); }
Bool JsonWriter_f64(JsonWriter *w, F64 v, Error *e_rr) { return JsonWriter_fmt(w, e_rr, "%g", v); }

Bool JsonWriter_keyObject(JsonWriter *w, const C8 *key, Error *e_rr) {
	return JsonWriter_key(w, key, e_rr) && JsonWriter_beginObject(w, e_rr);
}

Bool JsonWriter_keyArray(JsonWriter *w, const C8 *key, Error *e_rr) {
	return JsonWriter_key(w, key, e_rr) && JsonWriter_beginArray(w, e_rr);
}

Bool JsonWriter_keyStr(JsonWriter *w, const C8 *key, CharString v, Error *e_rr) {
	return JsonWriter_key(w, key, e_rr) && JsonWriter_str(w, v, e_rr);
}

Bool JsonWriter_keyCstr(JsonWriter *w, const C8 *key, const C8 *v, Error *e_rr) {
	return JsonWriter_key(w, key, e_rr) && JsonWriter_cstr(w, v, e_rr);
}

Bool JsonWriter_keyBool(JsonWriter *w, const C8 *key, Bool v, Error *e_rr) {
	return JsonWriter_key(w, key, e_rr) && JsonWriter_bool(w, v, e_rr);
}

Bool JsonWriter_keyNull(JsonWriter *w, const C8 *key, Error *e_rr) {
	return JsonWriter_key(w, key, e_rr) && JsonWriter_null(w, e_rr);
}

Bool JsonWriter_keyU64(JsonWriter *w, const C8 *key, U64 v, Error *e_rr) {
	return JsonWriter_key(w, key, e_rr) && JsonWriter_u64(w, v, e_rr);
}

Bool JsonWriter_keyI64(JsonWriter *w, const C8 *key, I64 v, Error *e_rr) {
	return JsonWriter_key(w, key, e_rr) && JsonWriter_i64(w, v, e_rr);
}

Bool JsonWriter_keyF64(JsonWriter *w, const C8 *key, F64 v, Error *e_rr) {
	return JsonWriter_key(w, key, e_rr) && JsonWriter_f64(w, v, e_rr);
}

Bool JsonWriter_keyRaw(JsonWriter *w, const C8 *key, const C8 *v, Error *e_rr) {
	return JsonWriter_key(w, key, e_rr) && JsonWriter_raw(w, v, e_rr);
}

Bool JsonWriter_keyHex(JsonWriter *w, const C8 *key, Buffer v, Error *e_rr) {
	return JsonWriter_key(w, key, e_rr) && JsonWriter_hex(w, v, e_rr);
}
