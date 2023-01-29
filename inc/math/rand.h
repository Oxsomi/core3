#pragma once
#include "vec.h"

//These are Psuedo RNG (PRNG), don't use these for critical uses!
//More info:
//	https://cheatsheetseries.owasp.org/cheatsheets/Cryptographic_Storage_Cheat_Sheet.html#secure-random-number-generation

U32 Random_seed(U16 x, U16 y, U16 w, U32 val1);

//Generate 'random' value [0, 1>
F32 Random_sample(U32 *seed);
F32x4 Random_sample2(U32 *seed);
