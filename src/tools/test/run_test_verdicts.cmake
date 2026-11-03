# Checks each verdict of cmake/run_test.cmake against a command that exits with a known code.
# 0 passes and writes the stamp, the skip code lets the build go on without one, and any other code fails it without one.
# The stamp is written before every case, so a stale one has to be removed as well.
#
#   cmake -DPYTHON=<python> -DSKIP_CODE=<code> -P run_test_verdicts.cmake

set(runTest "${CMAKE_CURRENT_LIST_DIR}/../../../cmake/run_test.cmake")
set(stamp "${CMAKE_CURRENT_BINARY_DIR}/run_test_verdicts.passed")

foreach(verdict "0 0 1" "${SKIP_CODE} 0 0" "1 1 0")

	separate_arguments(verdict)
	list(GET verdict 0 exitCode)
	list(GET verdict 1 expectFailure)
	list(GET verdict 2 expectStamp)

	file(TOUCH "${stamp}")

	execute_process(
		COMMAND ${CMAKE_COMMAND}
			"-DTEST_COMMAND=${PYTHON}|-c|raise SystemExit(${exitCode})" -DTEST_NAME=exit${exitCode}
			-DTEST_STAMP=${stamp} -DTEST_SKIP_CODE=${SKIP_CODE} -P "${runTest}"
		RESULT_VARIABLE result
		OUTPUT_QUIET
		ERROR_QUIET
	)

	if(expectFailure AND result EQUAL 0)
		message(FATAL_ERROR "exit ${exitCode} passed the run, it has to fail it")
	elseif(NOT expectFailure AND NOT result EQUAL 0)
		message(FATAL_ERROR "exit ${exitCode} failed the run (${result})")
	endif()

	if(expectStamp AND NOT EXISTS "${stamp}")
		message(FATAL_ERROR "exit ${exitCode} left no stamp")
	elseif(NOT expectStamp AND EXISTS "${stamp}")
		message(FATAL_ERROR "exit ${exitCode} left a stamp")
	endif()

endforeach()

file(REMOVE "${stamp}")
message(STATUS "run_test.cmake: pass, skip and fail verdicts hold")
