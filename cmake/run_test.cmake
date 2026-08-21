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
#   cmake -DTEST_EXE=<path> [-DTEST_NAME=<name>] [-DTEST_WORKING_DIR=<dir>] -P run_test.cmake

if(NOT TEST_EXE)
	message(FATAL_ERROR "run_test.cmake: TEST_EXE is required")
endif()

if(NOT TEST_NAME)
	get_filename_component(TEST_NAME "${TEST_EXE}" NAME_WE)
endif()

if(NOT TEST_WORKING_DIR)
	get_filename_component(TEST_WORKING_DIR "${TEST_EXE}" DIRECTORY)
endif()

execute_process(
	COMMAND "${TEST_EXE}"
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
