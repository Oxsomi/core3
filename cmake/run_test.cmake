# Runs one test and judges it by its EXIT CODE alone.
#
# Needed because MSBuild's Exec task scans a command's output for error shaped lines and fails the step when it finds
# one, whatever the process returned.
# OxC3's tests print error shaped text on purpose: a negative test that proves an error is reported has to report one,
# and Test_logOOM deliberately logs under OOM.
# Run directly as a POST_BUILD command, those passing tests fail the build.
#
# Output is captured and only printed when the test actually fails.
# That keeps a successful build's log readable (one line per suite instead of thousands), and a failing one still shows
# everything.
#
#   cmake -DTEST_EXE=<path> | -DTEST_COMMAND=<arg|arg|...>
#        [-DTEST_NAME=<name>] [-DTEST_WORKING_DIR=<dir>] [-DTEST_ENV_FILE=<file>] -P run_test.cmake
#
# TEST_COMMAND is | separated, because a ; separated list would be split by -D itself.

# TEST_EXE is one binary; TEST_COMMAND is a whole command line, which is what a suite driven by a script
# needs. Exactly one of them.

if(TEST_COMMAND)
	string(REPLACE "|" ";" testCommand "${TEST_COMMAND}")
elseif(TEST_EXE)
	set(testCommand "${TEST_EXE}")
else()
	message(FATAL_ERROR "run_test.cmake: TEST_EXE or TEST_COMMAND is required")
endif()

if(NOT TEST_NAME)
	list(GET testCommand 0 firstArg)
	get_filename_component(TEST_NAME "${firstArg}" NAME_WE)
endif()

if(NOT TEST_WORKING_DIR)
	list(GET testCommand 0 firstArg)
	get_filename_component(TEST_WORKING_DIR "${firstArg}" DIRECTORY)
endif()

# The same environment ctest would apply through ENVIRONMENT_MODIFICATION, so a suite behaves identically
# whether it runs here after being built or through ctest at the end.
# Test properties are a ctest concept and this is a plain execute_process, so the entries arrive through a
# file the build generates; ctest's own operation syntax is reused rather than inventing a second one.

if(TEST_ENV_FILE AND EXISTS "${TEST_ENV_FILE}")

	file(STRINGS "${TEST_ENV_FILE}" envEntries)

	if(WIN32)
		set(pathSep ";")
	else()
		set(pathSep ":")
	endif()

	foreach(entry ${envEntries})

		if(NOT entry MATCHES "^([^=]+)=([a-z_]+):(.*)$")
			continue()
		endif()

		set(envName "${CMAKE_MATCH_1}")
		set(envOp "${CMAKE_MATCH_2}")
		set(envVal "${CMAKE_MATCH_3}")

		if(envOp STREQUAL "set")
			set(ENV{${envName}} "${envVal}")

		elseif(envOp STREQUAL "string_append")
			set(ENV{${envName}} "$ENV{${envName}}${envVal}")

		elseif(envOp STREQUAL "string_prepend")
			set(ENV{${envName}} "${envVal}$ENV{${envName}}")

		elseif(envOp STREQUAL "path_list_prepend")
			if(DEFINED ENV{${envName}} AND NOT "$ENV{${envName}}" STREQUAL "")
				set(ENV{${envName}} "${envVal}${pathSep}$ENV{${envName}}")
			else()
				set(ENV{${envName}} "${envVal}")
			endif()

		elseif(envOp STREQUAL "path_list_append")
			if(DEFINED ENV{${envName}} AND NOT "$ENV{${envName}}" STREQUAL "")
				set(ENV{${envName}} "$ENV{${envName}}${pathSep}${envVal}")
			else()
				set(ENV{${envName}} "${envVal}")
			endif()

		else()
			message(FATAL_ERROR "run_test.cmake: unknown environment operation '${envOp}' in '${entry}'")
		endif()

	endforeach()

endif()

execute_process(
	COMMAND ${testCommand}
	WORKING_DIRECTORY "${TEST_WORKING_DIR}"
	RESULT_VARIABLE testResult
	OUTPUT_VARIABLE testOutput
	ERROR_VARIABLE testError
)

if(NOT testResult EQUAL 0)

	message("${testOutput}")

	if(testError)
		message("${testError}")
	endif()

	message(FATAL_ERROR "${TEST_NAME} FAILED (exit ${testResult})")
endif()

message(STATUS "${TEST_NAME} passed")
