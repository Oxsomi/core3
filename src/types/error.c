#include "types/error.h"
#include "types/string.h"

const C8 *EGenericError_toString[] = {
	"None",
	"Out of memory",
	"Out of bounds",
	"Null pointer",
	"Unauthorized",
	"Not found",
	"Divide by zero",
	"Overflow",
	"Underflow",
	"NaN",
	"Invalid enum",
	"Invalid parameter",
	"Invalid operator",
	"Invalid cast",
	"Invalid state",
	"Rate limit",
	"Loop limit",
	"Already defined",
	"Unsupported operation",
	"Timed out",
	"Const data",
	"Platform error"
};

const EErrorParamValues EGenericError_hasParamValues[] = {
	EErrorParamValues_None,
	EErrorParamValues_None,
	EErrorParamValues_V0_1,
	EErrorParamValues_None,
	EErrorParamValues_None,
	EErrorParamValues_None,
	EErrorParamValues_V0_1,
	EErrorParamValues_V0_1,
	EErrorParamValues_V0_1,
	EErrorParamValues_None,
	EErrorParamValues_None,
	EErrorParamValues_None,
	EErrorParamValues_None,
	EErrorParamValues_None,
	EErrorParamValues_None,
	EErrorParamValues_V1,
	EErrorParamValues_V1,
	EErrorParamValues_None,
	EErrorParamValues_None,
	EErrorParamValues_V1,
	EErrorParamValues_None,
	EErrorParamValues_None
};
