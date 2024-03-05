# OxC3 platforms (id: 0x1C32)

OxC3 platforms contains mostly platform related things such as window creation, input handling, file input and extended functions (simplified allocator).

## TODO: File

### TODO: Virtual files

## InputDevice

InputDevice is a generic interface to multiple types of input devices such as keyboards, mice and controllers. It basically consists of a bunch of buttons, axes and the names/configurations.

It contains the following properties:

- buttons, axes: Count of how many of these bindings are present.
- flags: InputDevice type dependent flags.
- type: such as Undefined, Keyboard, Mouse or Controller.
- handles, states: Buffer containing the current values and button/axis data (such as name info).
- dataExt: Platform/implementation dependent data that is required to represent the current type of input device.

The input device can be created for custom devices, but generally it's created through the Keyboard/Mouse interface.

Getting information about the axes and buttons can be done like the following:

- InputAxis ***InputDevice_getAxis**(InputDevice dev, U16 localHandle): Foreach dev.axes it contains an axis with the following properties: name, deadZone and resetOnInputLoss.
- InputButton ***InputDevice_getButton**(InputDevice dev, U16 localHandle): Foreach dev.buttons it contains a button which is just a name.

The current state can be accessed as follows:

- Bool **InputDevice_hasFlag**(InputDevice d, U8 flag): If the input device has the requested flag (dependent on input device type).
- EInputState **InputDevice_getState**(InputDevice d, InputHandle handle): The input state that the key is in (Up, Pressed, Released, Down), it includes both current (&1) and previous state (&2). These individual states can also be gotten by the helper functions: getCurrentState or getPreviousState. The isDown, isUp, isPressed or isReleased functions can also be used. This function only works on buttons.
- F32 **InputDevice_getDeltaAxis**(InputDevice d, InputHandle handle): Get the difference between current (getCurrentAxis) and previous axis (getPreviousAxis). Only works on axes.
- CharString **InputDevice_getName**(InputDevice d, InputHandle handle): Get the name of the handle, for serialization only! Ideally the input handle should be localized into the respective language using the current device by either the application or the interface.
- InputHandle **InputDevice_getHandle**(InputDevice d, CharString name): Get the handle from the name, for deserialization. Returns InputDevice_invalidHandle on fail.
- F32 **InputDevice_getDeadZone**(InputDevice d, InputHandle handle): Get the deadzone of the axis.

The polling system shouldn't always be used because it doesn't always trigger when low frame rate is present for example. In these cases, it might be better to rely on the events that are dispatched by the Window.

### InputHandle

An input handle ranges from [ 0, axes > for the axes and [ axes, button + axes > for the buttons. This handle can be manipulated through the following functions:

- InputHandle **InputDevice_createHandle**(InputDevice d, U16 localHandle, EInputType type): Where EInputType is Button or Axis and the localHandle is the axis or button id.
- U16 **InputDevice_getLocalHandle**(InputDevice d, InputHandle handle): Gets back the local id from the InputHandle.
- Bool **InputDevice_isValidHandle**(InputDevice d, InputHandle handle): Checks if the handle exists for the InputDevice.
- Bool **InputDevice_isAxis**(InputDevice d, InputHandle handle): Checks if the handle is an axis.
- Bool **InputDevice_isButton**(InputDevice d, InputHandle handle): Checks if the handle is a button.
- InputHandle **InputDevice_invalidHandle**(): Returned if the handle couldn't be found for example.
- U32 **InputDevice_getHandles**(): Gets how many handles are accessible.

### Keyboard

A keyboard is created/destroyed by the Window/WindowManager implementation and contains the following flags:

- Caps (0), NumLock (1), ScrollLock (2), Shift (3), Control (4), Alt (5).

And the following keys (EKey_) can be accessed:

- 0-9, A-Z
- F1-F12
- Numpad0-Numpad9, NumpadMul, NumpadAdd, NumpadDot, NumpadDiv, NumpadSub
- Left/Right/Up/Down
- Backspace/Space/Tab/Enter
- (L/R)(Shift/Ctrl/Alt/Menu)
- Pause
- Caps
- Escape
- PageUp/PageDown
- End/Home
- PrintScreen
- Insert/Delete
- NumLock/ScrollLock
- Select/Print/Execute/Sleep/Refresh/Search/Favorites/Clear/Zoom/Help/Apps
- Back/Forward/Start/Stop/Mute/VolumeDown/VolumeUp/Skip/Previous
- Bar/Options
- Equals/Comma/Minus/Period/Slash/Backtick/Semicolon/LBracket/RBracket/Backslash/Quote

EKey is remapped using scan codes for QWERTY iso layout (https://kbdlayout.info/kbdusx)
The main part of the keyboard uses scan codes, but the extended keyboard doesn't.

Because the main part of the keyboard uses scan codes, you shouldn't assume for example Key_W is really the W key! (e.g. AZERTY would be Z instead of W. Might even be non latin char). This means that *Keyboard_remap* should be used in favor of *InputDevice_getName* if it's something that's visual to the user. For serialization it's fine to still use it (since it's ASCII, so it's super easy to read).
Use Keyboard_remap(keyboard, key) to get the localized keyboard unicode codepoint for GUI control screens. When typing text the typeChar callback of Window should be used to ensure IME/unicode/etc. are all properly available.

- CharString **Keyboard_remap**(EKey key): Used for remapping the key for GUI text only. Don't use this to build text fields/text areas. This is purely for debugging the key or key mapping. This function allocates, so be sure to CharString_free.

### Mouse

A mouse has the following bindings:

- Axes:
  - X, Y
  - ScrollWheel_X, ScrollWheel_Y
- Buttons:
  - Left, Middle, Right, Back, Forward.

Just like a Keyboard, the mouse is created/destroyed through the Window/WindowManager implementation.

### Extending InputDevice

For creating a custom input device, the following functions can be used:

- Error **InputDevice_create**(U16 buttons, U16 axes, EInputDeviceType type, InputDevice *result): Create N buttons and axes, set the type and create an empty InputDevice. These bindings should be filled up before the input device is given to anything else.
- Error **InputDevice_createButton**(InputDevice dev, U16 localHandle, const C8 *keyName, InputHandle *result): Instantiate the button that was pre-allocated in the create function. keyName should be present for the whole duration of the InputDevice's lifetime. Returns the input handle into the result.
- Error **InputDevice_createAxis**(InputDevice dev, U16 localHandle, const C8 *keyName, F32 deadZone, Bool resetOnInputLoss, InputHandle *result): Instantiate the axis that was pre-allocated in the create function. keyName should be present for the whole duration of the InputDevice's lifetime. Returns the input handle into the result. deadZone is when the one reading from the device should collapse the axis to 0 and resetOnInputLoss indicates that on unfocus of the window the axis should be reset (e.g. to avoid hanging input).

The one that created the device is responsible for updating it using the following functions:

- void **InputDevice_markUpdate**(InputDevice d): Indicate a successful input poll, which means the previous state should be overwritten by the current state.
- Update the current state for buttons/axes/flags:
  - Bool **InputDevice_setCurrentState**(InputDevice d, InputHandle handle, Bool v)
  - Bool **InputDevice_setCurrentAxis**(InputDevice d, InputHandle handle, F32 v);
  - Bool **InputDevice_setFlag**(InputDevice *d, U8 flag)
  - Bool **InputDevice_resetFlag**(InputDevice *d, U8 flag)
  - Bool **InputDevice_setFlagTo**(InputDevice *d, U8 flag, Bool value)

Finally, InputDevice_free should be used to free the data that belongs to the InputDevice when it's unused.

## TODO: Window

### TODO: Monitor

### TODO: WindowManager

## TODO: Extended functions

