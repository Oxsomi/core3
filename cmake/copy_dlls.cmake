foreach(LIBDIR ${LIBDIRS})
	if(IS_DIRECTORY "${LIBDIR}")
		file(COPY "${LIBDIR}/./" DESTINATION "${DESTDIR}" FILES_MATCHING PATTERN *.dll)
		file(COPY "${LIBDIR}/./" DESTINATION "${DESTDIR}" FILES_MATCHING PATTERN *.pdb)
		file(COPY "${LIBDIR}/./" DESTINATION "${DESTDIR}" FILES_MATCHING PATTERN LICENSE*)
	endif()
endforeach()

