/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "platforms/platform.h"
#include "platforms/osx/objective_c.h"
#include "platforms/ext/stringx.h"
#include "types/error.h"

static const C8 *EObjCClass_names[] = {
	"NSString",
	"NSAutoreleasePool",
	"NSObject",
	"NSApplication",
	"NSWindow"
};

static const C8 *EObjCFunc_names[] = {
	"stringWithUTF8String:",
	"setTitle:",
	"init",
	"applicationDidFinishLaunching:",
	"sharedApplication",
	"alloc",
	"setDelegate:",
	"run",
	"toggleFullScreen:"
};

typedef id (*ObjCRetId)(id, SEL);
typedef void (*ObjCRetVoid)(id, SEL);
typedef id (*ObjCVoidPtrRetId)(const void*, SEL, const void*);
typedef id (*ObjCRectRetId)(id, SEL, NSRect);
typedef id (*ObjCI32x2BoolRetId)(id, SEL, NSRect, I32, I32, Bool);

id ObjC_sendId(id a, SEL b) {
	return ((ObjCRetId)objc_msgSend)(a, b);
}

void ObjC_send(id a, SEL b) {
	return ((ObjCRetVoid)objc_msgSend)(a, b);
}

id ObjC_sendVoidPtr(const void *a, SEL b, const void *c) {
	return ((ObjCVoidPtrRetId)objc_msgSend)(a, b, c);
}

id ObjC_sendRect(id a, SEL b, NSRect c) {
	return ((ObjCRectRetId)objc_msgSend)(a, b, c);
}

id ObjC_sendWindowInit(id a, SEL b, NSRect c, I32 d, I32 e, Bool f) {
	return ((ObjCI32x2BoolRetId)objc_msgSend)(a, b, c, d, e, f);
}

Class clsNSString() { return EObjCClass_obj[EObjCClass_NSString]; }
Class clsNSAutoreleasePool() { return EObjCClass_obj[EObjCClass_NSAutoreleasePool]; }
Class clsNSObject() { return EObjCClass_obj[EObjCClass_NSObject]; }
Class clsNSApplication() { return EObjCClass_obj[EObjCClass_NSApplication]; }
Class clsNSWindow() { return EObjCClass_obj[EObjCClass_NSWindow]; }

SEL selUTF8String() { return EObjCFunc_obj[EObjCFunc_StringWithUTF8String]; }
SEL selSetTitle() { return EObjCFunc_obj[EObjCFunc_SetTitle]; }
SEL selInit() { return EObjCFunc_obj[EObjCFunc_Init]; }
SEL selApplicationDidFinishLaunching() { return EObjCFunc_obj[EObjCFunc_ApplicationDidFinishLaunching]; }
SEL selSharedApplication() { return EObjCFunc_obj[EObjCFunc_SharedApplication]; }
SEL selAlloc() { return EObjCFunc_obj[EObjCFunc_Alloc]; }
SEL selSetDelegate() { return EObjCFunc_obj[EObjCFunc_SetDelegate]; }
SEL selRun() { return EObjCFunc_obj[EObjCFunc_Run]; }
SEL selToggleFullScreen() { return EObjCFunc_obj[EObjCFunc_ToggleFullScreen]; }

Error ObjC_wrapString(CharString original, CharString *copy, id *wrapped) {

	if(!copy || !wrapped)
	return Error_nullPointer(!copy ? 1 : 2, "ObjC_wrapString()::copy and wrapped are required");

	Error err = Error_none();

	const C8 *ptr = original.ptr;

	if(!CharString_isNullTerminated(original)) {
		gotoIfError(clean, CharString_createCopyx(original, copy));
		ptr = copy->ptr;
	}

	*wrapped = ObjC_sendVoidPtr(clsNSString(), selUTF8String(), ptr);

	if(!*wrapped)
		gotoIfError(clean, Error_invalidState(0, "ObjC_wrapString() failed to create NSString"));

clean:

	if(err.genericError)
		CharString_freex(copy);

	return err;
}
