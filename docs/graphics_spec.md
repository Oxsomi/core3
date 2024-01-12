# Minimum spec

OxC3 is not made for devices older than 3 years. OxC3's spec for newer versions would require more recent devices to ensure developers don't keep on having to deal with the backwards compatibility for devices that aren't relevant anymore. For example; phones get replaced approx every 3 years. So it's safe to take 2 years back if you assume a development time/rollout of a year (at the time of OxC3 0.2). 

*Keep in mind that a minor or major version change can change this spec! It might be that there was an oversight and a device/api was released that disagreed with our minimum spec. If the device/api is important enough, we might change the future spec (but never in patch versions. Only in minor/major). Please check the changelog to verify.*

We're targeting the minimum specs of following systems in OxC3 0.2:

- Phones:
  - Apple iPhone 12 (A14); only via Metal3 support.

  - Samsung S21 (Samsung SM-G991x).
  - Google Pixel 5a.
  - Xiaomi Redmi Note 8.
  - Raytracing capable:
    - Samsung S22 (SM-S901x).

- Laptop:
  - Nvidia RTX 3060 Laptop GPU.
  - AMD RX 6600M.
  - Apple Macbook Pro 14 (M1 Pro); only via Metal3 support.
- PCs:
  - Nvidia RTX 3060.
  - AMD 6600 XT.
  - Intel A750.

Binding tier is high for all laptops and PCs excluding Apple devices. All Apple devices are binding tier low until they release proper devices.

Just because these are the target minimum specs doesn't mean older hardware is unsupported. The hard requirements should be looked at instead to determine if the device is supported. ***These are just minimum feature targets, they aren't all tested.***

## List of Vulkan requirements

Because of this, a device needs the following requirements to be OxC3 compatible:

- Vulkan 1.1 or higher.
- Tessellation shaders and geometry shaders are optional.
- More than 512 MiB of CPU + GPU visible memory (At least 1GB total).
- Required instance extensions:
  - VK_KHR_get_physical_device_properties2
  - VK_KHR_external_memory_capabilities
- Supported instance extensions (optional:
  - Debug build only:
    - VK_EXT_debug_report
    - VK_EXT_debug_utils
  - VK_KHR_surface (and the variant of the platform such as VK_KHR_win32_surface)
  - VK_EXT_swapchain_colorspace
- Required device extensions:
  - VK_KHR_get_physical_device_properties2
  - VK_KHR_shader_draw_parameters
  - VK_KHR_external_memory_capabilities
  - VK_KHR_push_descriptor
  - VK_KHR_dedicated_allocation
  - VK_KHR_bind_memory2
  - VK_KHR_get_memory_requirements2
  - VK_EXT_shader_subgroup_ballot
  - VK_EXT_shader_subgroup_vote
  - VK_EXT_descriptor_indexing with all features true except shaderInputAttachmentArrayDynamicIndexing and shaderInputAttachmentArrayNonUniformIndexing.
  - VK_KHR_driver_properties
  - VK_KHR_synchronization2
  - VK_KHR_timeline_semaphore
- Supported device extensions:
  - Debug build only:
    - VK_EXT_debug_marker as internal flag
  - VK_KHR_shader_float16_int8 as F16
  - VK_KHR_draw_indirect_count as MultiDrawIndirectCount
  - VK_KHR_shader_atomic_int64 as AtomicI64
  - VK_KHR_performance_query as internal flag
  - VK_KHR_ray_tracing_pipeline as RaytracingPipeline
  - VK_KHR_ray_query as RayQuery
  - VK_KHR_acceleration_structure && (RaytracingPipeline || RaytracingQuery) as Raytracing
  - VK_KHR_swapchain as Swapchain
    - Requires at least 1 image layer.
    - Requires ability to make 2 (vsync) or 3 (!vsync) images.
    - Requires usage flags of transfer (src, dst), sampled, storage, color attachment bits.
    - Requires FIFO or MAILBOX. Allows FIFO_RELAXED and IMMEDIATE as well.
    - Requires inherit alpha or opaque support.
    - Application is responsible for presenting image in the right rotation if SwapchainInfo::requiresManualComposite.
  - VK_NV_ray_tracing_motion_blur as RayMotionBlur
  - VK_NV_ray_tracing_invocation_reorder as RayReorder
  - VK_EXT_mesh_shader as MeshShader
  - VK_EXT_opacity_micromap as RayMicromapOpacity
  - VK_NV_displacement_micromap as RayMicromapDisplacement
  - VK_KHR_dynamic_rendering as DirectRendering
  - VK_KHR_deferred_host_operations is required for raytracing. Otherwise all raytracing extensions will be forced off.
- subgroupSize of 16 - 128.
- subgroup operations of basic, vote, ballot are required. Available only in compute by default. arithmetic and shuffle are optional.
- shaderSampledImageArrayDynamicIndexing, shaderStorageBufferArrayDynamicIndexing, shaderUniformBufferArrayDynamicIndexing, shaderStorageBufferArrayDynamicIndexing, descriptorIndexing turned on.
- samplerAnisotropy, drawIndirectFirstInstance, independentBlend, imageCubeArray, fullDrawIndexUint32, depthClamp, depthBiasClamp, multiDrawIndirect turned on.
- Either BCn (textureCompressionBC) or ASTC (textureCompressionASTC_LDR) compression *must* be supported (can be both supported).
- shaderInt16 support.
- maxColorAttachments and maxFragmentOutputAttachments of 8 or higher.
- maxDescriptorSetInputAttachments, maxPerStageDescriptorInputAttachments of 7 or higher.
- MSAA support of 1 and 4 or higher (framebufferColorSampleCounts, framebufferDepthSampleCounts, framebufferNoAttachmentsSampleCounts, framebufferStencilSampleCounts, sampledImageColorSampleCounts, sampledImageDepthSampleCounts, sampledImageIntegerSampleCounts, sampledImageStencilSampleCounts). Support for MSAA 2 is non default.
  - MSAA2x, MSAA8x and MSAA16x graphics features are enabled if all of these support it.
- maxComputeSharedMemorySize of 16KiB or higher.
- maxComputeWorkGroupCount[N] of U16_MAX or higher.
- maxComputeWorkGroupInvocations of 512 or higher.
- maxFragmentCombinedOutputResources of 16 or higher.
- maxFragmentInputComponents of 112 or higher.
- maxFramebufferWidth, maxFramebufferHeight, maxImageDimension1D,  maxImageDimension2D, maxImageDimensionCube, maxViewportDimensions[i] of 16Ki or higher.
- maxFramebufferLayers, maxImageDimension3D, maxImageArrayLayers of 256 or higher.
- maxPushConstantsSize of 128 or higher.
- maxSamplerAllocationCount of 4000 or higher.
- maxSamplerAnisotropy of 16 or higher.
- maxStorageBufferRange of .25GiB or higher.
- maxSamplerLodBias of 4 or higher.
- maxUniformBufferRange of 64KiB or higher.
- maxVertexInputAttributeOffset of 2047 or higher.
- maxVertexInputAttributes, maxVertexInputBindings of 16 or higher.
- maxVertexInputBindingStride of 2048 or higher.
- maxVertexOutputComponents of 124 or higher.
- maxComputeWorkGroupSize[0,1] of 1024 or higher. and maxComputeWorkGroupSize[2] of 64 or higher.
- maxMemoryAllocationCount of 4096 or higher.
- maxBoundDescriptorSets of 4 or higher.
- maxPerStageDescriptorSamplers of 2Ki or higher.
- maxPerStageDescriptorUniformBuffers of 1 or higher.
- maxPerStageDescriptorStorageBuffers of 499'999 or higher.
- maxPerStageDescriptorSampledImages of 250k or higher.
- maxPerStageDescriptorStorageImages of 250k or higher.
- maxPerStageResources of 1M or higher.
- maxDescriptorSetSamplers of 2Ki or higher.
- maxDescriptorSetUniformBuffers of 1 or higher.
- maxDescriptorSetStorageBuffers of 499'999 or higher.
- maxDescriptorSetSampledImages of 250k or higher.
- maxDescriptorSetStorageImages of 250k or higher.
- viewportBoundsRange[0] <= -32768.
- viewportBoundsRange[1] >= 32767.
- nonCoherentAtomSize of 0 -> 256 and has to be base2.
- Requires UBO alignment of <=256.

### Extensions

#### Multi draw indirect count

maxDrawIndirectCount of 1Gi or higher indicates support for multi draw indirect count, otherwise only regular MDI can be used if supported.

#### Geometry shader

Geometry shaders aren't always supported. They're only supported if there's hardware support and:

- maxGeometryInputComponents of >=64.
- maxGeometryOutputComponents of >=128.
- maxGeometryOutputVertices of >=256.
- maxGeometryShaderInvocations of >=32.
- maxGeometryTotalOutputComponents of >=1024.

#### Tessellation shader

Tessellation shaders are mostly supported, but not always. It's also possible that the limits don't qualify it for enabling in OxC3. The limits require:

- maxTessellationControlPerVertexInputComponents, maxTessellationControlPerVertexOutputComponents, maxTessellationEvaluationInputComponents, maxTessellationEvaluationOutputComponents of 124 or higher.
- maxTessellationControlTotalOutputComponents of 4088 or higher.
- maxTessellationControlPerPatchOutputComponents of 120 or higher.
- maxTessellationGenerationLevel of 64 or higher.
- maxTessellationPatchSize of 32 or higher.

#### Raytracing

Raytracing requires VK_KHR_acceleration_structure, but also requires either VK_KHR_ray_tracing_pipeline or VK_KHR_ray_query to be set.

- Limits of:
- VK_KHR_acceleration_structure:
  - maxGeometryCount, maxInstanceCount >= U24_MAX (16777215).
  - maxPerStageDescriptorAccelerationStructures, maxPerStageDescriptorUpdateAfterBindAccelerationStructures, maxDescriptorSetAccelerationStructures, maxDescriptorSetUpdateAfterBindAccelerationStructures of >= 16.
  - maxPrimitiveCount of >= 0.5Gi.
- VK_KHR_ray_tracing_pipeline:
  - maxRayDispatchInvocationCount >= 1 Gi.
  - maxRayHitAttributeSize >= 32.
  - maxRayRecursionDepth >= 1.
  - maxShaderGroupStride >= 4096.
  - rayTraversalPrimitiveCulling should be enabled.

- Motion blur:
  - Both indirect and normal rays are allowed to be traced if RayIndirect is on. Otherwise RayIndirect has to be turned off.

#### Mesh shaders

Requires task shaders to be present and multiviewMeshShader, primitiveFragmentShadingRateMeshShader to be available too.

- Limits of:
  - maxMeshMultiviewViewCount >= 4.
  - maxMeshOutputComponents of >= 127.
  - maxMeshOutputLayers of >= 8.
  - maxMeshOutputMemorySize of >= 32Ki.
  - maxMeshOutputPrimitives and maxMeshOutputVertices of >= 256.
  - maxMeshPayloadAndOutputMemorySize of >= 48128.
  - maxMeshPayloadAndSharedMemorySize, maxMeshSharedMemorySize of >= 28672.
  - maxMeshWorkGroupCount[i], maxTaskWorkGroupCount[i] of >= U16_MAX.
  - maxMeshWorkGroupInvocations, maxMeshWorkGroupSize[i], maxTaskWorkGroupInvocations, maxTaskWorkGroupSize[i] of >=128.
  - maxMeshWorkGroupTotalCount of >=4Mi.
  - maxPreferredMeshWorkGroupInvocations of >=32.
  - maxPreferredTaskWorkGroupInvocations of >= 32.
  - maxTaskPayloadAndSharedMemorySize of >= 32Ki.
  - maxTaskPayloadSize of >= 16Ki.
  - maxTaskSharedMemorySize of >= 32Ki.
  - maxTaskWorkGroupTotalCount of >= 4 Mi.
  - meshOutputPerPrimitiveGranularity, meshOutputPerVertexGranularity of >= 32.
  - prefersCompactPrimitiveOutput of true.

#### Atomics

- AtomicI64 means atomic operations of a 64-bit int on a buffer. Images are optional.
- AtomicF32 means atomic exchange/read and atomic float add operations on a buffer. Images are optional.
- AtomicF64 ^ except 64-bit.

### Formats

A lossless format is defined as "supported" if it has the following flags in the format properties when using a tiled/non linear layout (check  "49.3. Required Format Support" of the Vulkan spec).

- VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT.
- VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
- VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT
- VK_FORMAT_FEATURE_BLIT_DST_BIT
- VK_FORMAT_FEATURE_BLIT_SRC_BIT
- VK_FORMAT_FEATURE_TRANSFER_DST_BIT
- VK_FORMAT_FEATURE_TRANSFER_SRC_BIT

The following lossless formats have to be supported for a valid OxC3 implementation:

- R8, R8s, R8u, R8i
- RG8, RG8s, RG8u, RG8i
- RGBA8, RGBA8s, RGBA8i, RGBA8u
- R16, R16s, R16u, R16i, R16f
- RG16, RG16s, RG16u, RG16i, RG16f
- RGBA16, RGBA16s, RGBA16u, RGBA16i, RGBA16f
- R32u, R32i, R32f
- RG32u, RG32i, RG32f
- RGBA32u, RGBA32i, RGBA32f
- BGRA8, BGR10A2

The following are required with VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT only:

- RGB32u, RGB32i, RGB32f

The following depth stencil formats are required (meaning VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT and VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT are set):

- D16, D32, D32S8.

Only the following are not always supported properly by the Vulkan implementation (so certain devices might not support them and thus they won't support OxC3). OxC3 will still enforce them to be present. Minimum spec was enforced here and it seems like all targeted systems support them.

- R8s, RG8s, RGBA8s
- R16, R16s
- RG16, RG16s
- RGBA16, RGBA16s
- D32, D32S8
- BGRA8 (write is optional, but is enforced by OxC3. The rest is required)

Lossy formats:

- The following always have to be supported (this is already forced by the Vulkan spec):
  - VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT
  - VK_FORMAT_FEATURE_BLIT_SRC_BIT
  - VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT
- All ASTC formats have to be supported if EGraphicsDataTypes_ASTC is on.
- All BCn formats have to be supported if EGraphicsDataTypes_BCn is on.

The following are optional: If they're not properly supported `GraphicsDeviceInfo_supportsFormat` or `GraphicsDeviceInfo_supportsDepthStencilFormat` will return false.

- All ASTC formats have to be supported if EGraphicsDataTypes_ASTC is on.
- All BCn formats have to be supported if EGraphicsDataTypes_BCn is on.

- RGB32u, RGB32i, RGB32f if they allow all other required bits (as mentioned at the start) besides VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT.
- S8, D24S8.

`GraphicsDeviceInfo_supportsFormatVertexAttribute` returns whether or not a format is applicable as a vertex attribute. The following are explicitly prohibited:

- Any format that uses compression (ASTC or BCn).

If raytracing is enabled, the following formats will be enabled for BLAS building:

- RG32f, RGBA32f, RG16f, RGBA16f, RG16s, RGBA16s

## List of DirectX12 requirements

- DirectX12 Feature level 12_1. 
- WDDM 2.7 and above.
- GPU:
  - Nvidia Maxwell 2nd gen and above.
  - AMD GCN 5 and above.
  - Intel Arc Alchemist and above.
  - Intel Gen 9 and above.

#### Default features in DirectX

Since Vulkan is more fragmented, the features are more split up. However in DirectX, the features supported by default are the following:

- EGraphicsFeatures_SubgroupArithmetic, EGraphicsFeatures_SubgroupShuffle. Wave intrinsics are all supported by default.
- EGraphicsFeatures_GeometryShader, EGraphicsFeatures_TessellationShader and EGraphicsFeatures_MultiDrawIndirectCount, EGraphicsFeatures_SupportsSwapchain, EGraphicsFeatures_SupportsLUID are enabled by default.
- EGraphicsFeatures_Raytracing, EGraphicsFeatures_RayPipeline, EGraphicsFeatures_RayQuery, EGraphicsFeatures_MeshShaders, EGraphicsFeatures_VariableRateShading are a part of DirectX12 Ultimate (Turing, RDNA2, Arc and up).
- If EGraphicsFeatures_Raytracing is enabled, so is EGraphicsFeatures_RayPipeline. The other RT extensions are optional.
- EDeviceDataTypes_BCn, EGraphicsDataTypes_I64, EGraphicsDataTypes_F64 are always set.

### List of Metal requirements

- Metal 3 (Apple7 tier). Argument buffers tier 2.
- Phone:
  - Apple iPhone 12 (A14, Apple7).
- Laptop:
  - Apple Macbook Pro 14 (M1 Pro, Apple7).

#### Default features in Metal

- EGraphicsFeatures_DirectRendering is never set.
- EGraphicsFeatures_TessellationShader is always set.
- EGraphicsDataTypes_ASTC is always set.
- EGraphicsDataTypes_BCn can be set as well on Mac/MacBook.
- EGraphicsDataTypes_AtomicF32, EGraphicsDataTypes_AtomicI64, EGraphicsDataTypes_F16, EGraphicsDataTypes_I64 are always set.

## TODO: List of WebGPU requirements

Since WebGPU is still expiremental, no limitations will be made to OxC3 to support it yet.

## Writes/reads to/from invalid ranges/unknown resources

Due to inconsistent behavior across APIs and vendors, this will be defined as undefined behavior. Please avoid this.