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
# along with this program. If not, see https://github.com/Oxsomi/rt_core/blob/main/LICENSE.
# Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.
# To prevent this a separate license will have to be requested at contact@osomi.net for a premium;
# This is called dual licensing.

import os
import sys
import platform
import subprocess
import filecmp

TMP      = "mytest.tmp.txt"
TMP1     = "mytest1.tmp.txt"
TMP_ENC  = "mytest.tmp.txt.enc"
AES_KEY  = "CD00324F4CBAAE3467924E0578012F155A8F573AA066652DDDB8C2E1F76AF7FE"

def oxc3(bin):
	return os.path.join(bin, "OxC3" + (".exe" if platform.system() == "Windows" else ""))

def run(cmd):
	print(f"){cmd}")
	result = subprocess.run(cmd, shell=True)
	if result.returncode != 0:
		print(f"-- Command failed (exit {result.returncode}): {cmd}", file=sys.stderr)
		sys.exit(result.returncode)

def cleanup():
	for f in (TMP, TMP1, TMP_ENC):
		try:
			os.remove(f)
		except FileNotFoundError:
			pass

def main():

	bin = sys.argv[1] if len(sys.argv) > 1 else "."
	exe = oxc3(bin)

	# Bare invocation
	run(f'"{exe}"')

	# rand
	run(f'"{exe}" rand ?')
	run(f'"{exe}" rand key -count 5')
	run(f'"{exe}" rand char -count 5 -length 32 --alphanumeric')
	run(f'"{exe}" rand num -length 64 -bits 32')
	run(f'"{exe}" rand data -length 256')

	# hash
	run(f'"{exe}" hash ?')

	with open(TMP, "w") as f:
		f.write("Hello world\n")

	run(f'"{exe}" hash file -format CRC32C -input {TMP}')
	run(f'"{exe}" hash file -format SHA256  -input {TMP}')
	run(f'"{exe}" hash file -format MD5     -input {TMP}')
	run(f'"{exe}" hash string -format CRC32C -input "Hello world"')
	run(f'"{exe}" hash string -format SHA256  -input "Hello world"')
	run(f'"{exe}" hash string -format MD5     -input "Hello world"')

	# file
	run(f'"{exe}" file ?')
	run(f'"{exe}" file encr -input {TMP} -output {TMP_ENC} -aes {AES_KEY}')
	run(f'"{exe}" file decr -output {TMP1} -input {TMP_ENC} -aes {AES_KEY}')

	if not filecmp.cmp(TMP, TMP1, shallow=False):
		print("Failed encryption test!", file=sys.stderr)
		cleanup()
		sys.exit(1)

	run(f'"{exe}" file header -input {TMP_ENC}')
	run(f'"{exe}" file data -input {TMP_ENC} -aes {AES_KEY}')
	run(f'"{exe}" file data -input {TMP_ENC} -aes {AES_KEY} -entry 000')

	cleanup()

	print("Test reached end. Please double check output to ensure everything is correct.")

if __name__ == "__main__":
	main()