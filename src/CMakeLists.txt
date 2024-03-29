set(LIBRARIES)
add_definitions(-DQOSRTP)
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
		list(APPEND LIBRARIES "Ws2_32.lib")
	else()
	endif()
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
endif()
add_subdirectory("rtp_rtcp")
add_subdirectory("utils")
add_subdirectory("include")
add_subdirectory("session")
add_subdirectory("network")
source_group("include" FILES ${QOSRTP_FILES_INCLUDE})
source_group("rtp_rtcp" FILES ${QOSRTP_FILES_RTP_RTCP})
source_group("utils" FILES ${QOSRTP_FILES_UTILS})
source_group("session" FILES ${QOSRTP_FILES_SESSION})
source_group("network" FILES ${QOSRTP_FILES_NETWORK})
set(QOSRTP_FILES_LIBRARY
	${QOSRTP_FILES_INCLUDE} 
	${QOSRTP_FILES_RTP_RTCP} 
	${QOSRTP_FILES_UTILS}
	${QOSRTP_FILES_SESSION}
	${QOSRTP_FILES_NETWORK}
)
add_library(qosrtp SHARED ${QOSRTP_FILES_LIBRARY})
target_link_libraries(qosrtp PRIVATE ${LIBRARIES})