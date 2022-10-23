#pragma once
#include "types/string.h"
#include "platforms/window_manager.h"

enum EPlatform {
	Platform_Uninitialized,
	Platform_Windows,
	Platform_Linux,
	Platform_Android,
	Platform_Web
};

struct Platform {

	enum EPlatform platformType;

	struct StringList args;

	struct Allocator alloc;
	struct WindowManager windowManager;

	void *data;
};

extern struct Platform Platform_instance;

struct Error Platform_create(
	int cmdArgc, const C8 *cmdArgs[], 
	void *data,
	FreeFunc free, AllocFunc alloc, void *allocator
);

void Program_cleanup();

user_impl extern int Program_run();
user_impl extern void Program_exit();

//Errors with stacktraces

struct Error Error_traced(enum GenericError err, U32 subId, U32 paramId, U32 paramSubId, U64 paramValue0, U64 paramValue1);
