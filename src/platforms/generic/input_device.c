#include "types/assert.h"
#include "types/string.h"
#include "platforms/input_device.h"
#include "platforms/platform.h"

//Private helpers

//Bitset for the states of the buttons/axes
//f32[floatStates*2]
//u2[booleanStates]

struct Buffer states;

InputButton *InputDevice_getButtonName(struct InputDevice dev, u16 localHandle) {
	ocAssert("Handle out of bounds", localHandle < dev.buttons);
	return (InputButton*)((struct InputAxis*) dev.handles.ptr + dev.axes) + localHandle;
}

struct InputAxis *InputDevice_getAxisName(struct InputDevice dev, u16 localHandle) {
	ocAssert("Handle out of bounds", localHandle < dev.axes);
	return (struct InputAxis*) dev.handles.ptr + + localHandle;
}

f32 *InputDevice_getAxisValue(struct InputDevice dev, u16 localHandle, bool isCurrent) {
	return (f32*)dev.states.ptr + (localHandle << 1) + isCurrent;
}

//

struct InputDevice InputDevice_create(u16 buttons, u16 axes) {

	usz handles = sizeof(struct InputAxis) * axes + sizeof(InputButton) * buttons;
	usz states = sizeof(f32) * 2 * axes + (buttons * 2 + 7) >> 3;

	void *allocator = Platform_instance.alloc.ptr;
	AllocFunc alloc = Platform_instance.alloc.alloc;

	return (struct InputDevice) {

		.buttons = buttons,
		.axes = axes,

		.handles = (struct Buffer) {
			.ptr = alloc(handles, allocator),
			.siz = handles
		},

		.states = (struct Buffer) {
			.ptr = alloc(states, allocator),
			.siz = states
		}
	};
}

InputHandle InputDevice_createButton(struct InputDevice d, u16 localHandle, const c8 *keyName) {

	InputButton *name = InputDevice_getButtonName(d, localHandle);

	usz len = String_len(keyName, 128);

	ocAssert("Invalid string; empty or strings longer than ")!len || len == 128)

	ocAssert("Handle already defined", !*name);
	*name = keyName;

	return InputDevice_createHandle(d, localHandle, InputType_Button);
}

InputHandle InputDevice_createAxis(struct InputDevice d, u16 localHandle, const c8 *keyName, f32 deadZone) {

	struct InputAxis *name = InputDevice_getAxisName(d, localHandle);

	ocAssert("Handle already defined", !*name);
	*name = (struct InputAxis) {
		.name = keyName,
		.deadZone = deadZone
	};

	return InputDevice_createHandle(d, localHandle, InputType_Axis);
}

void InputDevice_free(struct InputDevice dev) {

	void *allocator = Platform_instance.alloc.ptr;
	FreeFunc free = Platform_instance.alloc.free;

	free(allocator, dev.handles);
	free(allocator, dev.states);
}

enum InputState InputDevice_getState(struct InputDevice d, InputHandle handle);

bool InputDevice_getCurrentState(struct InputDevice d, InputHandle handle) {
	ocAssert("Invalid button", InputDevice_isButton(d));
	//TODO:
}

bool InputDevice_getPreviousState(struct InputDevice d, InputHandle handle);

f32 InputDevice_getCurrentAxis(struct InputDevice d, InputHandle handle) {
	ocAssert("Invalid axis handle", InputDevice_isAxis(d, handle));
	return *InputDevice_getAxisValue(d, InputDevice_getLocalHandle(d, handle), true);
}

f32 InputDevice_getPreviousAxis(struct InputDevice d, InputHandle handle) {
	ocAssert("Invalid axis handle", InputDevice_isAxis(d, handle));
	return *InputDevice_getAxisValue(d, InputDevice_getLocalHandle(d, handle), false);
}

f32 InputDevice_getDeltaAxis(struct InputDevice d, InputHandle handle) {
	return InputDevice_getCurrentAxis(d, handle) - InputDevice_getPreviousAxis(d, handle);
}

InputHandle InputDevice_getHandle(struct InputDevice d, const c8 *name) {

	for(u16 i = 0; i < d.buttons; ++i)
		if(String_compare(InputDevice_getButtonName(d, i), name))

}

const c8 *InputDevice_getName(struct InputDevice d, InputHandle handle) {

	u16 localHandle = InputDevice_getLocalHandle(d, handle);

	if(InputDevice_isAxis(d, handle))
		return InputDevice_getAxisName(d, localHandle)->name;

	return InputDevice_getButtonName(d, localHandle);
}

f32 InputDevice_getDeadZone(struct InputDevice d, InputHandle handle) {
	ocAssert("Invalid axis handle", InputDevice_isAxis(d, handle));
	return InputDevice_getAxisName(d, InputDevice_getLocalHandle(d, handle))->deadZone;
}

//This should only be handled by platform updating the input device

void InputDevice_setCurrentState(struct InputDevice d, InputHandle handle);
void InputDevice_setCurrentAxis(struct InputDevice d, InputHandle handle);

void InputDevice_markUpdate(struct InputDevice d);