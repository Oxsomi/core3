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

#include "platforms/keyboard.h"
#include "platforms/input_device.h"
#include "types/error.h"
#include "types/string.h"

#define _key(name)																	\
	if ((err = InputDevice_createButton(											\
		*result, EKey_##name, CharString_createConstRefCStr("EKey_" #name), &res	\
	)).genericError) {																\
		InputDevice_free(result);													\
		return err;																	\
	}

Error Keyboard_create(Keyboard *result) {

	Error err = InputDevice_create(EKey_Count, 0, EInputDeviceType_Keyboard, result);

	if(err.genericError)
		return err;

	InputHandle res = 0;

	_key(1);	_key(2);	_key(3);	_key(4);	_key(5); 
	_key(6);	_key(7);	_key(8);	_key(9);	_key(0); 

	_key(A);	_key(B);	_key(C);	_key(D);	_key(E);
	_key(F);	_key(G);	_key(H);	_key(I);	_key(J);
	_key(K);	_key(L);	_key(M);	_key(N);	_key(O);
	_key(P);	_key(Q);	_key(R);	_key(S);	_key(T);
	_key(U);	_key(V);	_key(W);	_key(X);	_key(Y);
	_key(Z);

	_key(Equals);			_key(Comma);		_key(Minus);
	_key(Period);			_key(Slash);		_key(Acute);
	_key(Semicolon);		_key(LBracket);		_key(RBracket);
	_key(Backslash);		_key(Quote);

	_key(F1);	_key(F2);	_key(F3);	_key(F4);		_key(F5);	_key(F6);
	_key(F7);	_key(F8);	_key(F9);	_key(F10);		_key(F11);	_key(F12);
	_key(F13);	_key(F14);	_key(F15);	_key(F16);		_key(F17);	_key(F18);
	_key(F19);	_key(F20);	_key(F21);	_key(F22);		_key(F23);	_key(F24);

	_key(Backspace);	_key(Shift);		_key(Ctrl);			_key(Alt);
	_key(Pause);		_key(Caps);			_key(Escape);		_key(PageUp);
	_key(PageDown);		_key(End);			_key(Home);			_key(Select);
	_key(Print);		_key(Execute);		_key(PrintScreen);	_key(Insert);
	_key(ScrollLock);	_key(Apps);			_key(Back);			_key(Forward);
	_key(Sleep);		_key(Refresh);		_key(Stop);			_key(Search);
	_key(Favorites);	_key(Start);		_key(Mute);			_key(VolumeDown);
	_key(VolumeUp);		_key(Skip);			_key(Previous);		_key(Clear);
	_key(Zoom);
	_key(Delete);		_key(Help);			_key(NumLock);

	_key(Space);
	_key(Tab);
	_key(Enter);

	_key(Left);
	_key(Up);
	_key(Right);
	_key(Down);

	_key(Numpad1);		_key(Numpad2);		_key(Numpad3);
	_key(Numpad4);		_key(Numpad5);		_key(Numpad6);
	_key(Numpad7);		_key(Numpad8);		_key(Numpad9);
	_key(Numpad0);		_key(NumpadMul);	_key(NumpadAdd);
	_key(NumpadDec);	_key(NumpadDiv);	_key(NumpadSub);

	return Error_none();
}
