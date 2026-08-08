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

//types/container/log.hpp

#pragma once
#include <type_traits>

//Pre-include system headers used by the C headers below at global scope;
//they must not be pulled in for the first time inside a namespace.

#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <inttypes.h>
#include <assert.h>

#include "types/container/string.hpp"

//Thin front for the C logging functions.
//
//  oxc::Log::debugLn(alloc, "loaded %" PRIu64 " entries", count);
//  oxc::Log::error(alloc, oxc::ELogOptions_Default, view);
//  oxc::Log::print(alloc, err);
//
//The format overloads forward straight into the C variadic functions, so the same rule applies as in
//log.h: NEVER pass user generated content as the format string, use "%.*s" with an explicit length.
//To make the common mistake impossible the argument pack is restricted to trivially copyable types
//(passing a class type through ... is undefined), and there's a StringView overload for the case where
//you just want to print a string that isn't a literal.

namespace oxc {

	namespace c {
		#include "types/container/log.h"
	}

	//Re-exported so callers don't have to reach into oxc::c for the two enums they'll actually name

	using ELogLevel = c::ELogLevel;
	using ELogOptions = c::ELogOptions;

	constexpr ELogOptions ELogOptions_None = c::ELogOptions_None;
	constexpr ELogOptions ELogOptions_NewLine = c::ELogOptions_NewLine;
	constexpr ELogOptions ELogOptions_Default = c::ELogOptions_Default;

	class Log {

		//A class type passed through ... is undefined behaviour; catch it at compile time instead of
		//discovering it as garbage in the output.
		template<typename... Args>
		static constexpr void assertPrintable() {
			static_assert(
				(std::is_trivially_copyable<Args>::value && ...),
				"oxc::Log format arguments must be trivially copyable; pass a StringView's length + data "
				"with \"%.*s\" rather than a String/StringView itself"
			);
		}

	public:

		Log() = delete;

		//Preformatted string; nothing is parsed, so this one is safe for arbitrary content.

		static void log(
			const c::Allocator &alloc, ELogLevel lvl, const StringView &str, ELogOptions opt = ELogOptions_Default
		) noexcept {
			c::Log_log(&alloc, lvl, opt, &str.handle());
		}

		static void logLn(const c::Allocator &alloc, ELogLevel lvl, const StringView &str) noexcept {
			log(alloc, lvl, str, ELogOptions_NewLine);
		}

		//Format variants. Args go straight into the C varargs call.

		template<typename... Args>
		static void logFormat(
			const c::Allocator &alloc, ELogLevel lvl, ELogOptions opt, const c::C8 *fmt, Args... args
		) noexcept {
			assertPrintable<Args...>();
			c::Log_logFormat(&alloc, lvl, opt, fmt, args...);
		}

		template<typename... Args>
		static void debug(const c::Allocator &alloc, ELogOptions opt, const c::C8 *fmt, Args... args) noexcept {
			assertPrintable<Args...>();
			c::Log_debug(&alloc, opt, fmt, args...);
		}

		template<typename... Args>
		static void performance(const c::Allocator &alloc, ELogOptions opt, const c::C8 *fmt, Args... args) noexcept {
			assertPrintable<Args...>();
			c::Log_performance(&alloc, opt, fmt, args...);
		}

		template<typename... Args>
		static void warn(const c::Allocator &alloc, ELogOptions opt, const c::C8 *fmt, Args... args) noexcept {
			assertPrintable<Args...>();
			c::Log_warn(&alloc, opt, fmt, args...);
		}

		template<typename... Args>
		static void error(const c::Allocator &alloc, ELogOptions opt, const c::C8 *fmt, Args... args) noexcept {
			assertPrintable<Args...>();
			c::Log_error(&alloc, opt, fmt, args...);
		}

		//Ln variants, matching the Log_debugLn / Log_warnLn / ... macros in log.h

		template<typename... Args>
		static void debugLn(const c::Allocator &alloc, const c::C8 *fmt, Args... args) noexcept {
			debug(alloc, ELogOptions_NewLine, fmt, args...);
		}

		template<typename... Args>
		static void performanceLn(const c::Allocator &alloc, const c::C8 *fmt, Args... args) noexcept {
			performance(alloc, ELogOptions_NewLine, fmt, args...);
		}

		template<typename... Args>
		static void warnLn(const c::Allocator &alloc, const c::C8 *fmt, Args... args) noexcept {
			warn(alloc, ELogOptions_NewLine, fmt, args...);
		}

		template<typename... Args>
		static void errorLn(const c::Allocator &alloc, const c::C8 *fmt, Args... args) noexcept {
			error(alloc, ELogOptions_NewLine, fmt, args...);
		}

		//Errors and stack traces

		static void print(
			const c::Allocator &alloc,
			const c::Error &err,
			ELogLevel lvl = c::ELogLevel_Error,
			ELogOptions opt = ELogOptions_Default
		) noexcept {
			c::Error_print(&alloc, &err, lvl, opt);
		}

		static void printStackTrace(
			const c::Allocator &alloc,
			c::U8 skip = 1,
			ELogLevel lvl = c::ELogLevel_Debug,
			ELogOptions opt = ELogOptions_Default
		) noexcept {
			c::Log_printStackTrace(&alloc, skip, lvl, opt);
		}
	};
}
