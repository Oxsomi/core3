#include "types/types.h"
#include <ctype.h>

inline C8 C8_toLower(C8 c) {
	return (C8) tolower(c);
}

inline C8 C8_toUpper(C8 c) {
	return (C8) toupper(c);
}
