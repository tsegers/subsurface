execute_process(
	COMMAND sh ${CMAKE_TOP_SRC_DIR}/scripts/get-version linux
	WORKING_DIRECTORY ${CMAKE_TOP_SRC_DIR}
	OUTPUT_VARIABLE GIT_VERSION_STRING
	OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
	COMMAND sh ${CMAKE_TOP_SRC_DIR}/scripts/get-version full
	WORKING_DIRECTORY ${CMAKE_TOP_SRC_DIR}
	OUTPUT_VARIABLE CANONICAL_VERSION_STRING
	OUTPUT_STRIP_TRAILING_WHITESPACE
)
set(MOBILE_VERSION_STRING "3.3.1")

configure_file(${SRC} ${DST} @ONLY)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	execute_process(
		COMMAND cat ${CMAKE_TOP_SRC_DIR}/packaging/windows/subsurface.nsi.in
		COMMAND sed -e "s/VERSIONTOKEN/${GIT_VERSION_STRING}/"
		COMMAND sed -e "s/PRODVTOKEN/${CANONICAL_VERSION_STRING}/"
		OUTPUT_FILE ${CMAKE_BINARY_DIR}/staging/subsurface.nsi
	)
endif()
