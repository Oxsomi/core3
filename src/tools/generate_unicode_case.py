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

"""Generate the simple case mapping tables behind UnicodeCodePoint_caseFold, toLower and toUpper.

The fold is Unicode's CaseFolding.txt reduced to its one to one mappings (statuses C and S), plus one mapping of
our own: U+0131 dotless i folds to i. NTFS compares names through an uppercase table, which takes dotless i to I and
so merges it with i, while simple case folding leaves it alone; without the extra mapping an oiCA could hold two
names that one file on Windows has to receive. Folding more than a file system does only refuses more names, which
is the safe direction; that is all that separates this fold from NTFS's. Lower and upper are UnicodeData.txt's simple
lowercase and uppercase mappings.

    python src/tools/generate_unicode_case.py            # download the pinned files and (re)write the tables
    python src/tools/generate_unicode_case.py --check    # non-zero if the tables are stale
    python src/tools/generate_unicode_case.py --latest   # warn when Unicode has a newer version

--ucd reads local copies of both files instead of downloading; they have to match the pinned hashes all the same.
The version is a deliberate choice, since the fold decides which names oiCA treats as equal: VERSION and SHA256 only
change together, by hand.
"""

import argparse
import hashlib
import os
import re
import sys
import urllib.request

OUTPUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "types", "base", "unicode_case.inc.h")

EXTRA_FOLDS = { 0x0131: 0x0069 }        #Dotless i -> i, as NTFS's uppercase table merges them

VERSION = "18.0.0"

SHA256 = {
	"CaseFolding.txt": "a004797658a457bec4dc11683e39f69249ea3b595b752dbea6721c4c9f587b0d",
	"UnicodeData.txt": "0736451de439ae7baf1425136617da495e09ee5afbe6e394374db7009ea08950"
}

URL = "https://www.unicode.org/Public/%s/ucd/%s"
LATEST_URL = "https://www.unicode.org/Public/UCD/latest/ucd/CaseFolding.txt"

PER_LINE = 4

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
*/
"""


def fetch(url):

	try:
		with urllib.request.urlopen(url, timeout=60) as response:
			return response.read()
	except (OSError, ValueError) as e:
		sys.exit("generate_unicode_case: couldn't download %s: %s" % (url, e))


def load(ucd, name):
	"""Returns the pinned file, from the ucd directory or downloaded, after checking it against SHA256."""

	if ucd:
		with open(os.path.join(ucd, name), "rb") as f:
			data = f.read()
	else:
		data = fetch(URL % (VERSION, name))

	digest = hashlib.sha256(data).hexdigest()

	if digest != SHA256[name]:
		sys.exit("generate_unicode_case: %s has SHA-256 %s, but %s pins %s" % (name, digest, VERSION, SHA256[name]))

	return data.decode("utf-8")


def version_of(text):

	match = re.match(r"# CaseFolding-(\d+\.\d+\.\d+)\.txt", text)

	if not match:
		sys.exit("generate_unicode_case: the first line doesn't name a CaseFolding-<version>.txt")

	return match.group(1)


def parse_folds(text):
	"""Returns {code point: folded code point} for statuses C and S."""

	folds = {}

	for line in text.splitlines():

		line = line.split("#")[0].strip()

		if not line:
			continue

		code, status, mapping = [part.strip() for part in line.split(";")[:3]]

		if status not in ("C", "S"):
			continue

		target = mapping.split()

		if len(target) != 1:
			sys.exit("generate_unicode_case: status %s of U+%s maps to more than one code point" % (status, code))

		folds[int(code, 16)] = int(target[0], 16)

	return folds


def parse_cases(text):
	"""Returns ({code point: simple lowercase}, {code point: simple uppercase}) from UnicodeData.txt."""

	lower, upper = {}, {}

	for line in text.splitlines():

		fields = line.split(";")

		if len(fields) != 15:
			sys.exit("generate_unicode_case: UnicodeData.txt has a line without 15 fields: %s" % line)

		code = int(fields[0], 16)

		if fields[12]:
			upper[code] = int(fields[12], 16)

		if fields[13]:
			lower[code] = int(fields[13], 16)

	return lower, upper


def check_scalars(name, mapping):

	for code, target in mapping.items():
		if 0xD800 <= code <= 0xDFFF or 0xD800 <= target <= 0xDFFF or code > 0x10FFFF or target > 0x10FFFF:
			sys.exit("generate_unicode_case: %s maps U+%04X to U+%04X, which isn't a scalar value" % (name, code, target))


def ranges(mapping):
	"""Runs of (first, delta, count, stride): count code points from first, every stride (1 or 2), add delta."""

	out = []

	for code, target in sorted(mapping.items()):

		delta = target - code

		if out:

			first, runDelta, count, stride = out[-1]
			step = code - (first + (count - 1) * stride)

			if runDelta == delta and (step == stride or (count == 1 and step == 2)) and count < 0xFFFF:
				out[-1] = (first, delta, count + 1, step)
				continue

		out.append((code, delta, 1, 1))

	# A run only spans consecutive mappings, so the code point a stride of 2 skips never maps; check it round trips

	expanded = {}

	for first, delta, count, stride in out:
		for i in range(count):
			expanded[first + i * stride] = first + i * stride + delta

	if expanded != mapping:
		sys.exit("generate_unicode_case: the runs don't reproduce the mapping")

	return out


def table(name, comment, mapping):

	runs = ranges(mapping)
	out = ["//" + comment, "static const UnicodeCaseRange %s[%d] = {" % (name, len(runs))]

	for i in range(0, len(runs), PER_LINE):
		row = ", ".join("{ 0x%05X, %d, %d, %d }" % run for run in runs[i:i + PER_LINE])
		out.append("\t" + row + ("," if i + PER_LINE < len(runs) else ""))

	out.append("};")
	out.append("")
	return out


def build(folds, lower, upper):

	for code, target in EXTRA_FOLDS.items():

		if code in folds:
			sys.exit("generate_unicode_case: U+%04X already folds, the extra mapping is stale" % code)

		folds[code] = target

	check_scalars("CaseFolding.txt", folds)
	check_scalars("UnicodeData.txt lowercase", lower)
	check_scalars("UnicodeData.txt uppercase", upper)

	for code, target in folds.items():
		if folds.get(target, target) != target:        #A folded code point has to stay put, or equality isn't transitive
			sys.exit("generate_unicode_case: U+%04X folds to U+%04X, which folds again" % (code, target))

	out = [LICENSE]
	out.append("//types/base/unicode_case.inc.h")
	out.append("//Generated by src/tools/generate_unicode_case.py from Unicode %s's CaseFolding.txt and UnicodeData.txt;" % VERSION)
	out.append("// rerun it rather than editing.")
	out.append("//Derived from the Unicode Character Database, (c) Unicode, Inc., under the Unicode License")
	out.append("// (https://www.unicode.org/license.txt).")
	out.append("//Each run is { first, delta, count, stride }: count code points from first, every stride, map to themselves")
	out.append("// plus delta. Runs are sorted by first and don't overlap, for a binary search.")
	out.append("")
	out.append("#pragma once")
	out.append("")
	out += table("UnicodeCase_fold", "Simple case folding: CaseFolding.txt statuses C and S, plus U+0131 dotless i to i", folds)
	out += table("UnicodeCase_lower", "UnicodeData.txt's simple lowercase mappings", lower)
	out += table("UnicodeCase_upper", "UnicodeData.txt's simple uppercase mappings", upper)

	return "\n".join(out)


def latest(ucd):
	"""Warns when Unicode's latest CaseFolding.txt is newer than VERSION, with what updating would change in the fold."""

	data = fetch(LATEST_URL)
	text = data.decode("utf-8")
	version = version_of(text)

	if tuple(map(int, version.split("."))) <= tuple(map(int, VERSION.split("."))):
		print("generate_unicode_case: %s is the latest" % VERSION)
		return 0

	old, new = parse_folds(load(ucd, "CaseFolding.txt")), parse_folds(text)
	added = sum(1 for code in new if code not in old)
	removed = sum(1 for code in old if code not in new)
	changed = sum(1 for code in new if code in old and old[code] != new[code])

	print(
		"generate_unicode_case: warning: Unicode %s is out and the tables pin %s; its fold adds %d mappings, changes %d"
		" and removes %d. Updating changes which names oiCA treats as equal, so it's a deliberate choice: set VERSION"
		" and both hashes in SHA256 (%s's CaseFolding.txt is %s)."
		% (version, VERSION, added, changed, removed, version, hashlib.sha256(data).hexdigest())
	)

	return 0


def main():

	parser = argparse.ArgumentParser(description="Generate the simple case mapping tables")
	parser.add_argument("--ucd", help="Directory with local copies of the pinned CaseFolding.txt and UnicodeData.txt")
	parser.add_argument("--check", action="store_true", help="Fail if the tables are out of date")
	parser.add_argument("--latest", action="store_true", help="Warn if Unicode has a newer version")
	args = parser.parse_args()

	if args.latest:
		return latest(args.ucd)

	lower, upper = parse_cases(load(args.ucd, "UnicodeData.txt"))
	text = build(parse_folds(load(args.ucd, "CaseFolding.txt")), lower, upper)
	output = os.path.normpath(OUTPUT)

	if args.check:

		try:
			with open(output, encoding="utf-8") as f:
				current = f.read().replace("\r\n", "\n")
		except OSError:
			current = None

		if current != text:
			print("generate_unicode_case: %s is out of date" % output)
			return 1

		return 0

	with open(output, "w", encoding="utf-8", newline="\n") as f:
		f.write(text)

	print("generate_unicode_case: wrote %s" % output)
	return 0


if __name__ == "__main__":
	sys.exit(main())
