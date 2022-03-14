#pragma once
#include <assert.h>

#ifndef NDEBUG
	#define _ENABLE_ASSERT
#endif

#ifdef _ENABLE_ASSERT
	#define ocAssert(err, ...) assert(err " at " __FILE__ && __VA_ARGS__)
#else
	#define ocAssert(err, ...)
#endif