#pragma once
#include "types/types.h"

enum EPlatform {
	Platform_Uninitialized,
	Platform_Windows,
	Platform_Linux,
	Platform_Android,
	Platform_Web
};

struct Platform {

	enum EPlatform platformType;

	int cmdArgc;
	const c8* const *cmdArgs;

	void *data;
};

extern struct Platform Platform_instance;

void Platform_init(int cmdArgc, const c8 *cmdArgs[], void *data);

extern int Program_run();