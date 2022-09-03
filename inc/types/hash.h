#pragma once
#include "types.h"

//Simple FNV hashes (Fowler-Noll-Vo), NOT INTENDED FOR CRYPTOGRAPHY!

inline u64 FNVHash_create() { return 0xcbf29ce484222325; }
inline u64 FNVHash_apply(u64 h, u64 dat) { return (h ^ dat) * 0x00000100000001B3; }