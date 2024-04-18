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

- Caps (0), NumLock (1), ScrollLock (2).

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

**Note**: On Windows, it's possible to distinguish multiple keyboards. This might be useful for local multiplayer or simulators (e.g. flight sim). However, this is not standard across all platforms. This means that it is possible for multiple keyboards to have different key states, keep that in mind.

### Mouse

A mouse has the following bindings:

- Axes:
  - X, Y
  - ScrollWheel_X, ScrollWheel_Y
- Buttons:
  - Left, Middle, Right, Back, Forward.

Just like a Keyboard, the mouse is created/destroyed through the Window/WindowManager implementation.

**Note**: On Windows and OSX, it's possible to distinguish multiple keyboards. This might be useful for local multiplayer. However, this is not standard across all platforms. This means that it is possible for multiple keyboards to have different key states, keep that in mind.

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

## WindowManager

To create a Window, first a WindowManager has to be created. This is a single threaded manager that handles the window(s) that have been created using it. Multiple window managers may not be supported, depending on the platform and multiple (physical) windows may not be supported depending on the platform either.

| Platform | Multiple WindowManagers | Multiple windows |
| -------- | ----------------------- | ---------------- |
| Android  | **No**                  | **No**           |
| Windows  | Yes, 1 per thread only  | Yes              |
| OS X     | Yes                     | Yes              |
| iOS      | **No**                  | **No**           |
| Linux    | Yes                     | Yes              |
| Web      | **No**                  | Yes              |

As a recommendation, try to keep only one WindowManager and one physical window unless only desktop is targeted. Keep 1 WindowManager per thread max. New window creation is only supported on desktop, on other platforms it is only possible to fill existing windows (e.g. the current app window or currently existing canvas windows). 

Available functions:

- **create**(WindowManagerCallbacks, U64 extendedDataSize, WindowManager*) creates a WindowManager with the following callback functions (WindowManagerCallbacks):
  - void **onCreate**(WindowManager*) called after the native window manager has been initialized.
  - void **onDestroy**(WindowManager*) called before the window manager is freed.
  - void **onDraw**(WindowManager*) called after drawing and updating all windows and after manager update.
  - void **onUpdate**(WindowManager*, F64) called before onDraw (manager) and after updating/drawing all windows.
  - After use, call free() on it.
- **isAccessible**() if the WindowManager is accessible by the current calling thread. 
- Bool **supportsFormat**(EWindowFormat format) where EWindowFormat is one of BGRA8 (always supported), BGR10A2, RGBA16f, RGBA32f (rarely supported).
- Error **wait**() wait for all current windows to exit. If they are physical windows, this means the user (or program) has to close them. If they are virtual windows then the program has to close them explicitly (for example after finishing the renders).
- **createWindow** and **freeWindow** are specified in Window.

Members:

- U64 **owningThread**; the thread that owns the WindowManager is the only one allowed to interact with it.
- All of these members are illegal to read if the owningThread is not the same as the current calling thread id:
  - U32 **isActive**; value that is set to WindowManager_magic to indicate it's active. 
  - ListWindowPtr **windows**; the currently active windows.
  - WindowManagerCallbacks **callbacks**; the callbacks as mentioned in the create function.
  - Buffer **extendedData**; user data that can be initialized in create with the size provided in the create function.
  - Ns **lastUpdate**; used to track deltaTime between updates.
  - Buffer **platformData**; platform specific data that's required to make the native WindowManager.

## Window

A Window is a view which can be rendered into. Though there's a distinction between native (physical), extended and virtual windows. A physical and extended window can also receive user input. A physical window is native to the platform it runs on, while an extended window is for a connected device that's not natively supported (such as an XR device). A virtual window is just to render to a render texture or CPU-sided buffer without creating a real window for it (this can be useful in offline renders for example, as it might not have to stall as much).

A window has to be managed on the same thread as a WindowManager, through the following (WindowManager) functions:

- Error **createWindow**(EWindowType, I32x2 pos, size, minSize, maxSize, EWindowHint, CharString title, WindowCallbacks, EWindowFormat, U64 extendedDataSize, Window **result).
  - EWindowType must be one of Physical, Virtual.
  - size must be greater or equal to 1.
    - Default window sizes are in EResolution, such as: SD, 360, VGA, 480, WideScreen, HD, FWideScreen, FHD, QHD, UHD, 8K, 16K. They can be converted back to I32x2 using **EResolution_get** and back to EResolution using **EResolution_create(**I32x2) (though they must be >0 and <=65535).
  - minSize and maxSize are optional (though must be >=0).
    - minSize of 0 indicates 360p.
    - maxSize of 0 indicates 16384 x 16384.
    - They must be higher than 240p and lower than 16k always. And they auto resize the passed size to be within that range as well.
  - EWindowHint must be one of the following flags:
    - AllowFullscreen (0), DisableResize (1), ForceFullscreen (2), AllowBackgroundUpdates (3), ProvideCPUBuffer (4).
    - Where default is: AllowFullscreen.
  - title must not exceed 260 C8s. 
  - EWindowFormat is one of BGRA8 (always supported), BGR10A2, RGBA16f, RGBA32f (rarely supported).
  - extendedDataSize is the size for the buffer that will get allocated to represent the window.
  - WindowCallbacks define the window callbacks. The following are available:
    - void **onCreate**(Window*) after the underlying window is created.
    - void **onDestroy**(Window*) before the underlying window is destroyed.
    - void **onDraw**(Window*) to prepare the window for drawing.
    - void **onResize**(Window*) when the window itself has been resized.
    - void **onWindowMove**(Window*) when the window has moved (useful for text rendering).
    - void **onMonitorChange**(Window*) when a monitor that the window is on has been changed (useful for text rendering).
    - void **onUpdateFocus**(Window*) when the window is hidden or shown (minimized or unfocussed).
    - void **onUpdate**(Window*, F64) before rendering the window.
    - void **onDeviceAdd**`(Window*, InputDevice*)` when an input device is registered.
    - void **onDeviceRemove**`(Window*, InputDevice*)` when an input device is unregistered.
    - void **onDeviceButton**`(Window*, InputDevice*, InputHandle, Bool)` when a device button is pressed or released.
    - void **onDeviceAxis**`(Window*, InputDevice*, InputHandle, F32)` when a device axis is changed.
    - void **onTypeChar**`(Window*, CharString)` when a combination of UTF8 characters is typed in the current window.
- And destroyed through **freeWindow**(Window **w).

When a Window is created, it can be accessed through the following functions:

- **isMinimized**(), **isFocussed**(), **isFullScreen**(), **doesAllowFullScreen**() as a simple helper around checking the window hint & flags.
- **terminate**() the window is ready for destruction next update.
- Error **toggleFullScreen**() toggle full screen on or off. If it's not supported it will error.
- Error **updatePhysicalTitle**(CharString title) update the title of a physical window. Up to 260 C8s.
- Error **presentPhysical**() if the window is physical and has a cpu backed buffer, the CPU backed buffer can be used directly instead to simplify the dependencies. No direct GPU intervention is needed there. This has to be called on draw to present the CPU buffer.
- Error **resizeCPUBuffer**(Bool copyData, I32x2 newSize) resize the CPU buffer. This is only available for virtual windows or physical windows with the ProvideCPUBuffer hint. This has to be manually called onResize. Try to avoid copyData unless it's really required (most times, the entire render is done for the screen, so a copy is wasteful).
- Error **storeCPUBufferToDisk**(CharString filePath, Ns maxTimeout) store the CPU buffer as a DDS file.

It has the following properties:

- **owner**: WindowManager that owns it.
- **type**, **hint**, **format**, **minSize**, **maxSize**, **callbacks**: As specified in the create function.
- **flags**: Currently active window flags such as IsFocussed (0), IsMinimized (1), IsFullScreen (2), IsActive (3), ShouldTerminate (4).
- **offset**, **size**: The location and size of the window rect. Offset is irrelevant on some platforms.
- **prevSize**: Previous size when full screen was last toggled.
- **cpuVisibleBuffer**: If virtual window or physical window with EWindowHint_ProvideCPUBuffer is used, this is the CPU sided buffer which can be used to directly provide the image to the screen. This is not performant of course, but for a simple application this might be enough (no GPU stuff is needed).
- **nativeHandle**, **nativeData**: Platform specific data that represents the window. Such as nativeHandle as a HWND on Windows, NSWindow on OSX or an app_window on Android. nativeData is additional data that is required to manage it.
- **lastUpdate**: Last time the window was updated.
- **title**: The title of the window (mostly relevant to physical windows).
- **devices**: List of InputDevice that are registered to the Window.
- **monitors**: List of Monitor that the Window is visible on.
- **extendedData**: User data initialized by onCreate (though the buffer is already allocated by the create function).
- **buffer**: Temporary window data that will be used only for a super short period. For example, WM_CHAR could send two messages that correspond with one UTF16 character. This buffer will be used to store it for a short period of time. 

### Monitor (TODO: OxC3 0.3)

Is a combination of the following properties:

- **offsetPixels**, **sizePixels**: Offset and size of the monitor (offset might not be available and thus 0.xx, depending on platform).
- **offsetR**, **offsetG**, **offsetB**: Subpixel rendering information. For RGB landscape this would be 0,0 -> 2,0 while for RGB portrait this would be 0,0 -> 0,2. If BGR is used, they are flipped (B and R are swapped) and flipped portrait or landscape mode would flip this once again. If the pixel layout can't be determined or isn't BGR/RGB (example: pentile displays on mobile) then subpixel rendering is turned off by returning 0 for all of the offsets.
- **sizeInches**: Physical size of the monitor. If not available, this would return 0. Might be handy for determining the speed of dragging for example.
- **orientation**: What orientation the monitor has (EMonitorOrientation): Landscape (0), Portrait (90), FlippedLandscape (180), FlippedPortrait (270).
- **gamma**, **contrast**: Gamma and contrast the monitor works with.
- **refreshRate**: Monitor refresh rate in Hz.

## Extended functions

Extended functions (always suffixed with an x, but not all functions ending with x are extended) are OxC3 functions that require an Allocator, as such, a simplified version is provided that takes the default Platform allocator. This default allocator has extra utils, such as memleak detection, allocation stacktraces, invalid free detection and could provide things such as out of bound write detection in the future. A custom allocator can be provided to avoid the default allocation tracking or to handle it in a special way. Generally the extended functions are provided by platforms/ext and then the header for the respective functions.

Example: `Error Archive_movex(Archive *archive, CharString loc, CharString directoryName);` is the extended version of `Error Archive_move(Archive *archive, CharString loc, CharString directoryName, Allocator alloc)`. And even though OxC3 types defines this, OxC3 platforms adds the extended function with a default allocator.

*Note: The only special case here is listx.h and listx_impl.h, which need to be included before anything else. These includes define TListX and TListXImpl which have to be defined before any other TList is defined (if they're not then those functions will not exist, likely resulting in linker errors).*