# Graphics library

The core pillars of the abstraction of this graphics library are the following:

- Support feature sets as close as possible to Vulkan, DirectX12 and Metal3.
  - Limits from legacy graphics such as DirectX11 (and below), OpenGL, below Metal3, below Vulkan 1.2 and others like WebGL won't be considered for this spec. They'd add additional complexity for no gain.
- Simplify usage for these APIs, as they're too verbose.
  - But don't oversimplify them to the point of being useless.
  - This does mean features not deemed important enough might not be included in the main specification. Though a branch could maintain support if needed.
- Force modern minimum specs to avoid having to build too many diverging renderers to deal with limitations of older devices.
- Allow modern usage of these APIs such as raytracing and bindless.
- Support various systems such as Android, Apple, Windows and Linux (console should be kept in mind, though not officially supported).

## Ref counting

OxC3 graphics works in a similar way as DirectX's ref counting system; everything is ref counted. This allows you to just call `RefPtr_dec` on the RefPtr to ensure it's properly released. If the command list is still in flight, it will maintain this resource until it's out of flight. It also allows you to safely share resources between user libraries without worrying about resource deletion. When sharing a resource, you simply increment the refptr by using `RefPtr_inc` and when the library is done using it can decrement it again. This concept was added in OxC3 platforms, but widely used in the graphics library.

### Obtaining the real object

RefPtr doesn't contain the object itself, but it does contain information about the type and length of the constructed type. RefPtr contains this object after the data of the RefPtr itself. RefPtr_data can be used to obtain the data pointed to by the RefPtr. This is generally also a macro from the specialized RefPtr (e.g. `GraphicsInstanceRef_ptr`). It also generally contains the extended object after the common object. For example GraphicsInstance has `GraphicsInstance_ext` which can get the API-specific data of the GraphicsInstance. This is should only be used internally, as there is no public interface of this extended data that won't randomly change.

## Graphics instance

### Summary

The graphics instance is the way you can query physical devices from D3D12, Vulkan or Metal3. First one has to be created as follows:

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

- readonly application: The name and version of the application.
- readonly api: Which api is ran by the runtime: Vulkan, DirectX12, Metal3 or WebGPU.
- readonly apiVersion: What version of the graphics api is being ran (e.g. Vulkan 1.2, DirectX 12_2, Metal 3, etc.).

### (Member) Functions

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
    false, /* isVerbose; extra logging about the device properties */
    &deviceInfo
));
```

### Properties

- readonly name, driverName, driverInfo; All null-terminated UTF-8 strings giving information about the device and driver.
- readonly type; what type this device is (dedicated GPU, integrated GPU, simulated GPU, CPU or other (unrecognized)).
- readonly vendor; what company designed the device (Nvidia (NV), AMD, ARM, Qualcomm (QCOM), Intel (INTC), Imagination Technologies (IMGT), Apple (APPL) or unknown).
- readonly id; number in the list of supported devices.
- readonly luid; ID to identify this device primarily on Windows devices. This would allow sharing a resource between other APIs for interop (not supported yet). This is optional to support; check capabilities.features & LUID.
- readonly uuid; unique id to identify the device. In APIs that don't support this natively, the other identifier (luid) will be used here instead. For example DirectX12 would use the luid here and clear the other U64.
- readonly ext; extended physical device representation for the current API.
- readonly capabilities; what data types, features and api dependent features are enabled. See capabilities section.

#### Capabilities

- features: DirectRendering, VariableRateShading, MultiDrawIndirectCount, MeshShader, GeometryShader, TessellationShader, SubgroupArithmetic, SubgroupShuffle, Swapchain, Multiview, Raytracing, RayPipeline, RayIndirect, RayQuery, RayMicromapOpacity, RayMicromapDisplacement, RayMotionBlur, RayReorder, LUID, DebugMarkers, Wireframe, LogicOp, DualSrcBlend.
- features2: reserved for future features.
- dataTypes: I64, F16, F64, AtomicI64, AtomicF32, AtomicF64, ASTC, BCn, MSAA2x, MSAA8x, MSAA16x.
  - MSAA4x and MSAA1x (off) are supported by default.
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
    false, /* isVerbose; extra logging about the device properties */
    &device
));
```

### Properties

- readonly instance; owning instance.
- readonly info; physical device.
- readonly submitId; counter of how many times submit was called (can be used as frame id).
- readonly lastSubmit, firstSubmit; used to track when a submit was called.
- readonly pendingResources, resourcesInFlight; used to track if resources are dirty, in flight (in use on the GPU) and if they need updates in the next submit.
- readonly allocator; used to allocate memory.

### Functions

- ```c
  Error submitCommands(List<CommandListRef*> commandLists, List<SwapchainRef*> swapchains);
  ```

  - Submits commands to the device and readies the swapchains to present if available. If the device doesn't have any swapchains, it can be used to just submit commands. This is useful for multi GPU rendering as well.

- ```c
  wait();
  ```

  - Waits for all currently queued commands on the device.

- ```c
  Error createSwapchain(SwapchainInfo info, SwapchainRef **swapchain);
  ```

- ```c
  Error createCommandList(
  	U64 commandListLen, 
  	U64 estimatedCommandCount,
  	U64 estimatedResources,
      Bool allowResize,
  	CommandListRef **commandList
  );
  ```

- ```c
  Error createPipelinesCompute(
      List<Buffer> computeBinaries, 
      optional List<CharString> names,
      PipelineRef **computeShaders
  );
  ```
```
  
- ```c
  Error createPipelinesGraphics(
  	List<PipelineStage> stages, 
      List<PipelineGraphicsInfo> infos, 
      optional List<CharString> names,
      List<PipelineRef*> *pipelines
  );
```

- ```C
  Error createBuffer(
  	EDeviceBufferUsage usage,
   	CharString name,
      U64 len,
      DeviceBufferRef **ref
  );
  ```

- ```c
  Error createBufferData(
  	EDeviceBufferUsage usage,
   	CharString name,
      Buffer *data,					//Moves data to device buffer
      DeviceBufferRef **ref
  );
  ```

### Obtained

- Through GraphicsDeviceRef_create; see overview.

## Command list

### Summary

A command list in OxC3 is a virtual command list. The commands get recorded in a standardized format, but they don't get translated until they're submitted to the graphics device. The graphics device is then responsible for synchronization and ensuring the commands are recorded in the optimal way. Command lists themselves don't have any API specific implementation to avoid unexpected caching behavior. When submitting multiple command lists, they'll act as if they're one long command list. This would allow maintaining state from the old command list as well.

```c
CommandListRef *commandList = NULL;
_gotoIfError(clean, GraphicsDeviceRef_createCommandList(
    device, 	//See "Graphics device"
    2 * KIBI, 	//Max command buffer size
    64, 		//Estimated command count (can auto resize)
    64, 		//Estimated resource count (can auto resize)
    true,		//Allow resize
    &commandList
));
```

#### Recording a command list

```c
_gotoIfError(clean, CommandListRef_begin(commandList, true /* clear previous */));

_gotoIfError(clean, CommandListRef_startScope(commandList, (List) { 0 }, 0, (List) { 0 },));
_gotoIfError(clean, CommandListRef_clearImagef(
    commandList, F32x4_create4(1, 0, 0, 1), (ImageRange){ 0 }, swapchain
));
_gotoIfError(clean, CommandListRef_endScope(commandList));

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

- readonly device; owning device.
- readonly state; if the command list has been opened before and if it's open or closed right now.
- readonly allowResize; if command list is allowed to resize if it runs out of memory.
- readonly resources; Which resources are in use by the command list.
- private readonly:
  - data; The current recorded commands.
  - commandOps; The opcodes for which commands are recorded.
  - callstacks; On debug mode, contains the stacktrace of each command (up to 16 deep callstack). Used for debugging if an error occurs in the API-dependent layer.
  - next; the next offset into the data buffer for the next command to record.
  - transitions; list of transitions issued in the command list. Scopes point into this to execute transitions.
  - activeScopes; list of scopes that weren't collapsed (scope command id & scope id, what transitions it did and how many commands it contains & the length of the sub command buffer).
  - computePipeline/graphicsPipeline; currently bound pipeline.
  -  tempStateFlags; AnyScissor, AnyViewport, HasModifyOp, HasScope, InvalidState. Used for validation and optimization.
  - debugRegionStack; used for validating debug regions.
  - lastCommandId, lastOffset, lastTransition; locations at the start of a scope for command id, buffer location and transition offset. Used for the next scope description.
  - currentSize; size of the current renderpass.
  - pendingTransitions; list of transitions waiting for the current scope. Get pushed into transitions if the scope is visible.

### Functions

See the "Commands" section.

### Used functions and obtained

- Obtained through GraphicsDeviceRef's createCommandList, see overview.
- Used in GraphicsDeviceRef's submitCommands.
- Also used in command recording (See "Commands" section).

## Swapchain

### Summary

A swapchain is the interface between the platform Window and the API-dependent way of handling the surface, swapchain and any synchronization methods that are required.

Swapchain isn't always supported. In that case your final target can just be a render texture and it can be pulled back to the CPU to allow presenting there (video, photo, or whatever). To make sure it's supported, check deviceInfo.capabilities.features for Swapchain support.

```c
SwapchainRef *swapchain = NULL;
_gotoIfError(clean, GraphicsDeviceRef_createSwapchain(
    device, 		//See "Graphics device"
    (SwapchainInfo) { .window = w, .usage = ESwapchainUsage_AllowCompute }, 
    &swapchain
));

//Window callback to make sure format + size stays the same: 

void onResize(Window *w) {
	if(!(w->flags & EWindowFlags_IsVirtual))
		Swapchain_resize(SwapchainRef_ptr(swapchain));
}
```

info.presentModePriorities are the requests for what type of swapchains are desired by the application. Keeping this empty means [ mailbox, fifo, fifoRelaxed, immediate ]. On Android mailbox is unsupported because it may introduce another swapchain image, the rest is driver dependent if it's supported. Immediate is always supported, so make sure to always request immediate otherwise createSwapchain may fail (depending on device + driver). For more info see Swapchain/Present mode.

info.usage in this case allows the swapchain to be used for compute shaders. If this is not required, please try to avoid it. It might reduce or remove compression for the swapchain depending on the underlying hardware.

### Present mode

The present mode is how the device handles it when an image is already being presented. Some may introduce tearing while others may drop frames. The application should be given control over these modes, as some applications may want the latency improvements at the cost of tearing.

- Mailbox: While an image is presenting, it will drop the oldest queued image and continue rendering and queuing the next frame. This ensures you're not bound to your refresh rate, so the performance is better but lots of frames are rendered that may be discarded. This present mode is generally ideal for games and other interactive 3D applications, though some very low latency games might not want to use this. This mode is the default on most platforms.
- Fifo: While an image is presenting, it will just wait. This means no frames are dropped, but it does mean that the performance is lower. This is ideal for low power devices or if your application doesn't need constant updates. This is the default on mobile.
- FifoRelaxed: If the vsync interval is missed, it will try to skip frame(s) to catch up. Other than that it will behave similar to Fifo (it tries to keep up with the refresh rate of the screen).
- Immediate: Render over the current image anyways, this is ideal if you want the best low latency available but will introduce tearing. A good application might be shooting games or other games that require the lowest latency.

By default the swapchain will use triple buffering to ensure best performance. Even mobile benefits from this, so it was decided not to expose this setting to simplify the backend.

### Properties

- readonly info.window: the Window handle created using OxC3 platforms.
- readonly info.requiresManualComposite: Whether or not the application is requested to explicitly handle rotation from the device. For desktop this is generally false, for Android this is on to avoid the extra overhead of the compositor. 
- readonly info.presentModePriorities: What present modes were requested on create.
- readonly device: The owning device.
- readonly size: Current size of the swapchain.
- readonly format: Current format of the swapchain.
- readonly versionId: The version id changes everytime a change is applied. This could be resizes or format changes.
- readonly presentMode: What present mode is currently used (see Swapchain/Present mode).
- readonly usage: What the swapchain is allowed to be used for.

### Functions

- ```c
  resize();
  ```

  - Updates the swapchain with the current information from OxC3 platforms. Both format and size are queried again to ensure it really needs to resize. After this, all recorded command lists will still be valid, but the API implementation will likely re-record their command lists if it was already re-using them.

### Used functions and obtained

- Obtained through GraphicsDeviceRef's createSwapchain, see overview.
- Used in GraphicsDeviceRef's submitCommands as well as read & write image commands and dispatches.

## Pipeline

### Summary

A pipeline is a combination of the states and shader binaries that are required to run the shader. This represents a VkPipeline in Vulkan, an ID3D12PipelineState or ID3D12StateObject in DirectX12 and a `MTL<Render/Compute>PipelineState` in Metal.

### Properties

- readonly device: ref to the graphics device that owns it.
- readonly type: compute, graphics or raytracing pipeline type.
- readonly stages: `List<PipelineStage>` the binaries that are used for the shader.
  - stageType: which stage the binary is for. This is not necessarily unique, but should be unique for graphics shaders and compute. For raytracing shaders there can be multiple for the same stage.
  - shaderBinary: the format as explained in "Shader binary types" that is required by the current graphics API.
- readonly extraInfo: a pointer to behind the API dependent pipeline extension struct that allows extra info that's only applicable to a certain pipeline type. 
  - For compute: this is NULL.
  - For graphics: this is PipelineGraphicsInfo.
  - For raytracing: this is PipelineRaytracingInfoExt.

### Used functions and obtained

- Obtained through GraphicsDeviceRef's createPipelinesGraphics and createPipelinesCompute.
- Used in CommandListRef's bindComputePipeline & bindGraphicsPipeline.

### PipelineGraphicsInfo

Pipeline graphics info is the abstraction about the entire graphics state and roughly maps to VkGraphicsPipelineCreateInfo, D3D12_GRAPHICS_PIPELINE_STATE_DESC and Metal's descriptors. It provides a simplified usage while supporting most important features from the APIs.

The graphics pipeline has the following properties:

- vertexLayout: how to interpret vertex and instance data bound to the vertex bind points at draw call.
  - Optional if the shader generates all data itself (default to 0).
  - bufferStrides12_isInstance1[16]: buffer description and if it's an instance.
    - bufferStride is limited to 0-2048 for device limit reasons. So the other values (2048-4095) are invalid.
    - isInstance is when the buffer changes; if it's false it changes per vertex, otherwise it changes per instance.
    - The buffer id is the same as the index into the array. So [0] describes vertex buffer bound at id 0.
    - These are tightly packed to avoid having to dynamically allocate the PipelineGraphicsInfo and keeping it POD while still using limited resources. (bufferStride & 4095) | (isInstance << 12).
  - attributes[16]: the vertex attributes that use the buffers defined before.
    - inferred semanticName: for HLSL/DirectX12, semantic name is quite important. However, it supports semantic name and value, so we just use semantic name TEXCOORD and the binding id. So TEXCOORD1 would be attribute[1]. This is done to save a lot of space in the PipelineGraphicsInfo. Our custom HLSL syntax for this is `_bind(N)`.
    - format: ETextureFormatId (8-bit) such as 'RGB32f'.
    - buffer4 (0-15): buffer id the attribute point to. Points to vertexLayout.bufferStrides12_isInstance1.
    - offset11: offset into the buffer.
      - offset is 0-2047 for device limit reasons. Since the offset is into the buffer, offset + size can't exceed the stride of the buffer. 
    - attribute id is inferred from the index into the array. If attribute bindings are used [0] would refer to binding 0.
- rasterizer: how to rasterize the triangles into pixels.
  - Optional if no special rasterizer info is needed. Default to 0 will to create CCW backface-culled filled geometry with no depth clamp or bias.
  - cullMode: Back (default), None, Front
  - flags: IsCW (1), IsWireframeExt (2), EnableDepthClamp (4), EnableDepthBias (8).
    - IsWireframeExt requires the Wireframe feature.
  - depthBiasClamp, depthBiasConstantFactor, depthBiasSlopeFactor all define depthBias properties that only do something if enableDepthBias is on. depthBiasClamp needs enableDepthClamp as well.
- depthStencil: how to handle depth and stencil operations.
  - Optional if no special depth stencil is needed. Default to no depth or stencil operations.
  - flags: DepthTest (1), DepthWriteBit (2), StencilTest (4).
    - For DepthWrite (3), both DepthTest and DepthWriteBit are set.
  - depthCompare, stencilCompare: compare operations for depth and stencil.
    - Gt (default), Geq, Eq, Neq, Leq, Lt, Always, Never.
  - stencilFail, stencilPass, stencilDepthFail: operations for when a stencil event occurs (fail, pass, depthFail).
    - Keep, Zero, Replace, IncClamp, DecClamp, Invert, IncWrap, DecWrap.
  - stencilReadMask, stencilWriteMask: what value the stencil is compared to when reading or writing.
- blendState: how to handle blend operations.
  - Optional if no special blend state is needed. Default to writeMask on (see enable).
  - enable: whether or not to enable the blend state. If this is disabled then all attachments that are used for the pipeline will have the entire write mask enabled and blending disabled.
  - allowIndependentBlend: if disabled only reads from attachments[0], writeMask[0] and renderTargetMask & 1.
  - renderTargetMask: Bool[8] (U8) of which attachments have blend enabled.
  - logicOpExt (default = off): if the logicOp feature is enabled allows to define the logic op the blend will perform.
    - Off, Clear, Set, Copy, CopyInvert, None, Invert, And, Nand, Or, Nor, Xor, Equiv, AndReverse, AndInvert, OrReverse, OrInvert.
  - writeMask[16]: EWriteMask mask of which channel writes are enabled for write (R: 1, G: 2, B: 4, A: 8). Can also be RGBA, RGB, RG.
  - attachments[16]: blend information about each state.
    - srcBlend, dstBlend, srcBlendAlpha, dstBlendAlpha: what value to blend.
      - Zero, One, SrcColor, InvSrcColor, DstColor, InvDstColor, SrcAlpha, InvSrcAlpha, DstAlpha, InvDstAlpha, BlendFactor, InvBlendFactor, AlphaFactor, InvAlphaFactor, SrcAlphaSat.
      - If dualSrcBlend feature is enabled: Src1ColorExt, Src1AlphaExt, InvSrc1ColorExt, InvSrc1AlphaExt.
    - blendOp, blendOpAlpha: what operation to blend with.
      - Add, Subtract, ReverseSubtract, Min, Max.
- msaa: multi sample count. 
  - Optional if no special msaa settings are needed. Defaults to 1.
  - 1 and 4 are always supported (though 4 is slower and needs special care). 
  - 2, 8 and 16 aren't always supported, so make sure to query it and/or fallback to 1 or 4 if not present. EGraphicsDataTypes of the device capabilities lists this.
- topologyMode: type of mesh topology.
  - Defaults to TriangleList if not specified.
  - TriangleList, TriangleStrip, LineList, LineStrip, PointList, TriangleListAdj, TriangleStripAdj, LineListAdj, LineStripAdj.
- patchControlPointsExt: optional feature TessellationShader. Defines the number of tessellation points.
- stageCount: how many shader stages are available.
- Using DirectRendering:
  - If DirectRendering is enabled, a simpler way of creating can be used to aid porting and simplify development for desktop.
  - attachmentCountExt: how many render targets should be used.
  - attachmentFormatsExt[i < attachmentCountExt]: the ETextureFormatId of the format. Needs to match the render target's exactly (Swapchain BGRA8 doesn't match an RGBA8 pipeline!).
  - depthFormatExt: depth format of the depth buffer: None, D16, D32, D24S8, D32S8.
- **TODO**: Not using DirectRendering:
  - If DirectRendering is not supported or the developer doesn't want to use it; a unified mobile + desktop architecture can be used. However; generally desktop techniques don't lend themselves well for mobile techniques and vice versa. So it's still recommended to implement two separate rendering backends on mobile.
  - **TODO**: renderPass:
  - **TODO**: subPass: 

#### PipelineStages

A pipeline stage is simply a Buffer and an EPipelineStage. The Buffer is in the format declared in "Shader binary types" and EPipelineStage can be Vertex, Pixel, Compute, GeometryExt, HullExt or DomainExt. Hull and domain are enabled by the TessellationShader feature and GeometryExt by the GeometryShader feature.

### Compute example

```c
tempShader = ...;		//Buffer: Load from virtual file or hardcode in code.

CharString nameArr[] = {
    CharString_createConstRefCStr("Test compute pipeline")
};

List computeBinaries = (List) { 0 };
_gotoIfError(clean, List_createConstRef(&tempShader, 1, sizeof(Buffer), &computeBinaries));
_gotoIfError(clean, List_createConstRef((const U8*) &nameArr, 1, sizeof(nameArr[0]), &names));
_gotoIfError(clean, GraphicsDeviceRef_createPipelinesCompute(device, &computeBinaries, names, &computeShaders));

tempShader = Buffer_createNull();
```

Create pipelines will take ownership of the buffers referenced in computeBinaries and it will therefore free the list (if unmanaged). If the buffers are managed memory (e.g. created with Buffer_create functions that use the allocator) then the Pipeline object will safely delete it. This is why the tempShader is set to null after (the list is a ref, so doesn't need to be). In clean, this temp buffer gets deleted, just in case the createPipelines fails. Using virtual files for this is recommend, as they'll already be present in memory and our ref will be available for the lifetime of our app. If it's a ref that doesn't always stay active, be sure to manually copy the buffers to avoid referencing deleted memory. 

It is recommended to generate all pipelines that are needed in this one call at startup, to avoid stuttering at runtime.

### Graphics example

```c
//... Load shaders from virtual file system into tempShaders[0], [1]
//Define the entrypoints

PipelineStage stage[2] = {
    (PipelineStage) {
        .stageType = EPipelineStage_Vertex,
        .shaderBinary = tempShaders[0]
    },
    (PipelineStage) {
        .stageType = EPipelineStage_Pixel,
        .shaderBinary = tempShaders[1]
    }
};

List stageInfos = (List) { 0 };
_gotoIfError(clean, List_createConstRef(
    (const U8*) stage, sizeof(stage) / sizeof(stage[0]), sizeof(stage[0]), &stageInfos
));

//Define all pipelines.
//These pipelines require the graphics feature DirectRendering and will error otherwise!
//Otherwise .attachmentCountExt, .attachmentFormatsExt and/or .depthStencilFormat
//  need to be replaced with .renderPass and/or .subPass.

PipelineGraphicsInfo info[1] = {
    (PipelineGraphicsInfo) {
        .stageCount = 2,
        .attachmentCountExt = 1,
        .attachmentFormatsExt = { ETextureFormat_BGRA8 }
    }
};

//Create pipelines, this will take ownership of the binaries (if they're managed).
//Make sure to release them after to avoid freeing twice!

List infos = (List) { 0 };
_gotoIfError(clean, List_createConstRef((const U8*) info, sizeof(info) / sizeof(info[0]), sizeof(info[0]), &infos));
_gotoIfError(clean, GraphicsDeviceRef_createPipelinesGraphics(
    device, &stageInfos, &infos, &graphicsShaders
));

tempShaders[0] = tempShaders[1] = Buffer_createNull();
```

Create pipelines will take ownership of the buffers referenced in stages and it will therefore free the list (if unmanaged). If the buffers are managed memory (e.g. created with Buffer_create functions that use the allocator) then the Pipeline object will safely delete it. This is why the tempShader is set to null after (the list is a ref, so doesn't need to be). In clean, this temp buffer gets deleted, just in case the createPipelines fails. Using virtual files for this is recommend, as they'll already be present in memory and our ref will be available for the lifetime of our app. If it's a ref that doesn't always stay active, be sure to manually copy the buffers to avoid referencing deleted memory. 

It is recommended to generate all pipelines that are needed in this one call at startup, to avoid stuttering at runtime.

## Shader binary types

In OxC3 graphics, either the application or the OxC3 baker is responsible for compiling and providing binaries in the right formats. According to OxC3 graphics, the shaders are just a buffer, so they can contain anything (text or binary). Down here is a list of the expected inputs for each graphics API if the developer wishes to sidestep the baking process:

- DirectX12: DXIL (binary).
  - HLSL Semantics can only be TEXCOORDi where i < 16. This maps directly to the binding.
- Vulkan: SPIR-V (binary).
  - HLSL Entrypoint needs to be remapped to main.
- Metal: MSL (text).
- WebGPU: WGSL (text).

The OxC3 baker will (if used) convert HLSL to SPIR-V, DXIL, MSL or WGSL depending on which API is currently used. It can provide this as a pre-baked binary too (.oiCS Oxsomi Compiled Shader). The pre-baked binary contains all 4 formats to ensure it can be loaded on any platform. But the baker will only include the one relevant to the current API to prevent bloating.

When using the baker, the binaries can simply be loaded using the oiCS helper functions and passed to the pipeline creation, as they will only contain one binary. 

**TODO: The baker currently doesn't include this functionality just yet.** 

## DeviceBuffer

### Summary

A DeviceBuffer is a buffer partially or fully located on the device (such as a GPU). A device buffer defines the usage for various purposes such as; a vertex buffer, index buffer, indirect arguments buffer, shader read/write and if it is allocated on the CPU and accessible by the device or fully on the device (with potential access from the CPU). It also specifies if it should allocate a CPU copy to hold temporary data for future buffer updates. 

```c
VertexPosBuffer vertexPos[] = {
    (VertexPosBuffer) { { F32_castF16(-0.5f), F32_castF16(-0.5f) } },
    (VertexPosBuffer) { { F32_castF16(0.5f), F32_castF16(-0.5f) } },
    (VertexPosBuffer) { { F32_castF16(0.5f), F32_castF16(0.5f) } },
    (VertexPosBuffer) { { F32_castF16(-0.5f), F32_castF16(0.5f) } }
};

Buffer vertexData = Buffer_createConstRef(vertexPos, sizeof(vertexPos));
CharString name = CharString_createConstRefCStr("Vertex position buffer");
_gotoIfError(clean, GraphicsDeviceRef_createBufferData(
    device, EDeviceBufferUsage_Vertex, name, &vertexData, &vertexBuffers[0]
));
```

### Properties

- readonly device: ref to the graphics device that owns it.
- readonly usage: ShaderRead (accessible for read from shader), ShaderWrite (accessible for write from shader), Vertex (use as vertex buffer), Index (use as index buffer), Indirect (use for indirect draw calls), CPUBacked (There's a CPU copy of the buffer to facilitate reads/writes), CPUAllocated (The entire resource has to be located in "shared" memory or on the CPU if there's a dedicated GPU).
- readonly isPending(FullCopy): Information about if any data is pending for the next submit and if the entire resource is pending.
- readonly length: Length of the buffer.
- readonly cpuData: If CPUBacked stores the CPU copy for the resource or temporary data for the next submit to copy CPU data to the real resource.
- readonly pendingChanges: `[U64 startRange, U64 endRange][]` list of marked regions for copy.
- readonly readHandle, writeHandle: Places where the resource can be accessed on the GPU side. If a shader uses the writeHandle in a shader it has to transition the resource (or the subresource) to write state before it is accessed as such (at the relevant shader stage); same with the readHandle (but read state). If you're only reading/writing from a part of a resource it is preferred to only transition part of the resource. This will signal the implementation that other parts of the resource aren't in use. Which could lead to more efficient resource updates for example. Imagine streaming in/out meshes from a single buffer; only meshes that are in use need to be updated with the staging buffer, while others could be directly copied to GPU visible memory if available (ReBAR, shared mem, cpu visible, etc.). It could also reduce decompression/compression time occurring on the GPU due to changing the entire resource to write instead of readonly (with subresources this could be eased depending on the driver). 

### Functions

- `markDirty(U64 offset, U64 length)` marks part or entire resource dirty. This means that next commit the implementation will decide on how to copy to the resource in an efficient way. For example if the resource isn't in flight and ReBAR is turned on or shared memory is in use then it can directly copy to CPU accessible memory. Otherwise it might have to use a copy queue or something similar. The region is merged with any other pending region 256 bytes on either side to avoid lots of fragmented copies. Please make sure to only call this when necessary as this might cause extra allocations or copies to a RingBuffer; a good strategy (if the buffer changes each frame) might be to make the buffer 3x as big as necessary and index based on frameId % 3 and make the buffer CPUAllocated to allow direct writes always and then copy from this buffer only when needed. The framework will try to optimize these copies as much as possible (to avoid having to do that manually), but more work might be needed from the developers using it instead.

### Used functions and obtained

- Obtained through createBuffer and createBufferData from GraphicsDeviceRef. 
- Used in DeviceBufferRef's markDirty, as vertex/index/indirect buffer for commands such as draw/drawIndirect/drawIndirectCountExt, shaders if the resource is readable/writable (through transitions), copy and clear buffer operations. 

## Commands

### Summary

Command lists store the commands referenced in the next section. These are virtual commands; they approximately map to the underlying API. If the underlying API doesn't support commands, it might have to simulate the behavior with a custom shader (such as creating a mip chain of an image). It is also possible that a command might need certain extensions, without them the command will give an error to prevent it from being inserted into the command list (some unimportant ones such as debugging are safely ignored if not supported instead). These commands are then processed at runtime when they need to. If the command list remains the same, it's the same swapchain (if applicable) and the resources aren't recreated then it can safely be re-used. 

Invalid API usage will be attempted to be found out when inserting the command, but this is not always possible. 

### begin/end

To make sure a command list is ready for recording and submission it needs begin and end respectively. Begin has the ability to clear the command list (which is generally desired). However, it's also possible that the command list was closed by one library and then given back to another. In this case, it can safely be re-opened without the clear flag. After ending it, it can be passed to submitCommands. These "commands" are special, as they're not inserted into the command list itself, they just affect the state of the command list. Only one submit is allowed per frame, as it handles synchronization implicitly.

### setViewport/(And)Scissor

setViewport and setScissor are used to set viewport and scissor rects respectively. However, since in a lot of cases they are set at the same time, there's also a command that does both at once "setViewportAndScissor". 

```c
_gotoIfError(clean, CommandListRef_setViewportAndScissor(
	commandList,
    I32x2_create2(0, 0),	//Offset
    I32x2_create2(0, 0)		//Size
));
```

This has to be called during a render call. Size can also be 0 to indicate full size of render target.

Since this is relative to a render target, it has to be called after binding one. If the next render target bound doesn't change in resolution then this is still valid. The offset + size needs to be inside of the framebuffer's resolution (if size is 0 then it will be stretched to fill the rest of the render target).

### Immediate state: setStencil, setBlendConstants

The stencil reference can be set using the setStencil command.

```c
_gotoIfError(clean, CommandListRef_setStencil(commandList, 0xFF));
```

And the blend constants can be set like so:

```c
_gotoIfError(clean, CommandListRef_setBlendConstants(
    commandList, F32x4_create4(1, 0, 0, 1)
));
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

To clear multiple at once, call clearImages with a `List<ClearImage>`. ClearImage takes a color, a range and the image ref ptr. It's the same as a single clear image but it allows multiple.

Clear image is only allowed on images which aren't currently bound as a render target. The image is leading in determining the format which will be read out. If you use clearImagef but it's a uint texture then it will bitcast the float color to a uint for you.

Clear image can currently only be called on a Swapchain object.

### setComputePipeline/setGraphicsPipeline

The set pipeline command does one of the following; bind a graphics pipeline, raytracing pipeline or a compute pipeline. These pipelines are the only bind points and they're maintained separately. So a bind pipeline of a graphics shader and one of a compute shader don't interfere. This is used before a draw, dispatch or traceRaysExt to ensure the shader is used.

```c
_gotoIfError(clean, CommandListRef_setComputePipeline(commandList, pipeline));
```

### draw

The draw command will simply draw the currently bound primitive buffer (optional) with the currently bound graphics pipeline (required). If the primitive buffer is not present, the pipeline is expected to generate the vertices dynamically. It requires a render call to be started (see DirectRendering). It handles both indexed and non indexed draw calls:

```c
//Non indexed draw call
//Can also specify instanceOffset and vertexOffset if drawUnindexedAdv is used

_gotoIfError(clean, CommandListRef_drawUnindexed(commandList, 3, 1));

//Indexed draw call
//Can also specify instanceOffset, indexOffset and vertexOffset
//if drawIndexedAdv is used

_gotoIfError(clean, CommandListRef_drawIndexed(commandList, 3, 1));
```

When issuing the draw, the state needs to be valid: a render has to be started (render pass or direct rendering), a primitive buffer needs to be bound if relevant, graphics pipeline has to be bound, viewport & scissor has to be set and all relevant transitions need to be done.

- Graphics pipeline needs to be compatible with currently bound render targets; this means the formats specified in graphics pipeline creation need to match the same formats of the render targets.
- States of currently used resources need to be correct. If you write to a resource it needs to be transitioned to write using the transition command and it needs to specify the first shader which *might* read from/write to it. Same is also true when reading from it. The state of these resources stays as it was when transitioned unless the same resource was used in a different explicit or implicit transition. Implicit transitions can be: binding it as a render target, clearing it, copying it or any other command that is specified in this document as transitioning the resource. So this command should only be used if the state of the resource has already changed. So when the same resources are already transitioned to read then they stay that until they're modified by something else. *For writes however, it is **essential** to transition them even if they're in write already. This is to ensure the command that modified the resource is finished before writing again.* 
- If the state uses a stencil then it needs to set a stencil ref using CommandListRef_setStencil.
- If the state uses a blend type that uses the blend constants then it has to set the blend constants using CommandListRef_setBlendConstants.

#### Example of a legacy draw call system (Pseudocode)

```c
//Transition all drawn materials to read in scope
//Bind render target(s)
//Bind viewport/scissor
//Bind primitive buffers (allocate all meshes into one buffer for less rebinds)
//Foreach shader:
//  Bind pipeline 
//	All draw calls of relevant objects
```

The example above should be fine as long as the draw calls don't need extra synchronization because they write and read from a shared resource. In that case, it would need a transition before drawing the next (and might need to end the scope). However, this transition should only be done for the resources that can be modified from the draw call(s), as long as they're not the currently bound textures. These currently bound textures can't be used in a transition or the shader as a read/write input as well.

To find a more modern way of rendering, check out the multi draw indirect section.

This command is generalized with the `draw` command which takes the `Draw` struct which can issue both indexed an unindexed draw calls (with or without advanced usage).

Primitive buffers should only deviate when necessary. Please try to combine multiple meshes into a single mesh as a suballocation. Example could be counting the vertices and indices, making sure the index type and vertex formats/bindings/attributes are the same and allocating only once. Then when issuing draw calls, the index and vertex offset can be used in the draw command.

#### drawIndirect

Same as draw except the device reads the parameters of the draw from a DeviceBuffer.

- buffer: a buffer with the DrawCallUnindexed or DrawCallIndexed struct(s) depending on the 'indexed' boolean. Buffer needs to enable Indirect usage to be usable by indirect draws.
- bufferOffset: offset into the draw call buffer. Align to 16-byte (unaligned is disallowed).
- bufferStride: stride of the draw call buffer. If 0 it will be defaulted to tightly packed (16 or 32 byte depending on if it's indexed or not). Has to be bigger or equal to the current draw call struct size.
- drawCalls: how many draw calls are expected to be filled in this buffer. A draw can also set the draw parameters to zero to disable it (instanceCount / index / vertexCount), though for that purpose drawIndirectCountExt is recommended. Make sure the buffer has `U8[bufferStride][maxDrawCalls]` allocated at bufferOffset. 
- indexed: if the draw calls are indexed or not. The device can't combine non indexed and indexed draw calls, so if you want to combine them you need to do this as two separate steps.
  - If not indexed; the buffer takes a DrawCallUnindexed struct: U32 vertexCount, instanceCount, vertexOffset, instanceOffset.
  - Otherwise; the buffer takes a DrawCallIndexed struct: U32 indexCount, instanceCount, indexOffset, I32 vertexOffset, U32 instanceOffset, U32 padding[3].

drawIndirect transitions the input buffer to IndirectDraw. This means that the buffer can't also be bound as a Vertex/Index buffer or be used in the shader as a read/write buffer in the same scope. Buffer needs to enable Indirect usage to be usable by indirect draws.

#### drawIndirectCountExt

Same as drawIndirect, except it adds a DeviceBuffer counter which specifies how many active draw calls there are. This is very useful as it allows culling to be done entirely by compute. 

- drawCalls now represents 'maxDrawCalls' which limits how many draw calls might be issued by the device.
- countBuffer now represents a U32 in the DeviceBuffer at the offset. Make sure to align 4-byte to satisfy alignment requirements.
- Requires drawIndirectCount extension.

drawIndirect transitions the input buffer to IndirectDraw. This means that the buffer can't also be bound as a Vertex/Index buffer or be used in the shader as a read/write buffer in the same scope. Buffer needs to enable Indirect usage to be usable by indirect draws.

### setPrimitiveBuffers

Sets the primitive buffers (vertex + index buffer(s)) for use by draw commands such as draw, drawIndirect and drawIndirectCountExt. The buffers need the Vertex and/or Index usage set if they're used for that purpose. In the same scope of the setPrimitiveBuffers it is illegal to transition the subresource(s) back to a different state or to write/read from other sources that use it (check the Scope section of this document). The vertex buffer(s) need to have the same layout as specified in the pipeline and the ranges specified by the draw calls (such as count and offset) need to match up as well. 

### dispatch

Dispatch has some of the same requirements as draw calls as it needs a correct state of the resources and needs a compute pipeline bound as well. However, it doesn't need to be in an active render (render pass or direct rendering) and it also doesn't use the viewport/scissor. This makes compute one of the easiest to handle, though transitions are just as important as with graphics shaders (resources need to be transitioned through a scope).

```c
_gotoIfError(clean, CommandListRef_dispatch2D(commandList, tilesX, tilesY));
```

dispatch2D, dispatch1D and dispatch3D are the easiest implementations. You dispatch in groups, so it has to be aligned to the thread count of the compute shader. This command is also generalized with the dispatch command which takes in a `Dispatch` struct.

#### dispatchIndirect

Same thing as dispatch, except the device reads from a U32x3 into the DeviceBuffer at the offset and dispatches the groups stored there. Has to be aligned to 16-byte. For 2D dispatches please set z to 1, for 1D set y to 1 as well. 

dispatchIndirect transitions the input buffer to IndirectDraw. This means that the buffer can't also be bound as a Vertex/Index buffer or be used in the shader as a read/write buffer in the same scope. Buffer needs to enable Indirect usage to be usable by indirect draws.

### DebugMarkers feature

The DebugMarkers feature adds three commands: addMarkerDebugExt, startRegionDebugExt and endRegionDebugExt. If the DebugMarkers feature isn't present, these are safely ignored and won't be inserted into the virtual command list. This can be used to provide extra debugging info to tools such as RenderDoc, NSight, Pix, etc. to show where important events such as render calls happened and what they represented. A debug region or debug marker has a color and a name. A debug region is like a stack; you can only end the current region and push another region. Every region you start has to be ended in the same scope that is submitted.

```c
_gotoIfError(clean, CommandListRef_startRegionDebugExt(
    commandList, 							//See "Command list"
    F32x4_create4(1, 0, 0, 1), 				//Marker color
    CharString_createConstRefCStr("Test")	//Marker name
));

//Insert operation that needs to be in the debug region

_gotoIfError(clean, CommandListRef_endRegionDebugExt(commandList));
```

The same syntax as startRegionDebugExt can be used for addMarkerDebugExt. Except a marker doesn't need any end, it's just 1 event on the timeline. Every end region needs to correspond with a start region and vice versa.

### DirectRendering feature

DirectRendering allows rendering without render passes (default behavior in DirectX). This makes development for desktop a lot easier since AMD, Intel and NVIDIA aren't using tiled based deferred rendering (TBDR). However, all other vendors (such as Qualcomm, ARM, Apple, Imgtec) do use TBDR (mostly mobile architectures). The user is allowed to decide that this is a limitation they accept and can use this feature to greatly simplify the difficulty of the graphics layer (especially porting from existing apps). The user can also set up two different render engines; one that can deal with direct rendering and one that can't. The latter is targeted at mobile (lower hardware tier) and the former is for desktop/console. The two commands that are related to this feature are: startRenderExt and endRenderExt. They require the feature to be present and will return an error (and won't be inserted into the command list) otherwise.

Even if mobile chips do support this feature, it is automatically disabled to prevent the developer from accidentally enabling it causing performance issues.

Just like *most* commands, this will automatically transition the resources (render targets only) into the correct states for you. Color attachments can always be read, but the user is in charge of specifying if the contents should be cleared or kept. Color attachments can also be readonly if needed.

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

*Keep in mind that during the scope, you can't transition the resources passed into the attachments of the active render call. They're not allowed to be used as a read (SRV) or write textures (UAV); they're only allowed to be output attachments (RTV). Doing otherwise will cause the command list to give an error and remove the current scope.*

Every startRender needs to match an endRender. During the render it's not allowed to access the render textures as a write or read texture. Other operations that change state (implicit transitions) such as clears and copies are also not allowed. If this is required, the developer can end the render and restart it after this operation.

A graphics pipeline for use with DirectRendering needs to set the attachment count (and format(s)) or the depth stencil format. One that doesn't use direct rendering can't be used.

### Scope (startScope/endScope)

A scope is the replacement of the "transition" command. The scope makes sure all resources are in the right state for the commands that follow and it collects transitions from any other commands in the scope. It also makes sure to signal the api that the resources referenced are still in flight and shouldn't be deleted. When a scope is exited, it will undo the sets of temporary command states (though under the hood the api's command list is allowed to maintain these states to reduce api overhead). Scopes allow the implementation to figure out more clearly what areas of the command list are important together, and as such it can use them to determine dependencies between them and/or optimize unnecessary calls. It also allows the recorder to optimally record them in separate threads (if supported) since each scope doesn't maintain a global state. There can only be one scope active at a time: nesting scopes is unsupported.

```C
startScope		//Transitions resources
    
    addMarkerDebugExt
    startRegionDebugExt (starts deb region, push)
        endRegionDebugExt (end deb region, pop: req for each startRegionDebugExt)
    
	clearImages								//Keeps scope alive
	setGraphicsPipeline
    setComputePipeline
    	dispatch(Indirect)					//Keeps scope alive
    
    startRenderExt
        setPrimitiveBuffers
        setViewport/Scissor
        setBlendConstants
        setStencil
        draw(Indirect(Count))
            requires: 
                setGraphicsPipeline
                setViewport/Scissor
                probably setPrimitiveBuffers
                optional setBlendConstants & setStencil
        endRenderExt (required for each startRender)
                    
    endScope
```

Because a scope hoists the transitions of operations such as clearImages, drawIndirect(Count), setPrimitiveBuffers it is impossible to use the same subresource in the same scope for different usages (be it copy/shader write/read). If this is the case then a separate scope is needed.

All startRenderExts in a scope should be ended and all startRegionDebugExts as well. Since a scope should be self contained.

#### Transitions

Because the API requires bindless to function, it has certain limits. One of these limits/benefits is that a shader is now able to access all write buffers/textures and read buffers/textures. This would mean that everything is accessible by all shaders; making automatic transitions impossible. To fix this; the user will only have to manually do transitions for draw/dispatch calls. For other usages the current scope will handle the transition for you (though this disallows usages of the same subresource for different purposes). Here you specify the (sub)resource and in which shader stage it is first used and if it's a write (or if any subsequent shaders could write to it). Then the runtime will automatically transition only when it's needed. 

Even though Metal doesn't need transitions, they're still required to allow DirectX and Vulkan support. More importantly; transitions allow OxC3 to know which resources are required for the command list to be executed. It uses this to keep the resources alive until the command list was executed on the device. 

*It is important to limit how many transitions are called, since this requires extra data and processing. Batching them and making sure you need them as less as possible is good practice.*

```c
//Example to transition swapchain for use in a compute shader as the output.

Transition transitions[] = {
	(Transition) {
		.resource = swapchain,
		.range = (ImageRange) { 0 },
		.stage = EPipelineStage_Compute,
		.isWrite = true
	}
};

List transitionArr = (List) { 0 };		//No need to free since it's a ref.
_gotoIfError(
    clean, 
    List_createConstRef((const U8*) &transitionArr, 1, sizeof(Transition), transitions)
);

_gotoIfError(clean, CommandListRef_startScope(commandList, transitionArr, 0 /* id */, (List) { 0 } /* deps */));
//TODO: Bind compute shader(s) and dispatch
_gotoIfError(clean, CommandListRef_endScope(commandList));
```

Transitions can currently only be called on a Swapchain or DeviceBuffer object.

It's recommended to use enums to define the ids of scopes to avoid mistakes with hardcoding numbers for each scope.

##### Validation

Unfortunately, validation is only possible in DirectX and Vulkan using their respective validation layers. It is very hard to tell which resources were accessed in a certain frame (without running our own GPU-based validation layer). This makes it impossible to know if it was in the correct state. Make sure to validate on Vulkan since it's the strictest with transitions. However, if you're using Metal and doing transitions incorrectly, it could show up as some resources being deleted too early (if they're still in flight). Vulkan and DirectX's debug layers are automatically turned on on Debug mode.

#### Dependencies

The final parameter of startScope is the dependencies; this is a `List<ScopeDependency>` which references the scopes by id it has a dependency on. It contains the dependency type (unconditional, strong) and a scope id of the dependency. A strong dependency means that the dependency needs to be available, otherwise the scope is invalid and it should be an error to insert it if the dependency isn't available. An unconditional dependency is one where the execution will happen always even if the scope is not present; but it does mean that the scope needs to be executed before it. Consider the following:

```
startScope (0)					//Render visibility buffer
	startRenderExt
		setGraphicsPipeline
		setViewportAndScissor
		X draw calls			//Generates vertices dynamically
		endRenderExt
	endScope
	
startScope (1)
	setComputePipeline
	dispatch					//Unpack V-Buffer into G-Buffer
	endScope
```

Scope 1 is dependent on scope 0 since it uses the V-Buffer generated by it. The type of dependency in this case is strong, since if scope 0 doesn't issue any draw calls it won't have to execute the dispatch to unpack the G-Buffer.

Working in this way allows the implementation to thread scopes that are independent. Which results in better performance in cases where lots of GPU commands have to be issued.