#include "platforms/platform.h"
#include "types/assert.h"

struct Platform Platform_instance = { 0 };

void Platform_init(int cmdArgc, const c8 *cmdArgs[], void *data) {

	ocAssert(
		"Platform type was already initialized", 
		Platform_instance.platformType == Platform_Uninitialized
	);

	Platform_instance.cmdArgc = cmdArgc;
	Platform_instance.cmdArgs = cmdArgs;
	Platform_instance.data = data;
	Platform_instance.platformType = _PLATFORM_TYPE;
}