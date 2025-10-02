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
    # Add more test files here as they are created
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