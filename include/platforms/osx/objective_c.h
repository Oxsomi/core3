/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
*
*  This program is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program. If not, see https://github.com/Oxsomi/core3/blob/main/LICENSE.
*  Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.
*  To prevent this a separate license will have to be requested at contact@osomi.net for a premium;
*  This is called dual licensing.
*/

#pragma once
#include "types/base/types.h"

//Implementation of https://github.com/CodaFi/C-Macs/blob/master/CMacs/CMacsTypes.h

#include <objc/message.h>
#include <objc/runtime.h>

//Binding classes

typedef enum EObjCClass {
	EObjCClass_NSString,
	EObjCClass_NSAutoreleasePool,
	EObjCClass_NSObject,
	EObjCClass_NSApplication,
	EObjCClass_NSWindow,
	EObjCClass_Count
} EObjCClass;

extern const C8 *EObjCClass_names[(int)EObjCClass_Count];
extern Class EObjCClass_obj[(int)EObjCClass_Count];

Class clsNSString();
Class clsNSAutoreleasePool();
Class clsNSObject();
Class clsNSApplication();
Class clsNSWindow();

//Binding functions

typedef enum EObjCFunc {
	EObjCFunc_StringWithUTF8String,
	EObjCFunc_SetTitle,
	EObjCFunc_Init,
	EObjCFunc_ApplicationDidFinishLaunching,
	EObjCFunc_SharedApplication,
	EObjCFunc_Alloc,
	EObjCFunc_SetDelegate,
	EObjCFunc_Run,
	EObjCFunc_ToggleFullScreen,
	EObjCFunc_Count
} EObjCFunc;

extern const C8 *EObjCFunc_names[(int)EObjCFunc_Count];
extern SEL EObjCFunc_obj[(int)EObjCFunc_Count];

SEL selUTF8String();
SEL selSetTitle();
SEL selInit();
SEL selApplicationDidFinishLaunching();
SEL selSharedApplication();
SEL selAlloc();
SEL selSetDelegate();
SEL selRun();
SEL selToggleFullScreen();

//Binding structs

typedef struct NSPoint { F64 x, y; } NSPoint;
typedef struct NSSize { F64 width, height; } NSSize;
typedef struct NSRect { NSPoint origin; NSSize size; } NSRect;

//Binding enums

//https://developer.apple.com/documentation/appkit/nswindowstylemask
typedef enum ENSWindowMask {
	ENSWindowMask_Borderless				= 0,
	ENSWindowMask_Titled					= 1 << 0,
	ENSWindowMask_Closable					= 1 << 1,
	ENSWindowMask_Miniaturizable			= 1 << 2,
	ENSWindowMask_Resizable					= 1 << 3,
	ENSWindowMask_FullScreen				= 1 << 14,
	ENSWindowMask_FullSizeContentView		= 1 << 15
} ENSWindowMask;

//Binding messages between C and ObjC

id ObjC_sendId(id a, SEL b);
void ObjC_send(id a, SEL b);
id ObjC_sendVoidPtr(const void *a, SEL b, const void *c);
id ObjC_sendRect(id a, SEL b, NSRect c);
id ObjC_sendWindowInit(id a, SEL b, NSRect c, I32 d, I32 e, Bool f);

Error ObjC_wrapString(CharString original, CharString *copy, id *wrapped);
