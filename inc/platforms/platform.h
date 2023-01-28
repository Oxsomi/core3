#pragma once
#include "types/string.h"
#include "types/allocator.h"
#include "platforms/window_manager.h"

typedef enum EPlatform {

	EPlatform_Uninitialized,
	EPlatform_Windows,
	EPlatform_Linux,
	EPlatform_Android,
	EPlatform_Web,
	EPlatform_iOS,
	EPlatform_OSX

} EPlatform;

typedef struct Platform {

	EPlatform platformType;

	StringList args;
	String workingDirectory;		//Contains a trailing slash to make file stuff easier

	Allocator alloc;
	WindowManager windowManager;

	void *data, *dataExt;

} Platform;

extern Platform Platform_instance;

Error Platform_create(
	int cmdArgc, const C8 *cmdArgs[], 
	void *data,
	FreeFunc free, AllocFunc alloc, void *allocator
);

impl void Platform_cleanupExt(Platform *platform);
impl Error Platform_initExt(Platform *platform, String currentAppDir);

void Platform_cleanup();

user_impl extern int Program_run();
user_impl extern void Program_exit();
