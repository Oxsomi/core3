#include "platforms/keyboard.h"
#include "platforms/input_device.h"
#include "types/error.h"

struct Error Keyboard_free(Keyboard *dev) {
	return InputDevice_free(dev);
}

#define _key(name, resolve, resolveAlt)																	\
	if ((err = InputDevice_createButton(																\
		*result, Key_##name, String_createRefUnsafeConst("Key_" #name), resolve, resolveAlt, &res		\
	)).genericError) {																					\
		Keyboard_free(result);																			\
		return err;																						\
	}

#define _specKey(name) _key(name, '\0', '\0')
#define _specKeyResolve(name, resolve) _key(name, resolve, '\0')

struct Error Keyboard_create(Keyboard *result) {

	struct Error err = InputDevice_create(Key_Count, 0, InputDeviceType_Keyboard, result);

	if(err.genericError)
		return err;

	InputHandle res = 0;

	_key(1, '1', '!');	_key(2, '2', '@');	_key(3, '3', '#');	_key(4, '4', '$');	_key(5, '5', '%'); 
	_key(6, '6', '^');	_key(7, '7', '&');	_key(8, '8', '*');	_key(9, '9', '(');	_key(0, '0', ')'); 

	_key(A, 'a', 'A');	_key(B, 'b', 'B');	_key(C, 'c', 'C');	_key(D, 'd', 'D');	_key(E, 'e', 'E');
	_key(F, 'f', 'F');	_key(G, 'g', 'G');	_key(H, 'h', 'H');	_key(I, 'i', 'I');	_key(J, 'j', 'J');
	_key(K, 'k', 'K');	_key(L, 'l', 'L');	_key(M, 'm', 'M');	_key(N, 'n', 'N');	_key(O, 'o', 'O');
	_key(P, 'p', 'P');	_key(Q, 'q', 'Q');	_key(R, 'r', 'R');	_key(S, 's', 'S');	_key(T, 't', 'T');
	_key(U, 'u', 'U');	_key(V, 'v', 'V');	_key(W, 'w', 'W');	_key(X, 'x', 'X');	_key(Y, 'y', 'Y');
	_key(Z, 'z', 'Z');

	_key(Equals, '=', '+');			_key(Comma, ',', '<');		_key(Minus, '-', '_');
	_key(Period, '.', '>');			_key(Slash, '/', '?');		_key(Acute, '`', '~');
	_key(Semicolon, ';', ':');		_key(LBracket, '[', '{');	_key(RBracket, ']', '}');
	_key(Backslash, '\\', '|');		_key(Quote, '\'', '"');

	_specKey(F1);	_specKey(F2);	_specKey(F3);	_specKey(F4);	_specKey(F5);	_specKey(F6);
	_specKey(F7);	_specKey(F8);	_specKey(F9);	_specKey(F10);	_specKey(F11);	_specKey(F12);
	_specKey(F13);	_specKey(F14);	_specKey(F15);	_specKey(F16);	_specKey(F17);	_specKey(F18);
	_specKey(F19);	_specKey(F20);	_specKey(F21);	_specKey(F22);	_specKey(F23);	_specKey(F24);

	_specKey(Backspace);	_specKey(Shift);		_specKey(Ctrl);			_specKey(Alt);
	_specKey(Pause);		_specKey(Caps);			_specKey(Escape);		_specKey(PageUp);
	_specKey(PageDown);		_specKey(End);			_specKey(Home);			_specKey(Select);
	_specKey(Print);		_specKey(Execute);		_specKey(PrintScreen);	_specKey(Insert);
	_specKey(Delete);		_specKey(Help);			_specKey(Menu);			_specKey(NumLock);
	_specKey(ScrollLock);	_specKey(Apps);			_specKey(Back);			_specKey(Forward);
	_specKey(Sleep);		_specKey(Refresh);		_specKey(Stop);			_specKey(Search);
	_specKey(Favorites);	_specKey(Start);		_specKey(Mute);			_specKey(VolumeDown);
	_specKey(VolumeUp);		_specKey(Skip);			_specKey(Previous);		_specKey(Clear);
	_specKey(Zoom);

	_specKeyResolve(Space, ' ');
	_specKeyResolve(Tab, '\t');
	_specKeyResolve(Enter, '\n');

	_specKey(Left);
	_specKey(Up);
	_specKey(Right);
	_specKey(Down);

	_specKeyResolve(Numpad1, '1');		_specKeyResolve(Numpad2, '2');		_specKeyResolve(Numpad3, '3');
	_specKeyResolve(Numpad4, '4');		_specKeyResolve(Numpad5, '5');		_specKeyResolve(Numpad6, '6');
	_specKeyResolve(Numpad7, '7');		_specKeyResolve(Numpad8, '8');		_specKeyResolve(Numpad9, '9');
	_specKeyResolve(Numpad0, '0');		_specKeyResolve(NumpadMul, '*');	_specKeyResolve(NumpadAdd, '+');
	_specKeyResolve(NumpadDec, '.');	_specKeyResolve(NumpadDiv, '/');	_specKeyResolve(NumpadSub, '-');

	return Error_none();
}

C8 Keyboard_resolve(Keyboard keyboard, enum Key key) {

	//If it's not a key or it can't be resolved then don't resolve it

	struct InputButton* button = InputDevice_getButton(keyboard, key);

	if(!button || !button->contents)
		return '\0';

	//Numpad needs numlock to be activated in order to be resolved

	if (key >= Key_Numpad1 && key <= Key_NumpadSub) {

		if(!InputDevice_hasFlag(keyboard, KeyboardFlags_NumLock))
			return '\0';

		return button->contents;
	}

	//A-Z jumps to upper case if caps is on, but jumps back if shift is also held

	if(key >= Key_A && key <= Key_Z)
		return 
			InputDevice_getCurrentState(keyboard, Key_Shift) != InputDevice_hasFlag(keyboard, KeyboardFlags_Caps) ?
			button->altContents : button->contents;

	//Otherwise, we can just use normal shift behavior if it has an altContent

	if(button->altContents)
		return InputDevice_getCurrentState(keyboard, Key_Shift) ? button->altContents : button->contents;

	return button->contents;
}
