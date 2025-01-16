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

#include "platforms/keyboard.h"
#include "platforms/input_device.h"
#include "types/base/error.h"

#define KEY(name)																						\
	if ((err = InputDevice_createButton(*result, EKey_##name, "EKey_" #name, &res)).genericError) {		\
		InputDevice_free(result);																		\
		return err;																						\
	}

Error Keyboard_create(Keyboard *result) {

	Error err = InputDevice_create(EKey_Count, 0, EInputDeviceType_Keyboard, result);

	if(err.genericError)
		return err;

	InputHandle res = 0;

	KEY(0); KEY(1); KEY(2); KEY(3); KEY(4); KEY(5); KEY(6); KEY(7); KEY(8); KEY(9);

	KEY(A); KEY(B); KEY(C); KEY(D); KEY(E); KEY(F); KEY(G); KEY(H); KEY(I); KEY(J);
	KEY(K); KEY(L); KEY(M); KEY(N); KEY(O); KEY(P); KEY(Q); KEY(R); KEY(S); KEY(T);
	KEY(U); KEY(V); KEY(W); KEY(X); KEY(Y); KEY(Z);

	KEY(Backspace);		KEY(Space);			KEY(Tab);

	KEY(LShift);		KEY(LCtrl);			KEY(LAlt);			KEY(LMenu);
	KEY(RShift);		KEY(RCtrl);			KEY(RAlt);			KEY(RMenu);

	KEY(Pause);			KEY(Caps);			KEY(Escape);		KEY(PageUp);
	KEY(PageDown);		KEY(End);			KEY(Home);			KEY(PrintScreen);
	KEY(Insert);		KEY(Enter);			KEY(Delete);		KEY(NumLock);		KEY(ScrollLock);

	KEY(Back);			KEY(Forward);
	KEY(Sleep);			KEY(Refresh);		KEY(Search);
	KEY(Mute);			KEY(VolumeDown);	KEY(VolumeUp);		KEY(Skip);
	KEY(Previous);		KEY(Clear);			KEY(Help);

	KEY(Left);			KEY(Up);			KEY(Right);			KEY(Down);

	KEY(Numpad0);
	KEY(Numpad1);		KEY(Numpad2);		KEY(Numpad3);
	KEY(Numpad4);		KEY(Numpad5);		KEY(Numpad6);
	KEY(Numpad7);		KEY(Numpad8);		KEY(Numpad9);

	KEY(NumpadMul);		KEY(NumpadAdd);		KEY(NumpadDot);
	KEY(NumpadDiv);		KEY(NumpadSub);

	KEY(F1);			KEY(F2);			KEY(F3);			KEY(F4);			KEY(F5);
	KEY(F6);			KEY(F7);			KEY(F8);			KEY(F9);			KEY(F10);
	KEY(F11);			KEY(F12);

	KEY(Bar);			KEY(Options);

	KEY(Equals);		KEY(Comma);			KEY(Minus);			KEY(Period);
	KEY(Slash);			KEY(Backtick);		KEY(Semicolon);		KEY(LBracket);		KEY(RBracket);
	KEY(Backslash);		KEY(Quote);

	return Error_none();
}
