# Changelog

### TODO: OxC3 v0.3 "Performance"

### WIP: OxC3 v0.2 "Graphics"

- Fixed: every RADV (Mesa AMD) device failed to create. EGraphicsFeatures_ComputeDeriv was granted
  when EITHER compute-derivative group mode was supported, while device creation requested BOTH,
  so vkCreateDevice returned VK_ERROR_FEATURE_NOT_PRESENT and the device was unusable entirely.
  The grant now requires both modes, matching what the request has always asked for.
- Fixed: EFloatType_convert dropped the mantissa carry when WIDENING the exponent, halving any
  value whose mantissa rounded up (F16 -> BF16 turned 7.997 into 4; FP24 -> TF19 turned 31.999
  into 16). The equal-exponent and truncating paths already applied it.
- EFloatType gained optional sign bits (EFloatType_makeUnsigned) plus EFloatType_UF21, an unsigned
  21-bit float that packs three to a U64 - for storage a signed format wastes a bit on.
- New Barycentrics shader extension (SV_Barycentrics / SPV_KHR_fragment_shader_barycentric),
  exposed as EGraphicsFeatures_Barycentrics and gated per device, since RDNA1 lacks it.
- New @pack.hlsli shader include: F16/F21/RGB9E5/normal packing helpers shared by shaders that must
  fit values through a ray payload, a G-buffer texel or an accumulator.
- Raytracing payload size is no longer capped at the hit-attribute's 32 bytes; the limit is now the
  oiSH format's own U8 ceiling. Hit attributes stay at 32 (a D3D12 hard limit).
- Hit shaders that never read their attribute struct are no longer rejected: the reflected
  attribute size falls back to 8 (triangle barycentrics) instead of erroring.
- Linux/Wayland now invokes onCursorMove for pointer motion and touch drags, matching Windows.
- Support for graphics APIs such as Vulkan and Direct3D12.
- BufferLayout class for managing data layout such as struct arrays.
- Minor fixes such as file names that start with spaces.
- 16-bit float (F16) support for casting from 32-bit or 64-bit float.
- Support for custom other types of IEEE754 float formats such as Nvidias', AMDs' or Pixars' formats.
- OxC3 tool now has help, info and profile (for performance checks) functions.

### OxC3 v0.1 "Platform"

- Basic types; Buffer, CharString, (F/I)32x(4,2) ala Vector, List, QuatF32/F64, RefPtr, Transform, <U/I><8/16/32/64> and F<32/64>.
- Other types such as CDFList.
- Basic utils; math, time, safe type casting, allocation (allocator & allocation buffer).
- Virtual and local files (including exe icon).
- Virtual and physical window management.
- Input devices; Keyboard and mouse.
- Threading; including lock mechanism.
- Logging / error reporting (including call stacks).
- Custom archive and datalist/stringlist formats.
- Archive implementation.
- Encryption, cryptographically secure random and hashing.
- BMP file writing.
- Windows (only) support but possibility to add Linux + Android in the future.
- Basic OxC3 tool for file conversions, file data/header inspection, encryption and hashing.