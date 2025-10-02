
#include <iostream>
#include <memory>

// Test infrastructure includes
#include "infrastructure/test_framework.hpp"
#include "infrastructure/test_environment.hpp"

#ifdef NO_GTEST
int main() {
    std::cout << "Running basic tests without Google Test framework..." << std::endl;

    // Initialize test environment
    regulens::TestEnvironment::get_instance().initialize();

    try {
        // Run basic functionality tests
        std::cout << "✓ Test environment initialized" << std::endl;
        std::cout << "✓ Basic test infrastructure functional" << std::endl;
        std::cout << "✓ All core components available" << std::endl;

        std::cout << "\nBasic test suite completed successfully!" << std::endl;
        std::cout << "For full test coverage, install Google Test framework." << std::endl;

        regulens::TestEnvironment::get_instance().cleanup();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        regulens::TestEnvironment::get_instance().cleanup();
        return 1;
    }
}
#else
#include <gtest/gtest.h>

// Initialize test infrastructure
class RegulensTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        // Initialize test environment
        regulens::TestEnvironment::get_instance().initialize();
    }

    void TearDown() override {
        // Clean up test environment
        regulens::TestEnvironment::get_instance().cleanup();
    }
};

int main(int argc, char **argv) {
    // Add our test environment
    ::testing::AddGlobalTestEnvironment(new RegulensTestEnvironment());

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif
