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

//platforms/platform.hpp
//
//C++ layer over the environment half of platforms/platform.h: settings an application reads from environment
// variables, typed to what the caller stores them in.
//
//Three levels, by how much the caller wants to handle:
//  get       the value as a String
//  read      a T or a list of T, failing through e_rr when the value is not one (out of T's range included)
//  readOr    a T, or the default when unset; a value that is set but malformed is LOGGED by name and the default
//            returned, because a typo in a setting would otherwise pass for the default without a trace
//
//Every reader leaves its output untouched when the variable is unset, so a caller stores its default first.
//The grammar is Platform_getEnv's (see platform.h): 0/1/true/false, decimal or 0x hex, and an empty variable is unset.

#pragma once

#include "types/container/string.hpp"
#include "types/container/log.hpp"

#include <type_traits>
#include <limits>

//Pre-include system headers used by the C headers below at global scope; see file.hpp for why <atomic> is here.

#include <atomic>
#include <stdalign.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

namespace oxc {

	namespace c {
		#include "platforms/platform.h"
		#include "types/base/constants.h"
	}

	namespace env {

		namespace detail {

			//One value of the grammar into T, with T's range checked; false leaves value untouched.

			template<typename T>
			c::Bool parse(const c::CharString &str, T &value) noexcept {

				if constexpr(std::is_same_v<T, bool>) {
					return c::Platform_parseEnvBool(str, &value);
				}

				else if constexpr(std::is_integral_v<T> && std::is_unsigned_v<T>) {

					c::U64 v = 0;

					if(!c::Platform_parseEnvU64(str, &v) || v > (c::U64) std::numeric_limits<T>::max())
						return false;

					value = (T) v;
					return true;
				}

				else if constexpr(std::is_integral_v<T>) {

					c::I64 v = 0;

					if(
						!c::Platform_parseEnvI64(str, &v) ||
						v < (c::I64) std::numeric_limits<T>::min() || v > (c::I64) std::numeric_limits<T>::max()
					)
						return false;

					value = (T) v;
					return true;
				}

				else {

					static_assert(std::is_floating_point_v<T>, "oxc::env reads bool, integers and floats");

					c::F64 v = 0;

					const c::F64 max = (c::F64) std::numeric_limits<T>::max();

					if(!c::Platform_parseEnvF64(str, &v) || v > max || v < -max)
						return false;

					value = (T) v;
					return true;
				}
			}

			template<typename T>
			constexpr const c::C8 *typeName() noexcept {
				if constexpr(std::is_same_v<T, bool>) return "0, 1, true or false";
				else if constexpr(std::is_integral_v<T> && std::is_unsigned_v<T>) return "an unsigned integer in range";
				else if constexpr(std::is_integral_v<T>) return "an integer in range";
				else return "a number in range";
			}

			//The settings form's one voice for a value it had to throw away.

			inline void logMalformed(const StringView &name, const c::Allocator &alloc, const c::C8 *wants) noexcept {

				c::CharString value = c::CharString_createNull();
				c::Platform_getEnv(name.handle(), &alloc, &value, nullptr);

				const c::CharString &n = name.handle();

				Log::errorLn(
					alloc, "Environment variable %.*s=%.*s isn't %s, keeping the default",
					(int) c::CharString_length(n), n.ptr, (int) c::CharString_length(value), value.ptr ? value.ptr : "", wants
				);

				c::CharString_free(&value, &alloc);
			}
		}

		//The value, or an empty out when unset. out is reset first.

		[[nodiscard]] inline c::Bool get(
			const StringView &name, String &out, const c::Allocator &alloc, c::Error *e_rr = nullptr
		) noexcept {
			out = String(alloc);
			return c::Platform_getEnv(name.handle(), &alloc, &out.handle(), e_rr);
		}

		//No e_rr: a variable that cannot be read reads as absent.

		[[nodiscard]] inline c::Bool has(const StringView &name, const c::Allocator &alloc) noexcept {
			c::Bool result = false;
			return c::Platform_hasEnv(name.handle(), &alloc, &result, nullptr) && result;
		}

		template<typename T>
		[[nodiscard]] c::Bool read(
			const StringView &name, T &value, const c::Allocator &alloc, c::Error *e_rr = nullptr
		) noexcept {

			String str(alloc);

			if(!get(name, str, alloc, e_rr))
				return false;

			if(!str.handle().ptr || detail::parse(str.handle(), value))
				return true;

			if(e_rr)
				*e_rr = c::Error_invalidParameter(0, 2, "oxc::env::read() value isn't a T in range");

			return false;
		}

		//Exactly count comma separated values, "x,y,z". A wrong count is as malformed as a wrong value, and either
		// leaves values untouched: every entry is validated before the first is written, so no count is too many.

		template<typename T>
		[[nodiscard]] c::Bool readList(
			const StringView &name, T *values, c::U64 count, const c::Allocator &alloc, c::Error *e_rr = nullptr
		) noexcept {

			String str(alloc);

			if(!get(name, str, alloc, e_rr))
				return false;

			if(str.empty())
				return true;

			const c::C8 *s = str.data();
			const c::U64 len = str.length();

			for(int write = 0; write < 2; ++write) {

				c::U64 n = 0, start = 0;
				c::Bool ok = true;

				for(c::U64 i = 0; i <= len && ok; ++i) {

					if(i < len && s[i] != ',')
						continue;

					T parsed{};
					const c::CharString entry = c::CharString_createRefSizedConst(s + start, i - start, false);

					ok = n < count && detail::parse(entry, write ? values[n] : parsed);
					++n;
					start = i + 1;
				}

				if(!ok || n != count) {

					if(e_rr)
						*e_rr = c::Error_invalidParameter(0, 2, "oxc::env::readList() value isn't count comma separated T");

					return false;
				}
			}

			return true;
		}

		//The settings form: the value, or def when unset or malformed, a malformed one logged by name.

		template<typename T>
		[[nodiscard]] T readOr(const StringView &name, T def, const c::Allocator &alloc) noexcept {

			T value = def;

			if(!read(name, value, alloc)) {
				detail::logMalformed(name, alloc, detail::typeName<T>());
				return def;
			}

			return value;
		}

		//The settings form of a list: values keep what they held unless the variable is set and well formed, and a
		// malformed one is logged. True when the variable supplied them.

		template<typename T>
		c::Bool readListOr(const StringView &name, T *values, c::U64 count, const c::Allocator &alloc) noexcept {

			if(!readList(name, values, count, alloc)) {
				detail::logMalformed(name, alloc, "the expected comma separated list");
				return false;
			}

			return has(name, alloc);
		}
	}
}
