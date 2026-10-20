cmake_minimum_required(VERSION 3.18)
project(ytdlp_cpp VERSION 0.1.0 LANGUAGES C CXX)

option(YTDLP_BUILD_SHARED "Build ytdlp.cpp as a shared library" ON)
option(YTDLP_BUILD_TESTS "Build native utility tests" ON)
set(YTDLP_TLS_LIBRARIES "" CACHE STRING "Additional TLS libraries needed by static libcurl")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(CURL REQUIRED)

set(YTDLP_SOURCES
    src/ytdlp.cpp
    src/http.cpp
    src/network.cpp
    src/extractor.cpp
    src/html.cpp
    src/utils.cpp
)

if(YTDLP_BUILD_SHARED)
    add_library(ytdlp_core SHARED ${YTDLP_SOURCES})
else()
    add_library(ytdlp_core STATIC ${YTDLP_SOURCES})
endif()

target_include_directories(ytdlp_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(ytdlp_core PRIVATE CURL::libcurl ${YTDLP_TLS_LIBRARIES})
if(WIN32)
    # Static libcurl with Schannel uses these Windows system libraries.
    target_link_libraries(ytdlp_core PRIVATE ws2_32 crypt32 wldap32 normaliz bcrypt advapi32)
endif()
target_compile_definitions(ytdlp_core PRIVATE YTDLP_CPP_BUILDING=1)
if(MSVC)
    target_compile_options(ytdlp_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(ytdlp_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(ytdlp_core PROPERTIES
    OUTPUT_NAME ytdlp
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
)

if(YTDLP_BUILD_TESTS)
    enable_testing()
    add_executable(ytdlp_utils_test
        tests/native_utils_test.cpp
        src/utils.cpp
    )
    target_include_directories(ytdlp_utils_test PRIVATE include)
    if(MSVC)
        target_compile_options(ytdlp_utils_test PRIVATE /W4 /permissive-)
    else()
        target_compile_options(ytdlp_utils_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME native_utils_test COMMAND ytdlp_utils_test)
    add_executable(ytdlp_network_test tests/network_test.cpp src/network.cpp)
    target_include_directories(ytdlp_network_test PRIVATE include)
    target_link_libraries(ytdlp_network_test PRIVATE CURL::libcurl)
    if(MSVC)
        target_compile_options(ytdlp_network_test PRIVATE /W4 /permissive-)
    else()
        target_compile_options(ytdlp_network_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME native_network_test COMMAND ytdlp_network_test)
    add_executable(ytdlp_extractor_test tests/extractor_test.cpp src/extractor.cpp src/utils.cpp)
    target_include_directories(ytdlp_extractor_test PRIVATE include)
    if(MSVC)
        target_compile_options(ytdlp_extractor_test PRIVATE /W4 /permissive-)
    else()
        target_compile_options(ytdlp_extractor_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME native_extractor_test COMMAND ytdlp_extractor_test)
    add_executable(ytdlp_html_test tests/html_test.cpp src/html.cpp)
    target_include_directories(ytdlp_html_test PRIVATE include)
    if(MSVC)
        target_compile_options(ytdlp_html_test PRIVATE /W4 /permissive-)
    else()
        target_compile_options(ytdlp_html_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME native_html_test COMMAND ytdlp_html_test)
endif()

install(TARGETS ytdlp_core
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install(DIRECTORY include/ DESTINATION include)
