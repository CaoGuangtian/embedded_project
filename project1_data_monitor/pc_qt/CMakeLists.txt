cmake_minimum_required(VERSION 3.16)

project(Project1Monitor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 QUIET COMPONENTS Widgets Network)
if(Qt6_FOUND)
    set(QT_PACKAGE Qt6)
else()
    find_package(Qt5 REQUIRED COMPONENTS Widgets Network)
    set(QT_PACKAGE Qt5)
endif()

add_executable(project1_monitor
    main.cpp
    mainwindow.cpp
    mainwindow.h
)

target_link_libraries(project1_monitor PRIVATE
    ${QT_PACKAGE}::Widgets
    ${QT_PACKAGE}::Network
)

