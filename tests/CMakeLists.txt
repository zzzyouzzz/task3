add_executable(CommonTests ${CMAKE_SOURCE_DIR}/tests/common_tests.cpp)
add_executable(DataSetTests ${CMAKE_SOURCE_DIR}/tests/dataset_tests.cpp
    ${CMAKE_SOURCE_DIR}/server/src/FileManager.cpp)
add_executable(IntegrationTest ${CMAKE_SOURCE_DIR}/tests/test_runner.cpp)

target_include_directories(CommonTests PRIVATE ${CMAKE_SOURCE_DIR}/Common/include)
target_link_libraries(CommonTests PRIVATE Common)

target_include_directories(DataSetTests PRIVATE ${CMAKE_SOURCE_DIR}/Common/include ${CMAKE_SOURCE_DIR}/server/include)
target_link_libraries(DataSetTests PRIVATE Common)

# 全流程自动化集成测试: 链接 client_service 直接调用 Communication API
target_include_directories(IntegrationTest PRIVATE
    ${CMAKE_SOURCE_DIR}/Common/include
    ${CMAKE_SOURCE_DIR}/client/service/include)
target_link_libraries(IntegrationTest PRIVATE client_service Common)
if(WIN32)
    target_link_libraries(IntegrationTest PRIVATE ws2_32)
endif()

set_target_properties(CommonTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output/tests
    OUTPUT_NAME CommonTests
)
set_target_properties(DataSetTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output/tests
    OUTPUT_NAME DataSetTests
)
set_target_properties(IntegrationTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output/tests
    OUTPUT_NAME IntegrationTest
)

# 多客户端并发连接公平性测试
add_executable(MultiClientTests
    ${CMAKE_SOURCE_DIR}/tests/multi_client_tests.cpp
    ${CMAKE_SOURCE_DIR}/tests/test_server_helper.cpp
)
target_include_directories(MultiClientTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Common/include
    ${CMAKE_SOURCE_DIR}/client/service/include
    ${CMAKE_SOURCE_DIR}/tests
)
target_link_libraries(MultiClientTests PRIVATE client_service Common)
if(WIN32)
    target_link_libraries(MultiClientTests PRIVATE ws2_32)
endif()
set_target_properties(MultiClientTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output/tests
    OUTPUT_NAME MultiClientTests
)
add_test(NAME MultiClientTests COMMAND MultiClientTests
    --server=${CMAKE_SOURCE_DIR}/output/server/server.exe
    --workdir=${CMAKE_SOURCE_DIR}/output/test_multi_run)

add_test(NAME CommonTests COMMAND CommonTests)
add_test(NAME DataSetTests COMMAND DataSetTests)
add_test(NAME IntegrationTest COMMAND IntegrationTest
    --input=${CMAKE_SOURCE_DIR}/tests/full_scenario.in
    --server=${CMAKE_SOURCE_DIR}/output/server/server.exe
    --workdir=${CMAKE_SOURCE_DIR}/output/test_run)
