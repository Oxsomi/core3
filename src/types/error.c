#include "types/error.h"

struct Error Error_base(
	enum GenericError err, u32 subId, 
	u32 paramId, u32 paramSubId, 
	u64 paramValue0, u64 paramValue1
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