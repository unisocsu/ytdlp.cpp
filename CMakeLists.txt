cmake_minimum_required(VERSION 3.10)
project(ytdlp_cpp VERSION 0.1.0 LANGUAGES C CXX)

option(YTDLP_BUILD_SHARED "Build ytdlp.cpp as a shared library" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(YTDLP_SOURCES
    src/ytdlp.cpp
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

target_compile_definitions(ytdlp_core PRIVATE YTDLP_CPP_BUILDING=1)

if(ANDROID)
    # The CI workflow sets ANDROID_PLATFORM=android-19 and ANDROID_ABI=armeabi-v7a.
    target_compile_options(ytdlp_core PRIVATE -Wall -Wextra -Wpedantic)
else()
    target_compile_options(ytdlp_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(ytdlp_core PROPERTIES
    OUTPUT_NAME ytdlp
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
)

install(TARGETS ytdlp_core
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install(DIRECTORY include/ DESTINATION include)
