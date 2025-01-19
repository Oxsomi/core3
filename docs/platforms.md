# OxC3 platforms (id: 0x1C32)

OxC3 platforms contains mostly platform related things such as window creation, input handling, file input and extended functions (simplified allocator).

## Platform

The platform file is used to interface with platform dependent instructions. Most are internal functions and should not be called directly, though there are some useful functions here:

- **printAllocations**(U64 offset, U64 length, U64 minAllocationSize): Print allocations at [offset, offset + length> where allocationSize >= minAllocationSize. This can be useful to determine what allocations are currently active and is used as the final step on exit to list all leaked allocations.
- Bool **checkCPUSupport**(): Check if the CPU is capable of running OxC3. This can't be true when OxC3 is ran through the default C/C++ build process (as the main function(s) already check for it). Though it might be possible this has to be called in other languages, such as a C# or Java application, where the main is not owned by OxC3. If this returns false, then running any functions that use any SIMD instructions will have undefined behavior. It could also be useful for an existing application such as when calling via JNI (Java on Android) to know if it should show an error message or not run OxC3 (and rather have a fallback).
- I32 **Program_run**(): *<u>User defined function</u>* that is called by the OxC3 runtime when ready. This is only relevant for C/C++ applications where OxC3 takes over the main entrypoint.
- void **Program_exit**(): <u>User defined function</u> that is called by the OxC3 runtime when exiting. This is only relevant for C/C++ applications where OxC3 takes over the main entrypoint.
- **onAllocate**`(void *allocator, U64)`/**onFree**`(void *allocator,void*,)`: Callbacks to let the platform allocator know where allocations are. This is recommended to call even for internal allocators, as it makes tracking them a lot easier (and avoiding memleaks).

The rest are all internal function calls done automatically in C/C++ by the main function such as:

- **initExt**(), **cleanExt**(): Init/clean platform data.
- **create**()/**cleanup**(): Create or free the platform.
- **allocate**`(void *allocator, U64)`/**free**`(void *allocator, void*, U64)`: Allocate or free using the platform allocator (not advised to be called directly).

## File

File contains file utils for modifying the file system. There are two types of file systems:

- Virtual file system: Files that don't really exist in the place they are shown. These could be files loaded in memory, linked from another location or calls that can be filled via function calls (such as retrieving data in some sort of way). The virtual file system is always available.
  - //access is reserved to represent folders or files that are opened using the file explorer. In the future this function would return //access/0 for example for the file or folder opened there.
  - //network/x is reserved to represent the Windows file path `\\x` since virtual files replace it implicitly (backslash is replaced by forward slash). This would then access the network by name. This would have to be a separate location to clarify it is doing network operations, which are optional to support from the app's perspective.
  - //function is reserved to represent user loaded file systems. For example, mounting a zip file to memory and then allowing access to it through regular file system functions.
  - Other file names are files that are embedded somewhere in the executable. This could be in the root apk (or an extended apk), the exe, the so file, etc. The file table for this is represented in oiCA file format.
- Physical file system: Files that really exist in the current working or app directory. The user app has call create platform with the right setting (working directory or app directory as default). App directories are generally used for installation purposes and in this case it is next to the executable. A working directory is used for scripts that use the files in that directory (such as build scripts). The physical file system isn't always available, for example in Web.

### Functions

The following functions are available to interact with the file system:

- Error **getInfo**(CharString loc, FileInfo*, Allocator): Get FileInfo that represents the file at loc. For more details on FileInfo, see [OxC3 types Files](types.md#Files).
- Error **foreach**`(CharString loc, FileCallback callback, void *userData, Bool isRecursive)`: Loop through the folder at loc, if there is no folder at loc it will error. FileCallback is called for each file it finds. If the FileCallback `Error(FileInfo, void*)` returns an Error it will stop searching for more files. If isRecursive is false, it will only index the root of the folder.
- Error **remove**(CharString loc, Ns maxTimeout): Remove file and children.
- Error **add**(CharString loc, EFileType type, Ns maxTimeout, Bool createParentOnly): Add file with type (e.g. folder or file). If the file type is file and createParentOnly is true, then it won't create the file yet; but only the folders that contain it (or if it's a folder then a folder will be created).
- Error **rename**(CharString loc, CharString newFileName, Ns maxTimeout): Rename the file/folder at loc to newFileName. Where newFileName is the name itself, without a path (so it can't move it to another path).
- Error **move**(CharString loc, CharString directoryName, Ns maxTimeout): Move file/folder from loc to directoryName (full path). It isn't possible to both rename and move in one operation.
- Error **queryFileObjectCount**(CharString loc, EFileType type, Bool isRecursive, U64 *res): Count all file objects of type. If type is EFileType_Any it will count both files and folders in the directory. If isRecursive is false, it will only count direct children of the folder at loc.
- Error **queryFileObjectCountAll**(CharString loc, Bool isRecursive, U64 *res): Shortcut for queryFileObjectCount with EFileType_Any.
- Error **queryFileCount**(CharString loc, Bool isRecursive, U64 *res): Shortcut for queryFileObjectCount with EFileType_File.
- Error **queryFolderCount**(CharString loc, Bool isRecursive, U64 *res): Shortcut for queryFileObjectCount with EFileType_Folder.
- Bool **has**(CharString loc): Check if there's any file or folder at loc.
- Bool **hasType**(CharString loc, EFileType type): Check if there's any file object at loc that matches the type (file or folder).
- Bool **hasFile**(CharString loc): Shortcut for hasType EFileType_File.
- Bool **hasFolder**(CharString loc): Shortcut for hasType EFileType_File.
- Error **write**(Buffer buf, CharString loc, Ns maxTimeout): Write a buffer to a file if the file can be locked in maxTimeout ns.
- Error **read**(CharString loc, Ns maxTimeout, Buffer *output): Read a file to buffer if the file can be locked in maxTimeout ns.
- Error **loadVirtual**(CharString loc, const U32 encryptionKey[8]): Load a virtual section with an encryptionKey. Pass encryptionKey as NULL if there's no key needed.
  - When an encryptionKey is shared (or isn't required), a whole section can be loaded by calling loadVirtual on `//` or `//myLibrary`. Otherwise, every section needs to be loaded individually via `//myLibrary/mySection`. Only if the section is loaded, can the files in it be accessed. `//myLibrary/mySection/*` will then index into the oiCA folder attached into the executable and loaded into memory (decompressed/decrypted).
- Error **unloadVirtual**(CharString loc): Unload the section(s) at loc. Can unload all by using `//` or a certain library through `//library` as well as an individual section via `//library/section`.
- Bool **isVirtualLoaded**(CharString loc): Check if a virtual section is loaded.

### Virtual file system

OxC3 supports a virtual file system that allows baking/preprocessing of external file formats to our own, as well as embedding these files into the exe/apk directly. The executable embeds sections, which can be loaded individually and support dependencies. For example;

```
myLibrary
	shaders
	fonts
	textures
myOtherLibrary
	shaders
	fonts
	textures
```

The example above shows the sections that are supported for our example executable. To access these resources from our application we have to load either the root or the specific sections:

```c
gotoIfError(clean, File_loadVirtual("//myLibrary/fonts", NULL));	//Load section.
gotoIfError(clean, File_loadVirtual("//myLibrary", NULL));			//Load myLibrary.
gotoIfError(clean, File_loadVirtual("//.", NULL));					//Load everything.
```

These files are decompressed and unencrypted (if they were) and kept in memory, so they can be quickly accessed. They can then be unloaded if they're deemed unimportant.

The example layout above is only useful if the dependencies have very little resources. The moment you have lots of resources (or little RAM) then you probably want to split them up based on how they're accessed. For example; if assets are only used in a certain level then consider splitting up the file structure per level to ensure little RAM is wasted on it. Another example could be splitting up assets based on level environment, so if only all forest environment levels use a certain tree, then it's wasteful to load that for every level. The levels would then reference dependencies to sections and these sections would then be loaded.

When a virtual file system is loaded, it can be accessed the same way as a normal file is loaded. The exception being that write access isn't allowed anymore. The only limitation is that files need both a library and a section folder `//myLibrary/fonts/*` rather than `//myLibrary/*` and the section has to be loaded. To ensure it is loaded, a File_loadVirtual can be called or File_isVirtualLoaded can be called.

The same limitations as with a normal file system apply here; the file names have to be windows compatible and section/library names are even stricter (Nytodecimal only; 0-9A-Za-z$_). These are case insensitive and will likely be transformed to a different casing depending on platform.

The only reserved library names besides the windows ones (NUL, COM, etc.) are: access, function, network. So `//access/...` is reserved for future access to directories and files outside of the working directory and app directory (access has to be allowed through selecting for example from the Windows file explorer). `//function/...` is reserved for future functionality to allow custom functionality to emulate for example old file formats (or loading a zip in memory); the fully resolved path would be passed to a user function to allow custom behavior.
`//network/..` is reserved for future use to enable the usage of Windows like `\\resolveName` with custom permissions.

#### Usage in CMake

to add the virtual files to your project, you can use the following:

```cmake
add_virtual_files(TARGET myProject NAME mySection ROOT ${CMAKE_CURRENT_SOURCE_DIR}/res/mySectionFolder SELF ${CMAKE_CURRENT_SOURCE_DIR})
configure_icon(myProject "${CMAKE_CURRENT_SOURCE_DIR}/res/logo.ico")
configure_virtual_files(myProject)
```

Virtual files are then linked into the project.

To add a dependency, use the following:

```cmake
add_virtual_dependencies(TARGET myProject DEPENDENCIES myDep)
```

This should be done before the configure_virtual_files and ensures the files for the dependency are present in this project. A dependency itself can't include an icon or use configure_virtual_files; as this is reserved for executables only.

*Note: Dependencies can't be overlapping. So if B and C both include A then including B and C in D won't work.*

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
  - void **onDraw**(WindowManager*) called after drawing and updating all windows and after manager update. With
  - void **onUpdate**(WindowManager*, F64) called before onDraw (manager) and after updating/drawing all windows.
  - After use, call free() on it.
    - **onUpdate**(), **onDraw**() are generally called when **wait**() is called. However, sometimes events bypass the polling system, in which case it has to call these functions internally. This is the case with Windows resizing/moving, in those cases, WM_PAINT is dispatched only to the window and not to the poll loop. That behavior induces a full window manager update and will cause all windows to receive updates (virtual windows also receive those), but they won't receive paint notifications themselves.
- **isAccessible**() if the WindowManager is accessible by the current calling thread.
- Bool **supportsFormat**(EWindowFormat format) where EWindowFormat is one of BGRA8/RGBA8 (one of which is always supported), BGR10A2, RGBA16f, RGBA32f (rarely supported).
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
  - EWindowFormat is one of BGRA8/RGBA8 (one of which is always supported), BGR10A2, RGBA16f, RGBA32f (rarely supported).
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
  - isMinimized on Windows also happens when the window is resizing.
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

### Monitor

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