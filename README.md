# Core v3
![](https://github.com/Oxsomi/core3/workflows/C%2FC++%20CI/badge.svg)

Core v3 is the successor to core v2 and v1. Focused more on being minimal abstraction compared to the predecessors by using C17 instead of C++20. Written so it can be wrapped with other languages (bindings) or even a VM in the future. Could also provide a C++20 layer for easier usage, such as operator overloads.

For the time being it is quite minimal, with only types, though it could support more in the future; such as platform abstraction, graphics abstraction, file formats and possibly networking.

## Requirements

- CMake >=3.13

## Installing Core v3

```bash
git clone --recurse-submodules -j8 https://github.com/Oxsomi/core3
```
