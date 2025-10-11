# Test executable

# Find Google Test (optional)
find_package(GTest QUIET)

# Always include test infrastructure
add_executable(regulens_tests
    test_main.cpp
    infrastructure/test_framework.cpp
    infrastructure/test_environment.cpp
    example_tests.cpp
    advanced_agent_tests.cpp
    advanced_agent_test_runner.cpp
    unit_tests.cpp
    # Add more test files here as they are created
)

# API Integration Tests (separate executable)
add_executable(api_integration_tests
    api_integration_tests.cpp
)

# Production Features Integration Tests (separate executable)
add_executable(production_features_tests
    integration/production_features_test.cpp
)

target_include_directories(regulens_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/infrastructure
)

if(GTest_FOUND)
    target_link_libraries(regulens_tests
        PRIVATE
            regulens_core
            regulens_shared
            GTest::gtest
            GTest::gtest_main
    )

    # Enable test discovery
    include(GoogleTest)
    gtest_discover_tests(regulens_tests)
else()
    # Define NO_GTEST when Google Test is not available
    target_compile_definitions(regulens_tests PRIVATE NO_GTEST)
    target_link_libraries(regulens_tests
        PRIVATE
            regulens_core
            regulens_shared
    )
endif()

# Create test_main.cpp if it doesn't exist
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_main.cpp")
    file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/test_main.cpp" "
#include <iostream>

#ifdef NO_GTEST
int main() {
    std::cout << \"Tests not available - Google Test not found\" << std::endl;
    return 0;
}
#else
#include <gtest/gtest.h>

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif
")
endif()

# Advanced Agent Test Runner (standalone executable)
add_executable(regulens_advanced_tests
    advanced_agent_tests.cpp
    advanced_agent_test_runner.cpp
)

target_include_directories(regulens_advanced_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/infrastructure
)

target_link_libraries(regulens_advanced_tests
    PRIVATE
        regulens_core
        regulens_shared
)

# Link API Integration Tests
if(GTest_FOUND)
    target_link_libraries(api_integration_tests
        PRIVATE
            regulens_core
            regulens_shared
            GTest::gtest
            GTest::gtest_main
            curl
    )
    
    target_include_directories(api_integration_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
    )
    
    # Enable test discovery for integration tests
    gtest_discover_tests(api_integration_tests)
    
    # Link and discover production features tests
    target_link_libraries(production_features_tests
        PRIVATE
            regulens_core
            regulens_shared
            GTest::gtest
            GTest::gtest_main
            pqxx
            pq
    )
    
    target_include_directories(production_features_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
    )
    
    gtest_discover_tests(production_features_tests)
endif()