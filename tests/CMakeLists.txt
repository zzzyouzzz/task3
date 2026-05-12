file(GLOB TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(CommonTests ${CMAKE_SOURCE_DIR}/tests/common_tests.cpp)
add_executable(ServerTests ${CMAKE_SOURCE_DIR}/tests/server_tests.cpp
    ${CMAKE_SOURCE_DIR}/server/src/FileManager.cpp
    ${CMAKE_SOURCE_DIR}/server/src/LogisticsSystem.cpp)
add_executable(DataSetTests ${CMAKE_SOURCE_DIR}/tests/dataset_tests.cpp
    ${CMAKE_SOURCE_DIR}/server/src/FileManager.cpp)

target_include_directories(CommonTests PRIVATE ${CMAKE_SOURCE_DIR}/Common/include)
target_link_libraries(CommonTests PRIVATE Common)

target_include_directories(ServerTests PRIVATE ${CMAKE_SOURCE_DIR}/Common/include ${CMAKE_SOURCE_DIR}/server/include)
target_link_libraries(ServerTests PRIVATE Common)

target_include_directories(DataSetTests PRIVATE ${CMAKE_SOURCE_DIR}/Common/include ${CMAKE_SOURCE_DIR}/server/include)
target_link_libraries(DataSetTests PRIVATE Common)

set_target_properties(CommonTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output/tests
    OUTPUT_NAME CommonTests
)
set_target_properties(ServerTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output/tests
    OUTPUT_NAME ServerTests
)
set_target_properties(DataSetTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output/tests
    OUTPUT_NAME DataSetTests
)

add_test(NAME CommonTests COMMAND CommonTests)
add_test(NAME ServerTests COMMAND ServerTests)
add_test(NAME DataSetTests COMMAND DataSetTests)
