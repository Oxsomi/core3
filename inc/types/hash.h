#pragma once
#include "types.h"

//Simple FNV hashes (Fowler-Noll-Vo), NOT INTENDED FOR CRYPTOGRAPHY!

inline U64 FNVHash_create() { return 0xcbf29ce484222325; }
inline U64 FNVHash_apply(U64 h, U64 dat) { return (h ^ dat) * 0x00000100000001B3; }
