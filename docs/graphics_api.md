# Graphics library (OxC3 id: 0x1C33)

The core pillars of the abstraction of this graphics library are the following:

- Support feature sets as close as possible to Vulkan, Direct3D12 and Metal3.
  - Limits from legacy graphics such as DirectX11 (and below), OpenGL, below Metal3, below Vulkan 1.2 and others like WebGL won't be considered for this spec. They'd add additional complexity for no gain.
- Simplify usage for these APIs, as they're too verbose.
  - But don't oversimplify them to the point of being useless.
  - This does mean features not deemed important enough might not be included in the main specification. Though a branch could maintain support if needed.
- Force modern minimum specs to avoid having to build too many diverging renderers to deal with limitations of old devices.
- Allow modern usage of these APIs such as raytracing and bindless.
- Support various systems such as Android, Apple, Windows and Linux (console should be kept in mind, though not officially supported).

## Getting started

Before getting started, there needs to be a header file included in the main entrypoint. Not including this will result in linker errors. This is to prevent the user from running without the Agility SDK, which requires certain symbols to be injected into the main executable. This header file should only be included into the main exe/apk, it should not be linked into a dynamic or static lib!

```c
#include "graphics/generic/application.h" //Enables proper OxC3 graphics support
```

## Ref counting

OxC3 graphics works in a similar way as DirectX's ref counting system; everything is ref counted. This allows you to just call `RefPtr_dec` on the RefPtr to ensure it's properly released. If the command list is still in flight, it will maintain this resource until it's out of flight. It also allows you to safely share resources between user libraries without worrying about resource deletion. When sharing a resource, you simply increment the refptr by using `RefPtr_inc` and when the library is done using it can decrement it again. This concept was added in OxC3 platforms, but widely used in the graphics library.

### Obtaining the real object

RefPtr doesn't contain the object itself, but it does contain information about the type and length of the constructed type. RefPtr contains this object after the data of the RefPtr itself. RefPtr_data can be used to obtain the data pointed to by the RefPtr. This is generally also a macro from the specialized RefPtr (e.g. `GraphicsInstanceRef_ptr`). It also generally contains the extended object after the common object. For example GraphicsInstance has `GraphicsInstance_ext` which can get the API-specific data of the GraphicsInstance. This should only be used internally, as there is no public interface of this extended data that won't randomly change.

In the case of multiple inheritance, the data pointed to can be linked however the creator of the object sees fit. For example: The first data could be a UnifiedTexture, then a UnifiedTextureImage * 3, then a UnifiedTextureImageExt * 3 and then a SwapchainExt.

## Graphics instance

### Summary

The graphics instance is the way you can query physical devices from D3D12, Vulkan or Metal3. First one has to be created as follows:

```c
GraphicsInstanceRef *instance = NULL;

gotoIfError(clean, GraphicsInstance_create(
	(GraphicsApplicationInfo) {
	    .name = CharString_createConstRefCStr("Rt core test"),
    	.version = OXC3_MAKE_VERSION(0, 2, 0)
	},
    EGraphicsInstanceFlags_None,
    &instance
));
```

Once this instance is acquired, it can be used to query devices and to detect what API the runtime supports.

*Note: Before creating a GraphicsInstance with shared linking, make sure to initialize the GraphicsInterface using GraphicsInterface_init. For dynamic libraries it is important to set GraphicsInterface_instance and Platform_instance correctly to the final exe/dll's instance to ensure the same exact settings and memory allocation is handled by the same instance.*

### Properties

- application: The name and version of the application.
- api: Which api is ran by the runtime: Vulkan, Direct3D12, Metal3 or WebGPU.
- apiVersion: What version of the graphics api is being ran (e.g. Vulkan 1.2, DirectX SDK version, Metal 3, etc.).

### (Member) Functions

- ```c
  Error getDeviceInfos(Bool isVerbose, ListGraphicsDeviceInfo *infos);
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

gotoIfError(clean, GraphicsInstance_getPreferredDevice(
  GraphicsInstanceRef_ptr(instance),		//See "Graphics instance"
  (GraphicsDeviceCapabilities) { 0 },		//No required features or data types
  GraphicsInstance_vendorMaskAll,			//All vendors supported
  GraphicsInstance_deviceTypeAll,			//All device types supported
  EGraphicsDeviceFlags_None,        		//Disallow debug, RT and/or verbose logging
  &deviceInfo
));
```

### Properties

- name, driverInfo; All null-terminated UTF-8 strings giving information about the device and driver.
- type; what type this device is (dedicated GPU, integrated GPU, simulated GPU, CPU or other (unrecognized)).
- vendor; what company designed the device (Nvidia (NV), AMD, ARM, Qualcomm (QCOM), Intel (INTC), Imagination Technologies (IMGT), Microsoft (MSFT) or unknown).
- id; number in the list of supported devices.
- luid; ID to identify this device primarily on Windows devices. This would allow sharing a resource between other APIs for interop (not supported yet). This is optional to support; check capabilities.features & LUID.
- uuid; unique id to identify the device. In APIs that don't support this natively, the other identifier (luid) will be used here instead. For example Direct3D12 would use the luid here and clear the other U64.
- ext; extended physical device representation for the current API. Can be NULL if LUID is used to share this.
- capabilities; what data types, features and api dependent features are enabled. See capabilities section. This also includes dedicated and shared memory.

#### Capabilities

- features: DirectRendering, VariableRateShading, MultiDrawIndirectCount, MeshShader, GeometryShader, SubgroupArithmetic, SubgroupShuffle, Multiview, Raytracing, RayPipeline, RayQuery, RayMicromapOpacity, RayMicromapDisplacement, RayMotionBlur, RayReorder, RayValidation, LUID, DebugMarkers, Wireframe, LogicOp, DualSrcBlend, Workgraphs, SwapchainCompute.
  - RayValidation: extra raytracing validation for NV cards; requires envar NV_ALLOW_RAYTRACING_VALIDATION=1 and reboot.
- features2: reserved for future usage.
- dataTypes: F64, I64, F16, I16, AtomicI64, AtomicF32, AtomicF64, ASTC, BCn, MSAA2x, MSAA8x, RGB32f, RGB32i, RGB32u, D24S8, S8.
  - MSAA4 and MSAA1 (off) are supported by default.
- featuresExt: API dependent features that aren't expected to be standardized in the same way.
  - Vulkan: PerformanceQuery.
  - Direct3D12: WriteBufferImmediate (for crash debugging), ReBAR (for checking if quick access path to GPU is available), HardwareCopyQueue (If the copy queue makes sense to use).
- maxBufferSize and maxAllocationSize: Device limit on how big a buffer or a single allocation may be.

### Functions

- ```
  print(EGraphicsApi api, Bool printCapabilities);
  ```

  - Prints all relevant information about the device. If printCapabilities is on it will also show extensions and supported data types. api can be acquired from instance.

- `supportsFormat(ETextureFormat format)`

  - Checks if the format is supported for use as a texture by the current device.

- `supportsRenderTextureFormat(ETextureFormat format)`

  - Checks if the format is supported for use as a render texture format by the current device.

- `supportsFormatVertexAttribute(ETextureFormat format)`

  - Checks if the format is supported for use as a vertex attribute.

- `supportsDepthStencilFormat(EDepthStencilFormat format)`

  - Checks if the depth stencil format is supported for use by the current device.

### Used functions and obtained

- Obtained through GraphicsInstance's getDeviceInfos and getPreferredDevice.
- Passed to `GraphicsDeviceRef_create` to turn the physical device into a logical device.

## Graphics device

### Summary

The graphics device is the logical device that is used to create objects and execute commands to.

```c
GraphicsDeviceRef *device = NULL;
gotoIfError(clean, GraphicsDeviceRef_create(
    instance, 		//See "Graphics instance"
    &deviceInfo, 	//See "Graphics device info"
    false, /* isVerbose; extra logging about the device properties */
    &device
));
```

### Properties

- instance; owning instance.
- info; physical device.
- submitId; counter of how many times submit was called (can be used as frame id).
- lastSubmit, firstSubmit; used to track when a submit was called.
- pendingResources, resourcesInFlight; used to track if resources are dirty, in flight (in use on the GPU) and if they need updates in the next submit.
- allocator; used to allocate memory.
- lock; used to ensure flushes aren't done while a commit is busy for example.
- staging, stagingAllocations; staging allocations are used if the resources that are being updated are already in flight, if the resource is a tiled texture (non linear texture) or if the device doesn't support ReBAR/shared memory (and so the GPU memory isn't accessible).
- frameData; current frame data for this frame.
- currentLocks; which resources are currently locked while submitCommands is active.
- pendingBytes, flushThreshold; how many bytes of copy data are pending. The higher this is the bigger the chance of a flush happening. This is when there's so much data pending that the submitCommands will split the record in multiple submits. The reason it does this is because too much data can result in the operation taking too long and the GPU will cause a device lost error. Another reason is because these copies might make temporary staging resources which take up VRAM. Surpassing too many copies at once can result in out of memory errors or the device paging the memory to disk (resulting in too slow operations, which might cause a device lost). flushThreshold can be set to control when this happens; though it is set to a default value of < 4 GIBI (20% of cpuHeapSize when on shared memory otherwise 20% of gpuHeapSize + 10% of cpuHeapSize < 33% gpuHeapSize). For example on a system with an RTX 4090 (24GiB) and 128GiB of RAM (64 shared) the formula turns into 24GiB / 5 + 64GiB / 10 = 4.8 + 6.4 = 11.2GiB < 8GiB (24GiB/3) < 4 GiB (so limited to 4GiB). For more normal systems it is expected that flushThreshold is <4GiB.

### Functions

- ```c
  Error submitCommands(ListCommandListRef commandLists, ListSwapchainRef swapchains, Buffer runtimeData, F32 deltaTime, F32 time);
  ```

  - Submits commands to the device and readies the swapchains to present if available. If the device doesn't have any swapchains, it can be used to just submit commands. This is useful for multi GPU rendering as well. If deltaTime is set to -1 it will automatically calculate deltaTime itself, but some applications might want to bypass this by manually passing deltaTime and time (for example when rendering a movie or a photo). The time argument is also ignored if deltaTime is -1 and will be calculated automatically.

- There's a limit of 16 swapchains per device.

  - Runtime data is accessible from a CBuffer to all shaders and can be used for simple data such as resource handles. This buffer has a limit of 368 bytes.

- ```c
  wait();
  ```

  - Waits for all currently queued commands on the device.

- ```c
  Error createSwapchain(SwapchainInfo info, Bool allowWrite, SwapchainRef **swapchain);
  ```

  - Be aware that allowWrite doesn't work for all APIs and all devices. Compute to the swapchain is optional.

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
      ListBuffer computeBinaries,
      ListCharString names,		//Empty = ignore, otherwise computeBinaries.length
      PipelineRef **computeShaders
  );

  ```

- ```c
  Error createPipelinesGraphics(
  	ListPipelineStage stages,
  	ListPipelineGraphicsInfo infos,
  	ListCharString names,			//Empty = ignore, otherwise info.length
  	ListPipelineRef *pipelines
  );
  ```

- ```C
  Error createBuffer(
  	EDeviceBufferUsage usage,
      EGraphicsResourceFlag flags,
   	CharString name,
      U64 len,
      DeviceBufferRef **ref
  );
  ```

- ```c
    Error createBufferData(
    	EDeviceBufferUsage usage,
    	EGraphicsResourceFlag flags,
     	CharString name,
        Buffer *data,					//Can move data to device buffer (doesn't always)
        DeviceBufferRef **ref
    );
    ```
  ```

  ```

- ```c
  Error createSampler(SamplerInfo info, CharString name, SamplerRef **ref));
  ```

- ```c
  Error createRenderTexture(
      ETextureType type,
      U16 width,
      U16 height,
      U16 length,
      ETextureFormatId format,
      EGraphicsResourceFlag flags,
      EMSAASamples samples,
      CharString name,
      RenderTextureRef **ref
  );
  ```

- ```c
  Error createDepthStencil(
        U16 width,
        U16 height,
        EDepthStencilFormat format,
        Bool allowShaderRead,
        EMSAASamples samples,
        CharString name,
        DepthStencilRef **ref
    );
  ```

- ```c
  Error createBLASExt(
  	ERTASBuildFlags buildFlags,
  	EBLASFlag blasFlags,
  	ETextureFormatId posFormat,		//RGBA16f, RGBA32f, RGBA16s, RG16f, RG32f, RG16s
  	U16 positionOffset,				//Offset into first position for first vertex
  	ETextureFormatId indexFormat,	//R16u, R32u, Undefined
  	U16 positionBufferStride,		//<=2048 and multiple of 2 or 4 (RGBA32f, RG32f)
  	DeviceData positionBuffer,		//Required
  	DeviceData indexBuffer,			//Optional if indexFormat == Undefined
  	BLASRef *parent,				//If specified, indicates refit
  	CharString name,
  	BLASRef **blas
  );
  ```

- ```c
  Error createBLASUnindexedExt(
  	ERTASBuildFlags buildFlags,
  	EBLASFlag blasFlags,
  	ETextureFormatId posFormat,		//RGBA16f, RGBA32f, RGBA16s, RG16f, RG32f, RG16s
  	U16 positionOffset,				//Offset into first position for first vertex
  	U16 positionBufferStride,		//<=2048 and multiple of 2 or 4 (RGBA32f, RG32f)
  	DeviceData positionBuffer,		//Required
  	BLASRef *parent,				//If specified, indicates refit
  	CharString name,
  	BLASRef **blas
  );
  ```

- ```c
  Error createBLASProceduralExt(
  	ERTASBuildFlags buildFlags,
  	EBLASFlag blasFlags,
  	U32 aabbStride,						//Alignment: 8
  	U32 aabbOffset,						//Offset into the aabb array
  	DeviceData buffer,					//Required
  	BLASRef *parent,					//If specified, indicates refit
  	CharString name,
  	BLASRef **blas
  );
  ```

- ```c
  Error createTLASExt(
  	ERTASBuildFlags buildFlags,
  	TLASRef *parent,					//If specified, indicates refit
  	ListTLASInstanceStatic instances,
  	CharString name,
  	TLASRef **tlas
  );
  ```

- ```c
  Error createTLASMotionExt(
  	ERTASBuildFlags buildFlags,
  	TLASRef *parent,					//If specified, indicates refit
  	ListTLASInstanceMotion instances,
  	CharString name,
  	TLASRef **tlas
  );
  ```

- ```c
  Error createTLASDeviceExt(
  	ERTASBuildFlags buildFlags,
  	Bool isMotionBlurExt,				//Requires extension
  	TLASRef *parent,					//If specified, indicates refit
  	DeviceData instancesDevice,			//Instances on the GPU, correctly sized
  	CharString name,
  	TLASRef **tlas
  );
  ```

### Obtained

- Through GraphicsDeviceRef_create; see overview.

## Command list

### Summary

A command list in OxC3 is a virtual command list. The commands get recorded in a standardized format, but they don't get translated until they're submitted to the graphics device. The graphics device is then responsible for synchronization and ensuring the commands are recorded in the optimal way. Command lists themselves don't have any API specific implementation to avoid unexpected caching behavior. When submitting multiple command lists, they'll act as if they're one long command list. This would allow maintaining state from the old command list as well.

```c
CommandListRef *commandList = NULL;
gotoIfError(clean, GraphicsDeviceRef_createCommandList(
    device, 	//See "Graphics device"
    2 * KIBI, 	//Max command buffer size
    64, 		//Estimated command count (can auto resize)
    64, 		//Estimated resource count (can auto resize)
    true,		//Allow resize of command buffer data (the 2 KIBI in this example)
    &commandList
));
```

#### Recording a command list

```c
gotoIfError(clean, CommandListRef_begin(commandList, true /* clear previous */, U64_MAX /* long long timeout */));

gotoIfError(clean, CommandListRef_startScope(commandList, (ListTransition) { 0 }, 0, (ListCommandScopeDependency) { 0 }));
gotoIfError(clean, CommandListRef_clearImagef(
    commandList, F32x4_create4(1, 0, 0, 1), (ImageRange){ 0 }, swapchain
));
gotoIfError(clean, CommandListRef_endScope(commandList));

gotoIfError(clean, CommandListRef_end(commandList));
```

Every frame, this can be passed onto the submit commands call:

```c
ListCommandListRef commandLists = (ListCommandListRef) { 0 };
ListSwapchainRef swapchains = (ListSwapchainRef) { 0 };

gotoIfError(clean, ListCommandListRef_createRefConst(commandList, 1, &commandLists));
gotoIfError(clean, ListSwapchainRef_createRefConst(swapchain, 1, &swapchains));

gotoIfError(clean, GraphicsDeviceRef_submitCommands(device, commandLists, swapchains, -1, 0));
```

For more info on commands check out the "Commands" section.

### Properties

- device; owning device.
- state; if the command list has been opened before and if it's open or closed right now.
- allowResize; if command list is allowed to resize if it runs out of memory.
- resources; Which resources are in use by the command list.
- data; The current recorded commands.
- commandOps; The opcodes for which commands are recorded.
- transitions; list of transitions issued in the command list. Scopes point into this to execute transitions.
- lock; for allowing multiple threads to open the same command list (has to be closed by one thread first).
- activeScopes; list of scopes that weren't collapsed (scope command id & scope id, what transitions it did and how many commands it contains & the length of the sub command buffer).
- pipeline; currently bound pipelines.
- boundImages/boundImageCount/boundDepthFormat/boundSampleCount; currently bound images in the renderpass and/or render targets (DirectRendering). As well as the sample count that it was bound with.
- tempStateFlags; AnyScissor, AnyViewport, HasModifyOp, HasScope, InvalidState. Used for validation and optimization.
- debugRegionStack; used for validating debug regions.
- lastCommandId, next, lastTransition; locations at the start of a scope for command id, buffer location and transition offset. Used for the next scope description.
- lastScopeId, lastOffset; used for resetting to the end of last scope in case the current scope is invalidated.
- currentSize; size of the current renderpass.
- pendingTransitions; list of transitions waiting for the current scope. Get pushed into transitions if the scope is visible.
- activeSwapchains; what swapchains are used in the command list including their versionId, to ensure the command list can't be re-submitted once the swapchains were resized.

### Functions

See the "Commands" section.

### Used functions and obtained

- Obtained through GraphicsDeviceRef's createCommandList, see overview.
- Used in GraphicsDeviceRef's submitCommands.
- Also used in command recording (See "Commands" section).

## GraphicsResource

A GraphicsResource is defined as any object that has allocated memory on the GraphicsDevice. This includes two types of resources: DeviceBuffer and UnifiedTexture.

A graphics resource consists of the following:

- device: the owning graphics device.
- size: CPU visible buffer size.
- blockOffset/blockId/allocated: API dependent allocation information used to track the allocation.
- flags: ShaderRead, ShaderWrite, InternalWeakDeviceRef, CPUBacked, CPUAllocated(Bit).
  - ShaderRead: Whether the resource has a valid resource handle and can be accessed on the GPU through the descriptors. MSAA resources are incompatible with this flag, because there's no Texture2DMS[], it needs to be resolved before reading/writing the resource.
  - ShaderWrite: ^ but for write access. DepthStencil and MSAA disallows this always.
  - InternalWeakDeviceRef: Internal only; tells the resource it belongs to the device, so the device is the only one in charge of cleaning it up.
  - Only relevant for DeviceTexture and DeviceBuffer:
    - CPUBacked: The resource will have CPU backed memory to ensure it can continuously be pushed or pulled from the device whenever necessary. Without this flag, the upload data will only be available on first commit, after that the memory will be freed.
    - CPUAllocatedBit (Always use as CPUAllocated to force CPUBacked also): Indicates that the device memory with the resource should be CPU-sided whenever possible. This can free up more device memory if the resource is only rarely being accessed (because it's typically slower than local memory). In some cases, this flag won't do anything, because with shared memory models such as mobile or integrated GPUs the memory between CPU and GPU is shared anyways. This is generally not a useful flag, though it can be when consuming large amounts of memory.
- type: DeviceTexture, RenderTargetOrDepthStencil, DeviceBuffer, Swapchain. This is the base type; useful for memory allocation. Real type info can be obtained through the RefPtr itself.
- mappedMemoryExt: API dependent location of the memory. Never write to this directly, because of unexpected caching behavior or access limations. Even if CPUAllocatedBit is active, this is not guaranteed to be writable directly on runtime. The graphics API implementation decides if it can directly access it (e.g. the resource is not in flight) and then can directly write to it. If it can't, it will use the staging buffer or create a dedicated staging resource (temporary). Always access this through the markDirty/pullRegion function of DeviceTexture or DeviceBuffer.
- deviceAddress: the address of this resource on the GPU (generally only for device buffers).

### Resource handles

Resource handles are what is passed to the GPU to access the GraphicsResources, these are the following resource handle types:

- Sampler
- ShaderRead
- ShaderWrite

Between those types, there are also further subdivisions:

- Sampler
- ShaderRead: Texture2D, TextureCube, Texture3D, Buffer, TLAS
- ShaderWrite: RWBuffer, RWTexture(2/3)D(s/f/i/u/)

With resource handles, it is assumed that both U32_MAX and 0 indicate an invalid descriptor. The bottom 17 bits are the local id, while the 4 bits higher represent this EDescriptorType. Higher bits aren't currently used for anything to try and keep it possible to represent this resource handle as a float without losing any precision. If the EDescriptorType bits are all 1, then the top 4 bits of the localId represent the extended descriptor type (e.g. 1111 0001 would be EDescriptorTypeCount_TLASExt (16)) while 1111 0000 (15) would be sampler). Short ids are for Sampler and TLASExt descriptor types and they only allow use of 13 bit local ids; about 8192 ids. Of these, not even all are accessible (only 16 TLASes are accessible and 2048 samplers). For long ids, they have a maximum of 131072 (though the minimum used for one long id resource is 8192).

### Marking dirty (markDirty)

DeviceTexture and DeviceBuffer can be marked dirty by their respective markDirty functions. This means that at the start of the next frame (submitCommands), the changes will be submitted. Updating the resource in flight is only possible by creating a temporary copy resource to handle it and then manually calling copy commands.

### Pulling region (pullRegion)

DeviceTexture and DeviceBuffer can be pulled back from GPU by their pullRegion functions. These functions run at the end of the next frame (next submitCommands end) and will then be pulled back async to the CPU when the operation is completed on the device. On completion, the callback function can be ran (this is 3 frames later). If the result is important right now, it can be stalled by calling wait after the submitCommands, *though that fully stalls the device and should be prevented at all costs*. If it's desired to copy the current state of the resource (if it gets modified after) then a manual copy resource should be created and manual copy should be done to ensure it won't be modified in between.

## UnifiedTexture

A UnifiedTexture can represent the following types: DepthStencil, RenderTexture, Swapchain, DeviceTexture. It was chosen to not be only one interface because the four types are very distinct in use, so only the base functionality had to be unified like this.

A UnifiedTexture contains the following:

- resource: the base GraphicsResource.
- textureFormatId: ETextureFormatId, which can be ETextureFormatId_Undefined in the case of a DepthStencil.
- sampleCount: EMSAASamples. When this is defined as > 0, the resource can only be used as a render texture / depth stencil (not depth texture). There is no way to shader read/write MSAA textures in the current API spec, even though Texture2DMS is a type, there is no dedicated resource array for it and thus it's not supported. RWTexture2DMS has been added to later versions of HLSL, but this requires extensions that aren't properly supported yet. The only way to do this is to pass another render target / depth texture as the resolve texture.
- depthFormat: EDepthFormat, which can be EDepthFormat_None if the texture is not a DepthTexture.
- type: ETextureType; 2D, 3D or Cube.
- width, height, length: dimensions of the resource. Length needs to be 1 if the type isn't 3D. For cubes the length will be automatically set to 6. The limits are width/height of 16384 and length of 256.
- levels: how many mip levels are present.
- images: how many different images are present. This is generally 1, but for Swapchains it is 3 because of versioning.
- currentImageId: should only ever be accessed through the graphics API implementation. This value is a lagging value that's unpredictable if it's not in the submitCommands call. However, for Swapchains during that call, this represents what image is currently being rendered to.

The unified texture can be obtained through the RefPtr as follows:

```c
DeviceResourceVersion v;
UnifiedTexture tex = TextureRef_getUnifiedTexture(texRef, &v);
```

The DeviceResourceVersion should only be used if the size, versionId or format of the image are important. Otherwise, it is safe to pass NULL as that parameter. This is only important for Swapchains, because they can be resized and so they need to be locked to ensure it is safe to access that information.

Other helpers:

- U32 **TextureRef_getReadHandle**/**getWriteHandle**(TextureRef *tex, U32 subResource, U8 imageId): get the respective GPU accessible handles. Returns 0 if uninitialized.
  - getCurr(Read/Write)Handle can be used too, but only if it's obvious that the image isn't a versioned resource (e.g. Swapchain). In that case, the image is always 0 and so it can safely be used.
- Bool **TextureRef_isRenderTargetWritable**(TextureRef *tex): if the unified texture is accessible as a color render target.
- Bool **TextureRef_isDepthStencil**(TextureRef *tex): if the unified texture is accessible as a depth stencil.
- Bool **TextureRef_isTexture**(RefPtr *tex): if the resource contains a UnifiedTexture.

### Memory layout

Besides the base data, the texture interface should always contain the image information behind the UnifiedTexture struct:

- UnifiedTextureImage[base.images]: the read and write resource handle of the individual subresource and sub image.
- UnifiedTextureImageExt[base.images] (UnifiedTextureImageExt_size): the extended data of the image, useful for keeping API dependent state, views/image information.
- Extended data: Such as SwapchainExt (VkSwapchain, DxSwapchain, etc.).

This can be safely accessed through the helpers:

- UnifiedTextureImage **TextureRef_getImage**(TextureRef *tex, U32 subResource, U8 imageId): gets the image resource handles.

Other helpers are internal use only, such as getImgExtT and getImplExtT,

## Swapchain

### Summary

A swapchain is the interface between the platform Window and the API-dependent way of handling the surface, swapchain and any synchronization methods that are required.

```c
//onCreate

SwapchainRef *swapchain = NULL;
gotoIfError(clean, GraphicsDeviceRef_createSwapchain(
    device, 		//See "Graphics device"
    (SwapchainInfo) { .window = w },
    false,			//allowComputeExt
    &swapchain
));

//onResize: Window callback to make sure format + size stays the same:

if(!(w->flags & EWindowFlags_IsVirtual))
    SwapchainRef_resize(swapchain);
```

info.presentModePriorities are the requests for what type of swapchains are desired by the application. Keeping this empty means [ mailbox, immediate, fifo, fifoRelaxed ]. On Android mailbox is unsupported because it may introduce another swapchain image, the rest is driver dependent if it's supported and the default is changed to [ fifo, fifoRelaxed, immediate] to conserve power. Immediate is always supported, so make sure to always request immediate as well otherwise createSwapchain may fail (depending on device + driver). For more info see Swapchain/Present mode.

allowComputeExt in this case allows the swapchain to be used for compute shaders. If this is not required, please try to avoid it, since it's not always supported by all APIs and devices (Query for it through capabilities). It might reduce or remove compression for the swapchain depending on the underlying hardware and to avoid allocating a write descriptor for it.

### Present mode

The present mode is how the device handles it when an image is already being presented. Some may introduce tearing while others may drop frames. The application should be given control over these modes, as some applications may want the latency improvements at the cost of tearing.

- Mailbox: While an image is presenting, it will drop the oldest queued image and continue rendering and queuing the next frame into it. This ensures you're not bound to your refresh rate, so the performance is better but lots of frames are rendered that may be discarded. This present mode is generally ideal for games and other interactive 3D applications, though some very low latency games might not want to use this. This mode is the default on most platforms on Vulkan (if available) and unsupported on D3D12.
- Fifo: While an image is presenting, it will just wait. This means no frames are dropped, but it does mean that the performance is lower. This is ideal for low power devices or if your application doesn't need constant updates. This is the default on mobile.
- FifoRelaxed: If the vsync interval is missed, it will try to skip frame(s) to catch up. Other than that it will behave similar to Fifo (it tries to keep up with the refresh rate of the screen). Unsupported on D3D12.
- Immediate: Render over the current image anyways, this is ideal if you want the best low latency available but will introduce tearing. A good application might be shooting games or other games that require the lowest latency. This is the default on D3D12.

By default the swapchain will use triple buffering to ensure best performance. Even mobile benefits from this, so it was decided not to expose this setting to simplify the backend.

### Properties

- info.window: the Window handle created using OxC3 platforms.
- info.requiresManualComposite: whether or not the application is requested to explicitly handle rotation from the device. For desktop this is generally false, for Android this is on to avoid the extra overhead of the compositor.
- info.presentModePriorities: what present modes were requested on create.
- device: the owning device.
- base: UnifiedTexture that represents this texture.
- versionId: the version id changes everytime a change is applied. This could be resizes or format changes.
- presentMode: What present mode is currently used (see Swapchain/Present mode).
- lock: Multi-threading. Is used to maintain versionId. On submitCommands it has to lock the swapchain(s) to see if the versionId is still the same as the one the command list(s) was/were recorded with. A CommandList is deemed stale if the Swapchain has been resized to a different size. Not re-recording will result in submitCommands erroring. This is because lots of commands can use the swapchain size such as SetViewportAndScissor.

### Functions

- ```c
  resize();
  ```

  - Updates the swapchain with the current information from OxC3 platforms. Both format and size are queried again to ensure it really needs to resize. After this, all recorded command lists that include it will be invalidated.

### Used functions and obtained

- Obtained through GraphicsDeviceRef's createSwapchain, see overview.
- Used in GraphicsDeviceRef's submitCommands as well as read & write image commands and dispatches.

## Sampler

### Summary

A sampler is a standalone object that will be used to describe how a texture is sampled. These are not combined samplers because it is possible that one texture is used as two different usages (e.g. one for anisotropy and one for linear) and logically it doesn't make sense that it's linked to the texture rather than a standalone object. This object is given space in the bindless descriptor arrays just like shader visible buffers, depth stencils, render textures, swapchains and depth stencils.

Once on the GPU, the sampler resource index can be passed to the GPU and the sampler array can be accessed. Then this sampler can be used to sample any resource that's required.

```c
SamplerInfo nearestSampler = (SamplerInfo) { .filter = ESamplerFilterMode_Nearest };
gotoIfError(clean, GraphicsDeviceRef_createSampler(device, nearestSampler, samplerName, &nearest));
```

If the sampler is used on the GPU, it should be passed as a transition; stage is unused so can be safely ignored. The most important part here is that the transition keeps the Sampler alive while the command list is in flight. The graphics implementation is also allowed to manage eviction behind the scenes if it is determined that the resource has not been in flight for too long and it might be hogging space.

### Properties

- device: ref to the device that owns it.
- samplerLocation: resource index into the bindless array that specifies where the sampler is located. It does contain additional info in the upper 8 bits, so only the low 13 bits store the index (samplerUniform(resourceId) and sampler(resourceId) can be used to do this automatically).
- info: used to create the sampler and stores information about the sampler.
  - filter: determining how the sampler filters the input image. A bitset of three properties: Mag, Min and Mip. If the respective bit is true it represents linear filtering rather than nearest filtering. This means there's 7 combinations ranging from nearest min/mag/mip all the way to linear min/mag/mip.
  - addressU, addressV, addressW: determining how out of bounds access for each texture is treated: Repeat, MirrorRepeat, ClampToEdge, ClampToBorder. ClampToBorder uses the borderColor to be filtered.
  - aniso: is anisotropy is applied and how much. 0 means no anisotropy and 1-16 means anistropy of that level.
  - borderColor: what border color is used if one of the address modes (uvw) is ClampToBorder. TransparentBlack (0.xxxx), OpaqueBlackFloat (0.xxx, 1.f), OpaqueBlackInt (0.xxx, 1), OpaqueWhiteFloat(1.f.xxxx), OpaqueWhiteInt (1.xxxx).
  - comparisonFunction: comparison function for SamplerComparisonState. Same type (ECompareOp) as depth stencil state. One of Gt, Geq, Eq, Neq, Leq, Lt, Always, Never.
  - enableComparison: whether or not the comparison function is used.
  - mipBias, minLod, maxLod:
    - These properties are F16s (halfs) and require conversion from F32 by using F32_castF16 or F64_castF16.
    - mipBias: mip bias that is applied before reading from the mip.
    - minLod, maxLod: min and max mip. If maxLod is 0 it is assumed that this property isn't set and 65504 (F16_max) is used. If maxLod of 0 is desired it can be achieved by setting it to >5.97e-8 or just any other small number like 0.001.

### Used functions and obtained

- Obtained through GraphicsDeviceRef's createSampler.
- Used directly in shaders by passing the samplerLocation to the shader and using samplerUniform() or sampler() in the shader. Needs to be marked active by transitioning the sampler in the startScope command to ensure it is present.

## DeviceTexture

### Summary

A DeviceTexture is a texture that can be sent from the CPU to the device. It's essentially a UnifiedTexture that is allowed to be uploaded / downloaded from the device.

```c
gotoIfError(clean, GraphicsDeviceRef_createTexture(
    twm->device,
    ETextureType_2D,
    ETextureFormatId_BGRA8,
    EGraphicsResourceFlag_ShaderRead,
	128, 128, 1,
    CharString_createRefCStrConst("Test image"),
    &imageData,					//Is able to move the Buffer if it's not a ref
    &twm->deviceTextureRef
));
```

### Properties

- A DeviceTexture is almost identical in layout to a UnifiedTexture.
- Just like a DeviceBuffer, it has the following parameters:
  - isPending(FullCopy) / pendingChanges: used to detect what data it should send.
  - cpuData: the CPU backing data that will be pushed / was previously pulled.
  - lock: for multi threading, every time markDirty is used it will need to mark the range as dirty. The accessor should lock this only if this buffer can be accessed by another thread before reading/writing (lock must be released after of course).
  - isFirstFrame: If the resource was already uploaded before.
- base: UnifiedTexture it represents.

### Used functions and obtained

- Obtained through GraphicsDeviceRef's createTexture.
- Can be copied to (markDirty(x, y, z, w, h, l)) and read back (pullRegion(x, y, z, w, h, l)).
- Can be used in shaders (if EGraphicsResourceFlag_ShaderRead is on) by passing the readLocation to the shader and using texture2DUniform() or texture2D() in the shader. Can be written (if EGraphicsResourceFlag_ShaderWrite is on) by using the respective rwTexture slot (see GraphicsResource/Resource handles). Can also be used when copying images.

## DepthStencil

### Summary

A DepthStencil is an object that holds the depth and stencil buffers as a 2D image which can be used together with a RenderTexture or by itself for 3D rendering (to avoid overdraw) and to handle shadow maps or other tricks such as portals/reflections (stencil buffer).

The depth stencil is quite straight forward; it has up to 3 stencil enabled formats (D24S8Ext, D32S8Ext, S8Ext) that can be used to provide a stencil attachment to startRenderExt and 2 non stencil enabled formats (D16, D32). D24S8Ext is optional, but is important for NV and Intel GPUs since it packs the depth and stencil into 32-bits. D24S8Ext and S8Ext support can be queried through the GraphicsDeviceInfo's capabilities. Whenever possible please use D16 or D32 since it doesn't waste any space for a stencil buffer if it isn't needed. D16 should only be used if depth precision isn't a great priority (performance and memory usage is prioritized). D16 can be used on mobile to save space and time. If allowShaderRead is on, the depth stencil can be accessed through shader logic by passing the resource handle to the GPU.

```c
gotoIfError(clean, GraphicsDeviceRef_createDepthStencil(
    twm->device,
    width, height, EDepthStencilFormat_D16, false /* allow shader access */,
    EMSAASamples_Off,
    CharString_createRefCStrConst("Test depth stencil"),
    &tw->depthStencil
));
```

### Properties

- A DepthStencil is identical in layout to a UnifiedTexture.

### Used functions and obtained

- Obtained through GraphicsDeviceRef's createDepthStencil.
- Can be written to by passing the depth or stencil attachment in the startRenderExt command and ensuring the right pipeline state object DepthStencilState settings are active.
- Can be used in shaders (if allowShaderRead is on) by passing the readLocation to the shader and using texture2DUniform() or texture2D() in the shader.

## RenderTexture

### Summary

A RenderTexture is a texture that can be rendered to by either a startRenderExt or directly from a compute shader (if enabled). While a normal DeviceTexture would represent a texture that can only be written to by copying from the GPU or CPU. A DeviceTexture supports compression but a RenderTexture does not.

Some formats are optional such as RGB32f, RGB32i and RGB32u. These have to be queried through the GraphicsDeviceInfo, however they are always available for use as vertex attributes.

A RenderTexture itself can currently only be 2D, but will be possible to be 3D or a TextureCube in the future. Currently no mip levels are supported and 2DArray and TextureCubeArray won't be supported (since array of textures should be sufficient).

```c
gotoIfError(clean, GraphicsDeviceRef_createRenderTexture(
	twm->device,
	ETextureType_2D, 				//2D is only supported currently
    width, height, 1, 				//x, y, z
    ETextureFormatId_BGRA8, 		//Non compressed texture format
    EGraphicsResourceFlag_ShaderRW, //Allow both shader writes and reads
    EMSAASamples_Off,				//No MSAA
	CharString_createRefCStrConst("Virtual window backbuffer"),
	&tw->swapchain
));
```

### Properties

- Identical to a UnifiedTexture.

### Used functions and obtained

- Obtained through GraphicsDeviceRef's createRenderTexture.
- Can be written to by passing as an attachment in the startRenderExt command and ensuring the right pipeline state object settings are active.
- Can be read in shaders (if usage permits it) by passing the readLocation to the shader and using texture2DUniform() or texture2D() in the shader.
- Can be written in (compute) shaders (if usage permits it) by passing the writeLocation to the shader and using rwTexture2D(format)Uniform() or rwTexture2D(format)() in the shader.
  - Where format is the type of primitive the texture uses. Such as: (none): unorm, (s): snorm, (i): int, (u): uint, (f): float. e.g. rwTexture2DfUniform(resourceId) would access the writeonly float texture at resourceId and rwTexture2DUniform would access a unorm RW texture.

## Pipeline

### Summary

A pipeline is a combination of the states and shader binaries that are required to run the shader. This represents a VkPipeline in Vulkan, an ID3D12PipelineState or ID3D12StateObject in Direct3D12 and a `MTL<Render/Compute>PipelineState` in Metal.

### Properties

- device: ref to the graphics device that owns it.
- type: compute, graphics or raytracing pipeline type.
- stages: `ListPipelineStage` the binaries that are used for the shader.
  - stageType: which stage the binary is for. This is not necessarily unique, but should be unique for graphics shaders and compute. For raytracing shaders there can be multiple for the same stage.
  - Non raytracing shaders (compute / graphics):
    - binary: the format as explained in "Shader binary types" that is required by the current graphics API.
  - Raytracing shaders:
    - binaryId: the binary index in the binaries passed to createPipelineRaytracingExt (local id, not global: so if two identical pipelines are compiled at the same time, the binaryId should be the same in both). binaryId may be U32_MAX to indicate a NULL shader (but only for miss shaders). This could be useful for shadow shaders for example (they might not need a miss shader).
    - Accessible after construction for miss, callable and raygen shaders:
      - localShaderId: the id of that one specific shader type only (raygen 0,1,2, miss 0,1,2, etc.).
      - groupId: implementation specific id that is used to communicate internally about how the shader can be accessed (through a shader binding table for example).
- extraInfo: a pointer to behind the API dependent pipeline extension struct that allows extra info that's only applicable to a certain pipeline type.
  - For compute: this is NULL.
  - For graphics: this is PipelineGraphicsInfo.
  - For raytracing: this is PipelineRaytracingInfoExt.

### Used functions and obtained

- Obtained through GraphicsDeviceRef's createPipelinesGraphics, createPipelinesCompute and createPipelineRaytracingExt.
- Used in CommandListRef's setComputePipeline, setGraphicsPipeline and setRaytracingPipelineExt.

### Obtaining an extended Pipeline info

Both graphics and raytracing pipelines have additional info that was used during object creation. This data can be obtained by using "Pipeline_info". E.g. Pipeline_info(pipelinePtr, PipelineRaytracingInfo) or Pipeline_info(pipelinePtr, PipelineGraphicsInfo). Make sure that this is safe by checking pipelinePtr->type.

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
    - inferred semanticName: for HLSL/Direct3D12, semantic name is quite important. However, it supports semantic name and value, so we just use semantic name TEXCOORD and the binding id. So TEXCOORD1 would be attribute[1]. This is done to save a lot of space in the PipelineGraphicsInfo. Our custom HLSL syntax for this is `_bind(N)`.
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
  - 2 and 8 aren't always supported, so make sure to query it and/or fallback to 1 or 4 if not present. EGraphicsDataTypes of the device capabilities lists this.
  - See Features/MSAA for more info.
- topologyMode: type of mesh topology.
  - Defaults to TriangleList if not specified.
  - TriangleList, TriangleStrip, LineList, LineStrip, PointList, TriangleListAdj, TriangleStripAdj, LineListAdj, LineStripAdj.
- patchControlPoints: Defines the number of tessellation points (max 32).
- stageCount: how many shader stages are available.
- Using DirectRendering:
  - If DirectRendering is enabled, a simpler way of creating can be used to aid porting and simplify development for desktop.
  - attachmentCountExt: how many render targets should be used.
  - attachmentFormatsExt[i < attachmentCountExt]: the ETextureFormatId of the format. Needs to match the render target's exactly (BGRA8 doesn't match an RGBA8 pipeline!).
  - depthFormatExt: depth format of the depth buffer: None, D16, D32, D32S8Ext, D24S8Ext, S8Ext.
- **TODO**: Not using DirectRendering:
  - If DirectRendering is not supported or the developer doesn't want to use it; a unified mobile + desktop architecture can be used. However; generally desktop techniques don't lend themselves well for mobile techniques and vice versa. So it's still recommended to implement two separate rendering backends on mobile.
  - **TODO**: renderPass:
  - **TODO**: subPass:

#### PipelineStages

A pipeline stage is simply a Buffer and an EPipelineStage. The Buffer is in the format declared in "Shader binary types" and EPipelineStage can be Vertex, Pixel, Compute, GeometryExt, HullExt or DomainExt. GeometryExt is enabled by the GeometryShader feature.

### PipelineRaytracingInfo

The pipeline raytracing info struct contains two types of members; post init and pre init members. Pre init has to be provided before it can be constructed and post init is the data that represents the fully initialized pipeline.

- Post construction (available after createRaytracingPipelinesExt):
  - binaries: List of all the binaries that are used by the pipeline (stages can point to the same binaries, to reduce compilation time).
  - groups: List of all of the hit groups and the stages they point to.
  - entrypoints: What entrypoint in the binary represents the stage at index i.
  - shaderBindingTable: Implementation dependent shader binding table. As long as it aligns with the groupId specified in the PipelineRaytracingInfo->PipelineStages section in this document.
  - sbtOffset: Offset into the SBT (shader binding table). This is if multiple raytracing pipelines are initialized together, because only a single SBT is created per createRaytracingPipelinesExt. sbtOffset will also keep alignment in mind.
  - raygenCount, missCount, callableCount: Count per stage, useful to know what ids are valid miss/raygen or callable shaders.
- Pre construction (supply before createRaytracingPipelinesExt):
  - flags:
    - SkipTriangles/SkipAABBs: Triangle or AABB primitives are skipped for this raytracing pipeline.
    - AllowMotionBlurExt: Allow the hardware accelerated motion blur extension to be executed.
    - NoNull(AnyHit/ClosestHit/Miss/Intersection): (One of) These shaders aren't allowed to be null for both validation and linking reasons.
  - maxPayloadSize: 4-byte aligned payload size (>0 and <=32). This must be the max of all payload sizes used in the raytracing pipeline.
  - maxAttributeSize: 4-byte aligned attribute size for intersection shaders (>=8 and <=32). Must be the max of all intersection attribute sizes. If intersection shaders aren't used, this should be 8.
  - maxRecursionDepth: 1 or 2. 1 indicates having no further traces in the hit/miss shader while 2 indicates having only 1 nested trace in hit/miss shader. For deeper recursions, use a for loop to handle each bounce manually.
  - stageCount/binaryCount/groupCount: How many stages from the 'stages'/'binaries'/'groups' array belong to this pipeline stage. Post init this will be equal to pipeline->stages.length, binaries.length and groups.length respectively.

#### PipelineStages

A pipeline stage is simply a binary id and an EPipelineStage. binaryId points to a valid id of the raytracing pipeline, but can be U32_MAX for miss shaders if NoNullMiss isn't set. can be RaygenExt, CallableExt, MissExt, ClosestHitExt, AnyHitExt or IntersectionExt. After construction, the pipeline stage contains a "localShaderId" and "groupId". These are used to identify miss/raygen and callable shaders and the order they are in is predictable:

- All hit groups have localId 0->groupCount in the exact order that they were registered and groupId 0 -> groupCount.
- All miss shaders have localId 0->count(stages, EPipelineStage_MissExt) and groupId of groupCount->groupCount+missShaders.
- All raygen shaders have localId 0->count(stages, EPipelineStage_RaygenExt) and groupId of groupCount+missShaders->groupCount+missShaders+raygenShaders.
- All callable shaders have localId 0->count(stages, EPipelineStage_CallableExt) and groupId of groupCount+missShaders+raygenShaders->
  groupCount+missShaders+raygenShaders+callableShaders.

The groupId is used to determine where in the shader binding table it is located. This is only used internally. Miss shader localId / callable shader localId and raygen shader localId are all useful for knowing which shader to execute.

#### Compilation / binaries

Compilation spits out a single binary with HLSL, which means that only a single compile should be sufficient if all entrypoints are in a single file. The entrypoint names of them are maintained and have to be specified if it's not "main" (otherwise it's safe to avoid the entrypoint array or a single CharString entry).

### Compute example

```c
tempShader = ...;		//Buffer: Load from virtual file or hardcode.

CharString nameArr[] = {
    CharString_createConstRefCStr("Test compute pipeline")
};

ListBuffer computeBinaries = (ListBuffer) { 0 };
gotoIfError(clean, ListBuffer_createRefConst(tempShader, 1, &computeBinaries));
gotoIfError(clean, ListCharString_createConstRefConst(nameArr, 1, &names));
gotoIfError(clean, GraphicsDeviceRef_createPipelinesCompute(device, &computeBinaries, names, &computeShaders));

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

ListPipelineStage stageInfos = (ListPipelineStage) { 0 };
gotoIfError(clean, ListPipelineStage_createRefConst(stage, sizeof(stage) / sizeof(stage[0]), &stageInfos));

//Define all pipelines.
//These pipelines require the graphics feature DirectRendering and will error otherwise!
//Otherwise .attachmentCountExt, .attachmentFormatsExt and/or .depthStencilFormat
//  need to be replaced with .renderPass = renderPass and/or .subPass = subPassId.

PipelineGraphicsInfo info[1] = {
    (PipelineGraphicsInfo) {
        .stageCount = 2,
        .attachmentCountExt = 1,
        .attachmentFormatsExt = { ETextureFormat_BGRA8 }
    }
};

//Create pipelines, this will take ownership of the binaries (if they're managed).
//Make sure to release them after to avoid freeing twice!

ListPipelineGraphicsInfo infos = (ListPipelineGraphicsInfo) { 0 };
gotoIfError(clean, ListPipelineGraphicsInfo_createConstRef(info, sizeof(info) / sizeof(info[0]), &infos));
gotoIfError(clean, GraphicsDeviceRef_createPipelinesGraphics(
    device, &stageInfos, &infos, ListCharString_createNull(), &graphicsShaders
));

tempShaders[0] = tempShaders[1] = Buffer_createNull();
```

Create pipelines will take ownership of the buffers referenced in stages and it will therefore free the list (if unmanaged). If the buffers are managed memory (e.g. created with Buffer_create functions that use the allocator) then the Pipeline object will safely delete it. This is why the tempShader is set to null after (the list is a ref, so doesn't need to be). In clean, this temp buffer gets deleted, just in case the createPipelines fails. Using virtual files for this is recommend, as they'll already be present in memory and our ref will be available for the lifetime of our app. If it's a ref then the implementation will copy to avoid unsafe behavior.

It is recommended to generate all pipelines that are needed in this one call at startup, to avoid stuttering at runtime. This can be done from a different thread as well, though the implementation is free to delay the wait for finalization (real driver compiles) until it's first use in a command list (e.g. it starts compilation in async).

### Raytracing example

```c
//... Load shaders from virtual file system into tempShaders[0]

//Define our entrypoints; a single compiled binary with specified entry points.

PipelineStage stageArr[] = {
    (PipelineStage) { .stageType = EPipelineStage_ClosestHitExt, .binaryId = 0 },
    (PipelineStage) { .stageType = EPipelineStage_MissExt, .binaryId = 0 },
    (PipelineStage) { .stageType = EPipelineStage_RaygenExt, .binaryId = 0 }
};

CharString entrypointArr[] = {
    CharString_createRefCStrConst("mainClosestHit"),
    CharString_createRefCStrConst("mainMiss"),
    CharString_createRefCStrConst("mainRaygen")
};

//Define a hit group

PipelineRaytracingGroup hitArr[] = {
    (PipelineRaytracingGroup) {
        .closestHit = 0, .anyHit = U32_MAX, .intersection = U32_MAX
    }
};

//Define the pipeline that links these together as one.
//We can link multiple at the same time.
//All ids (in hitArr, stageArr) are relative to the stage itself (no global binary id).

CharString nameArr[] = {
    CharString_createRefCStrConst("Raytracing pipeline test")
};

U64 count = sizeof(nameArr) / sizeof(nameArr[0]);
U64 entrypointCount = sizeof(stageArr) / sizeof(stageArr[0]);
U64 hitCount = sizeof(hitArr) / sizeof(hitArr[0]);

PipelineRaytracingInfo infoArr[] = {
    (PipelineRaytracingInfo) {

        (U8) EPipelineRaytracingFlags_Default,	//No intersection shaders
        16,										//Payload size
        8,										//Attribute size
        1,										//Recursion

        (U32) entrypointCount,
        (U32) count,
        (U32) hitCount
    }
};

//Turn into lists

ListBuffer binaries = (ListBuffer) { 0 };
ListCharString names = (ListCharString) { 0 };
ListPipelineStage stages = (ListPipelineStage) { 0 };
ListCharString entrypoints = (ListCharString) { 0 };
ListPipelineRaytracingGroup hitGroups = (ListPipelineRaytracingGroup) { 0 };
ListPipelineRaytracingInfo infos = (ListPipelineRaytracingInfo) { 0 };

gotoIfError(clean, ListBuffer_createRefConst(tempBuffers, count, &binaries));
gotoIfError(clean, ListCharString_createRefConst(nameArr, count, &names));
gotoIfError(clean, ListPipelineRaytracingInfo_createRefConst(infoArr, count, &infos));

gotoIfError(clean, ListPipelineStage_createRefConst(stageArr, entrypointCount, &stages));
gotoIfError(clean, ListCharString_createRefConst(entrypointArr, entrypointCount, &entrypoints));

gotoIfError(clean, ListPipelineRaytracingGroup_createRefConst(hitArr, hitCount, &hitGroups));

//Finalize into pipeline

gotoIfError(clean, GraphicsDeviceRef_createPipelineRaytracingExt(
    twm->device, stages, &binaries, hitGroups, infos, &entrypoints, names, &raytracingShaders
));

tempBuffers[0] = tempBuffers[1] = tempBuffers[2] = Buffer_createNull();
```

## Shader binary types

In OxC3 graphics, either the application or the OxC3 baker (or the OxC3 compiler) is responsible for compiling and providing binaries in the right formats. According to OxC3 graphics, the shaders are just a Buffer that contain an oiSH file. This oiSH file should include one of the following compile types:

- Direct3D12: DXIL (binary).
- Vulkan: SPIR-V (binary).
  - HLSL Entrypoint needs to be remapped to main, except for raytracing shaders.
- Metal: MSL (UTF8 text).
- WebGPU: WGSL (UTF8 text).

With the following limitations:

- The resources require bindless to function, so shaders should use this as well. When using resources, resources.hlsl has to be included (the compiler automatically includes it). This file can be found in inc/shader_compiler/shaders.
  - types.hlsl also defines all HLSL types as OxC3 types, to ensure it could be cross compiled to GLSL in the future (since GLSL has some features that HLSL might not support, such as HW RT motion blur). Using these predefined types are fully optional if HLSL is the final target (Vulkan + D3D12), though it is recommended to use them to avoid getting stuck to one shading language.

The OxC3 baker will (if used) convert HLSL to SPIR-V, DXIL, MSL or WGSL depending on which API is currently used. It can provide this as a pre-baked binary too (.oiSH Oxsomi SHader). The pre-baked binary contains all 4 formats to ensure it can be loaded on any platform. But the baker will only include the one relevant to the current API to prevent bloating.

When using the baker, the binaries can simply be loaded using the oiCS helper functions and passed to the pipeline creation, as they will only contain one binary.

**TODO: The baker currently doesn't include this functionality just yet. Currently, compile.bat and compile_debug.bat are used to compile shaders. **

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
gotoIfError(clean, GraphicsDeviceRef_createBufferData(
    device, EDeviceBufferUsage_Vertex, name, &vertexData, &vertexBuffers[0]
));
```

### Properties

- resource: the GraphicsResource it inherits from.
- usage:
  - Vertex (use as vertex buffer).
  - Index (use as index buffer).
  - Indirect (use for indirect draw calls).
  - Raytracing related:
    - ScratchExt (used internally for scratch buffers for raytracing).
    - ASExt (used internally for creation of acceleration structures).
    - ReadASExt (If the acceleration structure needs to be able to read the vertex/index buffer).
    - SBTExt (used internally for shader binding table creation).
- isPending(FullCopy): Information about if any data is pending for the next submit and if the entire resource is pending.
- isFirstFrame: If the resource was already uploaded before.
- length: Length of the buffer.
- cpuData: If CPUBacked stores the CPU copy for the resource or temporary data for the next submit to copy CPU data to the real resource.
- pendingChanges: `[U64 startRange, U64 endRange][]` list of marked regions for copy.
- readHandle, writeHandle: Places where the resource can be accessed on the GPU side. If a shader uses the writeHandle in a shader it has to transition the resource (or the subresource) to write state before it is accessed as such (at the relevant shader stage); same with the readHandle (but read state). If you're only reading/writing from a part of a resource it is preferred to only transition part of the resource. This will signal the implementation that other parts of the resource aren't in use. Which could lead to more efficient resource updates for example. Imagine streaming in/out meshes from a single buffer; only meshes that are in use need to be updated with the staging buffer, while others could be directly copied to GPU visible memory if available (ReBAR, shared mem, cpu visible, etc.). It could also reduce decompression/compression time occurring on the GPU due to changing the entire resource to write instead of readonly (with subresources this could be eased depending on the driver).
- lock: Multi-threading helper. A buffer gets locked when it's being modified or used by the CPU while recording. For example DeviceBufferRef_markDirty will require a lock and GraphicsDeviceRef_submitCommands will too. So markDirty has to finish before or after the submitCommands is done.

### Functions

- `markDirty(U64 offset, U64 length)` marks part or entire resource dirty. This means that next commit the implementation will decide on how to copy to the resource in an efficient way. For example if the resource isn't in flight and ReBAR is turned on or shared memory is in use then it can directly copy to CPU accessible memory. Otherwise it might have to use a copy queue or something similar. The region is merged with any other pending region 256 bytes on either side to avoid lots of fragmented copies. Please make sure to only call this when necessary as this might cause extra allocations or copies to a RingBuffer; a good strategy (if the buffer changes each frame) might be to make the buffer 3x as big as necessary and index based on frameId % 3 and make the buffer CPUAllocated to allow direct writes always and then copy from this buffer only when needed. The framework will try to optimize these copies as much as possible (to avoid having to do that manually), but more work might be needed from the developers using it instead.

### Used functions and obtained

- Obtained through createBuffer and createBufferData from GraphicsDeviceRef.
- Used in DeviceBufferRef's markDirty, as vertex/index/indirect buffer for commands such as draw/drawIndirect/drawIndirectCountExt, shaders if the resource is readable/writable (through transitions), copy and clear buffer operations.

## Features

### MSAA

MSAA can be enabled by making sure all pipelines for MSAA targets have PipelineGraphicsInfo::msaa set to something that's not EMSAASamples_Off. This setting needs to match 1:1 with the MSAA setting passed to render target(s) and depth stencil(s). PipelineGraphicsInfo::msaaMinSampleShading can be set to a non zero value to indicate [sample shading](https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#primsrast-sampleshading) should be turned on and to what value (an MSAA feature to make texture detail anti alias better).

If MSAA is enabled for the current target, a resolve image can be passed in the attachmentInfo.resolveImage and resolveRange passed to startRenderExt. This can directly resolve the MSAA texture to a render target, swapchain or depth stencil. attachmentInfo.resolveMode can be used to change it from averaging all samples to EMSAAResolveMode_Min/Max.

### Raytracing

Raytracing enables the creation of acceleration structures that are usable by the underlying API; either as a hardware or software layer.

Raytracing has two modes:

- Inline raytracing (RayQuery); this can be invoked from any shader and has more flexibility. This is available on some mobile devices and certain desktop GPUs (excluding Pascal).
- Raytracing pipelines (RayPipeline); old pipeline which may or may not be more efficient because of certain features such as intersection or anyHit shaders or micromaps. Also makes porting from older raytracing implementations easier.

In both of these modes, EGraphicsFeatures_Raytracing will be set and that means that BLASes/TLASes can be created.

#### DeviceData

Device data is a subarea of a buffer; it contains the reference to the buffer resource, an offset and length (U64s).

#### RTAS

Contains the following properties:

- device: the device that the AS was created on. An AS is only compatible with other ASes that are from the same device.
- isMotionBlurExt: relevant for BLAS or TLAS; BLAS it means it contains the previous data of the geometry while TLAS means it contains previous data of all instances (so it can be motion blurred). This is the NV specific extension for Ampere and up (unless anyone else supports it).
- isCompleted: this is set when the BLAS has been signaled as fully built. For example when buildBLASExt has been called or when the first submitCommands has been triggered since it has been queued.
- flagsExt: BLAS or TLAS specific flags (currently only used for BLAS).
- asConstructionType: the BLAS or TLAS specific construction type.
- parent: the acceleration structure that was used as a base (for example: compaction or refitting).
- scratchBuffer: temporary data that is only available until the AS has been created and the frame has been completed on the CPU.
- asBuffer: the buffer resource that represents this acceleration structure.
- flags:
  - AllowUpdate (0): refitting is allowed. This is a faster way of updating acceleration structures, but at the cost of traversal time.
  - AllowCompaction (1): compaction is allowed. This reduces memory overhead for the acceleration structures.
  - FastTrace (2): optimize trace times over build times / memory.
  - FastBuild (3): optimize build times over trace times / memory.
  - MinimizeMemory (4): optimize memory over trace times / build times.
  - IsUpdate (5): this RTAS build is a refit (reuses old data to expand the ASes).
  - DisableAutomaticUpdate (6): next submitCommands shouldn't build this acceleration structure. This is useful when the mesh has to be initialized by the GPU using commands first (such as copies, compute or stream out).

#### BLAS

A BLAS is a bottom level acceleration structure; it is the representation of a couple volumes or triangle geometry. It has an acceleration structure built over this geometry to accelerate tracing rays through it. These BLASes are used through a TLAS (top level AS) and can then be accessed in raytracing.

There are two types of BLASes:

- Regular geometry (triangles).
- Procedural geometry (AABBs).

The latter can only be used through intersection shaders, while the former has way better traversal speeds. Procedural geometry is generally super slow and it's generally faster to approximate with triangle geometry.

##### Example: Triangle geometry

```c
gotoIfError(clean, GraphicsDeviceRef_createBLASExt(
	twm->device,
	ERTASBuildFlags_DefaultBLAS,			//Fast trace & allow compaction
	EBLASFlag_DisableAnyHit,				//No transparency needed, optimize for opaque
	ETextureFormatId_RG16f, 0,				//No pos attrib offset and format is RG16f
	ETextureFormatId_R16u,					//Indices are 16-bit
	(U16) sizeof(vertexPos[0]),				//Stride is F16[2]
	(DeviceData) {
        .buffer = twm->vertexBuffers[0]		//Entire buffer is accessible
    },
	(DeviceData) {
        .buffer = twm->indexBuffer,
        .len = sizeof(U16) * 6 				//Only use sub region
   	},
	NULL,									//No refit
	CharString_createRefCStrConst("BLAS"),	//Debug name
	&twm->blas								//BLASRef*
));
```

The example above assumes that there is an index buffer and a position buffer available. Those buffers need to have the ASReadExt buffer usage to be accessible during build time.

There is also a version of this that doesn't require index buffers; createBLASUnindexedExt. The DeviceData for indexBuffer and the index format can be left out for that function.

The BLAS flags are the following: EBLASFlag_AvoidDuplicateAnyHit and EBLASFlag_DisableAnyHit. This is only relevant for raytracing pipelines and is irrelevant if the TLAS instance itself turns off anyHit.

##### Example: Procedural geometry

```c
//Make simple AABB test

F32 aabbBuffer[] = {

	-1, -1, -1,		//min 0
	0, 0, 0,		//max 0

	0, 0, 0,		//min 1
	1, 1, 1			//max 1
};

Buffer aabbData = Buffer_createRefConst(aabbBuffer, sizeof(aabbBuffer));
name = CharString_createRefCStrConst("AABB buffer");
gotoIfError(clean, GraphicsDeviceRef_createBufferData(
	twm->device,
    EDeviceBufferUsage_ASReadExt, EGraphicsResourceFlag_None,
    name, &aabbData, &twm->aabbs
));

gotoIfError(clean, GraphicsDeviceRef_createBLASProceduralExt(
	twm->device,
	ERTASBuildFlags_DefaultBLAS,
	EBLASFlag_DisableAnyHit,
	sizeof(F32) * 3 * 2,
	0,
	(DeviceData) { .buffer = twm->aabbs },
	NULL,
	CharString_createRefCStrConst("Test BLAS AABB"),
	&twm->blasAABB
));
```

In the example above, two AABBs are created from a buffer.

##### Properties

- base: RTAS standardizes BLAS and TLAS a little bit.
- name: the debug name of the BLAS.
- Depending on base.asConstructionType: (Geometry, AABB, Serialized):
  - EBLASConstructionType_AABB:
    - aabbStride, aabbOffset (U32): the stride of the AABB buffer and the offset to encounter the first AABB. E.g. struct: F32x3 min, max at offset + stride * i. The stride has to be 8-byte aligned.
    - aabbBuffer: DeviceData pointing to the U8[aabbStride].
  - EBLASConstructionType_Serialized
    - cpuData: contains a serialized version of the BLAS. Might become invalidated because of a driver update.
  - EBLASConstructionType_Geometry
    - positionFormatId: RGBA16f, RGBA32f, RGBA16s, RG16f, RG32f, RG16s. What kind of format the position itself is stored in.
    - indexFormatId: R16u, R32u or Undefined. The index format; if undefined, the mesh is unindexed.
    - positionBufferStride: >0 and <=2048. The stride between each vertex. It is more optimal to only have positions in this buffer, so building won't waste time looking at other data. This is generally the case for normal mobile rendering as well (or visibility buffer approaches). This stride needs to be a multiple of 2 (if 16-bit formats are used) or 4 (if 32-bit formats are used).
    - positionOffset: offset in the vertex to point to the position. Needs to be less than positionBufferStride (including the position itself).
    - indexBuffer, positionBuffer: DeviceData pointing to the vertex and index buffers.

##### Used functions and obtained

- Obtained through createBLASProceduralExt, createBLASExt and createBLASUnindexedExt from GraphicsDeviceRef.
- Used in TLAS creation and can be copied to/from the CPU.

#### TLAS

A TLAS is a top level acceleration structure; it is the representation of a couple instances of triangle geometry or procedural geometry (a scene). It has an acceleration structure built over these instances to accelerate tracing rays through it. BLASes (Bottom level acceleration structures) are used through a TLAS (top level AS) and can be accessed in raytracing.

There are two types of TLASes:

- HW Static intersections (static geometry).
- HW Motion blur intersections (static, matrix or transform motion).

The latter can only be used with HW motion blur hardware, while the former is always available (if raytracing is available) and is simpler to construct.

A TLAS can be initialized through two different methods:

- CPU-side (Might introduce a temporary staging resource or ring buffer).
- GPU side by an already initialized GPU resource.

##### Example: Static instances

```c
TLASInstanceStatic instances[1] = {
    (TLASInstanceStatic) {
        .transform = {
            { 1, 0, 0, 0 },
            { 0, 1, 0, 0 },
            { 0, 0, 1, 0 }
        },
        .data = (TLASInstanceData) {
            .blasCpu = twm->blas,
            .instanceId24_mask8 = ((U32)0xFF << 24),
            .sbtOffset24_flags8 = (ETLASInstanceFlag_Default << 24) | 0	//Hit index 0
        }
    }
};

ListTLASInstanceStatic instanceList = (ListTLASInstanceStatic) { 0 };
gotoIfError(clean, ListTLASInstanceStatic_createRefConst(
    instances, sizeof(instances) / sizeof(instances[0]), &instanceList
));

gotoIfError(clean, GraphicsDeviceRef_createTLASExt(
    twm->device,
    ERTASBuildFlags_DefaultTLAS,
    NULL,
    instanceList,
    CharString_createRefCStrConst("Test TLAS"),
    &twm->tlas
));
```

For GPU construction, the same can be done and has to follow the exact struct; except blasCpu will become the address of the BLAS on the GPU.

##### Example: Motion blur

```c
TLASInstanceMotion instances[1] = {
    (TLASInstanceMotion) {
        .type = ETLASInstanceType_Matrix,			//Static for ETLASInstanceStatic
        .matrixInst = (TLASInstanceMatrixMotion) {
            .prev = {
                { 1, 0, 0, 0 },
                { 0, 1, 0, 0 },
                { 0, 0, 1, 0 }
            },
            .next = {
                { 2, 0, 0, 0 },		//Grow twice in size as an animation
                { 0, 2, 0, 0 },
                { 0, 0, 2, 0 }
            },
            .data = (TLASInstanceData) {
                .blasCpu = twm->blas,
                .instanceId24_mask8 = ((U32)0xFF << 24),
                .sbtOffset24_flags8 = (ETLASInstanceFlag_Default << 24) | 0	//Hit index 0
            }
        }
    }
};

ListTLASInstanceMotion instanceList = (ListTLASInstanceMotion) { 0 };
gotoIfError(clean, ListTLASInstanceMotion_createRefConst(
    instances, sizeof(instances) / sizeof(instances[0]), &instanceList
));

gotoIfError(clean, GraphicsDeviceRef_createTLASMotionExt(
    twm->device,
    ERTASBuildFlags_DefaultTLAS,
    NULL,
    instanceList,
    CharString_createRefCStrConst("Test TLAS"),
    &twm->tlas
));
```

For GPU construction, the same can be done and has to follow the exact struct; except blasCpu will become the address of the BLAS on the GPU.

##### Properties

- base: RTAS standardizes BLAS and TLAS a little bit.
- useDeviceMemory: If the TLAS was created through GPU memory or via a temporary CPU visible buffer.
- handle: Descriptor index of the acceleration structure.
- Depending on useDeviceMemory, base.isMotionBlurExt and base.asConstructionType: (Instances, Serialized):
  - ETLASConstructionType_Serialized:
    - Buffer cpuData: Indicates cpu memory that represents the TLAS. Might become invalidated because of a driver update.
  - useDeviceMemory:
    - deviceData: Represents the resource range that holds the TLASInstanceStatic[] or TLASInstanceMotion[] (isMotionBlurExt) on the GPU.
  - isMotionBlurExt (requires motion blur feature):
    - cpuInstancesMotion: List of TLASInstanceMotion.
  - else
    - cpuInstancesStatic: List of TLASInstanceStatic.

##### TLASInstanceStatic + TLASInstanceMotion

Both represent a single mesh instance. They both contain one instance data (TLAS_getInstanceDataCpu) while they contain 1 or 2 transforms. TLASInstanceMotion may contain two transforms if the type is not ETLASInstanceType_Static. Otherwise either transform or staticInst.transform contain a F32x4x3 transform matrix.  When the instance type is ETLASInstanceType_Matrix it contains two matrices (matrixInst.prev and matrixInst.next).

When the type is ETLASInstanceType_SRT, srtInst.prev and next contain a special transform format;

- Scale, pivots, translate and shear: x, y, z
- Orientation: Quaternion xyzw.

This type has helpers through TLASTransformSRT such as create(scale, pivot, translate, quat, shearing), createSimple(scale, translate, quat) and getters/setters for each type.

##### Instance data

Instance data contains the information that describes everything besides the transform to the acceleration structure:

- instanceId24_mask8: Low 24 bits contain the InstanceID() intrinsic while the high 8 bits are the mask, useful for masking out certain objects when tracing rays.
- sbtOffset24_flags8: Low 24 bits contain the shader binding table offset (hit shader id), while the high 8 bits contain the instance flags.
- blasCpu or blasDeviceAddress.
  - blasCpu is a BLASRef* used only when the TLAS is created from the CPU.
  - blasDeviceAddress is a U64 that is the GPU buffer address when the TLAS is created from the GPU.
  - If this parameter is NULL or 0, it indicates the instance should be hidden.

###### Instance flags

The instance flags are identical to both DXR and VkRT:

- DisableCulling (0): Don't listen to the back/front face culling flags of TraceRay.
- CCW (1): Counter clockwise winding order.
- ForceDisableAnyHit (2): Never run any hit.
- ForceEnableAnyHit (3): Always run any hit.

##### Used functions and obtained

- Obtained through createTLASExt, createTLASMotionExt and createTLASDeviceExt from GraphicsDeviceRef.
- Accessible from TraceRay/traceRayEXT only through the handle.

## Commands

### Summary

Command lists store the commands referenced in the next section. These are virtual commands; they approximately map to the underlying API. If the underlying API doesn't support commands, it might have to simulate the behavior with a custom shader (such as creating a mip chain of an image). It is also possible that a command might need certain extensions, without them the command will give an error to prevent it from being inserted into the command list (some unimportant ones such as debugging are safely ignored if not supported instead). These commands are then processed at runtime when they need to. If the command list remains the same, it's the same swapchain (if applicable) and the resources aren't recreated then it can safely be re-used.

Invalid API usage will be attempted to be found out when inserting the command, but this is not always possible.

*Note: A CommandList is only accessible to the thread that opened it. It can however be acquired on a separate thread after it closed on the other thread. This is why the begin also has a timeout value.*

### begin/end

To make sure a command list is ready for recording and submission it needs begin and end respectively. Begin has the ability to clear the command list (which is generally desired). However, it's also possible that the command list was closed by one library and then given back to another. In this case, it can safely be re-opened without the clear flag. After ending it, it can be passed to submitCommands. These "commands" are special, as they're not inserted into the command list itself, they just affect the state of the command list. Only one submit is allowed per frame, as it handles synchronization implicitly.

### setViewport/(And)Scissor

setViewport and setScissor are used to set viewport and scissor rects respectively. However, since in a lot of cases they are set at the same time, there's also a command that does both at once "setViewportAndScissor".

```c
gotoIfError(clean, CommandListRef_setViewportAndScissor(
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
gotoIfError(clean, CommandListRef_setStencil(commandList, 0xFF));
```

And the blend constants can be set like so:

```c
gotoIfError(clean, CommandListRef_setBlendConstants(
    commandList, F32x4_create4(1, 0, 0, 1)
));
```

### clearImage(f/u/i)/clearImages

clearImagef/clearImageu/clearImagei and clearImages are actually the same command. They allow clearing one or multiple images. clearImages should be used whenever possible because it can batch clear commands in a better way. However, it is possible that only one image needs to be cleared and in that case clearImage(f/u/i) are perfectly fine. The f, u and i suffix are to allow clearing uint, int and float targets. In clearImages these are handled manually. The format should match the format of the underlying images. The images passed here are automatically transitioned to the correct state.

```c
gotoIfError(clean, CommandListRef_clearImagef(
    commandList, 				//See "Command list"
    F32x4_create4(1, 0, 0, 1), 	//Clear red
    (ImageRange){ 0 }, 			//Clear layer and level 0
    swapchain					//See "Swapchain" or "RenderTexture"
));
```

To clear multiple at once, call clearImages with a `ListClearImageCmd`. ClearImageCmd takes a color, a range and the image ref ptr. It's the same as a single clear image but it allows multiple.

Clear image is only allowed on images which aren't currently bound as a render target. The image is leading in determining the format which will be read out. If you use clearImagef but it's a uint texture then it will bitcast the float color to a uint for you.

Clear image can currently only be called on a Swapchain or RenderTexture object.

### copyImage/copyImageRegions

copyImage and copyImageRegions are actually the same command. They allow copying either one or multiple regions from the same image to another, as long as the two have the same format.

```c
gotoIfError(clean, CommandListRef_copyImage(
    commandList,
    tw->swapchain, 				//src (Swapchain, DepthStencil, RenderTexture)
    tw->renderTextureBackup, 	//dst (^)
    ECopyType_All, 				//Copy depth & stencil or color
    (CopyImageRegion) { 0 }		//Copy everything
));
```

Clearing multiple ranges at once can be done by calling copyImageRegions with a ListCopyImageRegion. The src and dst must both be able to contain the pixels copied.

If depth, width or height isn't defined, they are automatically set to res[i] - offset[i].

DepthStencil is only compatible with depth stencil. If ECopyType_All is used, the depth stencil format has to be identical. If ECopyType_DepthOnly is used the depth has to be compatible (D32, D32S8). If ECopyType_StencilOnly is used the stencil has to be compatible.

Copy image is only allowed on images which aren't currently bound as a render target or for a different use in this scope.

Copy image (ranges) can currently only be called on a Swapchain, DepthStencil or RenderTexture object.

### setComputePipeline/setGraphicsPipeline/setRaytracingPipeline

The set pipeline command does one of the following; bind a graphics pipeline, raytracing pipeline or a compute pipeline. These pipelines are the only bind points and they're maintained separately. So a bind pipeline of a graphics shader and one of a compute shader don't interfere. This is used before a draw, dispatch or dispatchRaysExt to ensure the shader is used.

```c
gotoIfError(clean, CommandListRef_setComputePipeline(commandList, compPipeline));
gotoIfError(clean, CommandListRef_setGraphicsPipeline(commandList, gfxPipeline));
gotoIfError(clean, CommandListRef_setRaytracingPipeline(commandList, rtPipeline));
```

### draw

The draw command will simply draw the currently bound primitive buffer (optional) with the currently bound graphics pipeline (required). If the primitive buffer is not present, the pipeline is expected to generate the vertices dynamically. It requires a render call to be started (see DirectRendering). It handles both indexed and non indexed draw calls:

```c
//Non indexed draw call
//Can also specify instanceOffset and vertexOffset if drawUnindexedAdv is used

gotoIfError(clean, CommandListRef_drawUnindexed(commandList, 3, 1));

//Indexed draw call
//Can also specify instanceOffset, indexOffset and vertexOffset
//if drawIndexedAdv is used

gotoIfError(clean, CommandListRef_drawIndexed(commandList, 3, 1));
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
- (implied) bufferStride: 16 or 32 byte depending on if it's indexed or not.
- drawCalls: how many draw calls are expected to be filled in this buffer. A draw can also set the draw parameters to zero to disable it (instanceCount / index / vertexCount), though for that purpose drawIndirectCountExt is recommended. Make sure the buffer has `U8[bufferStride][maxDrawCalls]` allocated at bufferOffset.
- indexed: if the draw calls are indexed or not. The device can't combine non indexed and indexed draw calls, so if you want to combine them you need to do this as two separate steps.
  - If not indexed; the buffer takes a DrawCallUnindexed struct: U32 vertexCount, instanceCount, vertexOffset, instanceOffset.
  - Otherwise; the buffer takes a DrawCallIndexed struct: U32 indexCount, instanceCount, indexOffset, I32 vertexOffset, U32 instanceOffset, U32 padding[3].

drawIndirect transitions the input buffer to IndirectDraw. This means that the buffer can't also be bound as a Vertex/Index buffer or be used in the shader as a read/write buffer in the same scope. Buffer needs to enable Indirect usage to be usable by indirect draws.

#### drawIndirectCountExt

Same as drawIndirect, except it adds a DeviceBuffer counter which specifies how many active draw calls there are. This is very useful as it allows culling to be done entirely by compute.

- drawCalls now represents 'maxDrawCalls' which limits how many draw calls might be issued by the device.
- countBuffer now represents a U32 in the DeviceBuffer at the offset. Make sure to align 4-byte to satisfy alignment requirements.
- Requires MultiDrawIndirectCount extension.

drawIndirect transitions the input buffer to IndirectDraw. This means that the buffer can't also be bound as a Vertex/Index buffer or be used in the shader as a read/write buffer in the same scope. Buffer needs to enable Indirect usage to be usable by indirect draws.

### setPrimitiveBuffers

Sets the primitive buffers (vertex + index buffer(s)) for use by draw commands such as draw, drawIndirect and drawIndirectCountExt. The buffers need the Vertex and/or Index usage set if they're used for that purpose. In the same scope of the setPrimitiveBuffers it is illegal to transition the subresource(s) back to a different state or to write/read from other sources that use it (check the Scope section of this document). The vertex buffer(s) need to have the same layout as specified in the pipeline and the ranges specified by the draw calls (such as count and offset) need to match up as well.

```c
SetPrimitiveBuffersCmd primitiveBuffers = (SetPrimitiveBuffersCmd) {
    .vertexBuffers = { vertexBuffers[0], vertexBuffers[1] },
    .indexBuffer = indexBuffer,
    .isIndex32Bit = false
};

gotoIfError(clean, CommandListRef_setPrimitiveBuffers(commandList, primitiveBuffers));
```

### dispatch

Dispatch has some of the same requirements as draw calls as it needs a correct state of the resources and needs a compute pipeline bound as well. However, it doesn't need to be in an active render (render pass or direct rendering) and it also doesn't use the viewport/scissor. This makes compute one of the easiest to handle, though transitions are just as important as with graphics shaders (resources need to be transitioned through a scope).

```c
gotoIfError(clean, CommandListRef_dispatch2D(commandList, tilesX, tilesY));
```

dispatch2D, dispatch1D and dispatch3D are the easiest implementations. You dispatch in groups, so it has to be aligned to the thread count of the compute shader. This command is also generalized with the dispatch command which takes in a `Dispatch` struct.

#### dispatchIndirect

Same thing as dispatch, except the device reads from a U32x3 into the DeviceBuffer at the offset and dispatches the groups stored there. Has to be aligned to 16-byte. For 2D dispatches please set z to 1, for 1D set y to 1 as well.

dispatchIndirect transitions the input buffer to IndirectDraw. This means that the buffer can't also be bound as a Vertex/Index buffer or be used in the shader as a read/write buffer in the same scope. Buffer needs to enable Indirect usage to be usable by indirect draws.

### DebugMarkers feature

The DebugMarkers feature adds three commands: addMarkerDebugExt, startRegionDebugExt and endRegionDebugExt. If the DebugMarkers feature isn't present, these are safely ignored and won't be inserted into the virtual command list. This can be used to provide extra debugging info to tools such as RenderDoc, NSight, Pix, etc. to show where important events such as render calls happened and what they represented. A debug region or debug marker has a color and a name. A debug region is like a stack; you can only end the current region and push another region. Every region you start has to be ended in the same scope that is submitted.

```c
gotoIfError(clean, CommandListRef_startRegionDebugExt(
    commandList, 							//See "Command list"
    F32x4_create4(1, 0, 0, 1), 				//Marker color
    CharString_createConstRefCStr("Test")	//Marker name
));

//Insert operation that needs to be in the debug region

gotoIfError(clean, CommandListRef_endRegionDebugExt(commandList));
```

The same syntax as startRegionDebugExt can be used for addMarkerDebugExt. Except a marker doesn't need any end, it's just 1 event on the timeline. Every end region needs to correspond with a start region and vice versa.

### DirectRendering feature

DirectRendering allows rendering without render passes (default behavior in DirectX). This makes development for desktop a lot easier since MSFT (Warp), AMD, Intel and NVIDIA aren't using tiled based deferred rendering (TBDR). However, all other vendors (such as Qualcomm, ARM, Imgtec) do use TBDR (mostly mobile architectures). The user is allowed to decide that this is a limitation they accept and can use this feature to greatly simplify the difficulty of the graphics layer (especially porting from existing apps). The user can also set up two different render engines; one that can deal with direct rendering and one that can't. The latter is targeted at mobile (lower hardware tier) and the former is for desktop/console. The two commands that are related to this feature are: startRenderExt and endRenderExt. They require the feature to be present and will return an error (and won't be inserted into the command list) otherwise.

Even if mobile chips do support this feature, it is automatically disabled to prevent the developer from accidentally enabling it causing performance issues.

Just like *most* commands, this will automatically transition the resources (render targets only) into the correct states for you. Color attachments can always be read, but the user is in charge of specifying if the contents should be cleared or kept. Color attachments can also be marked as discardable, this means the final result won't necessarily be output into a buffer for use after the render. If it's marked as readonly, the shader won't be able to write to it.

```c
//In command list recording

AttachmentInfo attachmentInfo = (AttachmentInfo) {
    .range = (ImageRange) { 0 },					//Layer 0, level 0
    .image = swapchain,								//See "Swapchain" or "RenderTexture"
    .unusedAfterRender = false,						//Don't discard after endRender
    .readOnly = false,								//Allow draw calls to write
    .load = ELoadAttachmentType_Clear,				//Clear image
    .color = (ClearColor) { .colorf = {  1, 0, 0, 1 } }
};

ListAttachmentInfo colors = (ListAttachmentInfo) { 0 };
gotoIfError(clean, ListAttachmentInfo_createRefConst(attachmentInfo, 1, &colors));

gotoIfError(clean, CommandListRef_startRenderExt(
    commandList, 							//See "Command list"
    I32x2_zero(), 							//No offset
    I32x2_zero(), 							//Use attachment's size
    colors,
    (DepthStencilAttachmentInfo) { 0 }		//No depth stencil attachment
));

//Draw calls here

gotoIfError(clean, CommandListRef_endRenderExt(commandList));
```

*Keep in mind that during the scope, you can't transition the resources passed into the attachments of the active render call. They're not allowed to be used as a read (SRV) or write textures (UAV); they're only allowed to be output attachments (RTV). Doing otherwise will cause the command list to give an error and remove the current scope.*

Every startRender needs to match an endRender. During the render it's not allowed to access the render textures as a write or read texture. Other operations that change state (implicit transitions) such as clears and copies are also not allowed. If this is required, the developer can end the render and restart it after this operation.

A graphics pipeline for use with DirectRendering needs to set the attachment count (and format(s)) or the depth stencil format. One that doesn't use direct rendering can't be used.

### Raytracing feature

#### updateTLASExt/updateBLASExt

Are used to force update TLAS and BLAS respectively. This is useful when the TLAS/BLAS is entirely GPU generated with compute and thus, it needs to be issued on the GPU (not CPU). The CPU generated one only works if GPU buffers were filled previous frame or if the CPU does. For more complex scenarios, auto update should be turned off when BLAS/TLAS is generated and manual updates should be done.

### RayPipeline feature

#### dispatchRaysExt

dispatchRays has some of the same requirements as dispatch as it needs a correct state of the resources and needs a raytracing pipeline bound as well. It is similar to compute as it doesn't need to be in an active render (render pass or direct rendering) and it also doesn't use the viewport/scissor. Though transitions are just as important as with compute shaders (resources need to be transitioned through a scope).

```c
gotoIfError(clean, CommandListRef_dispatch2DRaysExt(commandList, rtId, width, height));
```

dispatch2DRaysExt, dispatch1DRaysExt and dispatch3DRaysExt are the easiest implementations. This command is also generalized with the dispatchRaysExt command which takes in a `DispatchRaysExt` struct. rtId is the raygen shader id, this is relevant since multiple raygen shaders can be linked into a single raygen pipeline. Each raygen shader has an id that can be passed here to execute it.

### Scope (startScope/endScope)

A scope is the replacement of the "transition" command. The scope makes sure all resources are in the right state for the commands that follow and it collects transitions from any other commands in the scope. It also makes sure to signal the api that the resources referenced are still in flight and shouldn't be deleted. When a scope is exited, it will undo the sets of temporary command states (though under the hood the api's command list is allowed to maintain these states to reduce api overhead). Scopes allow the implementation to figure out more clearly what areas of the command list are important together, and as such it can use them to determine dependencies between them and/or optimize unnecessary calls. It also allows the recorder to optimally record them in separate threads (if supported) since each scope doesn't maintain a global state. There can only be one scope active at a time: nesting scopes is unsupported.

```C
startScope		//Transitions resources

    addMarkerDebugExt
    startRegionDebugExt (starts deb region, push)
        endRegionDebugExt (end deb region, pop: req for each startRegionDebugExt)

	clearImages								//Keeps scope alive
    copyImageRegions						//Keeps scope alive
	setGraphicsPipeline
    setComputePipeline
    	dispatch(Indirect)					//Keeps scope alive

	//Update RTASes not allowed within startRenderExt
	updateBLASExt							//Keeps scope alive
	updateTLASExt							//Keeps scope alive

    setRaytracingPipelineExt
    	dispatchRaysExt						//Keeps scope alive

    startRenderExt
        setPrimitiveBuffers
        setViewport/Scissor
        setBlendConstants
        setStencil
        draw(Indirect(Count))				//Keeps scope alive
            requires:
                setGraphicsPipeline
                setViewport/Scissor
                probably setPrimitiveBuffers
                optional setBlendConstants & setStencil
        endRenderExt (required for each startRender)

    endScope
```

Because a scope hoists the transitions of operations such as clearImages, copyImages, drawIndirect(Count), setPrimitiveBuffers it is impossible to use the same (sub)resource in the same scope for different usages (be it copy/shader write/read). If this is the case then a separate scope is needed.

All startRenderExts in a scope should be ended and all startRegionDebugExts as well. Since a scope should be self contained.

#### Transitions

Because the API requires bindless to function, it has certain limits. One of these limits/benefits is that a shader is now able to access all write buffers/textures and read buffers/textures. This would mean that everything is accessible by all shaders; making automatic transitions impossible. To fix this; the user will only have to manually do transitions for draw/dispatch(rays) calls. For other usages the current scope will handle the transition for you (though this disallows usages of the same subresource for different purposes in one scope). Here you specify the (sub)resource and in which shader stage it is first used and if it's a write (or if any subsequent shaders could write to it). Then the runtime will automatically transition only when it's needed.

Even though Metal doesn't need transitions, they're still required to allow DirectX and Vulkan support. More importantly; transitions allow OxC3 to know which resources are required for the command list to be executed. It uses this to keep the resources alive until the command list was executed on the device.

For some types such as a Sampler, these transitions are required, even though the underlying API might not support the transitions for these types. In this case it signifies that the scope needs this resource to be active, to ensure it doesn't get deleted while in flight. It also ensures that the graphics API implementation doesn't evict it in an attempt to save memory (irrelevant to samplers though).

For raytracing shaders, EPipelineStage_RtStart can safely be used, as all RT shader stages are seen as a single stage. TLAS also needs to be transitioned to keep them active while rendering (BLAS also too if TLAS build is done from the GPU, since then the TLAS transition doesn't know what resource to transition).

*It is important to limit how many transitions are called, since this requires extra data and processing. Batching them and making sure you need them as less as possible is good practice.*

```c
//Example to transition swapchain for use in a compute shader as the output.

Transition transitions[] = {
	(Transition) {
		.resource = swapchain,				//See "Swapchain" or "RenderTexture"
		.range = (ImageRange) { 0 },
		.stage = EPipelineStage_Compute,
		.isWrite = true
	}
};

ListTransition transitionArr = (ListTransition) { 0 };
gotoIfError(clean, ListTransition_createRefConst(&transitionArr, 1, transitions));

gotoIfError(clean, CommandListRef_startScope(commandList, transitionArr, 0 /* id */, (ListCommandScopeDependency) { 0 } /* deps */));
//TODO: Bind compute shader(s) and dispatch
gotoIfError(clean, CommandListRef_endScope(commandList));
```

Transitions can currently only be called on a Swapchain, DeviceTexture, RenderTexture, DepthStencil, Sampler or DeviceBuffer object.

It's recommended to use enums to define the ids of scopes to avoid mistakes with hardcoding numbers for each scope.

##### Validation

Unfortunately, validation is only possible in DirectX and Vulkan using their respective validation layers. It is very hard to tell which resources were accessed in a certain frame (without running our own GPU-based validation layer). This makes it impossible to know if it was in the correct state. Make sure to validate on Vulkan since it's the strictest with transitions. However, if you're using Metal and doing transitions incorrectly, it could show up as some resources being deleted too early (if they're still in flight). Vulkan and DirectX's debug layers are automatically turned on on Debug mode.

#### Dependencies

The final parameter of startScope is the dependencies; this is a ListCommandScopeDependency which references the scopes by id it has a dependency on. It contains the dependency type (unconditional, conditional) and a scope id of the dependency. A conditional dependency means that the dependency needs to be available, otherwise the scope is invalid and it should be an error to insert it if the dependency isn't available. An unconditional dependency is one where the execution will happen always even if the scope is not present; but it does mean that the scope needs to be executed before it. Consider the following:

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

Scope 1 is dependent on scope 0 since it uses the V-Buffer generated by it. The type of dependency in this case is conditional, since if scope 0 doesn't issue any draw calls it won't have to execute the dispatch to unpack the G-Buffer.

Working in this way allows the implementation to thread scopes that are independent. Which results in better performance in cases where lots of GPU commands have to be issued.