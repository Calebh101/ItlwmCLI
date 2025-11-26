cmake_minimum_required(VERSION 3.16)
project(ItlwmCLI LANGUAGES CXX)
enable_language(C)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

list(APPEND CMAKE_PREFIX_PATH "${SMAKE_CURRENT_SOURCE_DIR}/../lib")
set_source_files_properties(HeliPort/ClientKit/Api.c PROPERTIES LANGUAGE C)

find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
if(NOT COREFOUNDATION_FRAMEWORK)
    message(FATAL_ERROR "CoreFoundation framework not found")
endif()

find_library(IOKIT_FRAMEWORK IOKit)
if(NOT IOKIT_FRAMEWORK)
    message(FATAL_ERROR "IOKit framework not found")
endif()

add_executable(ItlwmCLI
    ../HeliPort/ClientKit/Api.c
    main.cpp
)

target_include_directories(ItlwmCLI
    PRIVATE
    ${CMAKE_SOURCE_DIR}/../include
    ${CMAKE_SOURCE_DIR}/../HeliPort/ClientKit
)

target_link_libraries(ItlwmCLI
    PRIVATE
    ${IOKIT_FRAMEWORK}
    ${COREFOUNDATION_FRAMEWORK}
)

include(GNUInstallDirs)

install(TARGETS ItlwmCLI
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/../HeliPort/ClientKit/Api.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../HeliPort/ClientKit/Common.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../HeliPort/ClientKit/IoctlId.h

    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/../HeliPort
)
