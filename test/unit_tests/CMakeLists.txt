# Copyright (c) 2013, David Keller
# All rights reserved.
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of the University of California, Berkeley nor the
#       names of its contributors may be used to endorse or promote products
#       derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY DAVID KELLER AND CONTRIBUTORS ``AS IS AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Run all tests, timeout if it takes more than 30 seconds.
add_custom_target(unit_tests COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --timeout 30)
add_dependencies(check unit_tests)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
add_subdirectory(${PROJECT_SOURCE_DIR}/googletest ${PROJECT_SOURCE_DIR}/googletest/cmake-build-debug/lib)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
add_subdirectory(${PROJECT_SOURCE_DIR}/googletest ${PROJECT_SOURCE_DIR}/googletest/cmake-build-release/lib)
elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
add_subdirectory(${PROJECT_SOURCE_DIR}/googletest ${PROJECT_SOURCE_DIR}/googletest/cmake-build-relwithdebinfo/lib)
elseif(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
add_subdirectory(${PROJECT_SOURCE_DIR}/googletest ${PROJECT_SOURCE_DIR}/googletest/cmake-build-minsizerel/lib)
endif()

enable_testing()

include_directories(BEFORE .)
include_directories(${GTEST_INCLUDE_DIRS})

add_library(test_helpers STATIC
    common.cpp
    Network.cpp)

target_link_libraries(test_helpers
    gtest
    Poco::Foundation
    Poco::Net)

set_target_properties(test_helpers PROPERTIES
    COMPILE_DEFINITIONS TESTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

macro(build_test test_name)
    cmake_parse_arguments(ARG "" "" "LIBRARIES;SOURCES" ${ARGN})
    add_executable(${test_name} ${ARG_SOURCES})
    target_link_libraries(${test_name}
        test_helpers
        ${ARG_LIBRARIES})
    add_dependencies(unit_tests ${test_name})
    add_test(${test_name} ${test_name})
endmacro()

build_test(unit_tests_helpers
    SOURCES
        FakeSocketTest.cpp
    LIBRARIES
        kademlia_static)

build_test(unit_tests_lib
    SOURCES
        test_id.cpp
        EndpointTest.cpp
        MessageTest.cpp
        MessageSerializerTest.cpp
		IntegrationTest.cpp
        LookupTaskTest.cpp
        StoreValueTaskTest.cpp
        FindValueTaskTest.cpp
        PeerTest.cpp
        test_error.cpp
        DiscoverNeighborsTaskTest.cpp
        NotifyPeerTaskTest.cpp
        ResponseRouterTest.cpp
        ResponseCallbacksTest.cpp
        TimerTest.cpp
        NetworkTest.cpp
        MessageSocketTest.cpp
        test_log.cpp
        test_r.cpp
        RoutingTableTest.cpp
        test_session.cpp
        test_first_session.cpp
        EngineTest.cpp
        SocketAdapterTest.cpp
    LIBRARIES 
        kademlia_static
        Poco::Foundation
        Poco::Net)

if(ENABLE_COVERAGE)
    if(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
        if(${CMAKE_CXX_COMPILER_ID} STREQUAL "AppleClang")
            find_program(XCRUN xcrun)
            if(XCRUN)
                execute_process(COMMAND
                                    ${XCRUN} -f llvm-cov
                                OUTPUT_VARIABLE
                                    LLVM_COV
                                OUTPUT_STRIP_TRAILING_WHITESPACE)
            endif()
        else()
            find_program(LLVM_COV llvm-cov)
        endif()

        if (NOT LLVM_COV)
            message(FATAL_ERROR "Can't find llvm-cov tool")
        endif()

        set(GCOV ${CMAKE_CURRENT_BINARY_DIR}/llvm_gcov)
        configure_file(llvm_gcov.in ${GCOV} @ONLY)
    elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
        find_program(GCOV gcov)

        if (NOT GCOV)
            message(FATAL_ERROR "Can't find gcov tool")
        endif()
    endif()

    find_program(LCOV lcov)
    find_program(GENHTML genhtml)

    if (NOT LCOV OR NOT GENHTML)
        message(FATAL_ERROR "Can't find lcov tool(s)")
    endif()

    add_custom_target(coverage
        COMMAND
            ${LCOV} --quiet --output-file app_base.info --initial --capture --directory . --gcov-tool ${GCOV}
        COMMAND
            ${CMAKE_COMMAND} --build . --target unit_tests
        COMMAND
            ${LCOV} --quiet --output-file app_test.info --capture --directory . --gcov-tool ${GCOV}
        COMMAND
            ${LCOV} --quiet --output-file app_total.info --add-tracefile app_base.info --add-tracefile app_test.info
        COMMAND
            ${LCOV} --quiet --output-file app_total_stripped.info --extract app_total.info "*/include/kademlia/*" "*/src/kademlia/*"
        COMMAND
            ${GENHTML} --output-directory html --num-spaces 4 --title "Kademlia unit tests" --demangle-cpp --no-function-coverage --prefix ${CMAKE_SOURCE_DIR} app_total_stripped.info
        WORKING_DIRECTORY
            ${CMAKE_BINARY_DIR}
        VERBATIM)
    add_dependencies(coverage kademlia_static)
endif()

