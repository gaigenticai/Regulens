
#include <iostream>

#ifdef NO_GTEST
int main() {
    std::cout << "Tests not available - Google Test not found" << std::endl;
    return 0;
}
#else
#include <gtest/gtest.h>

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif
