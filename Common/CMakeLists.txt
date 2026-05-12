# Common/CMakeLists.txt

file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(Common STATIC ${SOURCES})

target_include_directories(Common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)