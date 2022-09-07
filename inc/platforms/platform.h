#pragma once
#include "types/types.h"
#include "types/allocator.h"
#include "platforms/window.h"

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
	struct String *cmdArgs;

	struct Allocator alloc;
	struct WindowManager windowManager;

	void *data;
};

extern struct Platform Platform_instance;

struct Error Platform_create(
	int cmdArgc, const c8 *cmdArgs[], 
	void *data,
	FreeFunc free, AllocFunc alloc, void *allocator
);

void Program_cleanup();

extern int Program_run();
extern void Program_exit();

//Errors with stacktraces

struct Error Error_traced(enum GenericError err, u32 subId, u32 paramId, u32 paramSubId, u64 paramValue0, u64 paramValue1);