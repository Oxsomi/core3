# oiPC (Oxsomi Pipeline Cache)

*The oiPC format is an [oiXX format](oiXX.md), as such it inherits the properties from that such as endianness. It doesn't support compression or encryption: it only holds what a driver compiled, which the next run can always compile again.*

oiPC is what `GraphicsDeviceRef_getPipelineCache` returns and `GraphicsDeviceRef_setPipelineCache` takes (see [graphics_api.md](../graphics_api.md)): the pipelines a device compiled, so a later run can skip compiling them. OxC3 never reads or writes the file itself; the application stores it, one per graphics API and device, since the driver data is only valid for the API, device and driver version that wrote it.

## File format spec

```c
typedef struct PipelineCacheHeader {	//Aligned to 8 bytes, 24 bytes

	U32 magicNumber;			//oiPC (0x4350696F)

	U8 version;					//10 (1.0)
	U8 flags;					//None yet
	U8 api;						//EGraphicsApi that wrote it
	U8 padding;

	U32 sizeCount;				//(key, bytes) pairs following the header
	U32 padding1;

	U64 driverBytes;			//Size of the driver's data following the pairs

} PipelineCacheHeader;

//PipelineCacheHeader header;
//U64 sizes[sizeCount][2];		//Sorted by key: { key, bytes }
//U8 driverData[driverBytes];
```

- **key**: a hash of everything a backend hands the driver to build a pipeline (shaders, fixed function state, pipeline layout).
- **bytes**: the driver's code size of that pipeline as it was measured when built, so a pipeline loaded from the cache still reports a known size in the memory stats.
- **driverData**: the API's own blob. Vulkan: `vkGetPipelineCacheData` of the device's pipeline cache, starting with a `VkPipelineCacheHeaderVersionOne`. D3D12: `ID3D12PipelineLibrary::Serialize`, which holds compute and graphics PSOs only (raytracing state objects can't be stored).

The file has to be exactly `sizeof(PipelineCacheHeader) + sizeCount * 16 + driverBytes` long. A file with another magic number, version or API, or driver data from another device or driver version, is ignored rather than refused: the device then starts with an empty cache.
