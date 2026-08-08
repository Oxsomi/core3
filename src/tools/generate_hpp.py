# OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
# Copyright (C) 2023 - 2026 Oxsomi / Nielsbishere (Niels Brunekreef)
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see https://github.com/Oxsomi/core3/blob/main/LICENSE.
# Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.
# To prevent this a separate license will have to be requested at contact@osomi.net for a premium;
# This is called dual licensing.

"""Generate barebones oxc:: C++ wrappers for C headers that follow OxC3's naming convention.

OxC3's C API is regular enough that most of a C++ wrapper is mechanical:

    Bool  Foo_bar(Foo *self, U64 x, const Allocator *alloc, Error *e_rr);
      ->  [[nodiscard]] c::Bool bar(c::U64 x, c::Error *e_rr = nullptr) noexcept;

That mapping is what this script automates. It is deliberately *not* a C parser: it recognises the
shapes this codebase actually uses and refuses everything else, listing what it skipped so a human can
decide. The hand-written wrappers (string.hpp, ref_ptr.hpp, log.hpp, vec.hpp, list.hpp, job_queue.hpp)
exist precisely because those types need judgement - ownership models, operator overloads, views - that
no generator should be guessing at.

HOW MUCH OF THE API THIS CAN ACTUALLY COVER
-------------------------------------------
Measured against rt_core/tst (168 distinct OxC3 symbols in real consumer code), and by generating
wrappers for a spread of headers and compiling them:

  ~43%  *Ref_ handle types (CommandListRef, GraphicsDeviceRef, TextureRef, ...). These are
        `typedef RefPtr XRef`, so there is no struct to hold by value - generating a class for one
        doesn't even compile ("field has incomplete type"). They need oxc::RefPtr<T>, by hand.
  ~21%  value/math types (F32x4, I32x2, F32, F64). A generated class would take the value as its
        first *parameter* rather than being it; see vec.hpp for what these should look like.
   ~9%  namespaces (Platform, File, Time, Error) - handled, via staticOnly.
  the rest, the shape this tool is built for: a struct + an allocator + a free.

Two further limits worth knowing before extending the spec:

  * Wrapping works by including the C header inside `namespace oxc::c`. That only holds for headers
    whose transitive system-header closure is small. platforms/file.h drags in pthread/time and fails
    outright; formats/oiCA pulls <stdlib.h> in and loses ldiv/lldiv to the namespace.
  * Types whose functions span several headers (CharString has six, CAFile four) need `alsoParse`,
    and every parsed header must also be in extraCIncludes or the emitted calls won't resolve.
  * The config-struct idiom - `AllocationBuffer_create(const AllocationBufferCreate*, ...)`, used to
    keep long signatures readable - hides the real self parameter, so those come out as *static*
    members of an otherwise RAII class. The result compiles and is worse than a failure: the
    destructor frees a `self` the static factories never filled. AllocationBuffer was dropped from
    the spec for exactly this. Any type whose creation goes through a config struct needs a human.

WHY THE SPEC IS SHORT
---------------------
Not because the classifier rejects things. Running classify() over every header in include/ and taking
the dominant type prefix in each, the skip count is zero almost everywhere - GenericList (69 accepted),
F32x2 (69), CLI (68), CharString (64 in one header alone), CommandListRef (44) all pass cleanly. The
tool would happily emit a wrapper for any of them.

It emits a *compiling* wrapper for far fewer, and a *good* one for fewer still. Accepting a declaration
only means the signature is shaped right; it says nothing about whether the resulting class models the
type. The three filters above (handles, arithmetic types, config-struct creation) are what actually
decide, and none of them are visible to the classifier:

  * CommandListRef and friends classify as 100% static members, because `RefPtr` isn't the spec type,
    so every function looks like a free function. It compiles and it is useless.
  * BigInt creates through BigIntCreate, the config-struct idiom - the same trap AllocationBuffer fell
    into, and the reason it isn't in the spec despite 32 clean declarations.
  * F32x2/I32x2 would generate methods without operators, which is the opposite of what vec4f.hpp does
    for their 4-wide siblings. Generating them would make the math layer inconsistent, not larger.

So the short spec is a judgement about which types are *worth* generating, not a capability limit.
Growing it means picking off leaf types by hand and checking the output, which is what was done for
EFloatType and Error. The lever that would actually widen coverage is oxc::RefPtr<T>, since handles are
the single biggest slice of real consumer code.

So this is a labour saver for the leaf/POD corner of the API, not a route to a C++ binding for OxC3.
Anything reference counted, arithmetic, or platform-touching is hand-written on purpose.

Usage:

    python src/tools/generate_hpp.py                 # write every header in the spec
    python src/tools/generate_hpp.py --check         # regenerate to memory and diff; non-zero if stale
    python src/tools/generate_hpp.py -spec x.json    # use a different spec

The spec (src/tools/generate_hpp.json) lists one entry per generated header; see that file for the
fields. Generated files carry a "do not edit" banner and are meant to be committed, so a reader never
has to run the generator to see what the C++ surface is.
"""

import argparse
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.realpath(__file__))))

LICENSE = """/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2026 Oxsomi / Nielsbishere (Niels Brunekreef)
*
*  This program is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program. If not, see https://github.com/Oxsomi/core3/blob/main/LICENSE.
*  Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.
*  To prevent this a separate license will have to be requested at contact@osomi.net for a premium;
*  This is called dual licensing.
*/"""

# Types that are always passed straight through; everything else gets an oxc::c:: qualification.
PRIMITIVES = { "void", "bool", "char", "int", "unsigned", "float", "double", "size_t", "long", "short" }

# Must match MAX_LINE_LENGTH in check_style.py, which also scans the generated .hpp files
MAX_LINE = 128

# ---------------------------------------------------------------------------------------------------
# Parsing
# ---------------------------------------------------------------------------------------------------

# Matches `[static inline] <ret> <Type>_<name>(<args>)` up to the ( ; deliberately anchored at column 0
# or one tab so it only sees declarations, not calls inside a function body.
DECL = re.compile(
	r"^(?P<storage>static\s+inline\s+|impl\s+)?"
	r"(?P<ret>[A-Za-z_][A-Za-z0-9_]*(?:\s*\*)*)\s+"
	r"(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*"
	r"\((?P<args>[^)]*)\)",
	re.MULTILINE
)

class Param:

	def __init__(self, raw):

		self.raw = raw.strip()
		self.isConst = self.raw.startswith("const ")
		self.pointer = self.raw.count("*")

		# Split the trailing identifier off; arrays (`U32 key[8]`) keep their suffix on the name
		match = re.match(r"^(?P<type>.+?)(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*(?P<array>\[[^\]]*\])?$", self.raw)

		if not match:
			self.type = self.raw
			self.name = ""
			self.array = ""
			return

		self.type = match.group("type").strip()
		self.name = match.group("name")
		self.array = match.group("array") or ""

	def baseType(self):
		"""The type with const/* stripped, i.e. what to compare against the wrapped type name."""
		return self.type.replace("const", "").replace("*", "").strip()

class Func:

	def __init__(self, ret, name, params, storage):
		self.ret = ret
		self.name = name
		self.params = params
		self.storage = storage

def qualify(typeName):
	"""Qualify a C type for use inside namespace oxc (where the C header lives in oxc::c)."""

	out = typeName.strip()
	stars = ""

	while out.endswith("*"):
		stars += "*"
		out = out[:-1].strip()

	isConst = out.startswith("const ")

	if isConst:
		out = out[len("const "):].strip()

	if out in PRIMITIVES:
		qualified = out
	else:
		qualified = "c::" + out

	# Stars stay attached to the name (`const c::Date *date`), matching the C headers' own style
	return ("const " if isConst else "") + qualified + ((" " + stars) if stars else " ")

def parseHeader(path, typeName):
	"""Pull every `TypeName_xxx(...)` declaration out of a C header."""

	with open(path, encoding="utf-8") as f:
		src = f.read()

	# Strip comments and preprocessor lines so they can't be mistaken for declarations
	src = re.sub(r"/\*.*?\*/", "", src, flags=re.DOTALL)
	src = re.sub(r"//[^\n]*", "", src)
	src = re.sub(r"^\s*#[^\n]*(?:\\\n[^\n]*)*", "", src, flags=re.MULTILINE)

	funcs = []

	for m in DECL.finditer(src):

		name = m.group("name")

		if not name.startswith(typeName + "_"):
			continue

		argsRaw = m.group("args").strip()

		if argsRaw in ("", "void"):
			params = []
		else:
			params = [ Param(a) for a in argsRaw.split(",") ]

		funcs.append(Func(m.group("ret").strip(), name, params, m.group("storage")))

	return funcs

# ---------------------------------------------------------------------------------------------------
# Classification
# ---------------------------------------------------------------------------------------------------

class Skipped:
	def __init__(self, name, why):
		self.name = name
		self.why = why

def classify(func, spec):
	"""Decide how a C function maps to a member. Returns (kind, skipReason)."""

	typeName = spec["type"]
	short = func.name[len(typeName) + 1:]

	if not short:
		return None, "empty method name"

	if short in spec.get("skip", []):
		return None, None                                  # explicitly excluded, not a failure

	if any(p.raw == "..." for p in func.params):
		return None, "variadic (forward it by hand, see log.hpp)"

	if any("(" in p.raw for p in func.params):
		return None, "function pointer parameter"

	if any(not p.name for p in func.params):
		return None, "unnamed parameter"

	if spec.get("staticOnly"):
		return "static", None

	# Constructors: `X_createFoo(..., const Allocator*, X *result, Error *e_rr)`.
	# The result out-param is what makes them unambiguous; they become a method that resets *this and fills it in.
	if short.startswith("create") and len(func.params) >= 2:

		last = func.params[-1]
		result = func.params[-2]

		if last.baseType() == "Error" and last.pointer and result.baseType() == typeName and result.pointer:
			return "constructor", None

	# Self is the leading parameter of the wrapped type, by value or by pointer
	if func.params and func.params[0].baseType() == typeName:
		return "method", None

	return "static", None

def emitParams(params, spec, skipSelf, allocName, resultIndex=-1, bindAlloc=True):
	"""C++ parameter list plus the argument list to forward to C.

	skipSelf binds the leading parameter to the wrapped object, resultIndex binds an out-parameter to it
	(constructors); both disappear from the C++ signature.
	"""

	decl = []
	call = []

	for i, p in enumerate(params):

		if skipSelf and i == 0:
			call.append("&self" if p.pointer else "self")
			continue

		if i == resultIndex:
			call.append("&self")
			continue

		# An Allocator the wrapper already owns is bound rather than asked for again.
		# Only for members: a static method has no object to take it from, so it keeps asking for one.
		if allocName and bindAlloc and p.baseType() == "Allocator":
			call.append("alloc" if p.pointer else "*alloc")
			continue

		# A trailing Error* becomes an optional out-parameter, matching the hand written wrappers
		if p.baseType() == "Error" and p.pointer and i == len(params) - 1:
			decl.append("c::Error *e_rr = nullptr")
			call.append("e_rr")
			continue

		decl.append(f"{qualify(p.type)}{p.name}{p.array}")
		call.append(p.name)

	return ", ".join(decl), ", ".join(call)

def emitClass(spec, funcs):

	typeName = spec["type"]
	cppName = spec.get("name", typeName)
	allocName = spec.get("allocator")
	freeFunc = spec.get("free")

	lines = []
	skipped = []

	add = lines.append

	add(f"\t//Generated wrapper around the C {typeName}.")

	if spec.get("doc"):
		for docLine in spec["doc"]:
			add(f"\t//{docLine}")

	# A static-only type has no instance state in C, so it becomes a namespace rather than a class nobody can construct.
	# Time::now() reads the same either way, but a namespace is honest about there being no object,
	# stays open for extension, and avoids creating an oxc::Error that isn't a c::Error.

	if spec.get("staticOnly"):
		add(f"\tnamespace {cppName} {{")
		add("")
	else:
		add(f"\tclass {cppName} {{")
		add("")
		add(f"\t\tc::{typeName} self;")

		if allocName:
			add("\t\tconst c::Allocator *alloc;")

		add("")
		add("\tpublic:")
		add("")

	# Lifetime

	if spec.get("staticOnly"):
		lines.pop()                                            # no lifetime members, so drop the blank line
	elif allocName and freeFunc:
		add(f"\t\t{cppName}(const c::Allocator &alloc) noexcept : self{{}}, alloc(&alloc) {{}}")
		add(f"\t\t~{cppName}() noexcept {{ release(); }}")
		add("")
		add(f"\t\t{cppName}(const {cppName}&) = delete;")
		add(f"\t\t{cppName} &operator=(const {cppName}&) = delete;")
		add("")
		add(f"\t\t{cppName}({cppName} &&other) noexcept : self(other.self), alloc(other.alloc) {{")
		add(f"\t\t\tother.self = c::{typeName}{{}};")
		add("\t\t}")
		add("")
		add(f"\t\t{cppName} &operator=({cppName} &&other) noexcept {{")
		add("")
		add("\t\t\tif(this == &other)")
		add("\t\t\t\treturn *this;")
		add("")
		add("\t\t\trelease();")
		add("\t\t\tself = other.self;")
		add("\t\t\talloc = other.alloc;")
		add(f"\t\t\tother.self = c::{typeName}{{}};")
		add("\t\t\treturn *this;")
		add("\t\t}")
		add("")
		add("\t\tvoid release() noexcept {")
		add(f"\t\t\tc::{freeFunc}(&self, alloc);")
		add(f"\t\t\tself = c::{typeName}{{}};")
		add("\t\t}")
	else:
		add(f"\t\t{cppName}() noexcept : self{{}} {{}}")
		add(f"\t\t{cppName}(const c::{typeName} &raw) noexcept : self(raw) {{}}")

	add("")

	# Methods

	for func in funcs:

		kind, why = classify(func, spec)

		if kind is None:
			if why:
				skipped.append(Skipped(func.name, why))
			continue

		short = func.name[len(typeName) + 1:]
		method = short[0].lower() + short[1:]

		isMethod = kind == "method"
		isCtor = kind == "constructor"
		selfConst = isMethod and (func.params[0].isConst or not func.params[0].pointer)

		decl, call = emitParams(
			func.params, spec, isMethod, allocName, len(func.params) - 2 if isCtor else -1,
			bindAlloc=isMethod or isCtor
		)

		ret = qualify(func.ret)
		nodiscard = "" if func.ret == "void" else "[[nodiscard]] "

		# At namespace scope `static` would mean internal linkage rather than "no this", so it becomes inline
		isNamespace = bool(spec.get("staticOnly"))
		static = "" if (isMethod or isCtor or isNamespace) else "static "
		inlineKw = "inline " if isNamespace else ""
		const = " const" if (isMethod and selfConst) else ""
		body = "" if func.ret == "void" else "return "

		# qualify() already leaves a trailing space (pointers keep their star on the name).
		# Signatures past the style limit break the parameter list onto its own line, the way the
		# hand-written headers do, so generated output passes check_style like everything else.

		signature = f"\t\t{nodiscard}{inlineKw}{static}{ret}{method}({decl}){const} noexcept {{"

		if len(signature) > MAX_LINE:
			add(f"\t\t{nodiscard}{inlineKw}{static}{ret}{method}(")
			add(f"\t\t\t{decl}")
			add(f"\t\t){const} noexcept {{")
		else:
			add(signature)

		# A constructor overwrites the object, so whatever was in it has to go first
		if isCtor and allocName and freeFunc:
			add("\t\t\trelease();")

		call = f"\t\t\t{body}c::{func.name}({call});"

		if len(call) > MAX_LINE:
			inner = call.strip()
			add(f"\t\t\t{body}c::{func.name}(")
			add("\t\t\t\t" + inner[inner.index("(") + 1:-2])
			add("\t\t\t);")
		else:
			add(call)

		add("\t\t}")
		add("")

	# Interop; a static-only wrapper has no instance to hand over

	if not spec.get("staticOnly"):
		add("\t\t//Interop with C")
		add("")
		add(f"\t\t[[nodiscard]] c::{typeName} &handle() noexcept {{ return self; }}")
		add(f"\t\t[[nodiscard]] const c::{typeName} &handle() const noexcept {{ return self; }}")

	if allocName and freeFunc:
		add("")
		add("\t\t//Caller takes ownership; must free with the same allocator")
		add(f"\t\t[[nodiscard]] c::{typeName} steal() noexcept {{")
		add(f"\t\t\tc::{typeName} tmp = self;")
		add(f"\t\t\tself = c::{typeName}{{}};")
		add("\t\t\treturn tmp;")
		add("\t\t}")

	add("\t}" if spec.get("staticOnly") else "\t};")

	return lines, skipped

def generate(spec):

	# A type's functions are routinely spread over several headers (CharString has six, CAFile four),
	# so a class is assembled from all of them; the first is the one that gets included, the rest come along through it.
	# Duplicate declarations across headers collapse to the first one seen.

	headers = [ spec["header"] ] + spec.get("alsoParse", [])
	funcs = []
	seen = set()

	for header in headers:
		for func in parseHeader(os.path.join(ROOT, header), spec["type"]):
			if func.name not in seen:
				seen.add(func.name)
				funcs.append(func)

	body, skipped = emitClass(spec, funcs)

	out = [ LICENSE, "" ]
	out.append("//" + spec["output"].replace("include/", "", 1))
	out.append("//")
	out.append("//GENERATED by src/tools/generate_hpp.py from " + ", ".join(headers) + " - do not edit by hand.")
	out.append("//Re-run the generator (or `--check` in CI) after changing the C header.")

	if skipped:
		out.append("//")
		out.append("//Not wrapped (the generator only handles shapes it can be sure about):")
		for s in skipped:
			out.append(f"//  {s.name}: {s.why}")

	out.append("")
	out.append("#pragma once")

	for inc in spec.get("systemIncludes", [ "stdbool.h", "stdint.h", "assert.h" ]):
		out.append(f"#include <{inc}>")

	for inc in spec.get("cppIncludes", []):
		out.append(f"#include \"{inc}\"")

	out.append("")
	out.append("namespace oxc {")
	out.append("")
	out.append("\tnamespace c {")

	for inc in [ spec["header"].replace("include/", "") ] + spec.get("extraCIncludes", []):
		out.append(f"\t\t#include \"{inc}\"")

	out.append("\t}")
	out.append("")
	out += body
	out.append("}")
	out.append("")

	return "\n".join(out), skipped

def isUpToDate(existing, content):
	"""Whether a generated header on disk already holds `content`, line endings aside.

	The generated headers are committed with LF, but a Windows checkout with core.autocrlf=true (git's
	default there, and the repo has no .gitattributes pinning eol) hands them back as CRLF. A byte
	compare then calls every one of them stale on a clean tree, which fails check_generated_hpp for
	reasons that have nothing to do with the C headers having changed.
	"""

	if existing is None:
		return False

	return existing.replace("\r\n", "\n") == content

def main():

	parser = argparse.ArgumentParser(description="Generate oxc:: C++ wrappers for C headers")
	parser.add_argument("-spec", type=str, default=os.path.join(ROOT, "src", "tools", "generate_hpp.json"))
	parser.add_argument("--check", action="store_true", help="Fail if a generated header is out of date")
	args = parser.parse_args()

	with open(args.spec, encoding="utf-8") as f:
		specs = json.load(f)

	stale = []

	for spec in specs:

		content, skipped = generate(spec)
		outPath = os.path.join(ROOT, spec["output"])

		existing = None

		if os.path.isfile(outPath):
			with open(outPath, encoding="utf-8", newline="") as f:
				existing = f.read()

		if args.check:

			if not isUpToDate(existing, content):
				stale.append(spec["output"])
				print(f"-- STALE: {spec['output']}", file=sys.stderr)

			continue

		# Written with LF regardless of host, since that's what's committed; an existing CRLF copy that
		# already matches is left alone rather than rewritten, so a Windows tree doesn't churn.

		if isUpToDate(existing, content):
			print(f"-- Unchanged: {spec['output']}")
		else:
			os.makedirs(os.path.dirname(outPath), exist_ok=True)
			with open(outPath, "w", encoding="utf-8", newline="") as f:
				f.write(content)
			print(f"-- Wrote: {spec['output']} ({len(skipped)} declaration(s) skipped)")

		for s in skipped:
			print(f"     skipped {s.name}: {s.why}")

	if stale:
		print(f"-- {len(stale)} generated header(s) out of date; run python src/tools/generate_hpp.py", file=sys.stderr)
		sys.exit(1)

if __name__ == "__main__":
	main()
