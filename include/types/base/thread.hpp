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

#pragma once

namespace oxc {

	namespace c {
		#include "types/base/thread.h"
		#include "types/base/error.h"
	}

	class Thread {

		c::Thread *thread;
		const c::Allocator *alloc;

		void release() {

			if (thread) {
				Thread_wait(thread);
				Thread_free(alloc, &thread);
			}

			thread = nullptr;
		}

	public:

		Thread() noexcept : thread{ nullptr }, alloc(nullptr) {}
		~Thread() noexcept { release(); }

		Thread(const Thread&) = delete;
		Thread &operator=(const Thread&) = delete;
		
		Thread(Thread &&other) noexcept
				: thread(other.thread), alloc(other.alloc) {
				other.thread = nullptr;
		}

		Thread& operator=(Thread&& other) noexcept {
			release();
			thread = other.thread;
			alloc = other.alloc;
			other.thread = nullptr;
			return *this;
		}

		void join() { if(thread) Thread_wait(thread); }
		
		static c::Error init(Thread &thread, const c::Allocator *alloc, c::ThreadCallbackFunction callback, void *obj) noexcept {
			thread.release();
			thread.alloc = alloc;
			return Thread_create(alloc, callback, obj, &thread.thread);
		}

		static c::U64 getId() { return c::Thread_getId(); }
		static c::Bool sleep(c::Ns ns) { return c::Thread_sleep(ns); }
	};
}

