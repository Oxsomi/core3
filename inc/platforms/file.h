#pragma once
#include "types/types.h"
#include "types/string.h"

//There are two types of files; virtual and local.
//Virtual are embedded into the binary, very nice for easy portable installation and harder to modify for avg users.
//	Writable virtual files can only be in the //access directory.
//	These are just links to existing local files, but are made accessible by the platform for access by the app.
//	As much, naming a folder in the root "access" is disallowed for virtual files.
//Local are in the current working directory.
//OxC3 doesn't support reading from other directories; this is to prevent unwanted access to other files.
//Importing/exporting other files in other locations will have to be done through 
// the //access folder using the platform's explorer (TBD).
//
//Virtual files are prefixed with // even though that can be valid
//E.g. //test.json refers to test.json embedded in the binary
//	   while test.json, ./test.json or .//test.json refers to test.json in the local file
//
//Backslashes are automatically resolved to forward slash

Error File_resolve(String loc, Bool *isVirtual, String *result);

Error File_writeLocal(Buffer buf, String loc);
Error File_readLocal(String loc, Buffer *output);

impl Error File_writeVirtual(Buffer buf, String loc);
impl Error File_readVirtual(String loc, Buffer *output);

inline Error File_read(String loc, Buffer *output) {

	Bool isVirtual = 
		String_startsWithString(loc, String_createConstRefUnsafe("//"), EStringCase_Insensitive);

	return isVirtual ? File_readVirtual(loc, output) : File_readLocal(loc, output);
}

inline Error File_write(Buffer buf, String loc) {

	Bool isVirtual = 
		String_startsWithString(loc, String_createConstRefUnsafe("//"), EStringCase_Insensitive);

	return isVirtual ? File_writeVirtual(buf, loc) : File_writeLocal(buf, loc);
}

//TODO: make it more like a DirectStorage-like api
