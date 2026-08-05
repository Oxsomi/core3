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

//types/container/test/test_types_container_hpp_wrappers.cpp
//
//Exercises oxc::StringView / String / SmallString, oxc::RefPtr, oxc::Log and every generated wrapper
// (oxc::Buffer, oxc::Time, oxc::C8, oxc::ETextureFormat).
//The suite's leak detector is what verifies the RAII half, so allocations here are left to a destructor on purpose.
//
//The generated wrappers forward one-for-one to C that's already covered elsewhere,
// so what's checked is that they forward to the right call, that RAII actually releases,
// and that a namespace-shaped wrapper really is a namespace.

#include "types/container/string.hpp"
#include "types/container/ref_ptr.hpp"
#include "types/container/log.hpp"
#include "types/container/buffer.hpp"
#include "types/container/texture_format.hpp"
#include "types/base/time.hpp"
#include "types/base/c8.hpp"
#include "types/base/error.hpp"
#include "types/math/flp.hpp"

//The C test framework carries an extern "C" guard, so include it inside oxc::c (after the wrappers,
//which already pulled its C-header deps into oxc::c). See test_types_container_hpp.cpp for why.
namespace oxc { namespace c {
	#include "types/test/test.h"
}}

namespace {

	//A payload for RefPtr; the free callback bumps a counter so the test can prove dec actually ran.
	struct Payload {
		oxc::c::U64 value;
		oxc::c::U64 *freed;
	};

	void Payload_free(void *ptr, const oxc::c::Allocator*) {
		Payload *p = (Payload*) ptr;
		if(p->freed)
			++*p->freed;
	}
}

extern "C" void Test_hppWrappers(oxc::c::Test *t) {

	using namespace oxc;

	const c::Allocator &alloc = *t->alloc;
	c::Error *e_rr = &t->err;

	Test_setModule(t, "hppWrappers");

	//========================= oxc::StringView =========================

	{
		StringView v = "hello world";

		Test_assert(t, "StringView: length", v.length() == 11);
		Test_assert(t, "StringView: not empty", !v.empty());
		Test_assert(t, "StringView: indexing", v[0] == 'h' && v[10] == 'd');

		Test_assert(t, "StringView: equals", v == StringView("hello world"));
		Test_assert(t, "StringView: not equals", v != StringView("Hello world"));
		Test_assert(t, "StringView: equalsInsensitive", v.equalsInsensitive(StringView("HELLO WORLD")));

		Test_assert(t, "StringView: startsWith", v.startsWith(StringView("hello")));
		Test_assert(t, "StringView: endsWith", v.endsWith(StringView("world")));
		Test_assert(t, "StringView: startsWith char", v.startsWith('h'));
		Test_assert(t, "StringView: endsWith char", v.endsWith('d'));
		Test_assert(t, "StringView: contains", v.contains(StringView("lo wo")));
		Test_assert(t, "StringView: doesn't contain", !v.contains(StringView("xyz")));

		Test_assert(t, "StringView: findFirst char", v.findFirst('o') == 4);
		Test_assert(t, "StringView: findLast char", v.findLast('o') == 7);
		Test_assert(t, "StringView: findFirst missing", v.findFirst('q') == c::U64_MAX);

		//Sub-views are refs into the same memory, so no allocation happens here
		Test_assert(t, "StringView: substr", v.substr(0, 5) == StringView("hello"));
		Test_assert(t, "StringView: trim", StringView("  padded  ").trim() == StringView("padded"));

		//An empty view is safe to touch
		StringView empty;
		Test_assert(t, "StringView: default is empty", empty.empty() && !empty.length());

		//A sized view over a non-null-terminated middle of a buffer
		const c::C8 raw[] = "abcdef";
		Test_assert(t, "StringView: sized", StringView(raw + 1, 3) == StringView("bcd"));
	}

	//========================= oxc::String =========================

	{
		String s(alloc);

		Test_assert(t, "String: starts empty", s.empty() && !s.length());
		Test_assert(t, "String: createCopy", s.createCopy(StringView("abc"), e_rr) && s.length() == 3);
		Test_assert(t, "String: converts to view", s == StringView("abc"));

		Test_assert(t, "String: append char", s.append('d', e_rr) && s == StringView("abcd"));
		Test_assert(t, "String: append string", s.append(StringView("ef"), e_rr) && s == StringView("abcdef"));
		Test_assert(t, "String: prepend", s.prepend(StringView("__"), e_rr) && s == StringView("__abcdef"));
		Test_assert(t, "String: insert", s.insert(2, '!', e_rr) && s == StringView("__!abcdef"));

		Test_assert(t, "String: popFront", s.popFront(3, e_rr) && s == StringView("abcdef"));
		Test_assert(t, "String: popEnd", s.popEnd(2, e_rr) && s == StringView("abcd"));

		Test_assert(t, "String: toUpper", s.toUpper() && s == StringView("ABCD"));
		Test_assert(t, "String: toLower", s.toLower() && s == StringView("abcd"));

		Test_assert(t, "String: mutable indexing", (s[0] = 'z', s == StringView("zbcd")));

		//createCopy on a live string must free the old contents first (leak check proves it)
		Test_assert(t, "String: recreate", s.createCopy(StringView("fresh"), e_rr) && s == StringView("fresh"));

		Test_assert(t, "String: format", s.format(e_rr, "%s-%u", "id", 42u) && s == StringView("id-42"));

		Test_assert(t, "String: repeated", s.createRepeated('x', 4, e_rr) && s == StringView("xxxx"));

		//Move must leave the source empty and not double free
		String moved = std::move(s);
		Test_assert(t, "String: move transfers", moved == StringView("xxxx"));
		Test_assert(t, "String: moved-from is empty", s.empty());

		String target(alloc);
		Test_assert(t, "String: move-assign target", target.createCopy(StringView("old"), e_rr));
		target = std::move(moved);
		Test_assert(t, "String: move-assign transfers", target == StringView("xxxx") && moved.empty());

		//steal hands ownership back to C; free it the way a C caller would
		c::CharString raw = target.steal();
		Test_assert(t, "String: steal empties wrapper", target.empty());
		Test_assert(t, "String: steal keeps content", c::CharString_length(raw) == 4);
		c::CharString_free(&raw, &alloc);

		//adopt takes it back
		c::CharString adopted{};
		Test_assert(t, "String: make for adopt", c::CharString_createCopy(
			StringView("adopted").handle(), &alloc, &adopted, e_rr
		));
		target.adopt(adopted);
		Test_assert(t, "String: adopt owns", target == StringView("adopted") && !adopted.ptr);
	}

	//========================= oxc::SmallString =========================

	{
		SmallString s;

		Test_assert(t, "SmallString: starts empty", s.empty());
		Test_assert(t, "SmallString: capacity", SmallString::capacity() == SHORTSTRING_LEN - 1);

		Test_assert(t, "SmallString: assign", s.assign(StringView("stack")) && s == StringView("stack"));
		Test_assert(t, "SmallString: append", s.append(StringView("ed")) && s == StringView("stacked"));
		Test_assert(t, "SmallString: append char", s.append('!') && s == StringView("stacked!"));
		Test_assert(t, "SmallString: length", s.length() == 8);

		//Over capacity must fail and leave the value untouched, since there's nothing to grow into
		SmallString small;
		Test_assert(t, "SmallString: assign fits", small.assign(StringView("0123456789012345678901234567890")));
		Test_assert(t, "SmallString: assign too long rejected", !small.assign(
			StringView("01234567890123456789012345678901234567890")
		));
		Test_assert(t, "SmallString: rejected assign kept old value", small.length() == 31);
		Test_assert(t, "SmallString: append past capacity rejected", !small.append('x'));

		s.clear();
		Test_assert(t, "SmallString: clear", s.empty() && s == StringView(""));

		//The buffer is layout compatible with the C ShortString
		LongString l;
		Test_assert(t, "LongString: capacity", LongString::capacity() == LONGSTRING_LEN - 1);
		Test_assert(t, "LongString: assign", l.assign(StringView("longer stack string")));
		Test_assert(t, "LongString: C ref sees same content", c::CharString_length(
			c::CharString_createRefLongStringConst(l.buffer())
		) == 19);
	}

	//========================= oxc::RefPtr =========================

	{
		c::U64 freed = 0;

		const c::RefPtrType type = {
			(c::TypeId) 0xC0FFEE, (c::U32) sizeof(Payload), &alloc, Payload_free
		};

		{
			RefPtr<Payload> p;
			Test_assert(t, "RefPtr: default is null", !p && !p.valid());
			Test_assert(t, "RefPtr: create", RefPtr<Payload>::create(&type, p, e_rr) && p);

			p->value = 7;
			p->freed = &freed;
			Test_assert(t, "RefPtr: operator->", p->value == 7);
			Test_assert(t, "RefPtr: operator*", (*p).value == 7);
			Test_assert(t, "RefPtr: typeId", p.typeId() == (c::TypeId) 0xC0FFEE);

			//A copy shares the object, so dropping it must not free
			{
				RefPtr<Payload> copy = p;
				Test_assert(t, "RefPtr: copy shares object", copy.data() == p.data());
				Test_assert(t, "RefPtr: copies compare equal", copy == p);
			}
			Test_assert(t, "RefPtr: copy destruction didn't free", freed == 0);

			//A move doesn't touch the count
			RefPtr<Payload> moved = std::move(p);
			Test_assert(t, "RefPtr: move transfers", moved && !p);
			Test_assert(t, "RefPtr: move didn't free", freed == 0);

			//Self-assignment must not drop the last reference.
			//Aliased through a pointer so the compiler doesn't fold it away as an obvious self-assign.
			RefPtr<Payload> *alias = &moved;
			moved = *alias;
			Test_assert(t, "RefPtr: self-assign survives", moved && freed == 0);

			//Last reference goes here
		}

		Test_assert(t, "RefPtr: freed once at last dec", freed == 1);
	}

	//========================= oxc::Log =========================

	//Nothing to assert on beyond "it compiles and doesn't crash"; the C side has its own coverage.
	{
		Log::debugLn(alloc, "hpp log: %" PRIu64 " and %.*s", (c::U64) 1, 3, "str");
		Log::warn(alloc, ELogOptions_NewLine, "hpp log: warn");
		Log::logLn(alloc, c::ELogLevel_Debug, StringView("hpp log: preformatted view"));

		String msg(alloc);
		Test_assert(t, "Log: build a message", msg.format(e_rr, "hpp log: built %u", 3u));
		Log::logLn(alloc, c::ELogLevel_Debug, msg);
		Test_assert(t, "Log: ran without crashing", true);
	}

	//========================= generated oxc::Buffer =========================

	{
		Buffer b(alloc);

		Test_assert(t, "Buffer: createEmptyBytes", b.createEmptyBytes(64, e_rr));
		Test_assert(t, "Buffer: length", c::Buffer_length(b.handle()) == 64);
		Test_assert(t, "Buffer: resize", b.resize(128, true, true, e_rr));
		Test_assert(t, "Buffer: resized length", c::Buffer_length(b.handle()) == 128);

		//A create on a live buffer releases the old allocation first (leak check proves it)
		Test_assert(t, "Buffer: recreate", b.createUninitializedBytes(32, e_rr));
		Test_assert(t, "Buffer: recreated length", c::Buffer_length(b.handle()) == 32);

		c::Buffer raw = b.steal();
		Test_assert(t, "Buffer: steal empties wrapper", !c::Buffer_length(b.handle()));
		c::Buffer_free(&raw, &alloc);
	}

	//========================= generated namespaces =========================

	//These are namespaces rather than classes; the point of the check is that they're callable without an
	//object at all, and that they forward to the C.

	{
		const c::Ns before = Time::now();
		Test_assert(t, "Time: now is non-zero", before != 0);
		Test_assert(t, "Time: dns", Time::dns(100, 400) == 300);
		Test_assert(t, "Time: elapsed is monotonic-ish", Time::now() >= before);

		c::Date date{};
		Test_assert(t, "Time: getDate round trips", (
			Time::getDate(before, &date, false) && date.year >= 1970
		));

		Test_assert(t, "C8: isUpperCase", C8::isUpperCase('A') && !C8::isUpperCase('a'));
		Test_assert(t, "C8: isLowerCase", C8::isLowerCase('z') && !C8::isLowerCase('Z'));
		Test_assert(t, "C8: isDec", C8::isDec('7') && !C8::isDec('x'));
		Test_assert(t, "C8: toLower", C8::toLower('Q') == 'q');
		Test_assert(t, "C8: toUpper", C8::toUpper('q') == 'Q');

		Test_assert(t, "ETextureFormat: getBits", ETextureFormat::getBits(c::ETextureFormat_RGBA8) == 32);
		Test_assert(t, "ETextureFormat: alpha bits", ETextureFormat::getAlphaBits(c::ETextureFormat_RGBA8) == 8);
		Test_assert(t, "ETextureFormat: not compressed", !ETextureFormat::getIsCompressed(c::ETextureFormat_RGBA8));

		//EFloatType: packed enum queries, so the widths have to come back out intact
		Test_assert(t, "EFloatType: F32 exponent bits", EFloatType::exponentBits(c::EFloatType_F32) == 8);
		Test_assert(t, "EFloatType: F32 mantissa bits", EFloatType::mantissaBits(c::EFloatType_F32) == 23);
		Test_assert(t, "EFloatType: F32 bytes", EFloatType::bytes(c::EFloatType_F32) == 4);
		Test_assert(t, "EFloatType: F16 bytes", EFloatType::bytes(c::EFloatType_F16) == 2);

		//Error: factories return a value, so the fields are checkable straight away
		{
			const c::Error err = Error::nullPointer(3, "test");
			Test_assert(t, "Error: genericError", err.genericError == c::EGenericError_NullPointer);
			Test_assert(t, "Error: paramId", err.paramId == 3);

			const c::Error none = Error::none();
			Test_assert(t, "Error: none is None", none.genericError == c::EGenericError_None);
		}

		//Same answer as calling the C directly, which is the whole contract
		Test_assert(t, "generated wrappers forward verbatim", (
			ETextureFormat::getBits(c::ETextureFormat_RGBA8) == c::ETextureFormat_getBits(c::ETextureFormat_RGBA8) &&
			C8::toLower('Q') == c::C8_toLower('Q') &&
			Time::dns(1, 2) == c::Time_dns(1, 2) &&
			EFloatType::bytes(c::EFloatType_F64) == c::EFloatType_bytes(c::EFloatType_F64)
		));
	}
}
