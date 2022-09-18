#include "platforms/platform.h"
#include "platforms/log.h"
#include "types/error.h"
#include "types/bit.h"

struct Platform Platform_instance = { 0 };

struct Error Platform_create(
	int cmdArgc, const c8 *cmdArgs[], 
	void *data, 
	FreeFunc free, AllocFunc alloc, void *allocator
) {

	if(Platform_instance.platformType != Platform_Uninitialized)
		return Error_base(GenericError_InvalidOperation, 0, 0, 0, Platform_instance.platformType, 0);

	if(!cmdArgc)
		return Error_base(GenericError_InvalidParameter, 0, 1, 0, 0, 0);

	struct Error err = WindowManager_createSelf(&Platform_instance.windowManager);

	if (err.genericError)
		return err;

	struct String *cmdArgsSized = NULL;

	if(cmdArgc > 1) {

		struct Buffer buf = (struct Buffer){ 0 };
		struct Error err = alloc(allocator, sizeof(struct String) * (cmdArgc - 1), &buf);

		if (err.genericError) {
			WindowManager_freeSelf(&Platform_instance.windowManager);
			return err;
		}

		cmdArgsSized = (struct String*) buf.ptr;

		//If we're passed invalid cmdArg this could be a problem
		//But that'd happen anyways

		for(int i = 1; i < cmdArgc; ++i)
			cmdArgsSized[i - 1] = String_createRefUnsafeConst(cmdArgs[i]);
	}

	Platform_instance =	(struct Platform) {
		.platformType = _PLATFORM_TYPE,
		.cmdArgc = cmdArgc,
		.cmdArgs = cmdArgsSized,
		.windowManager = Platform_instance.windowManager,		//This one is initialized already
		.data = data,
		.alloc = (struct Allocator) {
			.free = free,
			.alloc = alloc,
			.ptr = allocator
		}
	};

	return Error_none();
}

void Program_cleanup() {

	if(Platform_instance.platformType == Platform_Uninitialized)
		return;

	WindowManager_freeSelf(&Platform_instance.windowManager);

	if(Platform_instance.cmdArgc)
		Platform_instance.alloc.free(
			Platform_instance.alloc.ptr, 
			Bit_createRef(Platform_instance.cmdArgs, Platform_instance.cmdArgc * sizeof(struct String))
		);

	Platform_instance =	(struct Platform) { 0 };
}

struct Error Error_traced(enum GenericError err, u32 subId, u32 paramId, u32 paramSubId, u64 paramValue0, u64 paramValue1) {
	struct Error res = Error_base(err, subId, paramId, paramSubId, paramValue0, paramValue1);
	Log_captureStackTrace(res.stackTrace, ERROR_STACKTRACE, 0);
	return res;
}