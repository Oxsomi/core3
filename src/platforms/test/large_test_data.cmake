# Adds the largedata section to target: one entry above the medium size (128 KiB), which a loaded section keeps in its
# stream instead of copying it out, for platforms_interface to read back.
# Only OxC3's own test targets include this; the entry is written into the build tree, not kept in the source tree.

function(oxc3_add_large_test_data target self)

	set(dir "${CMAKE_CURRENT_BINARY_DIR}/largedata")
	string(REPEAT "0123456789abcdef" 10240 text)
	file(CONFIGURE OUTPUT "${dir}/large.txt" CONTENT "${text}")

	add_virtual_files(
		TARGET  ${target}
		NAME    largedata
		ROOT    ${dir}
		SELF    ${self}
		FORCE_PACKAGER
	)

endfunction()
