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

//platforms/unix/uplatform.c

#include "platforms/platform.h"
#include "types/container/string.h"
#include "types/base/string_read_helper.h"
#include "types/container/file_base.h"
#include "types/base/atomic.h"

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <sys/mman.h>
#include <string.h>

#if _PLATFORM_TYPE == PLATFORM_OSX || _PLATFORM_TYPE == PLATFORM_IOS
	#include <mach/mach.h>
#endif

void *Platform_allocPages(U64 size) {
	void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	return ptr == MAP_FAILED ? NULL : ptr;
}

void Platform_freePages(void *ptr, U64 size) {
	if(ptr)
		munmap(ptr, size);
}

//OxC3's widest type is I32x4, which is alignas(16) on every backend including the scalar one
// (see types/math/vec4_{sse,neon,wasm,none}.inc.h), so anything the platform allocator hands out has
// to be able to hold one.
//malloc only promises alignof(max_align_t), which is 16 on the x64/arm64 ABIs but only 8 on wasm,
// where long double is 8-aligned.
//So web has to ask for the alignment explicitly, otherwise every heap I32x4 (matrices, vector lists,
// the AES target/AAD buffers) is under-aligned.
//free() accepts aligned_alloc memory (C11 7.22.3, POSIX), so the free path is unchanged.

#if _PLATFORM_TYPE == PLATFORM_WEB

	void *Platform_allocate(void *allocator, U64 length) {
		(void)allocator;
		//aligned_alloc wants a size that's a multiple of the alignment (C11 7.22.3.1)
		return aligned_alloc(16, length ? (length + 15) &~ (U64)15 : 16);
	}

#else
	void *Platform_allocate(void *allocator, U64 length) { (void)allocator; return malloc(length); }
#endif

void Platform_free(void *allocator, void *ptr, U64 length) { (void) allocator; (void)length; free(ptr); }

impl Bool Platform_initUnixExt(Error *e_rr);
impl void Platform_cleanupUnixExt();

void Platform_cleanupExt() {
	Platform_cleanupUnixExt();
}

#if _PLATFORM_TYPE != PLATFORM_ANDROID

	void *Platform_getDataImpl(void *ptr) { (void) ptr; return NULL; }
	
	//No on screen keyboard here, so there's nothing a hardware one would have to be weighed against.

	Bool Platform_hasPhysicalKeyboard() { return true; }

	Bool Platform_setKeyboardVisibleForced(Bool isVisible, Bool force) {
		(void) isVisible;
		(void) force;
		return true;
	}

#endif

#if _PLATFORM_TYPE == PLATFORM_WEB && !defined(__EMSCRIPTEN_PTHREADS__)
	//Wasm built without -pthread: sysconf would report navigator.hardwareConcurrency, but no thread
	// can actually start.
	//1 keeps JobQueue in its inline mode (job_queue.h).
	//emcc defines __EMSCRIPTEN_PTHREADS__ for a -pthread build, where sysconf is correct and used.
	U64 Platform_getThreads() { return 1; }
#else
	U64 Platform_getThreads() { return sysconf(_SC_NPROCESSORS_ONLN); }
#endif

#if _PLATFORM_TYPE == PLATFORM_OSX || _PLATFORM_TYPE == PLATFORM_IOS
	#include <sys/sysctl.h>
#endif

U64 Platform_getPhysicalRAM() {

	#if _PLATFORM_TYPE == PLATFORM_OSX || _PLATFORM_TYPE == PLATFORM_IOS

		U64 mem = 0;
		size_t len = sizeof(mem);
		return sysctlbyname("hw.memsize", &mem, &len, NULL, 0) == 0 ? mem : 0;

	#else        //Linux / Android

		long pages    = sysconf(_SC_PHYS_PAGES);
		long pageSize = sysconf(_SC_PAGE_SIZE);
		return pages > 0 && pageSize > 0 ? (U64) pages * (U64) pageSize : 0;

	#endif
}

U64 Platform_getAvailableRAM() {

	#if _PLATFORM_TYPE == PLATFORM_OSX || _PLATFORM_TYPE == PLATFORM_IOS

		const mach_port_t host = mach_host_self();

		vm_size_t pageSize = 0;
		if(host_page_size(host, &pageSize) != KERN_SUCCESS)
			return 0;

		vm_statistics64_data_t vmStat;
		mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;

		if(host_statistics64(host, HOST_VM_INFO64, (host_info64_t) &vmStat, &count) != KERN_SUCCESS)
			return 0;

		//"Available" = free + reclaimable (inactive + purgeable) pages
		return (U64) (vmStat.free_count + vmStat.inactive_count + vmStat.purgeable_count) * (U64) pageSize;

	#else        //Linux / Android

		long pages    = sysconf(_SC_AVPHYS_PAGES);
		long pageSize = sysconf(_SC_PAGE_SIZE);
		return pages > 0 && pageSize > 0 ? (U64) pages * (U64) pageSize : 0;

	#endif
}

#if _PLATFORM_TYPE == PLATFORM_LINUX || _PLATFORM_TYPE == PLATFORM_ANDROID

	//A small sysfs file whole, null terminated, or false when it isn't there

	static Bool Platform_readSysfs(const C8 *path, C8 *text, U64 cap) {

		const int file = open(path, O_RDONLY);

		if(file < 0)
			return false;

		const ssize_t got = read(file, text, cap - 1);
		close(file);

		if(got <= 0)
			return false;

		text[got] = 0;
		return true;
	}

	//A cpu list as sysfs writes one, "0-7,16-23"

	static Bool Platform_readCpuList(const C8 *path, ListU32 *out, const Allocator *alloc, Error *e_rr) {

		Bool s_uccess = true;
		C8 text[1024];

		if(!Platform_readSysfs(path, text, sizeof(text)))
			goto clean;

		for(const C8 *at = text; *at; ) {

			C8 *end = NULL;
			const unsigned long first = strtoul(at, &end, 10);

			if(end == at)
				break;

			unsigned long last = first;
			at = end;

			if(*at == '-') {
				last = strtoul(at + 1, &end, 10);
				at = end;
			}

			for(unsigned long cpu = first; cpu <= last; ++cpu)
				gotoIfError3(clean, ListU32_pushBack(out, (U32) cpu, alloc, e_rr));

			while(*at == ',' || *at == '\n' || *at == ' ')
				++at;
		}

	clean:
		return s_uccess;
	}

	//A file under one cpu's sysfs directory, such as "/cpu_capacity"

	static Bool Platform_readCpuSysfs(U64 cpu, const C8 *suffix, C8 *text, U64 cap) {

		static const C8 prefix[] = "/sys/devices/system/cpu/cpu";

		C8 path[sizeof(prefix) + 20 + 64], digits[20];
		U64 n = 0, d = 0, v = cpu;

		for(U64 i = 0; i + 1 < sizeof(prefix); ++i)
			path[n++] = prefix[i];

		do { digits[d++] = (C8) ('0' + v % 10); v /= 10; } while(v);

		while(d)
			path[n++] = digits[--d];

		for(U64 i = 0; suffix[i] && n + 1 < sizeof(path); ++i)
			path[n++] = suffix[i];

		path[n] = 0;
		return Platform_readSysfs(path, text, cap);
	}

	//Arm's big.LITTLE gives each cpu a capacity relative to the fastest (1024); a quarter below that counts as
	// efficiency, which puts a mid core with the little ones

	static Bool Platform_classesByCapacity(ListU32 *performance, ListU32 *efficiency, const Allocator *alloc, Error *e_rr) {

		Bool s_uccess = true;
		const U64 cpus = Platform_getThreads();
		U32 highest = 0, lowest = U32_MAX;

		for(U64 pass = 0; pass < 2; ++pass)
			for(U64 cpu = 0; cpu < cpus; ++cpu) {

				C8 text[32];

				if(!Platform_readCpuSysfs(cpu, "/cpu_capacity", text, sizeof(text)))
					goto clean;

				const U32 capacity = (U32) strtoul(text, NULL, 10);

				if(!pass) {
					highest = capacity > highest ? capacity : highest;
					lowest = capacity < lowest ? capacity : lowest;
					continue;
				}

				if(lowest >= highest)
					goto clean;

				ListU32 *list = (U64) capacity * 4 >= (U64) highest * 3 ? performance : efficiency;
				gotoIfError3(clean, ListU32_pushBack(list, (U32) cpu, alloc, e_rr));
			}

	clean:
		return s_uccess;
	}

	//SMT siblings list the same cpus; the first of them counts the core, and a cpu without the file is one

	static U32 Platform_countCores(ListU32 cpus) {

		U32 cores = 0;

		for(U64 i = 0; i < cpus.length; ++i) {
			C8 text[64];
			cores +=
				!Platform_readCpuSysfs(cpus.ptr[i], "/topology/thread_siblings_list", text, sizeof(text)) ||
				strtoul(text, NULL, 10) == cpus.ptr[i];
		}

		return cores;
	}

	//A hybrid CPU's cores by class; a CPU with one class leaves both empty

	static Bool Platform_detectCoreClasses(PlatformCPUInfo *out, const Allocator *alloc, Error *e_rr) {

		Bool s_uccess = true;
		ListU32 *performance = &out->performance.cpus, *efficiency = &out->efficiency.cpus;

		//Intel's hybrid parts have a PMU per core type, each listing its cpus

		gotoIfError3(clean, Platform_readCpuList("/sys/devices/cpu_core/cpus", performance, alloc, e_rr));
		gotoIfError3(clean, Platform_readCpuList("/sys/devices/cpu_atom/cpus", efficiency, alloc, e_rr));

		if(!performance->length || !efficiency->length) {
			ListU32_clear(performance, NULL);
			ListU32_clear(efficiency, NULL);
			gotoIfError3(clean, Platform_classesByCapacity(performance, efficiency, alloc, e_rr));
		}

		if(!performance->length || !efficiency->length) {
			ListU32_clear(performance, NULL);
			ListU32_clear(efficiency, NULL);
			goto clean;
		}

		out->performance.logicalCores = (U32) performance->length;
		out->efficiency.logicalCores = (U32) efficiency->length;
		out->performance.physicalCores = Platform_countCores(*performance);
		out->efficiency.physicalCores = Platform_countCores(*efficiency);

	clean:
		return s_uccess;
	}

#elif _PLATFORM_TYPE == PLATFORM_OSX || _PLATFORM_TYPE == PLATFORM_IOS

	static U32 Platform_sysctlU32(const C8 *name) {
		int v = 0;
		size_t size = sizeof(v);
		return sysctlbyname(name, &v, &size, NULL, 0) || v < 0 ? 0 : (U32) v;
	}

	//Apple's core classes are perf levels, 0 the fastest. A thread can't be placed, so only the counts are known.

	static Bool Platform_detectCoreClasses(PlatformCPUInfo *out, const Allocator *alloc, Error *e_rr) {

		(void) alloc;
		(void) e_rr;

		const U32 levels = Platform_sysctlU32("hw.nperflevels");

		for(U32 i = 0; levels > 1 && i < levels && i < 10; ++i) {

			C8 physical[] = "hw.perflevel0.physicalcpu", logical[] = "hw.perflevel0.logicalcpu";
			physical[12] = logical[12] = (C8) ('0' + i);

			PlatformCPUClass *c = i ? &out->efficiency : &out->performance;
			c->physicalCores += Platform_sysctlU32(physical);
			c->logicalCores += Platform_sysctlU32(logical);
		}

		return true;
	}

#else

	static Bool Platform_detectCoreClasses(PlatformCPUInfo *out, const Allocator *alloc, Error *e_rr) {
		(void) out;
		(void) alloc;
		(void) e_rr;
		return true;
	}

#endif

Bool Platform_detectCPUInfo(PlatformCPUInfo *out, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	if(!out)
		retError(clean, Error_nullPointer(0, "Platform_detectCPUInfo()::out is required"));

	*out = (PlatformCPUInfo) { 0 };
	out->logicalCores = (U32) Platform_getThreads();

	//Vendor + brand via cpuid (x86 only); on ARM leave brand empty and mark the vendor generically.

	#if _ARCH == ARCH_X86_64

		U32 reg[4] = { 0 };
		Platform_getCPUId(0, reg);

		if(reg[1] == 0x756E6547 && reg[3] == 0x49656E69 && reg[2] == 0x6C65746E)        //"GenuineIntel"
			out->vendor = ECPUVendor_Intel;
		else if(reg[1] == 0x68747541 && reg[3] == 0x69746E65 && reg[2] == 0x444D4163)   //"AuthenticAMD"
			out->vendor = ECPUVendor_AMD;

		U32 brand[12] = { 0 };
		Platform_getCPUId((int) 0x80000002, &brand[0]);
		Platform_getCPUId((int) 0x80000003, &brand[4]);
		Platform_getCPUId((int) 0x80000004, &brand[8]);

		for(U64 i = 0; i < sizeof(brand) && i < sizeof(out->brand) - 1; ++i)
			out->brand[i] = ((const C8*) brand)[i];

	#elif _PLATFORM_TYPE == PLATFORM_OSX || _PLATFORM_TYPE == PLATFORM_IOS

		out->vendor = ECPUVendor_Apple;        //Apple silicon (or Apple-branded Intel Macs; still Apple here)

		size_t bsz = sizeof(out->brand);
		sysctlbyname("machdep.cpu.brand_string", out->brand, &bsz, NULL, 0);
		out->brand[sizeof(out->brand) - 1] = 0;

	#else        //Linux / Android on ARM: identify the implementer from /proc/cpuinfo

		out->vendor = ECPUVendor_Arm;

		//Read /proc/cpuinfo directly (raw open/read; the File API is sandboxed to the working dir and can't reach /proc).
		//The first core's "CPU implementer" line is well within the first read, so a single bounded read is enough.

		const int f = open("/proc/cpuinfo", O_RDONLY);

		if(f >= 0) {

			C8 buf[8192];
			const ssize_t n = read(f, buf, sizeof(buf) - 1);
			close(f);

			if(n > 0) {

				buf[n] = 0;

				//Can't be named "impl"; that's the implementation-dependent marker macro from types.h
				const CharString cpuinfo = CharString_createRefCStrConst(buf);
				const CharString implKey = CharString_createRefCStrConst("CPU implementer");
				const CharString hexKey = CharString_createRefCStrConst("0x");

				const U64 implOff = CharString_findFirstStringSensitive(&cpuinfo, &implKey, 0, 0);

				const U64 hexOff =
					implOff == U64_MAX ? U64_MAX : CharString_findFirstStringSensitive(&cpuinfo, &hexKey, implOff, 0);

				if(hexOff != U64_MAX)
					switch((unsigned) strtoul(buf + hexOff, NULL, 16)) {
						case 0x4e: out->vendor = ECPUVendor_Nvidia;   break;
						case 0x51: out->vendor = ECPUVendor_Qualcomm; break;
						case 0x53: out->vendor = ECPUVendor_Samsung;  break;
						case 0xc0: out->vendor = ECPUVendor_Ampere;   break;
						case 0x61: out->vendor = ECPUVendor_Apple;    break;
						default:   out->vendor = ECPUVendor_Arm;      break;
					}
			}
		}

	#endif

	//Without parsing /sys/devices/system/cpu topology we can't reliably separate SMT or P/E cores, so fall back
	// to logical == physical (accurate on the no-SMT ARM parts this branch mostly targets).
	//TODO: parse /sys topology for true physical core count, hybrid P/E split and NUMA node count.

	out->physicalCores = out->logicalCores;

	#ifdef _SC_LEVEL1_DCACHE_SIZE
		{ long v = sysconf(_SC_LEVEL1_DCACHE_SIZE); if(v > 0) out->l1DataCacheBytes = (U64) v; }
	#endif
	#ifdef _SC_LEVEL2_CACHE_SIZE
		{ long v = sysconf(_SC_LEVEL2_CACHE_SIZE); if(v > 0) out->l2CacheBytes = (U64) v; }
	#endif
	#ifdef _SC_LEVEL3_CACHE_SIZE
		{ long v = sysconf(_SC_LEVEL3_CACHE_SIZE); if(v > 0) out->l3CacheBytes = (U64) v; }
	#endif

	out->numaNodes = 1;

	gotoIfError3(clean, Platform_detectCoreClasses(out, alloc, e_rr));

clean:
	return s_uccess;
}

Bool Platform_initExt(Error *e_rr) {

	Bool s_uccess = true;

	//Get work directory; since Android doesn't have a working directory, it will work as following:
	//appDir = internal app dir
	//workDir = external app dir

	#if _PLATFORM_TYPE != PLATFORM_ANDROID

		C8 cwd[MAX_OXC_PATH + 1];
		if (!getcwd(cwd, sizeof(cwd)))
			retError(clean, Error_stderr(0, "Platform_initExt() getcwd failed"));

		gotoIfError3(clean, CharString_createCopy(
			CharString_createRefCStrConst(cwd), Platform_instance->alloc, &Platform_instance->workDirectory, e_rr
		));

		gotoIfError3(clean, CharString_append(&Platform_instance->workDirectory, '/', Platform_instance->alloc, e_rr));

	#endif

	//Initialize Linux/OSX dependent as well as app dir path

	gotoIfError3(clean, Platform_initUnixExt(e_rr));
	
	//Make default path

	Platform_instance->defaultDir = CharString_createRefStrConst(
		Platform_instance->useWorkingDir ? Platform_instance->workDirectory : Platform_instance->appDirectory
	);

clean:
	return s_uccess;
}

//getenv hands back the process's own storage, which the next setenv may free, so the value is copied out.
//Copying an empty string yields null, which is what makes an empty variable read as unset.

Bool Platform_getEnvExt(CharString name, const Allocator *alloc, CharString *result, Error *e_rr) {

	const C8 *value = getenv(name.ptr);

	if(!value)
		return true;

	return CharString_createCopy(CharString_createRefCStrConst(value), alloc, result, e_rr);
}
