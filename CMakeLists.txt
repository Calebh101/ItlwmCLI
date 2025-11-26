cmake_minimum_required(VERSION 3.16)
project(ItlwmCLI LANGUAGES CXX)
enable_language(C)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_PREFIX_PATH "${SMAKE_CURRENT_SOURCE_DIR}/lib")
set_source_files_properties(HeliPort/ClientKit/Api.c PROPERTIES LANGUAGE C)

find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
if(NOT COREFOUNDATION_FRAMEWORK)
    message(FATAL_ERROR "CoreFoundation framework not found")
endif()

find_library(IOKIT_FRAMEWORK IOKit)
if(NOT IOKIT_FRAMEWORK)
    message(FATAL_ERROR "IOKit framework not found")
endif()

set(FMT_USE_STATIC_LIBS ON CACHE BOOL "" FORCE)
find_package(fmt REQUIRED)

set(FTXUI_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
find_package(ftxui REQUIRED)

add_executable(ItlwmCLI
    ${CMAKE_SOURCE_DIR}/include/json.hpp
    ${CMAKE_SOURCE_DIR}/include/filesystem.hpp
    ${CMAKE_SOURCE_DIR}/include/Api.h
    HeliPort/ClientKit/Api.c
    main.cpp
)

target_include_directories(ItlwmCLI
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/HeliPort/ClientKit
)

target_link_libraries(ItlwmCLI
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/libfmt.a
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/libftxui-screen.a
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/libftxui-dom.a
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/libftxui-component.a
    ${IOKIT_FRAMEWORK}
    ${COREFOUNDATION_FRAMEWORK}
)

include(GNUInstallDirs)

install(TARGETS ItlwmCLI
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/HeliPort/ClientKit/Api.h
    ${CMAKE_CURRENT_SOURCE_DIR}/HeliPort/ClientKit/Common.h
    ${CMAKE_CURRENT_SOURCE_DIR}/HeliPort/ClientKit/IoctlId.h

    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/HeliPort
)
