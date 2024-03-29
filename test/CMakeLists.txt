set(QOSRTP_LIBRARY_NAME "qosrtp")
set(QOSRTP_LIBRARY_DIR
	${CMAKE_BINARY_DIR}/lib
	${CMAKE_BINARY_DIR}/bin
)
set(QOSRTP_INCLUDE_DIR ${CMAKE_CURRENT_LIST_FILE}/../../src/include)
link_directories(${QOSRTP_LIBRARY_DIR})
include_directories(${QOSRTP_INCLUDE_DIR})
add_subdirectory(test_sender)
add_subdirectory(test_receiver)
add_subdirectory(test_sender_with_rtx)
add_subdirectory(test_receiver_with_rtx)
add_subdirectory(test_sender_with_fec)
add_subdirectory(test_receiver_with_fec)