# client/cli/CMakeLists.txt

file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(client_cli ${SOURCES})

target_link_libraries(client_cli PRIVATE client_service)

target_include_directories(client_cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

set_target_properties(client_cli PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output/client
    OUTPUT_NAME client_cli
)

