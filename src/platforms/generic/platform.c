#include "platforms/platform.h"
#include "platforms/log.h"
#include "platforms/ext/stringx.h"
#include "types/error.h"

Platform Platform_instance = { 0 };

Error Platform_create(
	int cmdArgc, const C8 *cmdArgs[], 
	void *data, 
	FreeFunc free, AllocFunc alloc, void *allocator
) {

	if(Platform_instance.platformType != EPlatform_Uninitialized)
		return Error_invalidOperation(0);

	if(!cmdArgc)
		return Error_invalidParameter(0, 0, 0);

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

		err = StringList_createx(cmdArgc - 1, &sl);

		if (err.genericError) {
			WindowManager_free(&Platform_instance.windowManager);
			Platform_instance =	(Platform) { 0 };
			return err;
		}

		//If we're passed invalid cmdArg this could be a problem
		//But that'd happen anyways

		for(int i = 1; i < cmdArgc; ++i)
			sl.ptr[i - 1] = String_createConstRefUnsafe(cmdArgs[i]);
	}

	Platform_instance.args = sl;
	

	#if _WORKING_DIR

		//Grab current working directory.
		//This is only useful if you're building a tool that has to be run from cmd for example.

		if((err = Platform_initWorkingDirectory(&Platform_instance.workingDirectory)).genericError)
			goto cleanup;

	#else

		//Grab app directory of where the exe is installed

		String appDir;
		if ((err = String_createCopyx(String_createConstRefUnsafe(cmdArgs[0]), &appDir)).genericError)
			goto cleanup;

		String_replaceAll(&appDir, '\\', '/', EStringCase_Sensitive);

		U64 loc = String_findLast(appDir, '/', EStringCase_Sensitive);
		String basePath;

		if (loc == appDir.len)
			basePath = String_createConstRef(appDir.ptr, appDir.len);
	
		else String_cut(appDir, 0, loc + 1, &basePath);

		String workDir;

		if ((err = String_createCopyx(basePath, &workDir)).genericError)
			goto cleanup;

		String_freex(&appDir);
		Platform_instance.workingDirectory = workDir;

	#endif

	return Error_none();

cleanup:

	StringList_freex(&Platform_instance.args);
	WindowManager_free(&Platform_instance.windowManager);
	Platform_instance =	(Platform) { 0 };
	return err;
}

void Platform_cleanup() {

	if(Platform_instance.platformType == EPlatform_Uninitialized)
		return;

	String_freex(&Platform_instance.workingDirectory);
	WindowManager_free(&Platform_instance.windowManager);
	StringList_freex(&Platform_instance.args);

	Platform_instance =	(Platform) { 0 };
}
