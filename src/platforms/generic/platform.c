#include "platforms/platform.h"
#include "platforms/log.h"
#include "types/error.h"

struct Platform Platform_instance = { 0 };

struct Error Platform_create(
	int cmdArgc, const c8 *cmdArgs[], 
	void *data, 
	FreeFunc free, AllocFunc alloc, void *allocator
) {

	if(Platform_instance.platformType != Platform_Uninitialized)
		return Error_base(GenericError_InvalidOperation, 0, 0, 0, Platform_instance.platformType, 0);

	Platform_instance =	(struct Platform){
		.platformType = _PLATFORM_TYPE,
		.cmdArgc = cmdArgc,
		.cmdArgs = cmdArgs,
		.data = data,
		.alloc = (struct Allocator) {
			.free = free,
			.alloc = alloc,
			.ptr = allocator
		}
	};

	return Error_none();
}

struct Error Error_traced(enum GenericError err, u32 subId, u32 paramId, u32 paramSubId, u64 paramValue0, u64 paramValue1) {
	struct Error res = Error_base(err, subId, paramId, paramSubId, paramValue0, paramValue1);
	Log_captureStackTrace(res.stackTrace, ERROR_STACKTRACE, 0);
	return res;
}