# client/CMakeLists.txt

file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(client_service STATIC ${SOURCES})

target_include_directories(client_service PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# 链接公共库，PUBLIC链接让链接到Common的对象也可以使用Common的符号
target_link_libraries(client_service PUBLIC Common)

if(WIN32)
    target_link_libraries(client_service PRIVATE ws2_32)
endif()

