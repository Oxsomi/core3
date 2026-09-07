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
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see https://github.com/Oxsomi/core3/blob/main/LICENSE.
# Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.
# To prevent this a separate license will have to be requested at contact@osomi.net for a premium;
# This is called dual licensing.
#
# hosting_test.py: what a host serves, and what the vendored third party is pinned to.
#
# The page's bytes reach a visitor through precompressed siblings, so the rules that pick one are what
# decide whether a visitor gets the current file at all. They are checked here rather than through a
# live server, since the choosing is the part that can be wrong.
# Run: python dev/hosting_test.py (from web/), or through build_web.py --run_frontend_tests.

import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

import build_web

failures = 0

def check(name, condition, extra=""):

	global failures

	print(("  ok " if condition else "FAIL ") + name + ("" if condition else "  " + str(extra)))

	if not condition:
		failures += 1

def main():

	build_web.ensureEmsdk()        # compressStatic runs node, which the emsdk ships and a plain shell hasn't on PATH

	with tempfile.TemporaryDirectory() as tmp:

		source = os.path.join(tmp, "app.js")

		with open(source, "w") as f:
			f.write("console.log(1)\n")

		sibling = build_web.precompressedSibling

		check("no sibling on disk serves the file itself", sibling(source, "br, gzip") is None)

		build_web.compressStatic([source])

		check("brotli and gzip are both written",
			os.path.isfile(source + ".br") and os.path.isfile(source + ".gz"))

		check("brotli is preferred when both are accepted",
			sibling(source, "gzip, deflate, br") == (source + ".br", "br"))

		check("gzip serves a client that can't take brotli",
			sibling(source, "gzip, deflate") == (source + ".gz", "gzip"))

		check("a client accepting neither gets the file itself", sibling(source, "identity") is None)

		# An edit leaves the siblings behind. Serving one then would hide the edit, which during
		# development is a change that appears not to have happened.

		edited = os.path.getmtime(source)

		for suffix in (".br", ".gz"):
			os.utime(source + suffix, (edited - 10, edited - 10))

		check("a sibling older than its source is refused", sibling(source, "br, gzip") is None)

		build_web.compressStatic([source])

		check("recompressing makes it servable again",
			sibling(source, "br, gzip") == (source + ".br", "br"))

		# A font is already compressed, so the vendor step writes it no siblings and a host serves the file

		font = os.path.join(tmp, "icon.woff2")

		with open(font, "wb") as f:
			f.write(b"\x00" * 64)

		check("a file with no siblings serves itself", sibling(font, "br, gzip") is None)

	# The third party is pinned by content, so a name or a hash going missing is a build that fetches
	# something nobody reviewed.

	names = [name for _, name, _ in build_web.VENDOR]

	check("every vendored file is pinned to a sha256",
		all(len(digest) == 64 and all(c in "0123456789abcdef" for c in digest) for _, _, digest in build_web.VENDOR))

	check("no two vendored files land on one path", len(set(names)) == len(names))

	check("the page asks for exactly what is vendored", vendorMatchesPage(names))

	# A new asset on the page that nobody adds to the deploy set is a site that half loads once it is
	# unpacked somewhere else, and nothing else would notice.

	packaged = set(build_web.frontendPackageFiles())

	missing = sorted(pageReferences() - packaged)

	if missing:
		print("   the page loads " + ", ".join(missing) + " which the package leaves out")

	check("everything the page loads is in the deploy set", not missing)

	for required in ("index.html", "wasm/" + build_web.WEB_MODULE + ".js", "wasm/" + build_web.WEB_MODULE + ".wasm"):
		check("the deploy set carries " + required, required in packaged)

	for unwanted in ("dev/hosting_test.py", "package.json", "README.md", "TODO"):
		check("the deploy set leaves out " + unwanted, unwanted not in packaged)

	print(f"\n{failures} FAILURE(S)" if failures else "\nALL PASS")
	return 1 if failures else 0

def pageReferences():

	"""Every local file index.html asks a browser to fetch, as paths relative to web/."""

	import re

	with open(pagePath(), "r", encoding="utf-8") as f:
		html = f.read()

	found = set()

	for value in re.findall(r'(?:href|src)="([^"]+)"', html):
		if not value.startswith(("http:", "https:", "data:", "#", "//")):
			found.add(value.lstrip("./"))

	return found

def pagePath():
	return os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "index.html")

def vendorMatchesPage(names):

	"""Every vendor/ path index.html loads is one the table produces, so a bump can't leave a dead link."""

	with open(pagePath(), "r", encoding="utf-8") as f:
		html = f.read()

	asked = set()
	needle = '"vendor/'

	at = html.find(needle)

	while at != -1:
		end = html.find('"', at + len(needle))
		asked.add(html[at + len(needle):end])
		at = html.find(needle, end)

	missing = asked - set(names)

	if missing:
		print("   the page asks for " + ", ".join(sorted(missing)) + " which the VENDOR table doesn't produce")

	return not missing

if __name__ == "__main__":
	sys.exit(main())
