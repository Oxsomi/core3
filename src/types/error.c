#include "types/error.h"

struct Error Error_base(
	enum GenericError err, U32 subId, 
	U32 paramId, U32 paramSubId, 
	U64 paramValue0, U64 paramValue1
) {
	return (struct Error) {
		.genericError = err,
		.errorSubId = subId,
		.paramId = paramId,
		.paramValue0 = paramValue0,
		.paramValue1 = paramValue1
	};
}

struct Error Error_none() {
	return (struct Error) { 0 };
}