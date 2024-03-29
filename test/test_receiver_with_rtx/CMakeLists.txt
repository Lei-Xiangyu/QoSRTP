set(TEST_RECEIVER_WIEH_RTX_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/main.cc 
)
add_executable(test_receiver_with_rtx ${TEST_RECEIVER_WIEH_RTX_FILES})
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	target_link_libraries(test_receiver_with_rtx ${CMAKE_BINARY_DIR}/lib/${QOSRTP_LIBRARY_NAME}.lib)
endif()
target_link_libraries(test_receiver_with_rtx ${QOSRTP_LIBRARY_NAME}.dll)