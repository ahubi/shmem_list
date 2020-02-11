
cmake_minimum_required(VERSION 2.8)

set(LIB_NAME shmemutils)

set(LIBTST_LIST_SINK libtestsink)
set(LIBTST_LIST_SRC  libtestsrc)


#Compiler Flags
set(CMAKE_VERBOSE_MAKEFILE on)
set(CMAKE_CXX_FLAGS "-Wall -fPIC ${CMAKE_CXX_FLAGS} ")
set(CMAKE_C_FLAGS   "-Wall -fPIC ${CMAKE_C_FLAGS} ")
set(CMAKE_BUILD_TYPE Debug)

#Paths
set(LIB_PATH                    ${CMAKE_CURRENT_SOURCE_DIR})
set(LIB_TESTS_PATH              ${CMAKE_CURRENT_SOURCE_DIR}/tests)


file(GLOB LIB_SRCS              ${LIB_PATH}/*.c)
file(GLOB LIST_TEST_SINK_SRCS   ${LIB_TESTS_PATH}/list_test_sink.c)
file(GLOB LIST_TEST_SRC_SRCS    ${LIB_TESTS_PATH}/list_test_src.c)
include_directories(
    ${LIB_PATH}
)

message(STATUS "" ${LIB_NAME})

add_library(${LIB_NAME} STATIC ${LIB_SRCS})
add_executable(${LIBTST_LIST_SINK}  ${LIST_TEST_SINK_SRCS})
add_executable(${LIBTST_LIST_SRC}   ${LIST_TEST_SRC_SRCS})

target_link_libraries (${LIBTST_LIST_SINK} ${LIB_NAME} rt c pthread m )
target_link_libraries (${LIBTST_LIST_SRC}  ${LIB_NAME} rt c pthread m )

target_link_libraries(${LIB_NAME}  rt c pthread m )

install(TARGETS ${LIB_NAME} RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)

