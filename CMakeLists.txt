cmake_minimum_required(VERSION 3.16)
project(task3)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

add_subdirectory(Common)

add_subdirectory(client/service)

add_subdirectory(server)
add_subdirectory(client/cli)
#add_subdirectory(client/qt_ui)


