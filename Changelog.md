# Changelog

### TODO: OxC3 v0.3 "Performance"

### WIP: OxC3 v0.2 "Graphics"

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