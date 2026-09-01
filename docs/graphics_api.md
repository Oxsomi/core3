# Graphics library (OxC3 id: 0x1C33)

The core pillars of the abstraction of this graphics library are the following:

- Support feature sets as close as possible to Vulkan, Direct3D12 and Metal3.
  - Limits from legacy graphics such as DirectX11 (and below), OpenGL, below Metal3, below Vulkan 1.1 and others like WebGL won't be considered for this spec. They'd add additional complexity for no gain.
- Simplify usage for these APIs, as they're too verbose.
  - But don't oversimplify them to the point of being useless.
  - This does mean features not deemed important enough might not be included in the main specification. Though a branch could maintain support if needed.
- Force modern minimum specs to avoid having to build too many diverging renderers to deal with limitations of old devices.
- Allow modern usage of these APIs such as raytracing and bindless.
- Support various systems such as Android, Apple, Windows and Linux (console should be kept in mind, though not officially supported).

## Ref counting

OxC3 graphics works in a similar way as DirectX's ref counting system; everything is ref counted. This allows you to just call `RefPtr_dec` on the RefPtr to ensure it's properly released. If the command list is still in flight, it will maintain this resource until it's out of flight. It also allows you to safely share resources between user libraries without worrying about resource deletion. When sharing a resource, you simply increment the refptr by using `RefPtr_inc` and when the library is done using it can decrement it again. This concept was added in OxC3 types, but widely used in the graphics library.

`RefPtr_inc` and `RefPtr_dec` are the only way to manage graphics object lifetimes; the underlying `X_free` functions are internal (they're only used as the ObjectFreeFunc of the object's RefPtrType) and aren't exposed in the public headers.

### Error handling

All fallible graphics functions follow the OxC3 `Bool` convention: they return `Bool s_uccess` and take a trailing `Error *e_rr` which is filled on failure (pass `NULL` to ignore the details). `gotoIfError3(clean, ...)` can be used to jump to cleanup on failure. Allocations are done with the `const Allocator*` the instance was created with; every object created from a device uses `GraphicsDevice_getAlloc`/`GraphicsDeviceRef_getAlloc` internally, so only instance creation takes an allocator explicitly.

### Obtaining the real object

RefPtr doesn't contain the object itself, but it does contain information about the type and length of the constructed type. RefPtr contains this object after the data of the RefPtr itself. RefPtr_data can be used to obtain the data pointed to by the RefPtr. This is generally also a macro from the specialized RefPtr (e.g. `GraphicsInstanceRef_ptr`). It also generally contains the extended object after the common object. For example GraphicsInstance has `GraphicsInstance_ext` which can get the API-specific data of the GraphicsInstance. This should only be used internally, as there is no public interface of this extended data that won't randomly change.

In the case of multiple inheritance, the data pointed to can be linked however the creator of the object sees fit. For example: The first data could be a UnifiedTexture, then a UnifiedTextureImage * 3, then a UnifiedTextureImageExt * 3 and then a SwapchainExt.

## Graphics instance

### Summary

The graphics instance is the way you can query physical devices from D3D12, Vulkan or Metal3. First one has to be created as follows:

```c
GraphicsInstanceRef *instance = NULL;
const Allocator *alloc = Platform_instance->alloc;        //Or any allocator that outlives the instance

//Prepare the interface (queries which APIs are supported; guards against multiple inits)

gotoIfError3(clean, GraphicsInterface_create(e_rr));

//The RefPtrType must outlive the instance (and thus everything created through it)

RefPtrType instanceType = GraphicsInstance_makeType(EGraphicsApi_Count /* default api */, alloc);

const GraphicsApplicationInfo appInfo = (GraphicsApplicationInfo) {
	.name = CharString_createRefCStrConst("Rt core test"),
	.version = OXC3_MAKE_VERSION(0, 2, 0)
};

gotoIfError3(clean, GraphicsInstance_create(
	&appInfo,
	EGraphicsApi_Count,                //Default api for this platform (or e.g. EGraphicsApi_Vulkan)
	EGraphicsInstanceFlags_None,
	alloc,
	&instanceType,
	&instance,
	e_rr
));
```

Once this instance is acquired, it can be used to query devices and to detect what API the runtime supports.

*Note: GraphicsInterface_create must be called before GraphicsInstance_makeType/GraphicsInstance_create; it initializes the interface (and with dynamic linking, discovers the graphics API dylibs in the app directory). GraphicsInterface_supportsApi and EGraphicsApi_resolve can be used to query what's available. For dynamic libraries it is important that GraphicsInterface_instance and Platform_instance are set to the final exe/dll's instance (GraphicsInterface_getTable handles this) to ensure the same exact settings and memory allocation is handled by the same instance.*

### Properties

- application: The name and version of the application.
- api: Which api is ran by the runtime: Vulkan, Direct3D12, Metal3 or WebGPU.
- apiVersion: What version of the graphics api is being ran (e.g. Vulkan 1.2, DirectX SDK version, Metal 3, etc.).

### (Member) Functions

- ```c
  Bool getDeviceInfos(ListGraphicsDeviceInfo *infos, Error *e_rr);
  ```

  - Queries all physical devices to detect if they're supported and what features they have.

- ```c
  Bool getPreferredDevice(
  	const GraphicsDeviceCapabilities *requiredCapabilities,
  	U64 vendorMask,
  	U64 deviceTypeMask,
  	GraphicsDeviceInfo *deviceInfo,
  	Error *e_rr
  );
  ```

  - Select the physical device that has all specified extensions and is a supported vendor and/or device type.

- ```c
  U64 getValidationErrors();
  U64 getValidationWarnings();
  ```

  - How many validation errors and warnings the api's debug layers reported so far across every device of this instance (0 when validation is off). Performance and info messages aren't counted. CI uses these to hard fail whenever a run isn't validation clean; deliberate exceptions are suppressed explicitly at the source (the D3D12 info queue deny list, GPU assisted validation's settings adjustment notice on Vulkan) rather than subtracted here.

### Used functions and obtained

- Obtained through `Bool GraphicsInstance_create(const GraphicsApplicationInfo *info, EGraphicsApi api, EGraphicsInstanceFlags flags, const Allocator *alloc, const RefPtrType *type, GraphicsInstanceRef **inst, Error *e_rr);` see summary.
- Mostly used as `GraphicsInstanceRef` in `GraphicsDeviceRef_create` as well as GraphicsInstance's member functions.

## Graphics device info

### Summary

Graphics device info contains information about the underlying physical device. This is something that gets queried to figure out what hardware (or software) will be used to render and if they support the feature set that the application requires.

GraphicsInstance's getPreferredDevice can be used to query if there's any graphics device that supports the required feature set. For more complex setups such as multi GPU rendering, it is recommended to manually use `getDeviceInfos` to determine which GPUs (if any) support your use case(s). The devices that are needed can then be made into a GraphicsDevice manually.

```c
GraphicsDeviceInfo deviceInfo = (GraphicsDeviceInfo) { 0 };

gotoIfError3(clean, GraphicsInstance_getPreferredDevice(
  GraphicsInstanceRef_ptr(instance),		//See "Graphics instance"
  (GraphicsDeviceCapabilities) { 0 },		//No required features or data types
  GraphicsInstance_vendorMaskAll,			//All vendors supported
  GraphicsInstance_deviceTypeAll,			//All device types supported
  &deviceInfo,
  e_rr
));
```

### Properties

- name; null-terminated UTF-8 string giving information about the device.
- driverInfo; might not be available (always available on D3D12, but sometimes available on Vk) or might give a UTF-8 string in arbitrary format that specifies info about the driver version. Generally this follows (x.y or x.y.z) but that is vendor specific (at least NV, AMD and INTC seem to agree on it on desktop).
- type; what type this device is (dedicated GPU, integrated GPU, simulated GPU, CPU or other (unrecognized)).
- vendor; what company designed the device (Nvidia (NV), AMD, ARM, Qualcomm (QCOM), Intel (INTC), Imagination Technologies (IMGT), Microsoft (MSFT) or unknown).
- id; number in the list of supported devices.
- luid; ID to identify this device primarily on Windows devices. This would allow sharing a resource between other APIs for interop (not supported yet). This is optional to support; check capabilities.features & LUID.
- uuid; unique id to identify the device. In APIs that don't support this natively, the other identifier (luid) will be used here instead. For example Direct3D12 would use the luid here and clear the other U64.
- ext; extended physical device representation for the current API. Can be NULL if LUID is used to share this.
- capabilities; what data types, features and api dependent features are enabled. See capabilities section. This also includes dedicated and shared memory.

#### Capabilities

- features: DirectRendering, VariableRateShading, MultiDrawIndirectCount, MeshShader, GeometryShader, SubgroupArithmetic, SubgroupShuffle, Multiview, Raytracing, RayPipeline, RayQuery, RayMicromapOpacity, RayReorder, RayTriPosition, RayValidation, LUID, DebugMarkers, Wireframe, LogicOp, DualSrcBlend, SwapchainCompute, CoopVec, CoopMat, CoopFP8, CoopVecTraining.

- experimentalFeatures: the subset of `features` that is experimental/preview on this device+build (not final; may change or be removed across SDK/driver updates). On D3D12 the SM6.10-gated cooperative features land here (enabled best-effort via the preview Agility SDK + D3D12ExperimentalShaderModels + Developer Mode); on Vulkan they're real extensions so this stays empty. Check it if you want to opt into preview features knowingly.
  - RayValidation: extra raytracing validation for NV cards; requires envar NV_ALLOW_RAYTRACING_VALIDATION=1 and reboot.
- features2: RayReorderActual, DescriptorHeap, RayClusterAS, RayPartitionedTLAS, RayIndirectASBuild, RayMicromapOpacityActual, RayMicromapOpacityU8.
  - RayReorderActual: SER (RayReorder) means the shader-execution-reordering API is available (always valid to call, but may be a no-op); RayReorderActual means the device actually performs the reordering, so it's worth restructuring shaders around it.
  - RayMicromapOpacityActual: same shape one feature over. RayMicromapOpacity means the API accepts opacity micromaps; this bit means they're likely backed by dedicated hardware rather than emulated, so a real micromap object is worth building. Neither API reports it (D3D12 ships OMM wholesale with RAYTRACING_TIER_1_2, Vulkan's VkPhysicalDeviceOpacityMicromapFeaturesEXT is one bool, and an Ampere 3080 reports the same 12/12 subdivision maximum as hardware that has the units), so OxC3 derives it: NVIDIA needs RayReorderActual since the reordering and OMM hardware shipped in the same generation, every other vendor is taken at its word. It is a heuristic, so treat it as "worth it" rather than as a guarantee; special-index-only OMM costs nothing either way.
  - RayMicromapOpacityU8: 8-bit (R8u) OMM index buffers are legal. D3D12 ships this with opacity micromaps themselves; on Vulkan only the VK_KHR_opacity_micromap promotion permits VK_INDEX_TYPE_UINT8 (the EXT extension forbids it), and since OxC3's KHR path isn't implemented yet no Vulkan device claims the bit today; R8u is rejected at BLAS create there.
  - DescriptorHeap: full bindless; shaders index the descriptor heap directly without a fixed descriptor layout. D3D12: SM6.6 dynamic resources (ResourceDescriptorHeap/SamplerDescriptorHeap) + resource binding tier 3; Vulkan: VK_EXT_descriptor_heap. Always implies Bindless on both APIs.
  - RayClusterAS + RayPartitionedTLAS: mega geometry (RTXMG), split the way Vulkan splits it: cluster acceleration structures (CLAS/cluster BLAS) and partitioned TLAS. Vulkan: VK_NV_cluster_acceleration_structure / VK_NV_partitioned_acceleration_structure; D3D12: NVAPI raytracing caps (cluster operations / partitioned TLAS).
  - RayIndirectASBuild: GPU-driven acceleration structure builds. Vulkan: accelerationStructureIndirectBuild (vkCmdBuildAccelerationStructuresIndirectKHR for classic AS); D3D12: implied by either mega geometry bit, since those builds are indirect by design.
- dataTypes: F64, I64, F16, I16, AtomicI64, AtomicF32, AtomicF64, ASTC, BCn, MSAA2x, MSAA8x, RGB32f, RGB32i, RGB32u, D24S8, S8.
  - MSAA4 and MSAA1 (off) are supported by default.
- featuresExt: API dependent features that aren't expected to be standardized in the same way.
  - Vulkan: PerformanceQuery, PerformantPushDescriptor.
    - PerformantPushDescriptor: VK_KHR_push_descriptor with at least 32 push descriptors, so the globals constant buffer is pushed straight into the command buffer instead of bound from a set allocated to hold it. Named for the performance rather than the capability, because pushing descriptors always works; the flag only says the device does it natively. It lives here rather than in `features2` because Vulkan is the only API where that isn't a given; D3D12 has root descriptors and Metal has argument buffers. Without it OxC3 allocates one descriptor set per frame in flight, writes each once to that frame's globals buffer and binds it unchanged afterwards, so there's no per-frame update and nothing is rewritten while in flight. A one-time performance warning is logged on the first submit that takes the emulated path. Android emulators are the usual case, since gfxstream drops the extension from the guest even where the host driver exposes it.
  - Direct3D12: WriteBufferImmediate (for crash debugging), ReBAR (for checking if quick access path to GPU is available), HardwareCopyQueue (If the copy queue makes sense to use), BatchedAsyncCommandList (batched async command list submission, SM6.10 / Agility 1.720+), CacheCoherentUMA (UMA that snoops CPU caches; upload heaps then use WRITE_BACK instead of WRITE_COMBINE).
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
gotoIfError3(clean, GraphicsDeviceRef_create(
    instance, 						//See "Graphics instance"
    &deviceInfo, 					//See "Graphics device info"
    EGraphicsDeviceFlags_None,		//IsVerbose, IsDebug, DisableRt, DisableDebug, DisableBindless
    EGraphicsBufferingMode_Default,	//Frames in flight: Default, Double or Triple
    NULL,							//Bindless DescriptorLayoutInfo; NULL is OxC3's default layout
    &device,
    e_rr
));
```

The bindless layout is what the default descriptor table and pipeline layout are built from. Passing NULL uses OxC3's own layout, which is what the prebuilt shaders and the oiSH files OxC3 ships are compiled against. A caller that wants a different one can obtain the default through `GraphicsDevice_defaultBindlessLayout`, modify it and pass it in (it is freed with `DescriptorLayoutInfo_free`, the device copies whatever it gets). `EGraphicsDeviceFlags_DisableBindless` drops bindless entirely, even on a device that supports it; the feature bit is then cleared from the device's capabilities, so there is no default descriptor table or pipeline layout and every pipeline has to supply its own layout.

Whichever layout the device ends up with is the one every shader is held to. When a pipeline is created (or a binary is picked through `GraphicsDeviceRef_getFirstShaderEntry`), `GraphicsDeviceRef_checkShaderFeatures` walks the binary's reflected registers and refuses any bindless array that the device's layout doesn't have at the same space and binding, with an incompatible register type, or with fewer descriptors than the shader declares. A shader that wants bindless arrays on a device without a bindless layout is refused too. The offending register and what the layout has instead are logged, so an oiSH built against an older layout can be run again by recreating that layout and passing it to `GraphicsDeviceRef_create`. Only binaries the oiSH marks as needing bindless are checked; push constants, push descriptors and singular bound resources come from the pipeline layout the caller supplies and are left alone.

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
  Bool submitCommands(
  	const ListCommandListRef *commandLists,
  	const ListSwapchainRef *swapchains,
  	F32 deltaTime,		//< 0 = auto calculate time and deltaTime
  	F32 time,
  	Error *e_rr
  );
  ```

  - Submits commands to the device and readies the swapchains to present if available. If the device doesn't have any swapchains, it can be used to just submit commands. This is useful for multi GPU rendering as well. If deltaTime is set to -1 it will automatically calculate deltaTime itself, but some applications might want to bypass this by manually passing deltaTime and time (for example when rendering a movie or a photo). The time argument is also ignored if deltaTime is -1 and will be calculated automatically.

- There's a limit of 16 swapchains per device.

  - Runtime data is accessible from a CBuffer to all shaders and can be used for simple data such as resource handles. This buffer has a limit of 368 bytes.

- ```c
  Bool wait(Error *e_rr);
  ```

  - Waits for all currently queued commands on the device.

- ```c
  Bool GraphicsDevice_logOnce(GraphicsDevice *device, EGraphicsDeviceMessage message);
  ```

  - One time runtime hints: returns true exactly once per device per message (an atomic test and set on GraphicsDevice::runtimeMessages), so the caller logs on true and stays silent forever after, preventing a per call hint from spamming. Current messages: OmmLikelyEmulated (a BLAS links a real opacity micromap on a device without RayMicromapOpacityActual, where the free special indices usually serve better), SubmitFlushed (a submit was split mid recording because pending copies or AS builds crossed flushThreshold, adding a GPU sync point), RootSignature13Dwords (a pipeline layout exceeds the 13 root signature DWORDs D3D12 drivers keep in fast memory) and TooManyMemoryBlocks (a dedicated allocation fell back to shared because the device already holds >= 2000 blocks).

- ```c
  Bool createSwapchain(
  	SwapchainInfo info,
  	Bool allowComputeExt,
  	DescriptorTableRef *bindlessDescriptorTable,	//NULL = device's default bindless table
  	SwapchainRef **swapchain,
  	Error *e_rr
  );
  ```

  - Be aware that allowWrite doesn't work for all APIs and all devices. Compute to the swapchain is optional.

- ```c
  Bool createCommandList(
  	U64 commandListLen,
  	U64 estimatedCommandCount,
  	U64 estimatedResources,
  	Bool allowResize,
  	CommandListRef **commandList,
  	Error *e_rr
  );
  ```

- ```c
  Bool createPipelineCompute(
  	const SHFile *shaderBinary,
  	const CharString *name,			//Temporary name for debugging
  	U32 entryId,					//Identifier from getFirstShaderEntry
  	const CharString *entryName,	//Optional: SPIRV entrypoint to use
  	EPipelineFlags flags,
  	PipelineLayoutRef *layout,		//NULL = default bindless pipeline layout
  	PipelineRef **pipeline,
  	Error *e_rr
  );
  ```

- ```c
  Bool createPipelineGraphics(
  	const ListSHFile *shaderBinary,
  	ListPipelineStage *stages,		//Will be moved
  	const PipelineGraphicsInfo *info,
  	const CharString *name,			//Temporary name for debugging
  	EPipelineFlags flags,
  	PipelineLayoutRef *layout,		//NULL = default bindless pipeline layout
  	PipelineRef **pipelines,
  	Error *e_rr
  );
  ```

- ```C
  Bool createBuffer(
  	EDeviceBufferUsage usage,
  	EGraphicsResourceFlag resourceFlags,
  	DescriptorTableRef *bindlessDescriptorTable,	//NULL = device's default bindless table
  	const CharString *name,
  	U64 len,
  	DeviceBufferRef **buf,
  	Error *e_rr
  );
  ```

- ```c
    Bool createBufferData(
    	EDeviceBufferUsage usage,
    	EGraphicsResourceFlag resourceFlags,
    	DescriptorTableRef *bindlessDescriptorTable,	//NULL = device's default bindless table
    	const CharString *name,
    	Buffer *dat,			//Can move data to device buffer (doesn't always)
    	DeviceBufferRef **buf,
    	Error *e_rr
    );
    ```

- ```c
  Bool createSampler(
  	SamplerInfo info,
  	Bool disallowBindlessDescriptor,				//Won't allocate into a bindless table
  	DescriptorTableRef *bindlessDescriptorTable,	//NULL = device's default bindless table
  	const CharString *name,
  	SamplerRef **sampler,
  	Error *e_rr
  );
  ```

- ```c
  Bool createRenderTexture(
  	ETextureType type,
  	U16 width,
  	U16 height,
  	U16 length,
  	ETextureFormatId format,
  	EGraphicsResourceFlag flag,
  	EMSAASamples msaa,
  	DescriptorTableRef *bindlessDescriptorTable,	//NULL = device's default bindless table
  	const CharString *name,
  	RenderTextureRef **renderTexture,
  	Error *e_rr
  );
  ```

- ```c
  Bool createDepthStencil(
  	U16 width,
  	U16 height,
  	EDepthStencilFormat format,
  	Bool allowShaderRead,
  	EMSAASamples msaa,
  	DescriptorTableRef *bindlessDescriptorTable,	//NULL = device's default bindless table
  	const CharString *name,
  	DepthStencilRef **depthStencil,
  	Error *e_rr
  );
  ```

- ```c
  //Triangle geometry parameters travel as a struct so optional features become fields, not entry points.
  //Built through BLASCreateInfo_indexed/_unindexed rather than by hand: required parameters stay
  // positional there, so forgetting one is still a compile error.
  typedef struct BLASCreateInfo {
  	ERTASBuildFlags buildFlags;
  	EBLASFlag blasFlags;
  	ETextureFormatId positionFormat;	//RGBA16f, RGBA32f, RGBA16s, RG16f, RG32f, RG16s
  	ETextureFormatId indexFormat;		//R16u, R32u, Undefined for unindexed
  	U16 positionOffset;					//Offset into first position for first vertex
  	U16 positionBufferStride;			//<=2048 and multiple of 2 (if not 32f) or 4 (RGBA32f)
  	U32 padding;
  	DeviceData positionBuffer;			//Required
  	DeviceData indexBuffer;				//Only if indexFormat
  	ETextureFormatId ommIndexFormat;	//R16u, R32u, Undefined for no OMM
  	U32 padding1;
  	DeviceData ommIndexBuffer;			//Only if ommIndexFormat
  } BLASCreateInfo;

  BLASCreateInfo BLASCreateInfo_indexed(
  	ERTASBuildFlags buildFlags,
  	EBLASFlag blasFlags,
  	ETextureFormatId positionFormat,
  	U16 positionOffset,
  	U16 positionBufferStride,
  	DeviceData positionBuffer,
  	ETextureFormatId indexFormat,
  	DeviceData indexBuffer
  );

  BLASCreateInfo BLASCreateInfo_unindexed(
  	ERTASBuildFlags buildFlags,
  	EBLASFlag blasFlags,
  	ETextureFormatId positionFormat,
  	U16 positionOffset,
  	U16 positionBufferStride,
  	DeviceData positionBuffer
  );

  Bool createBLASExt(
  	const BLASCreateInfo *info,
  	const CharString *name,
  	BLASRef **blas,
  	Error *e_rr
  );
  ```

- ```c
  Bool createBLASProceduralExt(
  	ERTASBuildFlags buildFlags,
  	EBLASFlag blasFlags,
  	U32 aabbStride,						//Alignment: 8
  	U32 aabbOffset,						//Offset into the aabb array
  	DeviceData buffer,					//Required
  	const CharString *name,
  	BLASRef **blas,
  	Error *e_rr
  );
  ```

- ```c
  Bool createTLASExt(
  	ERTASBuildFlags buildFlags,
  	const ListTLASInstance *instances,
  	Bool disallowBindlessDescriptor,				//Won't allocate into a bindless table
  	DescriptorTableRef *bindlessDescriptorTable,	//NULL = device's default bindless table
  	const CharString *name,
  	TLASRef **tlas,
  	Error *e_rr
  );
  ```

- ```c
  //Refits a CPU built TLAS in place; see Refitting
  Bool TLASRef_setInstancesExt(TLASRef *tlas, const ListTLASInstance *instances, Error *e_rr);
  ```

- ```c
  Bool createTLASDeviceExt(
  	ERTASBuildFlags buildFlags,
  	const DeviceData *instancesDevice,	//Instances on the GPU, should be sized correctly
  	Bool disallowBindlessDescriptor,				//Won't allocate into a bindless table
  	DescriptorTableRef *bindlessDescriptorTable,	//NULL = device's default bindless table
  	const CharString *name,
  	TLASRef **tlas,
  	Error *e_rr
  );
  ```

### Obtained

- Through GraphicsDeviceRef_create; see overview.

## Command list

### Summary

A command list in OxC3 is a virtual command list. The commands get recorded in a standardized format, but they don't get translated until they're submitted to the graphics device. The graphics device is then responsible for synchronization and ensuring the commands are recorded in the optimal way. Command lists themselves don't have any API specific implementation to avoid unexpected caching behavior. When submitting multiple command lists, they'll act as if they're one long command list. This would allow maintaining state from the old command list as well.

```c
CommandListRef *commandList = NULL;
gotoIfError3(clean, GraphicsDeviceRef_createCommandList(
    device, 	//See "Graphics device"
    2 * KIBI, 	//Max command buffer size
    64, 		//Estimated command count (can auto resize)
    64, 		//Estimated resource count (can auto resize)
    true,		//Allow resize of command buffer data (the 2 KIBI in this example)
    &commandList,
    e_rr
));
```

#### Recording a command list

```c
gotoIfError3(clean, CommandListRef_begin(commandList, true /* clear previous */, U64_MAX /* long timeout */, e_rr));

gotoIfError3(clean, CommandListRef_startScope(
    commandList, (ListTransition) { 0 }, 0, (ListCommandScopeDependency) { 0 }, e_rr
));
gotoIfError3(clean, CommandListRef_clearImagef(
    commandList, F32x4_create4(1, 0, 0, 1), (ImageRange){ 0 }, swapchain, e_rr
));
gotoIfError3(clean, CommandListRef_endScope(commandList, e_rr));

gotoIfError3(clean, CommandListRef_end(commandList, e_rr));
```

Every frame, this can be passed onto the submit commands call:

```c
ListCommandListRef commandLists = (ListCommandListRef) { 0 };
ListSwapchainRef swapchains = (ListSwapchainRef) { 0 };

gotoIfError3(clean, ListCommandListRef_createRefConst(&commandList, 1, &commandLists, e_rr));
gotoIfError3(clean, ListSwapchainRef_createRefConst(&swapchain, 1, &swapchains, e_rr));

gotoIfError3(clean, GraphicsDeviceRef_submitCommands(
    device, commandLists, swapchains, -1, 0, e_rr
));
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

By default, a GraphicsResource's data is uninitialized (of questionable state) and has to be cleared or written right away.

A graphics resource consists of the following:

- device: the owning graphics device.
- size: CPU visible buffer size.
- blockOffset/blockId/allocated: API dependent allocation information used to track the allocation.
- flags: ShaderRead, ShaderWrite, InternalWeakDeviceRef, CPUBacked, CPUAllocated(Bit), CPUReadBit.
  - ShaderRead: Whether the resource has a valid resource handle and can be accessed on the GPU through the descriptors. MSAA resources are incompatible with this flag, because there's no Texture2DMS[], it needs to be resolved before reading/writing the resource.
  - ShaderWrite: ^ but for write access. DepthStencil and MSAA disallows this always.
  - InternalWeakDeviceRef: Internal only; tells the resource it belongs to the device, so the device is the only one in charge of cleaning it up.
  - CPUReadBit: Internal only; places the allocation in CPU readable memory (D3D12 readback heap) for the pullRegion staging buffer. It doesn't compose with user resources: readback heap buffers are limited to copy destination use, mappedMemoryExt isn't meant for direct access and nothing would report when the GPU's writes became visible. Use pullRegion, which handles all of that.
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

The staging memory that carries pulls back to the CPU is created at the first pull and only ever grows. On D3D12 that first allocation can bring in a whole new CPU sided memory block mid frame, so latency sensitive applications can call GraphicsDeviceRef_reserveReadback(device, sizePerFrame, e_rr) during load time to move that cost to a predictable place. Underestimating (or passing 0) is fine, the buffers simply grow at the next pull that needs more.

Texture pulls follow the same region semantics as markDirty: zero for an axis means the rest of that axis, and for block compressed formats the region snaps outward to whole blocks, so slightly more than asked can be refreshed. Compressed formats are supported; the pulled data lands in cpuData in the same tight block rows the upload reads from.

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
//onCreate window callback ('w' is the Window* the callback was invoked with)

SwapchainRef *swapchain = NULL;
gotoIfError3(clean, GraphicsDeviceRef_createSwapchain(
	device, 		//See "Graphics device"
	(SwapchainInfo) { .window = w },
	false,			//allowComputeExt
	NULL,			//bindlessDescriptorTable; NULL = device's default bindless table
	&swapchain,
	e_rr
));

//onResize: Window callback to make sure format + size stays the same:

if(!(w->flags & EWindowFlags_IsVirtual))
	SwapchainRef_resize(swapchain, e_rr);

//onDestroy: Window callback; the swapchain MUST be released here (or earlier), see below.

RefPtr_dec(&swapchain);
```

### Window lifetime (weak reference)

The swapchain only holds a **weak** reference to the window: it does **not** keep the window alive, and it has no way of knowing the window was destroyed other than through the window's own callbacks. This leads to a hard requirement:

- The swapchain (and every command list / pending present that targets it) **must be destroyed from within the window's onDestroy callback** (or earlier). After onDestroy returns, the window's memory is gone and the swapchain's `info.window` is a dangling pointer; any use of it after that point (resize, present, submit) is undefined behavior.
- The render loop should only touch the window (and thus the swapchain) from within the window's callbacks (onDraw/onResize/onDestroy); the window is guaranteed to be alive for the duration of those.
- Since the window doesn't own the swapchain either, dropping the last swapchain reference outside of the window's lifetime callbacks is legal, as long as no GPU work that presents to it is still pending (wait on the device first).

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

- info.window: the Window handle created using OxC3 platforms. This is a **weak** pointer: it's only valid while the window is alive (see Swapchain/Window lifetime); destroy the swapchain in the window's onDestroy callback.
- info.requiresManualComposite: whether or not the application is requested to explicitly handle rotation from the device. For desktop this is generally false, for Android this is on to avoid the extra overhead of the compositor.
- info.presentModePriorities: what present modes were requested on create.
- device: the owning device.
- base: UnifiedTexture that represents this texture.
- versionId: the version id changes everytime a change is applied. This could be resizes or format changes.
- presentMode: What present mode is currently used (see Swapchain/Present mode).
- lock: Multi-threading. Is used to maintain versionId. On submitCommands it has to lock the swapchain(s) to see if the versionId is still the same as the one the command list(s) was/were recorded with. A CommandList is deemed stale if the Swapchain has been resized to a different size. Not re-recording will result in submitCommands erroring. This is because lots of commands can use the swapchain size such as SetViewportAndScissor.

### Functions

- ```c
  Bool SwapchainRef_resize(SwapchainRef *swapchain, Error *e_rr);
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
gotoIfError3(clean, GraphicsDeviceRef_createSampler(
    device, nearestSampler, false /* disallowBindlessDescriptor */, NULL, samplerName, &nearest, e_rr
));
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
gotoIfError3(clean, GraphicsDeviceRef_createTexture(
    twm->device,
    ETextureType_2D,
    ETextureFormatId_BGRA8,
    EGraphicsResourceFlag_ShaderRead,
	128, 128, 1,
    NULL,						//bindlessDescriptorTable; NULL = device's default bindless table
    CharString_createRefCStrConst("Test image"),
    &imageData,					//Is able to move the Buffer if it's not a ref
    &twm->deviceTextureRef,
    e_rr
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

**Reverse Z:** the viewport is a plain 0..1 depth range on every backend; stored depth equals the NDC z the shader outputs. Reverse Z is therefore an application convention: fold the flip into the projection matrix (swap near/far, or z' = w - z in clip space), so nearer geometry stores the *higher* depth value, clear the "far" depth to 0 and test with ECompareOp_Gt (or Geq). Doing the flip in the projection (before the perspective divide) is also what preserves the reverse Z precision benefit, and it is applied exactly once regardless of which stage (vertex, domain or geometry) feeds the rasterizer. Inverted viewport ranges (minDepth > maxDepth) are deliberately not used: they're legal in Vulkan but violate the D3D functional spec (WARP collapses them to a constant 0).

```c
gotoIfError3(clean, GraphicsDeviceRef_createDepthStencil(
    twm->device,
    width, height, EDepthStencilFormat_D16, false /* allow shader access */,
    EMSAASamples_Off,
    NULL,							//bindlessDescriptorTable; NULL = device's default bindless table
    CharString_createRefCStrConst("Test depth stencil"),
    &tw->depthStencil,
    e_rr
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
gotoIfError3(clean, GraphicsDeviceRef_createRenderTexture(
	twm->device,
	ETextureType_2D, 				//2D is only supported currently
    width, height, 1, 				//x, y, z
    ETextureFormatId_BGRA8, 		//Non compressed texture format
    EGraphicsResourceFlag_ShaderRW, //Allow both shader writes and reads
    EMSAASamples_Off,				//No MSAA
    NULL,							//bindlessDescriptorTable; NULL = device's default bindless table
	CharString_createRefCStrConst("Virtual window backbuffer"),
	&tw->swapchain,
	e_rr
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

## DescriptorHeap

### Summary

A DescriptorHeap is the description of many resources are available max during the application. In Vulkan this would correspond to VkDescriptorPool while in D3D12 it'd be ID3D12DescriptorHeap. This can later be allocated into by a DescriptorSet, which will place descriptors at the location (in the case of D3D12) or will consume them from the descriptor pool (in the case of Vulkan).

### Properties

- device: ref to the graphics device that owns it.
- info.flags
  - If bindless is enabled which will enable use of bindless descriptors in descriptor sets.
  - If it's owned by a graphics device or not, useful for internal use only.
- info.max[T]: Defines the limits of each type of descriptor.
  - maxAccelerationStructures
  - maxSamplers
  - maxInputAttachments
  - maxCombinedSamplers
  - maxTextures
  - maxTexturesRW
  - maxBuffersRW
  - maxConstantBuffers
- descriptorTableCount: how many descriptor tables have been created through it, useful for tracking and to enforce the limit in info.maxDescriptorTables.

### Used functions and obtained

- Obtained through GraphicsDeviceRef's createDescriptorHeap.
- A DescriptorHeapInfo (to create the heap itself) has to be defined from the app or the device generally, this is to allow it to save memory or descriptors when it needs to.
- Used when creating a DescriptorTable and can be bound together with the DescriptorTable by the command list.

## DescriptorLayout

### Summary

A DescriptorLayout is the description of how the resources are bound to the pipeline. In Vulkan this would be VkDescriptorSetLayout[4] while in D3D12 it'd be D3D12_DESCRIPTOR_RANGE1[]. This would then later be able to be turned into a VkPipelineLayout or ID3D12RootSignature.

On Vulkan a binding's register space **is** its descriptor set index: SPIR-V reflection reports the set as the space, and every bind places a set at the index its space names. That has two consequences. Only spaces 0 to 3 can be bound at all (`maxBoundDescriptorSets` is required to be at least 4), and createDescriptorLayout refuses anything higher. And a pipeline layout's set list is indexed by space across BOTH of its DescriptorLayouts, the ordinary bindings and the push descriptors, with an empty set layout filling any index neither declares; so a layout whose push descriptor lives in space 0 next to table bindings in space 1 binds exactly as the shader declared it, and a layout using spaces 0 and 2 is legal with nothing at 1. The two layouts of one pipeline layout can't share a space, which createPipelineLayout enforces. D3D12 needs none of this: its root ranges carry the register space and bind by (register, space) natively. When doing full bindless, there's only one descriptor set layout that is declared when creating a device, this descriptor layout can be used by setting the PipelineLayout's DescriptorLayout to NULL.

### Properties

- device: ref to the graphics device that owns it.
- flags
  - If bindless is enabled only on arrays or on everything in the DescriptorLayout. Enabling bindless means that the descriptors themselves might not be valid and are dynamically changed. Doing this only on an array is recommended, since an array is generally the thing that contains bindless resources. Only enable bindless if it makes sense (e.g. raytracing, complicated render pipelines) to ensure full combability and performance on all target devices. 
  - If it's owned by a graphics device or not, useful for internal use only.
- bindings: a list of descriptor bindings that specifies the count, register type, id, visibility and space of a specific resource binding. The definition of this depends on the API; in Vulkan there are other binding types than DirectX12, so the implementation will try to merge ranges together if possible. As such, try to put UAVs, SRVs and CBVs closely together and a similar recommendation for Vulkan register types (images, textures, subpass inputs, ssbo, ubo, RTAS). The implementation can determine it can't merge a range if for example visibility mismatches (Vulkan) or the two have mismatching bindless flags.

### Reserved register space

OxC3 binds its own per frame globals (frame id, time, delta time and swapchain descriptors) to a register space it keeps for itself: `OXC3_RESERVED_SPACE`, which is `0xC3` (195). createDescriptorLayout refuses any caller binding that lands there.

This concerns DXIL only. On Vulkan the globals live in their own descriptor set, and set indices are far too few to hide a reservation in. On DXIL they used to sit at `b0 space0`, which is exactly what someone writing their first constant buffer types. That was harmless while the default bindless layout was the only layout there was, and became a silent collision the moment custom layouts let anyone declare their own `b0`.

A shader that genuinely declares this space was built against a different OxC3 than the one running it: either a newer one, or one modified to lay its globals out elsewhere. Neither can be honoured, so the layout is refused rather than allowed to quietly shadow the runtime's binding or be overwritten by it. The device's own layouts are exempt, since they are what occupies the space in the first place.

Shaders spell it as `OXC3_RESERVED_SPACE` (from types.hlsli), which expands to `space195`. The C and HLSL definitions have to stay in sync, in `graphics/generic/descriptor_layout.h` and `shader_compiler/shaders/types.hlsli` respectively; a shader compiled against a different value binds somewhere the runtime doesn't look.

### Used functions and obtained

- Obtained through GraphicsDeviceRef's createDescriptorLayout.
- A DescriptorLayoutInfo (to create the layout itself) can generally be obtained through the shader reflection by using `GraphicsDeviceRef_detectLayoutFromEntry` (one entrypoint) or `GraphicsDeviceRef_detectLayoutFromEntries` (several sharing one layout). The identifier they take is the packed one `GraphicsDeviceRef_getFirstShaderEntry` returns and `PipelineStage::binaryId` carries: the entrypoint in the low 16 bits, and in the high 16 bits an index into THAT entrypoint's binary list rather than into the file's binaries, so reflection reads the same variant the pipeline will be built from, though this might not be optimal if all shaders have a similar / the same DescriptorLayout (unless you manually avoid creating duplicates). Naming a register there also splits it out as push constants or push descriptors rather than an ordinary binding.
- Used when creating a PipelineLayout.

## Pipeline

### Summary

A pipeline is a combination of the states and shader binaries that are required to run the shader. This represents a VkPipeline in Vulkan, an ID3D12PipelineState or ID3D12StateObject in Direct3D12 and a `MTL<Render/Compute>PipelineState` in Metal.

### Properties

- device: ref to the graphics device that owns it.
- type: compute, graphics or raytracing pipeline type.
- flags: if it's owned by a graphics device or not, useful for internal use only.
- pipelineLayout: what kind of pipeline layout is used, can be NULL to indicate default bindless layout.
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

- Obtained through GraphicsDeviceRef's createPipelineGraphics, createPipelineCompute and createPipelineRaytracingExt.
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
SHFile tempShader = ...;		//SHFile: Load from virtual file or hardcode.
CharString name = CharString_createRefCStrConst("Test compute pipeline");
CharString entrypoint = CharString_createRefCStrConst("main");

U32 entryId = GraphicsDeviceRef_getFirstShaderEntry(
	device,
	&tempShader,
	&entrypoint,
	NULL,						//Defines ([ key, value ][], NULL = none)
	ESHExtension_None,			//Extensions to disallow
	ESHExtension_None			//Extensions to require
);

gotoIfError3(clean, GraphicsDeviceRef_createPipelineCompute(
	device, &tempShader, &name, entryId, EPipelineFlags_None, NULL, &computeShaders, e_rr
));

SHFile_free(&tempShader, alloc);
```

Create pipelines will take ownership of the buffers referenced in computeBinaries and it will therefore free the list (if unmanaged). If the buffers are managed memory (e.g. created with Buffer_create functions that use the allocator) then the Pipeline object will safely delete it. This is why the tempShader is set to null after (the list is a ref, so doesn't need to be). In clean, this temp buffer gets deleted, just in case the createPipelines fails. Using virtual files for this is recommend, as they'll already be present in memory and our ref will be available for the lifetime of our app. If it's a ref that doesn't always stay active, be sure to manually copy the buffers to avoid referencing deleted memory.

It is recommended to generate all pipelines that are needed in this one call at startup, to avoid stuttering at runtime.

### Graphics example

```c
ListSHFile tempShaders = ...;		//SHFile: Load from virtual file or hardcode.

//Define the entrypoints

U32 mainVSDepth = GraphicsDeviceRef_getFirstShaderEntry(...);		//Check the compute example for more
U32 mainPS = GraphicsDeviceRef_getFirstShaderEntry(...);

PipelineStage stage[2] = {
    (PipelineStage) { .binaryId = mainVSDepth, .shFileId = 1 },		//tempShaders[1] contains mainVSDepth entrypoint
    (PipelineStage) { .binaryId = mainPS, .shFileId = 0 }			//tempShaders[0] contains mainPS entrypoint
};

ListPipelineStage stageInfos = (ListPipelineStage) { 0 };
gotoIfError3(clean, ListPipelineStage_createRefConst(stage, sizeof(stage) / sizeof(stage[0]), &stageInfos, e_rr));

//Define all pipelines.
//These pipelines require the graphics feature DirectRendering and will error otherwise!
//Otherwise .attachmentCountExt, .attachmentFormatsExt and/or .depthStencilFormat
//  need to be replaced with .renderPass = renderPass and/or .subPass = subPassId.

PipelineGraphicsInfo info = (PipelineGraphicsInfo) {
	.stageCount = 2,
	.attachmentCountExt = 1,
	.attachmentFormatsExt = { ETextureFormat_BGRA8 }
};

//Create pipelines

CharString name = CharString_createRefCStrConst("Test graphics pipeline");

gotoIfError3(clean, GraphicsDeviceRef_createPipelineGraphics(
	device, &tempShaders, &stageInfos, &info, &name, EPipelineFlags_None, NULL, &graphicsShaders, e_rr
));

...;	//Free binaries (tempShaders)
```

Create pipelines will take ownership of the buffers referenced in stages and it will therefore free the list (if unmanaged). If the buffers are managed memory (e.g. created with Buffer_create functions that use the allocator) then the Pipeline object will safely delete it. This is why the tempShader is set to null after (the list is a ref, so doesn't need to be). In clean, this temp buffer gets deleted, just in case the createPipelines fails. Using virtual files for this is recommend, as they'll already be present in memory and our ref will be available for the lifetime of our app. If it's a ref then the implementation will copy to avoid unsafe behavior.

It is recommended to generate all pipelines that are needed in this one call at startup, to avoid stuttering at runtime. This can be done from a different thread as well, though the implementation is free to delay the wait for finalization (real driver compiles) until it's first use in a command list (e.g. it starts compilation in async).

### Raytracing example

```c
ListSHFile tempShaders = ...;		//SHFile: Load from virtual file or hardcode.

//Define our entrypoints; a single compiled binary with specified entry points.

U32 mainClosestHit = GraphicsDeviceRef_getFirstShaderEntry(...);		//Check the compute example for more
U32 mainMiss = GraphicsDeviceRef_getFirstShaderEntry(...);
U32 mainRaygen = GraphicsDeviceRef_getFirstShaderEntry(...);

PipelineStage stageArr[] = {
	(PipelineStage) { .binaryId = mainClosestHit,	.shFileId = 0 },
	(PipelineStage) { .binaryId = mainMiss,			.shFileId = 0 },
	(PipelineStage) { .binaryId = mainRaygen,		.shFileId = 0 }
};

//Define a hit group

PipelineRaytracingGroup hitArr[] = {
	(PipelineRaytracingGroup) { .closestHit = 0, .anyHit = U32_MAX, .intersection = U32_MAX }
};

//Define the pipeline that links these together as one.
//We can link multiple at the same time.
//All ids (in hitArr, stageArr) are relative to the stage itself (no global binary id).

U64 entrypointCount = sizeof(stageArr) / sizeof(stageArr[0]);
U64 hitCount = sizeof(hitArr) / sizeof(hitArr[0]);

PipelineRaytracingInfo info = (PipelineRaytracingInfo) {
	.flags = (U8) EPipelineRaytracingFlags_DefaultStrict,
	.maxRecursionDepth = 1
};

//Turn into lists

ListPipelineStage stages = (ListPipelineStage) { 0 };
ListPipelineRaytracingGroup hitGroups = (ListPipelineRaytracingGroup) { 0 };

gotoIfError3(clean, ListPipelineStage_createRefConst(stageArr, entrypointCount, &stages, e_rr));
gotoIfError3(clean, ListPipelineRaytracingGroup_createRefConst(hitArr, hitCount, &hitGroups, e_rr));

//Finalize into pipeline

const CharString rtName = CharString_createRefCStrConst("Raytracing pipeline test");

gotoIfError3(clean, GraphicsDeviceRef_createPipelineRaytracingExt(
	device,
	&stages,
	&tempShaders,
	&hitGroups,
	&info,
	&rtName,
	EPipelineFlags_None,
	NULL,
	&raytracingShaders,
	e_rr
));

...;	//Free binaries (tempShaders)
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
  - types.hlsl also defines all HLSL types as OxC3 types, to ensure it could be cross compiled to GLSL in the future (since GLSL has some features that HLSL might not support). Using these predefined types are fully optional if HLSL is the final target (Vulkan + D3D12), though it is recommended to use them to avoid getting stuck to one shading language.

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

Buffer vertexData = Buffer_createRefConst(vertexPos, sizeof(vertexPos));
CharString name = CharString_createRefCStrConst("Vertex position buffer");
gotoIfError3(clean, GraphicsDeviceRef_createBufferData(
	device, EDeviceBufferUsage_Vertex, EGraphicsResourceFlag_None, NULL, &name, &vertexData, &vertexBuffers[0], e_rr
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

- `Bool DeviceBufferRef_markDirty(DeviceBufferRef *buffer, U64 offset, U64 count, Error *e_rr)` marks part or the entire resource dirty (count 0 = rest of the buffer starting at offset). This means that next commit the implementation will decide on how to copy to the resource in an efficient way. For example if the resource isn't in flight and ReBAR is turned on or shared memory is in use then it can directly copy to CPU accessible memory. Otherwise it might have to use a copy queue or something similar. The region is merged with any other pending region 256 bytes on either side to avoid lots of fragmented copies. Please make sure to only call this when necessary as this might cause extra allocations or copies to a RingBuffer; a good strategy (if the buffer changes each frame) might be to make the buffer 3x as big as necessary and index based on frameId % 3 and make the buffer CPUAllocated to allow direct writes always and then copy from this buffer only when needed. The framework will try to optimize these copies as much as possible (to avoid having to do that manually), but more work might be needed from the developers using it instead.

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
- isCompleted: this is set when the BLAS has been signaled as fully built. For example when buildBLASExt has been called or when the first submitCommands has been triggered since it has been queued.
- flagsExt: BLAS or TLAS specific flags; EBLASFlag for a BLAS, ETLASFlag for a TLAS.
- asConstructionType: the BLAS or TLAS specific construction type.
- scratchBuffer: temporary data that is only available until the AS has been created and the frame has been completed on the CPU.
- asBuffer: the buffer resource that represents this acceleration structure.
- flags:
  - AllowUpdate (0): refitting is allowed. This is a faster way of updating acceleration structures, but at the cost of traversal time. See Refitting below.
  - AllowCompaction (1): compaction is allowed. This reduces memory overhead for the acceleration structures.
  - FastTrace (2): optimize trace times over build times / memory.
  - FastBuild (3): optimize build times over trace times / memory.
  - MinimizeMemory (4): optimize memory over trace times / build times.
  - Reserved5 (5): reserved, free to reuse. Used to mean "this build is a refit", which is no longer something the caller asks for.
  - DisableAutomaticUpdate (6): next submitCommands shouldn't build this acceleration structure. This is useful when the mesh has to be initialized by the GPU using commands first (such as copies, compute or stream out).

##### Refitting

A refit (also called an update) rebuilds an acceleration structure from the one already there instead of from nothing. It is much cheaper than a full build and is what skeletal animation, moving instances and deforming geometry use, at the cost of traversal quality that degrades the further the new data drifts from what the structure was originally built for. Build with `ERTASBuildFlags_AllowUpdate` to allow it.

**A refit is in place and does not produce a new object.** The first build of an AS is a full build; every build recorded after that one refits the same structure from itself, which is what both APIs mean by an update whose source and destination are the same. So the AS, its device address and (for a TLAS) its bindless handle all survive a refit unchanged: nothing that points at it has to be re-pointed, and refitting every frame allocates nothing.

To refit, change the inputs and record the update again:

- A **BLAS** reads its own position buffer, so rewriting that buffer (and marking it dirty if it is CPUBacked) is the change. Only the vertex data may move; the topology, formats and counts are fixed for the life of the structure, because an update refits an existing structure rather than sizing a new one.
- A **TLAS** takes new instances through `TLASRef_setInstancesExt`, which requires the same instance count it was built with. The upload happens at build time, so setting instances repeatedly before recording an update costs one upload rather than one per call.

A BLAS that was refitted leaves every TLAS over it stale, since an instance caches the bounds of the BLAS it points at, so record the TLAS update in the same submit right after the BLAS one.

There is no way to keep the previous generation of an AS alive while refitting, by design: that used to be the only shape available and it meant every refit both reallocated the whole structure and pinned its predecessor for as long as it lived, so a chain of refits grew memory without bound. Something that genuinely needs last frame's structure (temporal techniques) wants a deliberate copy, not a refit.

#### BLAS

A BLAS is a bottom level acceleration structure; it is the representation of a couple volumes or triangle geometry. It has an acceleration structure built over this geometry to accelerate tracing rays through it. These BLASes are used through a TLAS (top level AS) and can then be accessed in raytracing.

There are two types of BLASes:

- Regular geometry (triangles).
- Procedural geometry (AABBs).

The latter can only be used through intersection shaders, while the former has way better traversal speeds. Procedural geometry is generally super slow and it's generally faster to approximate with triangle geometry.

##### Example: Triangle geometry

```c
const BLASCreateInfo blasInfo = BLASCreateInfo_indexed(
	ERTASBuildFlags_DefaultBLAS,			//Fast trace & allow compaction
	EBLASFlag_DisableAnyHit,				//No transparency needed, optimize for opaque
	ETextureFormatId_RG16f, 0,				//No pos attrib offset and format is RG16f
	(U16) sizeof(vertexPos[0]),				//Stride is F16[2]
	(DeviceData) {
        .buffer = twm->vertexBuffers[0]		//Entire buffer is accessible
    },
	ETextureFormatId_R16u,					//Indices are 16-bit
	(DeviceData) {
        .buffer = twm->indexBuffer,
        .len = sizeof(U16) * 6 				//Only use sub region
   	}
);

const CharString name = CharString_createRefCStrConst("BLAS");
gotoIfError3(clean, GraphicsDeviceRef_createBLASExt(twm->device, &blasInfo, &name, &twm->blas, e_rr));
```

The example above assumes that there is an index buffer and a position buffer available. Those buffers need to have the ASReadExt buffer usage to be accessible during build time.

Unindexed geometry goes through BLASCreateInfo_unindexed instead, which drops the index format and buffer arguments.

The BLAS flags are the following: EBLASFlag_AvoidDuplicateAnyHit and EBLASFlag_DisableAnyHit. This is only relevant for raytracing pipelines and is irrelevant if the TLAS instance itself turns off anyHit.

###### Opacity micromaps (Ext)

BLASCreateInfo_indexedWithOmmIndicesExt extends the indexed form with a per triangle opacity index buffer, which requires EGraphicsFeatures_RayMicromapOpacity. It is the SPECIAL INDEX form: no micromap object is attached, so every element has to be an EOMMSpecialIndex rather than an index into one. Build the values with EOMMSpecialIndex_pack, since the special indices are negative constants matched against an unsigned element and so depend on the element width (FullyTransparent is 0xFFFF with R16u and 0xFFFFFFFF with R32u).

- FullyTransparent: the triangle is ignored entirely, so the ray passes through it.
- FullyOpaque: the triangle hits without ever running anyHit.
- FullyUnknownTransparent / FullyUnknownOpaque: anyHit decides, the name being the hint for what it usually is.

The index buffer holds exactly one element per TRIANGLE (so indexBuffer length / index stride / 3). R16u and R32u are accepted everywhere opacity micromaps are; R8u additionally requires the EGraphicsFeatures2_RayMicromapOpacityU8 capability, since Vulkan's EXT extension forbids 8-bit indices and only the KHR promotion (or D3D12) permits them.

Two opt-ins outside the BLAS have to line up or the micromap is silently ignored, which looks exactly like a micromap that did nothing:

- The pipeline needs EPipelineRaytracingFlags_AllowOpacityMicromapExt. Both APIs may traverse differently when micromaps are in play, so they make you say so at pipeline creation and ignore any micromap otherwise.
- The instance must NOT set ForceDisableAnyHit, and the ray must not use RAY_FLAG_FORCE_OPAQUE. Both mean FORCE_OPAQUE, which makes traversal treat every triangle as opaque and skip the micromap. Note that ETLASInstanceFlag_Default INCLUDES ForceDisableAnyHit, so an instance that wants micromaps has to spell its flags out rather than take the default.

Whether a real micromap object is worth building over special indices is what the EGraphicsFeatures2_RayMicromapOpacityActual capability bit is for; special indices cost nothing either way.

Micromap OBJECTS go through `GraphicsDeviceRef_createOpacityMicromapExt` (see opacity_micromap.h): the create takes an input buffer of packed opacity bits, an entry buffer of `OpacityMicromapEntry` records (dataOffset, subdivisionLevel, format) and the usage counts describing them, all needing EDeviceBufferUsage_ASReadExt. Input and entry buffers must sit at 256 byte aligned addresses on Vulkan (VUID-vkCmdBuildMicromapsEXT-pInfos-07515, validated at create) and 128 byte aligned on D3D12; OxC3's own ASRead allocations satisfy both, and on D3D12 SDK versions whose debug layer wrongly enforces 256 (stable before 620, preview before 722) the allocation floor stays 256 so validation runs clean. The build is recorded with `CommandListRef_updateOmmExt`, which must precede any BLAS build that links the micromap; recording it again after it completed is a no-op, since micromaps have no update mode. A BLAS links one through `BLASCreateInfo_indexedWithOmmExt`, whose OMM index buffer then holds ENTRY indices (special values still allowed per triangle) instead of only special values. On D3D12 an OMM array is itself an acceleration structure; on Vulkan the EXT extension builds it as a `VkMicromapEXT` (the KHR promotion's as-an-acceleration-structure build is not implemented yet for lack of a driver to test against, so micromap objects are refused there while special indices keep working).

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
gotoIfError3(clean, GraphicsDeviceRef_createBufferData(
	twm->device,
    EDeviceBufferUsage_ASReadExt, EGraphicsResourceFlag_None, NULL,
    name, &aabbData, &twm->aabbs, e_rr
));

gotoIfError3(clean, GraphicsDeviceRef_createBLASProceduralExt(
	twm->device,
	ERTASBuildFlags_DefaultBLAS,
	EBLASFlag_DisableAnyHit,
	sizeof(F32) * 3 * 2,
	0,
	(DeviceData) { .buffer = twm->aabbs },
	CharString_createRefCStrConst("Test BLAS AABB"),
	&twm->blasAABB,
	e_rr
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

- Obtained through createBLASProceduralExt and createBLASExt (via a BLASCreateInfo) from GraphicsDeviceRef.
- Used in TLAS creation and can be copied to/from the CPU.

#### TLAS

A TLAS is a top level acceleration structure; it is the representation of a couple instances of triangle geometry or procedural geometry (a scene). It has an acceleration structure built over these instances to accelerate tracing rays through it. BLASes (Bottom level acceleration structures) are used through a TLAS (top level AS) and can be accessed in raytracing.

A TLAS can be initialized through two different methods:

- CPU-side (Might introduce a temporary staging resource or ring buffer).
- GPU side by an already initialized GPU resource.

##### Example: Static instances

```c
TLASInstance instances[1] = {
    (TLASInstance) {
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

ListTLASInstance instanceList = (ListTLASInstance) { 0 };
gotoIfError3(clean, ListTLASInstance_createRefConst(
    instances, sizeof(instances) / sizeof(instances[0]), &instanceList, e_rr
));

gotoIfError3(clean, GraphicsDeviceRef_createTLASExt(
    twm->device,
    ERTASBuildFlags_DefaultTLAS,
    NULL,
    instanceList,
    false,				//disallowBindlessDescriptor
    NULL,				//bindlessDescriptorTable; NULL = device's default bindless table
    CharString_createRefCStrConst("Test TLAS"),
    &twm->tlas,
    e_rr
));
```

For GPU construction, the same can be done and has to follow the exact struct; except blasCpu will become the address of the BLAS on the GPU.

##### Properties

- base: RTAS standardizes BLAS and TLAS a little bit. TLAS specific state lives in `base.flagsExt` as ETLASFlag, read through `TLAS_hasFlag`:
  - UseDeviceMemory: the instances came from GPU memory rather than from a temporary CPU visible buffer.
  - DisallowBindlessDescriptor: no bindless descriptor was allocated, so the TLAS cannot be reached from a shader.
  - BlasDataAccessKnown / BlasDataAccessAll: cached at create, whether every visible instance's BLAS allows ray triangle position fetch. Only knowable for CPU side instances, so Known stays unset for device built and serialized TLASes.
  - InstancesDirty: `TLASRef_setInstancesExt` has supplied new instances that the next build has yet to upload.
- handle: Descriptor index of the acceleration structure.
- Depending on UseDeviceMemory and base.asConstructionType: (Instances, Serialized):
  - ETLASConstructionType_Serialized:
    - Buffer cpuData: Indicates cpu memory that represents the TLAS. Might become invalidated because of a driver update.
  - UseDeviceMemory:
    - deviceData: Represents the resource range that holds the TLASInstance[] on the GPU.
  - else
    - cpuInstances: List of TLASInstance.

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
- ForceDisableAnyHit (2): Never run any hit. This is FORCE_OPAQUE on both APIs, so it also turns off opacity micromaps for the instance; it is part of ETLASInstanceFlag_Default.
- ForceEnableAnyHit (3): Always run any hit.

##### Used functions and obtained

- Obtained through createTLASExt and createTLASDeviceExt from GraphicsDeviceRef.
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
gotoIfError3(clean, CommandListRef_setViewportAndScissor(
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
gotoIfError3(clean, CommandListRef_setStencil(commandList, 0xFF, e_rr));
```

And the blend constants can be set like so:

```c
gotoIfError3(clean, CommandListRef_setBlendConstants(
    commandList, F32x4_create4(1, 0, 0, 1), e_rr
));
```

### clearImage(f/u/i)/clearImages

clearImagef/clearImageu/clearImagei and clearImages are actually the same command. They allow clearing one or multiple images. clearImages should be used whenever possible because it can batch clear commands in a better way. However, it is possible that only one image needs to be cleared and in that case clearImage(f/u/i) are perfectly fine. The f, u and i suffix are to allow clearing uint, int and float targets. In clearImages these are handled manually. The format should match the format of the underlying images. The images passed here are automatically transitioned to the correct state.

```c
gotoIfError3(clean, CommandListRef_clearImagef(
    commandList, 				//See "Command list"
    F32x4_create4(1, 0, 0, 1), 	//Clear red
    (ImageRange){ 0 }, 			//Clear layer and level 0
    swapchain,					//See "Swapchain" or "RenderTexture"
    e_rr
));
```

To clear multiple at once, call clearImages with a `ListClearImageCmd`. ClearImageCmd takes a color, a range and the image ref ptr. It's the same as a single clear image but it allows multiple.

Clear image is only allowed on images which aren't currently bound as a render target. The image is leading in determining the format which will be read out. If you use clearImagef but it's a uint texture then it will bitcast the float color to a uint for you.

Clear image can currently only be called on a Swapchain or RenderTexture object.

### copyImage/copyImageRegions

copyImage and copyImageRegions are actually the same command. They allow copying either one or multiple regions from the same image to another, as long as the two have the same format.

```c
gotoIfError3(clean, CommandListRef_copyImage(
    commandList,
    tw->swapchain, 				//src (Swapchain, DepthStencil, RenderTexture)
    tw->renderTextureBackup, 	//dst (^)
    (CopyImageRegion) { 0 },	//Copy everything
    e_rr
));
```

Clearing multiple ranges at once can be done by calling copyImageRegions with a ListCopyImageRegion. The src and dst must both be able to contain the pixels copied.

If depth, width or height isn't defined, they are automatically set to res[i] - offset[i].

#### outputRotation

A region can ask for the copied pixels to land rotated, with `outputRotation` set to 0, 1, 2 or 3 for 0, 90, 180 and 270 degrees. Anything above 3 is refused.

Neither API's image copy can express a rotation, so a rotated region is replayed as a dispatch of the device's own copy shader rather than as a transfer. That changes what the command needs:

- A descriptor heap with `maxPushDescriptors >= 2` has to be bound in the scope (`CommandListRef_bindDescriptorHeap`): the replay's two transient descriptors come from that heap's push ring, and the runtime deliberately never binds a heap of its own choosing, because a hidden `SetDescriptorHeaps` is heavy (it can drain the GPU on NV). Vulkan needs no heap, but recording is refused on both backends so a command list that records on one records on the other. A bindless app does not need a heap of its own for this: the device's heap reserves a ring (`OXC3_MAX_PUSH_DESCRIPTORS * 64` per frame in flight, also when the device was created from a user supplied bindless layout), so binding `Device::defaultHeap()` explicitly serves the copy, and a scope already running bindless work has that heap current anyway, making the bind free.

The same heap can also hold **bindful descriptor tables**: `GraphicsDeviceRef_create` takes an optional `reservedDescriptors` (a `DescriptorHeapInfo`), extra capacity added on top of what the bindless set consumes, `maxDescriptorTables` included since the default table takes the one the heap starts with. A bindful table created from `Device::defaultHeap()` then lives beside the bindless set, so interleaving bindful and bindless work never switches heaps at all.

- `src` is read through a sampled descriptor and so requires `EGraphicsResourceFlag_ShaderRead`.
- `dst` is written through a storage descriptor and so requires `EGraphicsResourceFlag_ShaderWrite`.

Both are checked when the command is recorded, because neither backend fails usefully afterwards: D3D12 would build a UAV over a resource that never allowed one, and Vulkan cannot create the view at all.

One rotated region puts the *whole* command on that path, so every region in it, rotated or not, is copied by the shader and both images transition to shader read/write instead of transfer. A scope that also uses either image as a transfer or render target therefore needs to be split, the same as for any other conflicting usage.

An unrotated copy is unaffected and stays a plain transfer, needing neither flag.

DepthStencil is only compatible with depth stencil and the formats have to be compatible.

Copy image is only allowed on images which aren't currently bound as a render target or for a different use in this scope.

Copy image (ranges) can currently only be called on a Swapchain, DepthStencil or RenderTexture object.

### setComputePipeline/setGraphicsPipeline/setRaytracingPipeline

The set pipeline command does one of the following; bind a graphics pipeline, raytracing pipeline or a compute pipeline. These pipelines are the only bind points and they're maintained separately. So a bind pipeline of a graphics shader and one of a compute shader don't interfere. This is used before a draw, dispatch or dispatchRaysExt to ensure the shader is used.

```c
gotoIfError3(clean, CommandListRef_setComputePipeline(commandList, compPipeline, e_rr));
gotoIfError3(clean, CommandListRef_setGraphicsPipeline(commandList, gfxPipeline, e_rr));
gotoIfError3(clean, CommandListRef_setRaytracingPipeline(commandList, rtPipeline, e_rr));
```

### draw

The draw command will simply draw the currently bound primitive buffer (optional) with the currently bound graphics pipeline (required). If the primitive buffer is not present, the pipeline is expected to generate the vertices dynamically. It requires a render call to be started (see DirectRendering). It handles both indexed and non indexed draw calls:

```c
//Non indexed draw call
//Can also specify instanceOffset and vertexOffset if drawUnindexedAdv is used

gotoIfError3(clean, CommandListRef_drawUnindexed(commandList, 3, 1, e_rr));

//Indexed draw call
//Can also specify instanceOffset, indexOffset and vertexOffset
//if drawIndexedAdv is used

gotoIfError3(clean, CommandListRef_drawIndexed(commandList, 3, 1, e_rr));
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

gotoIfError3(clean, CommandListRef_setPrimitiveBuffers(commandList, primitiveBuffers, e_rr));
```

### dispatch

Dispatch has some of the same requirements as draw calls as it needs a correct state of the resources and needs a compute pipeline bound as well. However, it doesn't need to be in an active render (render pass or direct rendering) and it also doesn't use the viewport/scissor. This makes compute one of the easiest to handle, though transitions are just as important as with graphics shaders (resources need to be transitioned through a scope).

```c
gotoIfError3(clean, CommandListRef_dispatch2D(commandList, tilesX, tilesY, e_rr));
```

dispatch2D, dispatch1D and dispatch3D are the easiest implementations. You dispatch in groups, so it has to be aligned to the thread count of the compute shader. This command is also generalized with the dispatch command which takes in a `Dispatch` struct.

#### dispatchIndirect

Same thing as dispatch, except the device reads from a U32x3 into the DeviceBuffer at the offset and dispatches the groups stored there. Has to be aligned to 16-byte. For 2D dispatches please set z to 1, for 1D set y to 1 as well.

dispatchIndirect transitions the input buffer to IndirectDraw. This means that the buffer can't also be bound as a Vertex/Index buffer or be used in the shader as a read/write buffer in the same scope. Buffer needs to enable Indirect usage to be usable by indirect draws.

### DebugMarkers feature

The DebugMarkers feature adds three commands: addMarkerDebugExt, startRegionDebugExt and endRegionDebugExt. If the DebugMarkers feature isn't present, these are safely ignored and won't be inserted into the virtual command list. This can be used to provide extra debugging info to tools such as RenderDoc, NSight, Pix, etc. to show where important events such as render calls happened and what they represented. A debug region or debug marker has a color and a name. A debug region is like a stack; you can only end the current region and push another region. Every region you start has to be ended in the same scope that is submitted.

```c
gotoIfError3(clean, CommandListRef_startRegionDebugExt(
    commandList, 							//See "Command list"
    F32x4_create4(1, 0, 0, 1), 				//Marker color
    CharString_createConstRefCStr("Test")	//Marker name
));

//Insert operation that needs to be in the debug region

gotoIfError3(clean, CommandListRef_endRegionDebugExt(commandList, e_rr));
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
gotoIfError3(clean, ListAttachmentInfo_createRefConst(attachmentInfo, 1, &colors, e_rr));

gotoIfError3(clean, CommandListRef_startRenderExt(
    commandList, 							//See "Command list"
    I32x2_zero(), 							//No offset
    I32x2_zero(), 							//Use attachment's size
    colors,
    (DepthStencilAttachmentInfo) { 0 },		//No depth stencil attachment
    e_rr
));

//Draw calls here

gotoIfError3(clean, CommandListRef_endRenderExt(commandList, e_rr));
```

*Keep in mind that during the scope, you can't transition the resources passed into the attachments of the active render call. They're not allowed to be used as a read (SRV) or write textures (UAV); they're only allowed to be output attachments (RTV). Doing otherwise will cause the command list to give an error and remove the current scope.*

Every startRender needs to match an endRender. During the render it's not allowed to access the render textures as a write or read texture. Other operations that change state (implicit transitions) such as clears and copies are also not allowed. If this is required, the developer can end the render and restart it after this operation.

A graphics pipeline for use with DirectRendering needs to set the attachment count (and format(s)) or the depth stencil format. One that doesn't use direct rendering can't be used.

### Bindful descriptors

#### bindDescriptorHeap

Binds a descriptor heap: any descriptor table bound after this has to belong to it, which the work ops validate. Explicit on purpose: switching heaps can stall the GPU (notably on D3D12), so the cost must be a visible command rather than implied by whichever table was bound. On Vulkan there is nothing to emit today (a heap is a descriptor pool), but the state is recorded so VK_EXT_descriptor_heap can map the explicit bind directly later. The default (bindless) heap, root signature and table are no longer bound eagerly per command buffer either: they bind lazily at the first work op that runs a default layout pipeline, so a purely bindful frame never pays for them, and after a mid submit flush the next work op re-emits whatever was last bound.

#### bindDescriptorTable

Bindful: binds the descriptor table that pipelines with a CUSTOM pipeline layout read from. This only sets state; the work ops (draw/dispatch/dispatchRays) are the validators, so bind order never matters: at work time the bound pipeline's layout must reference the exact DescriptorLayout the table was created from, push descriptor layouts are refused until their writes exist, and a pipeline whose layout does not reference the table's DescriptorLayout ignores the bound table entirely rather than emitting it. That covers the device's default (bindless) layout and any custom layout built from the runtime bindless set, which is what lets a bindful dispatch and a bindless one interleave inside one scope. To be precise about what "stays bound" means there: the RECORDER keeps boundDescriptorTable across the pipeline switch, so the caller does not rebind it; the GPU-side state does not survive. The bindful table lives in the user's heap and the bindless set in the device's own, two different heaps, and each work op that crosses between the two classes lazily re-emits everything for its side, a SetDescriptorHeaps included (D3D12 allows one CBV/SRV/UAV heap bound at a time, so every crossing swaps it). The layout check is what keeps the held table from being emitted against the bindless pipeline in between: doing so would bind sets Vulkan rejects and write root parameters D3D12's signature does not have, so a table whose layout the pipeline does not reference is simply not emitted. Interleaving therefore works, but each direction change costs a heap switch, so batching by pipeline class is still worth it. Backends emit the actual binds lazily right before the work, which also re-emits the default bindings after a custom root signature dropped them on D3D12. Custom layout pipelines are opted out of the globals/frame data unless their layout declares that slot. There is no table index: a pipeline layout references exactly ONE bindings DescriptorLayout (which itself spans up to 4 spaces on Vulkan, each bound at the set index its space names, and up to 2 root tables on D3D12), so set indices and root parameters are baked into the layout/table pair; a slot index gets added if pipeline layouts ever grow multiple table slots. The table is kept alive by the command list; per resource scope transitions remain the caller's job until auto transitions land. Scope end resets the bind like it does bound pipelines.

#### setPushConstants

Writes the pipeline layout's push constants: a small block of bytes that rides in the command stream rather than in a descriptor heap (root constants on D3D12, `vkCmdPushConstants` on Vulkan). A write is 4 to 128 bytes and a multiple of 4. 128 is what both APIs guarantee: it is Vulkan's minimum `maxPushConstantsSize` and 32 of the 64 DWORDs a D3D12 root signature has.

Like the binds this only sets state, and the work op is the validator. At draw/dispatch/dispatchRays time the written size has to match exactly the `constantBufferSize` the bound pipeline layout declares. Writing constants a layout never declared is refused, since it can only mean the wrong pipeline is bound; a layout that declares them with nothing written is refused too, rather than reading back whatever the previous pipeline happened to leave in the range. That check sits ahead of the bind state cache, because unlike the pipeline/table/heap triple the cache keys on, the written size is mutable state that a later setPushConstants can change without rebinding anything.

The payload is copied into the command itself rather than referenced, so a replay never depends on the caller's buffer still being alive. Backends emit it lazily at the work op, which is what makes a root signature switch between two work ops harmless: D3D12 drops every root argument on such a switch, so an eager emit at the write would silently lose the values. Two dispatches with different constants and no rebinding in between therefore each see their own. Scope end resets the written size like every other bind state.

#### setPushDescriptors

Writes every push descriptor the bound pipeline's layout declares, in that layout's own binding order (the order `detectLayoutFromEntry`/`detectLayoutFromEntries` produced into `pushDescriptorInfo`, whose `bindingNames` name them). A push descriptor lives in the command stream rather than in a heap: a root CBV/SRV/UAV on D3D12, `vkCmdPushDescriptorSetKHR` on Vulkan. Nothing about it goes through a DescriptorTable, so no heap or table has to be bound for one.

All of them are written at once rather than one at a time, because a partial set would leave the rest pointing at whatever the previous pipeline bound, and both backends emit the whole set anyway. The work op validates the written count against the layout, refuses a count that doesn't match it, and refuses writes against a layout that declares none: the same three rules push constants follow, and for the same reason they sit ahead of the bind state cache. Scope end resets the write.

**Buffer class** push descriptors (a constant buffer, a byte address or structured buffer, or an acceleration structure) are root descriptors on both backends: one raw GPU virtual address, costing 2 of a D3D12 root signature's 64 DWORDs each, which is why at most `OXC3_MAX_PUSH_DESCRIPTORS` can be recorded.

**Textures** can be pushed too, but not as a root descriptor, because an address has nowhere to carry a format, a mip range or a swizzle. D3D12 gives the binding a single entry descriptor table instead and fills it at the work op; Vulkan pushes an image descriptor straight into the set and needs nothing further. That table needs a shader visible slot, and it has to come from a heap the CALLER put in play: a heap the runtime picked itself would need a `SetDescriptorHeaps` behind the caller's back, and that switch is heavy (it can drain the GPU on NV), which is the entire reason heap binds are explicit. So a heap carries a ring of its own for them, sized by `DescriptorHeapInfo::maxPushDescriptors`:

- The slot comes from the bound DescriptorHeap (`bindDescriptorHeap`), or, only when the pipeline layout uses the runtime bindless set, from the device's own heap, since that pipeline binds the device heap for itself either way and the push adds no switch. Anything else is refused at the work op; there is no silent fallback.
- `maxPushDescriptors` is **per frame in flight**, and every emission takes fresh slots rather than reusing them, because the earlier ones stay live until that frame retires. Size it for a frame's worth of texture pushes, not for a single bind. The device's own heap reserves `OXC3_MAX_PUSH_DESCRIPTORS * 64`. Exhausting it mid frame logs an error and wraps.
- The texture needs `EGraphicsResourceFlag_ShaderRead`, or `EGraphicsResourceFlag_ShaderWrite` when the binding writes, since that is what makes the backend give it `ALLOW_UNORDERED_ACCESS` / `VK_IMAGE_USAGE_STORAGE_BIT` at creation. Checked when the work op records.
- The requirement is enforced on Vulkan too, even though it pushes images directly, so a layout that records on one backend records on both.

**Samplers** cannot be pushed as descriptors, but they do not need to be: declare them **immutable** instead. `DescriptorLayoutInfo::immutableSamplers` holds `SamplerRef`s and a sampler binding names one through `immutableSamplerId` (1 based, 0 meaning none), added with `DescriptorLayoutInfo_addImmutableSampler`. A sampler in a push descriptor layout **has** to be immutable, since there is no root sampler on D3D12 to push one into.

An immutable sampler is baked into the layout rather than bound:

- **D3D12** puts it in the root signature as a `D3D12_STATIC_SAMPLER_DESC`, which costs none of the 64 DWORDs and needs no descriptor range or heap slot.
- **Vulkan** puts it in the set layout as `pImmutableSamplers`. Immutable samplers are legal in a push descriptor set layout and need no write, which is what lets the two backends agree.
- It takes a binding but **no descriptor**, so `setPushDescriptors` writes only the bindings that need one and the baked samplers are skipped in that order.
- Held by ref rather than by value so several layouts naming the same sampler share one `VkSampler`; D3D12 never makes an object of one and only reads its `SamplerInfo`.
- It cannot be an array. A dynamically indexed sampler array needs real descriptors.

`ESamplerBorderColor` is already the enumerated set a static sampler allows (transparent black, opaque black, opaque white), so every sampler OxC3 can express is expressible as an immutable one.

#### Dynamic samplers are opt in

The bindless `_samplers[]` array is **not** in the default layout unless the device was created with `EGraphicsDeviceFlags_EnableDynamicSamplers`. It is the one bindless array that owns a descriptor set to itself on Vulkan (set 0, while every resource array shares set 1, out of the four sets `maxBoundDescriptorSets` guarantees), and declaring it forces a sampler heap on both backends and a sampler root table on D3D12.

Without the flag:

- The default layout carries no sampler binding, so `heapForLayout` derives `maxSamplers = 0` and no sampler heap is created.
- Nothing renumbers: the resource arrays keep their own set and binding numbers, since only set 0 held the sampler.
- `_samplers`, `sampler(i)` and `samplerUniform(i)` are not declared in `resources.hlsli`. A shader wanting them annotates `[[oxc::extension("DynamicSamplers")]]`, which sets `__OXC_EXT_DYNAMICSAMPLERS` exactly like every other extension define and is what makes the reference compile at all.

`ESHExtension_DynamicSamplers` is set by the annotation, and reflection also infers it for any binary declaring a sampler **array** of its own, the same way `ESHExtension_Bindless` is derived. `GraphicsDeviceRef_checkShaderFeatures` refuses such a binary on a device whose layout has no sampler array, naming the flag, instead of letting it resolve against a binding that was never declared. A singular sampler is unaffected: it is a plain binding, or better, an immutable one.

Prefer immutable samplers. Dynamic samplers only pay for themselves when the sampler is genuinely selected by an index the shader computes.

The resources are kept alive by the command list, like a bound table's are, and per resource scope transitions remain the caller's job until auto transitions land.

**Known gap:** on Vulkan a caller owned push descriptor layout currently requires `VK_KHR_push_descriptor` (the `PerformantPushDescriptor` capability) and is refused at layout creation without it. The device's own globals layout is exempt because it carries its own emulation (one descriptor set per frame in flight, written once and bound unchanged), which works only because that buffer is fixed for the whole frame. A caller's push descriptors change per work op, so the general emulation needs a set allocated per push from a per frame pool. Android emulators are the case that hits this, since gfxstream drops the extension from the guest even where the host driver exposes it.

### Raytracing feature

#### updateTLASExt/updateBLASExt

Are used to force update TLAS and BLAS respectively. This is useful when the TLAS/BLAS is entirely GPU generated with compute and thus, it needs to be issued on the GPU (not CPU). The CPU generated one only works if GPU buffers were filled previous frame or if the CPU does. For more complex scenarios, auto update should be turned off when BLAS/TLAS is generated and manual updates should be done.

### RayPipeline feature

#### dispatchRaysExt

dispatchRays has some of the same requirements as dispatch as it needs a correct state of the resources and needs a raytracing pipeline bound as well. It is similar to compute as it doesn't need to be in an active render (render pass or direct rendering) and it also doesn't use the viewport/scissor. Though transitions are just as important as with compute shaders (resources need to be transitioned through a scope).

```c
gotoIfError3(clean, CommandListRef_dispatch2DRaysExt(commandList, rtId, width, height, e_rr));
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

    startRenderExt							//Keeps scope alive if any attachment uses a Clear load
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

A scope that never records one of the "keeps scope alive" operations is rewound at endScope as if it never happened: its commands, transitions and scope id are all discarded and it won't appear in activeScopes or execute at submit. State setters (pipelines, viewport, primitive buffers, debug markers) never keep a scope alive on their own. A render pass counts as alive when any of its attachments uses a Clear load, since the clear is a side effect all by itself; a pass that only loads/preserves and never draws is dead weight and gets rewound with the rest.

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
gotoIfError3(clean, ListTransition_createRefConst(transitions, 1, &transitionArr, e_rr));

gotoIfError3(clean, CommandListRef_startScope(commandList, transitionArr, 0 /* id */, (ListCommandScopeDependency) { 0 } /* deps */, e_rr));
//TODO: Bind compute shader(s) and dispatch
gotoIfError3(clean, CommandListRef_endScope(commandList, e_rr));
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