if(NOT EXISTS "${PACKAGE_MODULE}")
	message(FATAL_ERROR "Installed shader compiler module is missing: ${PACKAGE_MODULE}")
endif()

if(NOT EXISTS "${CONSUMER_MODULE}")
	message(FATAL_ERROR "apply_dependencies did not stage the shader compiler: ${CONSUMER_MODULE}")
endif()

file(SHA256 "${PACKAGE_MODULE}" packageHash)
file(SHA256 "${CONSUMER_MODULE}" consumerHash)

if(NOT packageHash STREQUAL consumerHash)
	message(FATAL_ERROR "The consumer's shader compiler differs from the installed package")
endif()
