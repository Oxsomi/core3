# Code standard

## Project structure

The following project structure should be used:

```cmake
src/*.c 			# Source files
inc/*.h 			# Header files
tst/*.c 			# (Unit) Test files
docs/*.md			# Documentation markdown files
tools/*.h,*.c		# Tools used to build the project
*.h.in				# Configurable project files (prebuild)
CMakeLists.txt 		# CMake
README.md			# GitHub project description
LICENSE				# How the project may be used
.github				# Things like GitHub CI pipeline
.gitignore			# Things like builds files to ignore
```

the files should be lower_snake_case and of course src, inc, tst, docs, tools can all have subdirectories, named the same way.

## Forward declaring / includes

All types that aren't used in struct definitions should be forward declared instead of included. This reduces compile time and keeps all direct dependencies clear (all typedeffed structs should be at the top of the file). If these functionality is needed, the include should be added in the source file. Enums can be typedeffed as following: `typedef enum E<x> E<x>; ` and struct as `typedef struct <x> <x>;`.

Only big types (more than ~32 functions) should have their own include file. Smaller helper functions can be included in other files, but can also have their own include file if future functionality might push it over this limit, or if it has no other include file it could logically belong to.

## Types

Structs/types should be PascalCase, while enums should be EPascalCase (e.g. EMyValues).

Our standard types such as `Bool, U<x>, I<x>, F<x>` should be used instead of C types (e.g. U32 = unsigned 32 bit int, I32 = signed 32-bit int, F32 = IEEE754 single precision float). This is because the sizes of C datatypes aren't standardized; a long is 32-bit on windows but isn't on certain platforms. Even though Bool works the same on the various platforms, it is named to be in line with the rest of the project. This is of course also true for all other types, since they might have platform specific optimizations (such as custom hash functions per platform, etc.).

Structs should use the syntax `typedef struct T { } T;` to ensure clean code.

`void*` should be avoided if possible; but sometimes they are needed.

_malloc_ and _free_ shouldn't be used if the goal is a cross platform application. There might be a custom allocator that's required for the platform to run properly. And using the same allocator as our STL can make debugging tools (in the future) easier.

## Functions

No inline should be used, headers are only for documentation, types and function declarations. This is useful to provide a clean overview of the functionality. Since we use link time code generation, it should properly inline anyways. This also makes the file significantly easier to be parsed.

"Member functions" should be using ClassName_functionName. This makes it clear that it's either a helper function or a member function. Using direct structs `T t` instead of `const T *t` is recommended only if the struct isn't too big (~64+ bytes) to be copied, though the compiler might inline. Data passed that is const should be marked as such.

If a function has more than 4 parameters, consider if it should have a struct as initializer. If not, make sure to reserve a newline per parameter to make the function easier to read:

```c++
void myFunction(U8 x, U16 y, U32 z, U32 w);		// Correct

void myFunction(								// Incorrect, prefer a custom struct for U8,U16,U32,U64
	U8 x,
    U16 y,
    U32 z,
    U64 w,
    U8 x0,
    U16 y0,
    U32 z0,
    U64 w0
);

Error myFunction(								//Correct, easily readable.
	MyClass test1,
    U64 value,
    U32 otherValue,
    Allocator alloc,
    T *myOutput
);
```

If a function allocates; it should be a) suffixed with an x or b) include an allocator and a pointer to the result at the end of the parameters. An x suffix indicates that it uses the Platform_instance's allocator will be used to allocate. If a function can return an error, it should return it and output the output in the last argument(s).

Don't expose internal helper functions in the header file. Only use them in the source file. If this helper function is needed by multiple dependencies, document who is allowed to use it.

Early returns should be preferred over nested ifs. Functions should use the checks & execution patterns; validate first and then execute.

If a function is swapped out based on architecture or platform, it should be marked as `impl`. This indicates that if a new architecture or platform is implemented, it requires a new version of this function.

## Variables

Both struct and local variables should use lowerCamelCase. Constants/externs and statics should use UPPER_SNAKE_CASE (though if it belongs to a type (enum or struct) it can have the typename prefixed before the constant name.

## Error handling

Always validate inputs to a function if that function is going to be generic or you're dealing with user input.

Error is the standard struct that holds the error code, parameters and callstack. This allows us to sort of handle C++-like exceptions in a more secure way; where it's very clear that a function can return an error and what type of error it is. As well as providing an (extended) function for printing the function and stacktrace. It also allows easy "rethrowing" by just returning the error and not messing with the stack trace. As well as the ability to ignore an error if it's not important (ex. a file check might want to figure out if Error_notFound is returned and might want to handle that in a custom way).

To handle cleanup code, use the following:

```c
	... //Init default structs and pointers to NULL or empty struct. Init Error err = Error_none()

	gotoIfError(fail, myFunctionThatReturnsError(x, &myResult));

	... //Other code

	... //We are a the end of our function


    goto clean;

fail:
	MyResult_freex(&myResult);
clean:
	MyResult_freex(&myOtherResultThatNeedsCleaning);
	return err;
```

Keep in mind that you might want to keep myResult if you had a successful function call, so you might have to skip a portion of the label's cleanup. If you don't return anything that allocates, feel free to simplify this to only a clean, without a goto (only fallthrough from end of function). If you're calling another function that should stop the execution of this one, you should still goto clean. You store the function's Error (if present) in the err (Error struct's name) and goto the cleanup part of your function. If your function doesn't allocate; using this is not recommended, as it adds unnecessary complexity.

Feel free to cleanup the variables that you don't need at any point throughout the function. These structs/types should be automatically reset on free (to prevent use after free), so freeing the same variable (provided it isn't restored to an invalid object) is perfectly fine. If an error can occur between the free and create, it should be freed by the clean label.

If an error isn't returned but a function will fail, document clearly what value it returns on fail.

## Formatting

Lines should be a maximum of 128.

Curly braces align to the end of a line.

Pointers align to the right (unfortunately C declares they belong to the variable to the right, not the type itself). The only exception being casts, where it does belong to the type.

There should be spaces between operators. E.g. `x + y` and not `x+y`.

Use brackets around operators that aren't in PEMDAS such as bitwise operators.

Switch cases are indented.

Using tabs instead of space; spaces are only for formatting if the alignment is slighly off (a few characters). Line width is 4 by default, but could be configured manually.

Using LF instead of CRLF.

## Defines & Macros

Constants as defines should only be used if required; e.g. An array is typedeffed and needs it to be constexpr. But since C doesn't have constexpr, a define should be used. If it's just a constant that's not used for that purpose, it should just be defined as extern.

Prefixing with _ is reserved for defines and macros.

Defines should use `_UPPER_CASE` (e.g. `_SIMD`) while macros can use `_<function>`. For example in error you have `_Error_base` which instantiates the base Error struct for different types. These macros follow naming style of functions, except they're prefixed with `_`. It can be used to create multiple functions more easily (e.g. vector swizzle), create enums that use multiple flags/values stored in the enum's value (`_EType`) or to create a function body multiple times that'd take a lot of copy pasting for no reason.

Functions should be preferred over macros, unless macros greatly reduce copy + paste and even then, a function should be preferred when possible. Sometimes this is not possible though.

Only impl defines can use the lower_snake_casing. This is because it's kind of seen as a function keyword. Another exception is if swapping out a function that is defined on both platforms, but one platform uses a non standard name for the function (Windows's unix-like file function calls) or if a define needs a certain formatting because of a library/api.

Macros should try to align the `\` required to do multi-line macros on the same column.

## Comments

If using code snippets from other places, make sure to reference the link to ensure the original source can be compared or an explanation can be found if needed in the future and to provide credits to the original author.

`//` should be preferred when dealing with small comments, unless a large section is commented out or for documentation; in that case `/*` and `*/` should be used. Another reason for `/**/` is if the current formatting doesn't support it or if in a macro definition (since these don't support normal comments).

## Documentation

### Sections

To add an an extensive section to the documentation, you can use the following syntax (example):

```C
/*
	@entry: FileSystem
	@authors: [ Nielsbishere ]
	@types: [ FileType, FileAccess, FileInfo, FileCallback, File ]
	<myMarkDown>
*/
```

This would add an entry for FileSystem. You could also add functions using `@functions: [ ]`, though if you include types that one automatically links to all functions that are available. To add subsections (and subsections for those), you could use `FileSystem/<subSectionName>`.

This assumes indentation of 1 tab and will remove that on every line, so your markdown has to be indented by 1 tab.

This kind of section is for extra details about a combination of classes and functions. Single functions or types can also be documented of course.

### Functions

```C
/*
	@short: Removes a file from the {FileSystem}.
	@params[loc]: File path using {FileSystem/Oxsomi notation}.
	@params[maxTimeout]: How long the file request can wait for a lock before it times out.
	@returns: [ Error ]
	@access: [ Any ]
	@error: [ InvalidParameter, Unauthorized, ... ]
	<myMarkDown (if a long description is needed)>
*/
Error File_remove(String loc, Ns maxTimeout);
```

@error indicates what errors are returned by this function. If it calls other functions that can return errors, it should use `...` to indicate more error types.

@access indicates who is allowed to access this function. There's nothing to enforce this in C, but it's a useful to indicate to the programmer. Keyword "Platform" could mean that only the Windows/Linux runtime is allowed to access this function. If this is absent or uses "Any" then it'll assume anybody can use it for any purpose (and it'll be missing from the documentation).

`{}` indicates a link to a documentation section, while `[]` links to a type. Keep in mind that sections can contain spaces and types/functions can't. E.g. `[File]` links to the File class, while `{File}` would link to an article named File (if it exists).

@returns can include multiple types. `[ Error (main, return type doesn't have name but can include it),  U64 result ]` an example would be File read: `[ Error, Buffer output ]`. This would require a `Buffer *output` parameter in the function. @params[output] would be used for the description there.

### Types & Static variable

```c
/*
	@short: <Short description>
	@access: [ Any ]
	<myMarkDown (if a long description is needed)>
*/
```

*See Functions for more info on the annotation types.*

### Struct member

```c
/*
	@short: <Short description>
	@access: [ PrivateSet ]
	<myMarkDown (if a long description is needed)>
*/
```

Access here is extended from *Function's annotation types* and contains `PrivateSet` and `Private`. PrivateSet means that changing the variable should only be done by the type itself, while Private means that it's only for usage of the struct's functions.

### Enums

Enum fields use the same as struct members. An enum can be marked as a flag by using @usage: Flag.

Enums don't need comments per enum value, but could if the enum value isn't self explanatory.