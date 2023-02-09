/* MIT License
*   
*  Copyright (c) 2022 Oxsomi, Nielsbishere (Niels Brunekreef)
*  
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*  
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.
*  
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE. 
*/

#include "platforms/keyboard.h"
#include "platforms/input_device.h"
#include "types/error.h"
#include "types/string.h"

#define _key(name)																\
	if ((err = InputDevice_createButton(										\
		*result, EKey_##name, String_createConstRefUnsafe("EKey_" #name), &res	\
	)).genericError) {															\
		InputDevice_free(result);												\
		return err;																\
	}

Error EKeyboard_create(EKeyboard *result) {

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
