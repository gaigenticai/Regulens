# Test executable

# Find Google Test (optional)
find_package(GTest QUIET)

if(GTest_FOUND)
    add_executable(regulens_tests
        # TODO: Add test files here
        test_main.cpp
    )

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
    # Create a dummy test target if GTest is not available
    add_executable(regulens_tests test_main.cpp)
    target_compile_definitions(regulens_tests PRIVATE NO_GTEST)
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


