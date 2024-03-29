set(TEST_RECEIVER_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/main.cc 
)
add_executable(test_receiver ${TEST_RECEIVER_FILES})
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	target_link_libraries(test_receiver ${CMAKE_BINARY_DIR}/lib/${QOSRTP_LIBRARY_NAME}.lib)
endif()
target_link_libraries(test_receiver ${QOSRTP_LIBRARY_NAME}.dll)