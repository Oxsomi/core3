#pragma once
#include "types/string.h"
#include "platforms/window_manager.h"

typedef enum EPlatform {
	EPlatform_Uninitialized,
	EPlatform_Windows,
	EPlatform_Linux,
	EPlatform_Android,
	EPlatform_Web
} EPlatform;

typedef struct Platform {

	EPlatform platformType;

	StringList args;

	Allocator alloc;
	WindowManager windowManager;

	void *data;

} Platform;

extern Platform Platform_instance;

Error Platform_create(
	int cmdArgc, const C8 *cmdArgs[], 
	void *data,
	FreeFunc free, AllocFunc alloc, void *allocator
);

impl void Program_cleanupExt();
void Program_cleanup();

user_impl extern int Program_run();
user_impl extern void Program_exit();
