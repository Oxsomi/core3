#include "types/buffer.h"
#include "types/time.h"

#ifdef _WIN32

	#define WIN32_LEAN_AND_MEAN
	#include <Windows.h>
	#include <bcrypt.h>

	Bool Buffer_csprng(Buffer target) {

		if(!Buffer_length(target) || Buffer_isConstRef(target))
			return false;

		NTSTATUS stat = BCryptGenRandom(
			BCRYPT_RNG_ALG_HANDLE, 
			target.ptr, 
			(ULONG) Buffer_length(target),
			BCRYPT_USE_SYSTEM_PREFERRED_RNG
		);

		return BCRYPT_SUCCESS(stat);
	}

#else
	#error Other platform random number generation not supported yet.
#endif
