/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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

typedef id (*ObjCRetId)(id, SEL);
typedef void (*ObjCRetVoid)(id, SEL);
typedef void (*ObjCVoidPtrRetVoid)(id, SEL, void*);
typedef id (*ObjCRectRetId)(id, SEL, NSRect);
typedef id (*ObjCI32x2BoolRetId)(id, SEL, NSRect, I32, I32, Bool);

id ObjC_sendId(id a, SEL b) {
    return ((ObjCRetId)objc_msgSend)(a, b);    
}

void ObjC_send(id a, SEL b) {
    return ((ObjCRetVoid)objc_msgSend)(a, b);    
}

void ObjC_sendVoidPtr(id a, SEL b, void *c) {
    return ((ObjCVoidPtrRetVoid)objc_msgSend)(a, b, c);    
}

id ObjC_sendRect(id a, SEL b, NSRect c) {
    return ((ObjCRectRetId)objc_msgSend)(a, b, c);    
}

id ObjC_sendWindowInit(id a, SEL b, NSRect c, I32 d, I32 e, Bool f) {
    return ((ObjCI32x2BoolRetId)objc_msgSend)(a, b, c, d, e, f);    
}
