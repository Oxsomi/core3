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
	String workingDirectory;		//Contains a trailing slash to make file stuff easier

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

impl void Platform_cleanupExt();

//Called on setup. Don't use this over Platform_instance.workingDirectory
//
impl Error Platform_initWorkingDirectory(String *result);

void Platform_cleanup();

user_impl extern int Program_run();
user_impl extern void Program_exit();
