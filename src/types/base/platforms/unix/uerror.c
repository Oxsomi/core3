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

//types/base/platforms/unix/uerror.c

#include "types/base/platform_types.h"
#include "types/base/error.h"
#include "types/base/atomic.h"

#if _PLATFORM_TYPE == PLATFORM_WEB

	#include <emscripten/emscripten.h>

	//emscripten's sysroot has no execinfo.h and no per frame unwinding API: the whole callstack arrives as
	// one newline separated string from emscripten_get_callstack, formatted as
	// "    at <symbol> (<file>:<line>:<col>)" per line (see libstack_trace.js).
	//A frame is therefore a pointer INTO that text at the start of its line, and the printer reads to the
	// next newline (see the web branch in types/container/platforms/unix/ulog.c).
	//That keeps the void* array's contract, one entry per frame and null terminated, without owning a
	// string per frame or writing terminators into the buffer.
	//
	//The text lives in a global ring because Error is a POD returned by value with no destructor, so a per
	// Error allocation would never be freed, and because a trace outlives the thread that captured it as
	// soon as a JobQueue worker hands its Error to whoever waits on it.
	//Slots are claimed atomically rather than under a lock, so an error path never blocks and cannot
	// deadlock against a lock the failing code already holds.
	//
	//The ring recycles constantly, the tracked allocator alone capturing a trace per allocation and
	// printing them at shutdown, so no slot count keeps those alive.
	//stackTrace[0] holds the generation that owned the slot at capture and the frames start at
	// stackTrace[1], which lets a reader tell a live trace from a recycled one and say so rather than
	// print an unrelated stack.
	//It goes in the array rather than in Error because most callers pass a bare void* array and have no
	// Error at all.

	//Slots are sized for the traces that are realistically in flight at once, which is a debug build's
	// error paths rather than anything unbounded.
	//CHARS is not a tuning knob: emscripten's lines run about 100 characters
	// ("    at <module>.<symbol> (<module>-<hash>:wasm-function[N]:0xADDR)"), so STACKTRACE_SIZE frames
	// need roughly 3200 and anything smaller truncates deep stacks instead of saving memory.

	#ifdef NDEBUG
		#define WEBERROR_TRACE_SLOTS 8
	#else
		#define WEBERROR_TRACE_SLOTS 64
	#endif

	#define WEBERROR_TRACE_CHARS 4096      //Power of two slots: the claim masks instead of dividing

	static C8 WebError_traces[WEBERROR_TRACE_SLOTS][WEBERROR_TRACE_CHARS];
	static AtomicI64 WebError_slotGeneration[WEBERROR_TRACE_SLOTS];
	static AtomicI64 WebError_nextTrace;

	//A generation of 0 never validates, so a zeroed array reads as expired rather than as slot 0.

	Bool Error_webStackTraceIsLive(const void *const *stackTrace) {

		if(!stackTrace || !stackTrace[0])
			return false;

		//Owned copies outlive the ring by construction, so there is no generation to check.

		if(stackTrace[0] == ERROR_WEB_STACKTRACE_OWNED)
			return true;

		const U64 generation = (U64) stackTrace[0];
		const U64 slot = generation & (WEBERROR_TRACE_SLOTS - 1);

		return (U64) AtomicI64_load(&WebError_slotGeneration[slot]) == generation;
	}

	void Error_captureStackTrace(void **stack, U8 stackSize, U8 skip) {

		//Two entries minimum: the generation and a null terminator.

		if(!stack || stackSize < 2)
			return;

		stack[0] = NULL;

		//Generation starts at 1 so it is never 0, which Error_webStackTraceIsLive treats as "no trace".
		//Wraps on I64 overflow, which the mask handles because the cast is unsigned.

		const U64 generation = (U64) AtomicI64_inc(&WebError_nextTrace);
		const U64 slot = generation & (WEBERROR_TRACE_SLOTS - 1);
		C8 *trace = WebError_traces[slot];

		AtomicI64_store(&WebError_slotGeneration[slot], (I64) generation);

		//EM_LOG_C_STACK is a no-op now ("no longer has any effect", libstack_trace.js), and one of the two
		// stack flags has to be set or the result is empty. NO_PATHS keeps the basename instead of a URL.

		if(emscripten_get_callstack(EM_LOG_JS_STACK | EM_LOG_NO_PATHS, trace, WEBERROR_TRACE_CHARS) <= 0)
			return;

		//One pointer per line, skipping the frames the caller asked to drop.

		U64 written = 1;        //stack[0] is the generation, frames follow it

		for(const C8 *line = trace; *line && written < stackSize; ) {

			const C8 *next = line;

			while(*next && *next != '\n')
				++next;

			if(skip)
				--skip;

			else stack[written++] = (void*) line;

			line = *next ? next + 1 : next;
		}

		if(written < stackSize)
			stack[written] = NULL;

		//Published last: until this is set the array reads as expired, so a reader never follows frame
		// pointers into a slot that is still being written.

		stack[0] = (void*) generation;
	}

#elif _PLATFORM_TYPE != PLATFORM_ANDROID

	#include <execinfo.h>

	void Error_captureStackTrace(void **stack, U8 stackSize, U8 skipTmp) {

		if(!stack || !stackSize)
			return;

		void *tmpStack[128];
		I32 count = backtrace(tmpStack, 128);

		if(count <= 0 || ((U64)stackSize + skipTmp + 1) > 128) {
			stack[0] = NULL;
			return;
		}

		U64 j = (U64)skipTmp + 1;

		for(U64 i = j; i < 128 && i < (U32) count && i < j + stackSize; ++i)
			stack[i - j] = tmpStack[i];

		if(count - j < stackSize)
			stack[count - j] = NULL;
	}

#endif
