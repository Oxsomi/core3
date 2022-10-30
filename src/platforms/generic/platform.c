#include "platforms/platform.h"
#include "platforms/log.h"
#include "types/error.h"
#include "types/buffer.h"

Platform EPlatform_instance = { 0 };

Error EPlatform_create(
	int cmdArgc, const C8 *cmdArgs[], 
	void *data, 
	FreeFunc free, AllocFunc alloc, void *allocator
) {

	if(EPlatform_instance.platformType != EPlatform_Uninitialized)
		return Error_base(EGenericError_InvalidOperation, 0, 0, 0, EPlatform_instance.platformType, 0);

	if(!cmdArgc)
		return Error_base(EGenericError_InvalidParameter, 0, 1, 0, 0, 0);

	EPlatform_instance =	(Platform) {
		.platformType = _PLATFORM_TYPE,
		.data = data,
		.alloc = (Allocator) {
			.free = free,
			.alloc = alloc,
			.ptr = allocator
		}
	};

	Error err = WindowManager_create(&EPlatform_instance.windowManager);

	if (err.genericError) {
		EPlatform_instance =	(Platform) { 0 };
		return err;
	}

	StringList sl = (StringList){ 0 };

	if(cmdArgc > 1) {

		err = StringList_create(cmdArgc - 1, EPlatform_instance.alloc, &sl);

		if (err.genericError) {
			WindowManager_free(&EPlatform_instance.windowManager);
			EPlatform_instance =	(Platform) { 0 };
			return err;
		}

		//If we're passed invalid cmdArg this could be a problem
		//But that'd happen anyways

		for(int i = 1; i < cmdArgc; ++i)
			sl.ptr[i - 1] = String_createRefUnsafeConst(cmdArgs[i]);
	}

	EPlatform_instance.args = sl;

	return Error_none();
}

void Program_cleanup() {

	if(EPlatform_instance.platformType == EPlatform_Uninitialized)
		return;

	WindowManager_free(&EPlatform_instance.windowManager);
	StringList_free(&EPlatform_instance.args, EPlatform_instance.alloc);

	EPlatform_instance =	(Platform) { 0 };
}
