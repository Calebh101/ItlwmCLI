cmake_minimum_required(VERSION 3.16)
project(ItlwmCLI LANGUAGES CXX)
enable_language(C)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

list(APPEND CMAKE_PREFIX_PATH "/usr/local/opt/ftxui")
find_package(ftxui REQUIRED)
set_source_files_properties(HeliPort/ClientKit/Api.c PROPERTIES LANGUAGE C)

find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
if(NOT COREFOUNDATION_FRAMEWORK)
    message(FATAL_ERROR "CoreFoundation framework not found")
endif()

add_executable(ItlwmCLI
    main.cpp
    HeliPort/ClientKit/Api.c
    Api.h
)

target_include_directories(ItlwmCLI
    PRIVATE
    /usr/local/include
    /usr/local/opt
    ${CMAKE_CURRENT_SOURCE_DIR}/HeliPort/ClientKit
)

target_link_libraries(ItlwmCLI
    PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    ftxui::screen
    ftxui::dom
    ftxui::component
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
