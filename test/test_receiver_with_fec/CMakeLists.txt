set(TEST_RECEIVER_WIEH_FEC_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/main.cc 
)
add_executable(test_receiver_with_fec ${TEST_RECEIVER_WIEH_FEC_FILES})
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	target_link_libraries(test_receiver_with_fec ${CMAKE_BINARY_DIR}/lib/${QOSRTP_LIBRARY_NAME}.lib)
endif()
target_link_libraries(test_receiver_with_fec ${QOSRTP_LIBRARY_NAME}.dll)