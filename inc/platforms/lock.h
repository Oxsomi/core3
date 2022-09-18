#pragma once
#include "types/types.h"

struct Lock {
	void *data;
};

impl struct Error Lock_create(struct Lock *res);
impl struct Error Lock_free(struct Lock *res);
impl bool Lock_lock(ns maxTime);
impl bool Lock_unlock();
