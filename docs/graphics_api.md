# Graphics library

The core pillars of the abstraction of this graphics library are the following:

- Support feature sets as close as possible to Vulkan, DirectX and Metal.
- Simplify usage for these APIs, as they're too verbose.
  - But don't oversimplify them to the point of being useless.
- Force modern minimum specs to avoid having to build too many diverging renderers to deal with limitations of older devices.
- Allow modern usage of these APIs such as raytracing and bindless.
- Support various systems such as Android, Apple, Windows and Linux (console should be kept in mind, though not officially supported).

## Ref counting

OxC3 graphics works in a similar way as DirectX's ref counting system; everything is ref counted. This allows you to just call `RefPtr_dec` on the RefPtr to ensure it's properly released. If the command list is still in flight, it will maintain this resource until it's out of flight. It also allows you to safely share resources between user libraries without worrying about resource deletion. When sharing a resource, you simply increment the refptr by using `RefPtr_add` and when the library is done using it can decrement it again. This concept was added in OxC3 platforms, but widely used in the graphics library.

### Obtaining the real object

RefPtr doesn't contain the object itself, but it does contain information about the type and length of the constructed type. RefPtr contains this object after the data of the RefPtr itself. RefPtr_data can be used to obtain the data pointed to by the RefPtr. This is generally also a macro from the specialized RefPtr (e.g. `GraphicsInstanceRef_ptr`). It also generally contains the extended object after the common object. For example GraphicsInstance has `GraphicsInstance_ext` which can get the API-specific data of the GraphicsInstance. This is should only be used internally, as there is no public interface of this extended data that won't randomly change.

## Graphics instance

### Summary

The graphics instance is the way you can query physical devices from DirectX, Vulkan or Metal. First one has to be created as follows:

```c
GraphicsInstanceRef *instance = NULL;

_gotoIfError(clean, GraphicsInstance_create(
	(GraphicsApplicationInfo) {
	    .name = CharString_createConstRefCStr("Rt core test"),
    	.version = GraphicsApplicationInfo_Version(0, 2, 0)
	}, 
    false /* isVerbose; extra logging about the instance properties */, 
    &instance
));
```

Once this instance is acquired, it can be used to query devices and to detect what API the runtime supports.

### Properties

- application: The name and version of the application.
- api: Which api is ran by the runtime: Vulkan, DirectX12, Metal or WebGPU.
- apiVersion: What version of the graphics api is being ran (e.g. Vulkan 1.2, DirectX 12_2, etc.).

### Functions

- ```c
  Error getDeviceInfos(Bool isVerbose, List<GraphicsDeviceInfo> *infos);
  ```

  - Queries all physical devices to detect if they're supported and what features they have.

- ```c
  Error getPreferredDevice(
  	GraphicsDeviceCapabilities requiredCapabilities, 
  	U64 vendorMask,
  	U64 deviceTypeMask,
  	Bool verbose,
  	GraphicsDeviceInfo *deviceInfo
  );
  ```

  - Select the physical device that has all specified extensions and is a supported vendor and/or device type.

### Used functions and obtained

- Obtained through `Error create(GraphicsApplicationInfo info, Bool isVerbose, GraphicsInstanceRef **inst);` see summary.
- Mostly used as `GraphicsInstanceRef` in `GraphicsDeviceRef_create` as well as GraphicsInstance's member functions.

## Graphics device info

### Summary

Graphics device info contains information about the underlying physical device. This is something that gets queried to figure out what hardware (or software) will be used to render and if they support the feature set that the application requires.

GraphicsInstance's getPreferredDevice can be used to query if there's any graphics device that supports the required feature set. For more complex setups such as multi GPU rendering, it is recommended to manually use `getDeviceInfos` to determine which GPUs (if any) support your use case(s). The devices that are needed can then be made into a GraphicsDevice manually.

```c
GraphicsDeviceInfo deviceInfo = (GraphicsDeviceInfo) { 0 };

_gotoIfError(clean, GraphicsInstance_getPreferredDevice(
    GraphicsInstanceRef_ptr(instance),		//See "Graphics instance"
    (GraphicsDeviceCapabilities) { 0 },		//No required features or data types
    GraphicsInstance_vendorMaskAll,			//All vendors supported
    GraphicsInstance_deviceTypeAll,			//All device types supports
    false /* isVerbose; extra logging about the device properties */,
    &deviceInfo
));
```

### Properties

- name, driverName, driverInfo; All null-terminated UTF-8 strings giving information about the device and driver.
- type; what type this device is (dedicated GPU, integrated GPU, simulated GPU, CPU or other (unrecognized)).
- vendor; what company designed the device (Nvidia (NV), AMD, ARM, Qualcomm (QCOM), Intel (INTC), Imagination Technologies (IMGT), Apple (APPL) or unknown).
- id; number in the list of supported devices.
- luid; ID to identify this device primarily on Windows devices. This would allow sharing a resource between other APIs for interop (not supported yet). This is optional to support; check capabilities.features & LUID.
- uuid; unique id to identify the device. In APIs that don't support this natively, the other identifier will be used here instead. For example DirectX would use the luid here and clear the other U64.
- ext; extended physical device representation for the current API.
- capabilities; what data types, features and api dependent features are enabled. See capabilities section.

#### Capabilities

- features: TiledRendering, VariableRateShading, MultiDrawIndirectCount, MeshShader, GeometryShader, TessellationShader, SubgroupArithmetic, SubgroupShuffle, Swapchain, Multiview, Raytracing, RayPipeline, RayIndirect, RayQuery, RayMicromapOpacity, RayMicromapDisplacement, RayMotionBlur, RayReorder, LUID, DebugMarkers.
- features2: reserved for future features.
- dataTypes: I64, F16, F64, AtomicI64, AtomicF32, AtomicF64, ASTC, BCn, MSAA2x, MSAA8x, MSAA16x.
  - MSAA4x and MSAA1x are supported by default.
- featuresExt: API dependent features.
  - Vulkan: Performance query.

### Functions

- ```
  print(Bool printCapabilities);
  ```

  - Prints all relevant information about the device. If printCapabilities is on it will also show extensions and supported data types.

### Used functions and obtained

- Obtained through GraphicsInstance's getDeviceInfos and getPreferredDevice. 
- Passed to `GraphicsDeviceRef_create` to turn the physical device into a logical device.

## Graphics device

### Summary

The graphics device is the logical device that is used to create objects and execute commands to. 

```c
GraphicsDeviceRef *device = NULL;
_gotoIfError(clean, GraphicsDeviceRef_create(
    instance, 		//See "Graphics instance"
    &deviceInfo, 	//See "Graphics device info"
    false /* isVerbose; extra logging about the device properties */, 
    &device
));
```

### Properties

- instance; owning instance.
- info; physical device.
- submitId; counter of how many times submit was called (can be used as frame id).

### Functions

- ```c
  Error submitCommands(List<CommandListRef*> commandLists, List<SwapchainRef*> swapchains);
  ```

- ```c
  wait();
  ```

- ```c
  Error createSwapchain(SwapchainInfo info, SwapchainRef **swapchain);
  ```

- ```c
  Error createCommandList(
  	U64 commandListLen, 
  	U64 estimatedCommandCount,
  	U64 estimatedResources,
  	CommandListRef **commandList
  );
  ```

### Obtained

- Through GraphicsDeviceRef_create; see overview.

## Command list

### Summary

A command list in OxC3 is similar to a virtual command list. The commands get recorded in a standardized format, but they don't get translated until they're submitted to the graphics device. The graphics device is then responsible for synchronization and ensuring the commands are recorded in the optimal way. Command lists themselves don't have any API specific implementation to avoid unexpected caching behavior. When submitting multiple command lists, they'll act as if they're one long command list. This would allow maintaining state from the old command list as well.

```c
CommandListRef *commandList = NULL;
_gotoIfError(clean, GraphicsDeviceRef_createCommandList(
    device, 	//See "Graphics device"
    4 * KIBI, 	//Max command buffer size
    128, 		//Estimated command count (can auto resize)
    KIBI, 		//Estimated resource count (can auto resize)
    &commandList
));
```

#### Recording a command list

```c
_gotoIfError(clean, CommandListRef_begin(commandList, true /* clear previous */));

_gotoIfError(clean, CommandListRef_clearImagef(
    commandList, F32x4_create4(1, 0, 0, 1), (ImageRange){ 0 }, swapchain
));

_gotoIfError(clean, CommandListRef_end(commandList));
```

Every frame, this can be passed onto the submit commands call:

```c
List commandLists = (List) { 0 };
List swapchains = (List) { 0 };

_gotoIfError(clean, List_createConstRef((const U8*) &commandList, 1, sizeof(commandList), &commandLists));
_gotoIfError(clean, List_createConstRef((const U8*) &swapchain, 1, sizeof(swapchain), &swapchains));

_gotoIfError(clean, GraphicsDeviceRef_submitCommands(device, commandLists, swapchains));
```

For more info on commands check out the "Commands" section.

### Properties

- device; owning device.
- state; if the command list has been opened before and if it's open or closed right now.
- private:
  - data: The current recorded commands.
  - commandOps: The opcodes for which commands are recorded.
  - resources: Which resources are in use by the command list.
  - callstacks: On debug mode, contains the stacktrace of each command. Used for debugging if an error occurs in the API-dependent layer.
  - next: the next offset into the data buffer for the next command to record.

### Functions

See the "Commands" section.

### Used functions and obtained

- Obtained through GraphicsDeviceRef's createCommandList, see overview.
- Used in GraphicsDeviceRef's submitCommands.

## Swapchain

### Summary

A swapchain is the interface between the platform Window and the API-dependent way of handling the surface, swapchain and any synchronization methods that are required.

Swapchain isn't always supported. In that case your final target can just be a render texture and it can be pulled back to the CPU to allow presenting there (video, photo, or whatever). To make sure it's supported, check deviceInfo.capabilities.features for Swapchain support.

```c
SwapchainRef *swapchain = NULL;
_gotoIfError(clean, GraphicsDeviceRef_createSwapchain(
    device, 		//See "Graphics device"
    (SwapchainInfo) { .window = w, .vsync = false }, 
    &swapchain
));

//Window callback to make sure format + size stays the same: 

void onResize(Window *w) {
	if(!(w->flags & EWindowFlags_IsVirtual))
		Swapchain_resize(SwapchainRef_ptr(swapchain));
}
```

### Properties

- info.window: the Window handle created using OxC3 platforms.
- info.requiresManualComposite: Whether or not the application is requested to explicitly handle rotation from the device. For desktop this is generally false, for Android this is on to avoid the extra overhead of the compositor. 
- info.vsync: Whether or not the window is expected to use vsync. Keep in mind that all swapchains that are presented need to share this setting.
- device: The owning device.
- size: Current size of the swapchain.
- format: Current format of the swapchain.
- versionId: The version id changes everytime a change is applied. This could be resizes or format changes.

### Functions

- ```c
  resize();
  ```

  - Updates the swapchain with the current information from OxC3 platforms. Both format and size are queried again to ensure it really needs to resize. After this, all recorded command lists will still be valid, but the API implementation will likely re-record their command lists if it was already re-using them.

### Used functions and obtained

- Obtained through GraphicsDeviceRef's createSwapchain, see overview.
- Used in GraphicsDeviceRef's submitCommands as well as read & write image commands and dispatches.

## Commands

### Summary

Command lists store the commands referenced in the next section. These are virtual commands; they approximately map to the underlying API. If the underlying API doesn't support a commands, it might have to simulate the behavior with a custom shader (such as creating a mip chain of an image). It is also possible that a command might need certain extensions, without them the command will give an error to prevent it from being inserted into the command list (some unimportant ones such as debugging are safely ignored if not supported instead). These commands are then processed at runtime when they need to. If the command list remains the same, it's the same swapchain (if applicable) and the resources aren't recreated then it can safely be re-used. 

### begin/end

To make sure a command list is ready for recording and submission it needs begin and end respectively. Begin has the ability to clear the command list (which is generally desired). However, it's also possible that the command list was closed by one library and then given back to another. In this case, it can safely be re-opened without the clear flag. After ending it, it can be passed to submitCommands. These "commands" are special, as they're not inserted into the command list itself, they just affect the state of the command list. Only one submit is allowed per frame, as it handles synchronization implicitly.

### setViewport/(And)Scissor

setViewport and setScissor are used to set viewport and scissor rects respectively. However, since in a lot of cases they are set at the same time, there's also a command that does both at once "setViewportAndScissor". 

```c
_gotoIfError(clean, CommandListRef_setViewportAndScissor(
	commandList,
    I32x2_create2(0, 0),
    I32x2_create2(1920, 1080)
));
```

### setStencil

The stencil reference can be set using the setStencil command.

```c
_gotoIfError(clean, CommandListRef_setStencil(commandList, 0xFF));
```

### clearImage(f/u/i)/clearImages

clearImagef/clearImageu/clearImagei and clearImages are actually the same command. They allow clearing one or multiple images. clearImages should be used whenever possible because it can batch clear commands in a better way. However, it is possible that only one image needs to be cleared and in that case clearImage(f/u/i) are perfectly fine. The f, u and i suffix are to allow clearing uint, int and float targets. In clearImages these are handled manually. The format should match the format of the underlying images. The images passed here are automatically transitioned to the correct state. 

```c
_gotoIfError(clean, CommandListRef_clearImagef(
    commandList, 				//See "Command list"
    F32x4_create4(1, 0, 0, 1), 	//Clear red
    (ImageRange){ 0 }, 			//Clear layer and level 0
    swapchain					//See "Swapchain"
));
```

To clear multiple at once, call clearImages with a `List<ClearImage>`. ClearImage takes a color as a `U32[4]` (It's safe to copy a F32x4 or I32x4 to this as long as you bitcast it), a range and the image ref ptr. It's the same as a single clear image but it allows multiple.

### DebugMarkers feature

DebugMarkers adds three commands: addMarkerDebugExt, startRegionDebugExt and endRegionDebugExt. If the DebugMarkers feature isn't present, these are safely ignored and won't be inserted into the virtual command list. This can be used to provide extra debugging info to tools such as RenderDoc, NSight, Pix, etc. to show where important events such as render calls happened and what they represented. A debug region or debug marker has a color and a name. A debug region is like a stack; you can only end the current region and push another region. Every region you start has to be ended over the command lists that are submitted (you can start & end the same region in two separate command lists, as long as they're always both submitted).

```c
_gotoIfError(clean, CommandListRef_startRegionDebugExt(
    commandList, 							//See "Command list"
    F32x4_create4(1, 0, 0, 1), 				//Marker color
    CharString_createConstRefCStr("Test")	//Marker name
));

//Insert operation that needs to be in the debug region

_gotoIfError(clean, CommandListRef_endRegionDebugExt(commandList));
```

The same syntax as startRegionDebugExt can be used for addMarkerDebugExt. Except a marker doesn't need any end, it's just 1 event on the timeline.

### DirectRendering feature

DirectRendering allows rendering without render passes (default behavior in DirectX). This makes development for desktop a lot easier since AMD, Intel and NVIDIA aren't using tiled based deferred rendering (TBDR). However, all other vendors (such as Qualcomm, ARM, Apple, Imgtec) do use TBDR (mostly mobile architectures). The user is allowed to decide that this is a limitation they accept and can use this feature to greatly simplify the difficulty of the graphics layer (especially porting from existing apps). The user can also set up two different render engines; one that can deal with direct rendering and one that can't. The latter is targeted at mobile (lower hardware tier) and the former is for desktop/console. The two commands that are related to this feature are: startRenderExt and endRenderExt. They require the feature to be present and will return an error (and won't be inserted into the command list) otherwise.

Just like *most* commands, this will automatically transition the resources into the correct states for you. Color attachments can always be read, but the user is in charge of specifying if the contents should be cleared or kept. Color attachments can also be readonly if needed.

```c
//In command list recording

AttachmentInfo attachmentInfo = (AttachmentInfo) {
    .range = (ImageRange) { 0 },						//Layer 0, level 0
    .image = swapchain,									//See "Swapchain"
    .load = ELoadAttachmentType_Clear,					//Clear image
    .readOnly = false,									//Allow draw calls to write
    .color = (ClearColor) { .colorf = {  1, 0, 0, 1 } }
};

List colors = (List) { 0 };
_gotoIfError(clean, List_createConstRef(
    (const U8*) &attachmentInfo, 1, sizeof(AttachmentInfo), &colors
));

_gotoIfError(clean, CommandListRef_startRenderExt(
    commandList, 		//See "Command list"
    I32x2_zero(), 		//No offset
    I32x2_zero(), 		//Use attachment's size
    colors, 
    (List) { 0 }		//No depth stencil buffers
));

//Draw calls here

_gotoIfError(clean, CommandListRef_endRenderExt(commandList));
```

Keep in mind that during a render call, you can't transition the resources passed into the attachments of the active render call. They're not allowed to be used as a read (SRV) or write textures (UAV); they're only allowed to be output attachments (RTV). Special care should be taken on the developer's side to avoid this from happening.

### TODO: Transitions

Because the API requires bindless to function, it has certain limits. One of these limits/benefits is that a shader is now able to access all write buffers/textures and read buffers/textures. This would mean that everything is accessible by all shaders; making automatic transitions impossible. To fix this; the user will only have to manually do transitions for draw/dispatch calls. Here you specify the (sub)resource and in which shader stage it is first used and if it's a write (or if any subsequent shaders could write to it). Then the runtime will automatically transition only when it's needed. 

Even though Metal doesn't need transitions, they're still required to allow DirectX and Vulkan support. More importantly; transitions allow OxC3 to know which resources are required for the command list to be executed. It uses this to keep the resources alive until the command list was executed on the device. 

*It is important to limit how many transitions are called, since this requires extra data and processing. Batching them and making sure you need them as less as possible is good practice.*

#### Validation

Unfortunately, validation is only possible in DirectX and Vulkan using their respective validation layers. It is impossible to tell which resource was accessed in a certain frame (without running our own GPU-based validation layer). This makes it impossible to know if it was in the correct state. Make sure to validate on Vulkan since it's the strictest with transitions. However, if you're using Metal and doing transitions incorrectly, it could show up as some resources being deleted too early (if they're still in flight).