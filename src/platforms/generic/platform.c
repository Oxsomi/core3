#include "platforms/platform.h"
#include "platforms/log.h"
#include "types/error.h"
#include "types/buffer.h"

Platform Platform_instance = { 0 };

Error Platform_create(
	int cmdArgc, const C8 *cmdArgs[], 
	void *data, 
	FreeFunc free, AllocFunc alloc, void *allocator
) {

	if(Platform_instance.platformType != Platform_Uninitialized)
		return Error_base(GenericError_InvalidOperation, 0, 0, 0, Platform_instance.platformType, 0);

	if(!cmdArgc)
		return Error_base(GenericError_InvalidParameter, 0, 1, 0, 0, 0);

	Platform_instance =	(Platform) {
		.platformType = _PLATFORM_TYPE,
		.data = data,
		.alloc = (Allocator) {
			.free = free,
			.alloc = alloc,
			.ptr = allocator
		}
	};

	Error err = WindowManager_create(&Platform_instance.windowManager);

	if (err.genericError) {
		Platform_instance =	(Platform) { 0 };
		return err;
	}

	StringList sl = (StringList){ 0 };

	if(cmdArgc > 1) {

		err = StringList_create(cmdArgc - 1, Platform_instance.alloc, &sl);

		if (err.genericError) {
			WindowManager_free(&Platform_instance.windowManager);
			Platform_instance =	(Platform) { 0 };
			return err;
		}

		//If we're passed invalid cmdArg this could be a problem
		//But that'd happen anyways

		for(int i = 1; i < cmdArgc; ++i)
			sl.ptr[i - 1] = String_createRefUnsafeConst(cmdArgs[i]);
	}

	Platform_instance.args = sl;

	return Error_none();
}

void Program_cleanup() {

	if(Platform_instance.platformType == Platform_Uninitialized)
		return;

	WindowManager_free(&Platform_instance.windowManager);
	StringList_free(&Platform_instance.args, Platform_instance.alloc);

	Platform_instance =	(Platform) { 0 };
}
