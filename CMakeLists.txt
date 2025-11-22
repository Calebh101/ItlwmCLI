cmake_minimum_required(VERSION 3.16)
project(ItlwmCLI LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

add_executable(ItlwmCLI
  main.cpp
)

target_include_directories(ItlwmCLI PRIVATE /usr/local/include)
target_link_libraries(ItlwmCLI Qt${QT_VERSION_MAJOR}::Core ftxui-dom ftxui-component ftxui-screen)

include(GNUInstallDirs)
install(TARGETS ItlwmCLI
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION HeliPort/ClientKit/*
)
