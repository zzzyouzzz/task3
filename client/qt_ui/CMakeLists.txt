# qt_ui/CMakeLists.txt
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 找 Qt 库
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

file(GLOB_RECURSE SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)


add_executable(client_qt ${SOURCES})

target_link_libraries(client_qt 
    PRIVATE
        Qt6::Widgets
        Qt6::Core
        client_service)

target_include_directories(client_qt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# 设置输出目录和可执行文件名称
set_target_properties(client_qt PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output/client
    OUTPUT_NAME client_qt
)
