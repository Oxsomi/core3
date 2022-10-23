#include "platforms/platform.h"
#include "platforms/log.h"
#include "types/error.h"
#include "types/buffer.h"

struct Platform Platform_instance = { 0 };

struct Error Platform_create(
	int cmdArgc, const C8 *cmdArgs[], 
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

	struct Allocator allocatorStruct = (struct Allocator) {
		.free = free,
		.alloc = alloc,
		.ptr = allocator
	};

	struct String *cmdArgsSized = NULL;
	struct StringList sl = (struct StringList){ 0 };

	if(cmdArgc > 1) {

		err = StringList_create(cmdArgc - 1, allocatorStruct, &sl);

		if (err.genericError) {
			WindowManager_freeSelf(&Platform_instance.windowManager);
			return err;
		}

		//If we're passed invalid cmdArg this could be a problem
		//But that'd happen anyways

		for(int i = 1; i < cmdArgc; ++i)
			cmdArgsSized[i - 1] = String_createRefUnsafeConst(cmdArgs[i]);
	}

	Platform_instance =	(struct Platform) {
		.platformType = _PLATFORM_TYPE,
		.args = sl,
		.windowManager = Platform_instance.windowManager,		//This one is initialized already
		.data = data,
		.alloc = allocatorStruct
	};

	return Error_none();
}

void Program_cleanup() {

	if(Platform_instance.platformType == Platform_Uninitialized)
		return;

	WindowManager_freeSelf(&Platform_instance.windowManager);
	StringList_free(&Platform_instance.args, Platform_instance.alloc);

	Platform_instance =	(struct Platform) { 0 };
}

struct Error Error_traced(enum GenericError err, U32 subId, U32 paramId, U32 paramSubId, U64 paramValue0, U64 paramValue1) {
	struct Error res = Error_base(err, subId, paramId, paramSubId, paramValue0, paramValue1);
	Log_captureStackTrace(res.stackTrace, ERROR_STACKTRACE, 0);
	return res;
}
