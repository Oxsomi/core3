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

//types/container/string.hpp

#pragma once
#include <utility>
#include <type_traits>

//Pre-include system headers used by the C headers below at global scope;
//they must not be pulled in for the first time inside a namespace.

#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <assert.h>

//Wrappers around the C CharString.
//Storage, growth and freeing all stay in the C functions,
// so anything built here is still the exact same CharString a C caller consumes;
// handle() hands it straight over with no conversion.
//
//The C type is one struct covering three lifetimes, distinguished by capacityAndRefInfo (see string_base.h):
//owned (capacity > 0), mutable ref (0) and const ref (U64_MAX).
//That distinction is what the three C++ types make explicit, so ownership stops being something you have to remember:
//
//  oxc::StringView   const ref;    non-owning, trivially copyable, no allocator, read only
//  oxc::String       owned;        RAII, move only, needs an allocator
//  oxc::SmallString  stack buffer; fixed capacity, never allocates (the C ShortString / LongString)
//
//StringView is what every read-only API should take: String and SmallString both convert to it implicitly,
// and so does a string literal.

namespace oxc {

	namespace c {
		#include "types/container/string.h"
		#include "types/base/string_base.h"
		#include "types/base/string_read.h"
		#include "types/base/string_read_helper.h"
		#include "types/base/string_mut.h"
		#include "types/base/string_mut_helper.h"
		#include "types/base/error.h"
		#include "types/base/allocator.h"
	}

	class String;

	//Non-owning view over an existing string. Copyable and cheap; it never frees and never allocates,
	//so the memory it points at has to outlive it. Everything that only reads lives here, and String /
	//SmallString inherit that surface by converting.

	class StringView {

		c::CharString str;

	public:

		StringView() noexcept : str{} {}

		//From a C string literal or any null terminated buffer
		StringView(const c::C8 *cstr) noexcept : str(c::CharString_createRefCStrConst(cstr)) {}

		StringView(const c::C8 *ptr, c::U64 len, bool nullTerminated = false) noexcept :
			str(c::CharString_createRefSizedConst(ptr, len, nullTerminated)) {}

		//From an existing C CharString of any flavour; the view is always const-ref regardless of the source
		StringView(const c::CharString &other) noexcept : str(c::CharString_createRefStrConst(other)) {}

		[[nodiscard]] c::U64 length() const noexcept { return c::CharString_length(str); }
		[[nodiscard]] bool empty() const noexcept { return !c::CharString_length(str); }
		[[nodiscard]] bool nullTerminated() const noexcept { return c::CharString_isNullTerminated(str); }

		[[nodiscard]] const c::C8 *data() const noexcept { return str.ptr; }
		[[nodiscard]] const c::C8 *begin() const noexcept { return c::CharString_beginConst(str); }
		[[nodiscard]] const c::C8 *end() const noexcept { return c::CharString_endConst(str); }

		[[nodiscard]] c::C8 operator[](c::U64 i) const noexcept { return str.ptr[i]; }

		//Comparison. Sensitive by default; the C API spells the casing out, so mirror that in the name
		//rather than hiding it behind a default argument nobody reads.

		[[nodiscard]] bool equals(const StringView &o) const noexcept {
			return c::CharString_equalsStringSensitive(&str, &o.str);
		}

		[[nodiscard]] bool equalsInsensitive(const StringView &o) const noexcept {
			return c::CharString_equalsStringInsensitive(&str, &o.str);
		}

		[[nodiscard]] bool operator==(const StringView &o) const noexcept { return equals(o); }
		[[nodiscard]] bool operator!=(const StringView &o) const noexcept { return !equals(o); }

		[[nodiscard]] c::ECompareResult compare(const StringView &o, bool sensitive = true) const noexcept {
			return c::CharString_compare(&str, &o.str, sensitive ? c::EStringCase_Sensitive : c::EStringCase_Insensitive);
		}

		[[nodiscard]] bool operator<(const StringView &o) const noexcept {
			return compare(o) == c::ECompareResult_Lt;
		}

		//Prefix / suffix / contains

		[[nodiscard]] bool startsWith(const StringView &o, c::U64 off = 0) const noexcept {
			return c::CharString_startsWithStringSensitive(&str, &o.str, off);
		}

		[[nodiscard]] bool startsWithInsensitive(const StringView &o, c::U64 off = 0) const noexcept {
			return c::CharString_startsWithStringInsensitive(&str, &o.str, off);
		}

		[[nodiscard]] bool endsWith(const StringView &o, c::U64 off = 0) const noexcept {
			return c::CharString_endsWithStringSensitive(&str, &o.str, off);
		}

		[[nodiscard]] bool endsWithInsensitive(const StringView &o, c::U64 off = 0) const noexcept {
			return c::CharString_endsWithStringInsensitive(&str, &o.str, off);
		}

		[[nodiscard]] bool startsWith(c::C8 ch, c::U64 off = 0) const noexcept {
			return c::CharString_startsWithSensitive(str, ch, off);
		}

		[[nodiscard]] bool endsWith(c::C8 ch, c::U64 off = 0) const noexcept {
			return c::CharString_endsWithSensitive(str, ch, off);
		}

		[[nodiscard]] bool contains(c::C8 ch, c::U64 off = 0, c::U64 len = 0) const noexcept {
			return c::CharString_containsSensitive(&str, ch, off, len);
		}

		[[nodiscard]] bool contains(const StringView &o, c::U64 off = 0, c::U64 len = 0) const noexcept {
			return c::CharString_containsStringSensitive(&str, &o.str, off, len);
		}

		//Search; all return U64_MAX when not found, matching the C API

		[[nodiscard]] c::U64 findFirst(c::C8 ch, c::U64 off = 0, c::U64 len = 0) const noexcept {
			return c::CharString_findFirstSensitive(&str, ch, off, len);
		}

		[[nodiscard]] c::U64 findLast(c::C8 ch, c::U64 off = 0, c::U64 len = 0) const noexcept {
			return c::CharString_findLastSensitive(&str, ch, off, len);
		}

		[[nodiscard]] c::U64 findFirst(const StringView &o, c::U64 off = 0, c::U64 len = 0) const noexcept {
			return c::CharString_findFirstStringSensitive(&str, &o.str, off, len);
		}

		[[nodiscard]] c::U64 findLast(const StringView &o, c::U64 off = 0, c::U64 len = 0) const noexcept {
			return c::CharString_findLastStringSensitive(&str, &o.str, off, len);
		}

		//Sub-views. These are refs into the same memory, so they carry the same lifetime as *this.

		[[nodiscard]] StringView substr(c::U64 off, c::U64 len = 0) const noexcept {
			c::CharString out{};
			if(!c::CharString_cut(&str, off, len, &out))
				return StringView();
			return StringView(out);
		}

		[[nodiscard]] StringView trim() const noexcept { return StringView(c::CharString_trim(str)); }

		//Hash of content; for maps only, not cryptography (see CharString_hash)
		[[nodiscard]] c::U64 hash() const noexcept { return c::CharString_hash(str); }

		[[nodiscard]] c::U64 unicodeCodepoints() const noexcept { return c::CharString_unicodeCodepoints(str); }

		//Interop: the underlying const-ref CharString, ready to hand to any C function

		[[nodiscard]] const c::CharString &handle() const noexcept { return str; }
	};

	//Owning string. Move only, like oxc::List, because a copy would need an allocator decision that the
	//caller should be making explicitly (use copy()).

	class String {

		c::CharString str;
		const c::Allocator *alloc;

		void release() noexcept {
			c::CharString_free(&str, alloc);
			str = c::CharString{};
		}

	public:

		String(const c::Allocator &alloc) noexcept : str{}, alloc(&alloc) {}
		~String() noexcept { release(); }

		String(const String&) = delete;
		String &operator=(const String&) = delete;

		String(String &&other) noexcept : str(other.str), alloc(other.alloc) {
			other.str = c::CharString{};
		}

		String &operator=(String &&other) noexcept {

			if(this == &other)
				return *this;

			release();
			str = other.str;
			alloc = other.alloc;
			other.str = c::CharString{};
			return *this;
		}

		//Creation. Each resets *this first, so a String can be refilled without leaking.

		[[nodiscard]] c::Bool createCopy(const StringView &from, c::Error *e_rr = nullptr) noexcept {
			release();
			return c::CharString_createCopy(from.handle(), alloc, &str, e_rr);
		}

		[[nodiscard]] c::Bool createRepeated(c::C8 ch, c::U64 count, c::Error *e_rr = nullptr) noexcept {
			release();
			return c::CharString_create(ch, count, alloc, &str, e_rr);
		}

		//NEVER pass user generated content as the format; use "%.*s" with length + ptr instead.
		[[nodiscard]] c::Bool format(c::Error *e_rr, const c::C8 *fmt, ...) noexcept {

			release();

			va_list args;
			va_start(args, fmt);
			const c::Bool res = c::CharString_formatVariadic(alloc, &str, e_rr, fmt, args);
			va_end(args);
			return res;
		}

		//Mutation

		[[nodiscard]] c::Bool append(c::C8 ch, c::Error *e_rr = nullptr) noexcept {
			return c::CharString_append(&str, ch, alloc, e_rr);
		}

		[[nodiscard]] c::Bool append(const StringView &o, c::Error *e_rr = nullptr) noexcept {
			const c::CharString &h = o.handle();
			return c::CharString_appendString(&str, &h, alloc, e_rr);
		}

		[[nodiscard]] c::Bool insert(c::U64 i, c::C8 ch, c::Error *e_rr = nullptr) noexcept {
			return c::CharString_insert(&str, ch, i, alloc, e_rr);
		}

		[[nodiscard]] c::Bool insert(c::U64 i, const StringView &o, c::Error *e_rr = nullptr) noexcept {
			const c::CharString &h = o.handle();
			return c::CharString_insertString(&str, &h, i, alloc, e_rr);
		}

		[[nodiscard]] c::Bool prepend(c::C8 ch, c::Error *e_rr = nullptr) noexcept { return insert(0, ch, e_rr); }
		[[nodiscard]] c::Bool prepend(const StringView &o, c::Error *e_rr = nullptr) noexcept { return insert(0, o, e_rr); }

		[[nodiscard]] c::Bool resize(c::U64 len, c::C8 fill = ' ', c::Error *e_rr = nullptr) noexcept {
			return c::CharString_resize(&str, len, fill, alloc, e_rr);
		}

		[[nodiscard]] c::Bool reserve(c::U64 len, c::Error *e_rr = nullptr) noexcept {
			return c::CharString_reserve(&str, len, alloc, e_rr);
		}

		[[nodiscard]] c::Bool popEnd(c::U64 count = 1, c::Error *e_rr = nullptr) noexcept {
			return c::CharString_popEndCount(&str, count, e_rr);
		}

		[[nodiscard]] c::Bool popFront(c::U64 count = 1, c::Error *e_rr = nullptr) noexcept {
			return c::CharString_popFrontCount(&str, count, e_rr);
		}

		[[nodiscard]] c::Bool toLower() noexcept { return c::CharString_toLower(&str); }
		[[nodiscard]] c::Bool toUpper() noexcept { return c::CharString_toUpper(&str); }

		void clear() noexcept { c::CharString_clear(&str); }

		//Read-only surface comes from StringView; implicit so a String can be passed to anything taking one

		operator StringView() const noexcept { return StringView(str); }
		[[nodiscard]] StringView view() const noexcept { return StringView(str); }

		[[nodiscard]] c::U64 length() const noexcept { return c::CharString_length(str); }
		[[nodiscard]] bool empty() const noexcept { return !c::CharString_length(str); }
		[[nodiscard]] c::U64 capacity() const noexcept { return c::CharString_capacity(str); }

		[[nodiscard]] const c::C8 *data() const noexcept { return str.ptr; }
		[[nodiscard]] c::C8 *data() noexcept { return str.ptrNonConst; }

		[[nodiscard]] c::C8 operator[](c::U64 i) const noexcept { return str.ptr[i]; }
		[[nodiscard]] c::C8 &operator[](c::U64 i) noexcept { return str.ptrNonConst[i]; }

		[[nodiscard]] bool operator==(const StringView &o) const noexcept { return view() == o; }
		[[nodiscard]] bool operator!=(const StringView &o) const noexcept { return view() != o; }

		//Interop with C: view or transfer ownership of the underlying CharString

		[[nodiscard]] c::CharString &handle() noexcept { return str; }
		[[nodiscard]] const c::CharString &handle() const noexcept { return str; }

		//Caller takes ownership; must free with the same allocator
		[[nodiscard]] c::CharString steal() noexcept {
			c::CharString tmp = str;
			str = c::CharString{};
			return tmp;
		}

		//Take ownership of a CharString allocated with this wrapper's allocator
		void adopt(c::CharString &s) noexcept {
			release();
			str = s;
			s = c::CharString{};
		}
	};

	//Fixed capacity string living entirely in the object; never allocates and so never fails on memory.
	//N matches the C stack string sizes (SHORTSTRING_LEN / LONGSTRING_LEN), and the buffer is layout
	//compatible with them, so buffer() can be handed to any C function taking a ShortString / LongString.
	//Capacity is N - 1 usable characters; the null terminator is always kept.

	template<c::U64 N>
	class SmallStringN {

		static_assert(N >= 2, "oxc::SmallStringN needs room for at least one character and a null terminator");

		c::C8 buf[N];
		c::U64 len;

	public:

		SmallStringN() noexcept : buf{}, len(0) {}

		SmallStringN(const StringView &from) noexcept : buf{}, len(0) { (void) assign(from); }

		[[nodiscard]] static constexpr c::U64 capacity() noexcept { return N - 1; }

		[[nodiscard]] c::U64 length() const noexcept { return len; }
		[[nodiscard]] bool empty() const noexcept { return !len; }

		//False (and unchanged) when it wouldn't fit; there's no allocator to grow into.

		[[nodiscard]] bool assign(const StringView &from) noexcept {

			const c::U64 n = from.length();

			if(n > capacity())
				return false;

			for(c::U64 i = 0; i < n; ++i)
				buf[i] = from[i];

			buf[n] = '\0';
			len = n;
			return true;
		}

		[[nodiscard]] bool append(const StringView &from) noexcept {

			const c::U64 n = from.length();

			if(len + n > capacity())
				return false;

			for(c::U64 i = 0; i < n; ++i)
				buf[len + i] = from[i];

			len += n;
			buf[len] = '\0';
			return true;
		}

		[[nodiscard]] bool append(c::C8 ch) noexcept {

			if(len + 1 > capacity())
				return false;

			buf[len++] = ch;
			buf[len] = '\0';
			return true;
		}

		void clear() noexcept { len = 0; buf[0] = '\0'; }

		[[nodiscard]] const c::C8 *data() const noexcept { return buf; }
		[[nodiscard]] c::C8 *data() noexcept { return buf; }

		[[nodiscard]] c::C8 operator[](c::U64 i) const noexcept { return buf[i]; }
		[[nodiscard]] c::C8 &operator[](c::U64 i) noexcept { return buf[i]; }

		operator StringView() const noexcept { return StringView(buf, len, true); }
		[[nodiscard]] StringView view() const noexcept { return StringView(buf, len, true); }

		[[nodiscard]] bool operator==(const StringView &o) const noexcept { return view() == o; }
		[[nodiscard]] bool operator!=(const StringView &o) const noexcept { return view() != o; }

		//Interop: the raw C8[N], layout compatible with ShortString / LongString

		[[nodiscard]] c::C8 (&buffer() noexcept)[N] { return buf; }
		[[nodiscard]] const c::C8 (&buffer() const noexcept)[N] { return buf; }

		//A mutable CharString ref over the buffer, for C functions that write in place
		[[nodiscard]] c::CharString handle() noexcept { return c::CharString_createRefSized(buf, len, true); }
		[[nodiscard]] c::CharString handle() const noexcept { return c::CharString_createRefSizedConst(buf, len, true); }
	};

	//The two sizes the C API already has stack strings for
	using SmallString = SmallStringN<SHORTSTRING_LEN>;
	using LongString  = SmallStringN<LONGSTRING_LEN>;
}
