# Regulens - Agentic AI Compliance System
# Production-grade CMake configuration for enterprise deployment

cmake_minimum_required(VERSION 3.20)
project(Regulens VERSION 1.0.0 LANGUAGES CXX)

# Enable modern C++ standards with strict compilation
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Production-grade compiler flags
if(MSVC)
    add_compile_options(/W4 /WX /permissive- /Zc:__cplusplus)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion)
endif()

# Thread sanitizer for development builds
option(ENABLE_THREAD_SANITIZER "Enable thread sanitizer" OFF)
if(ENABLE_THREAD_SANITIZER)
    add_compile_options(-fsanitize=thread)
    add_link_options(-fsanitize=thread)
endif()

# Address sanitizer for development builds
option(ENABLE_ADDRESS_SANITIZER "Enable address sanitizer" OFF)
if(ENABLE_ADDRESS_SANITIZER)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

# Enable code coverage for testing
option(ENABLE_COVERAGE "Enable code coverage" OFF)
if(ENABLE_COVERAGE)
    add_compile_options(-fprofile-arcs -ftest-coverage)
    add_link_options(-fprofile-arcs -ftest-coverage)
endif()

# Find required dependencies
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

# External dependencies (production-grade libraries)
include(FetchContent)

# spdlog for structured logging
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.11.0
)
FetchContent_MakeAvailable(spdlog)

# nlohmann/json for configuration management
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(nlohmann_json)

# Boost for enterprise-grade utilities
find_package(Boost 1.78 REQUIRED COMPONENTS
    system
    filesystem
    program_options
    date_time
    regex
)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/shared
    ${CMAKE_CURRENT_SOURCE_DIR}/core
)

# Subdirectories for modular components
add_subdirectory(shared)
add_subdirectory(core)
add_subdirectory(data_ingestion)
add_subdirectory(regulatory_monitor)
add_subdirectory(agents)
add_subdirectory(tests)

# Create alias libraries for easier linking
add_library(regulens::shared ALIAS regulens_shared)
add_library(regulens::core ALIAS regulens_core)

# Main executable
add_executable(regulens
    main.cpp
)

target_link_libraries(regulens
    PRIVATE
        regulens_core
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        Boost::system
        Boost::filesystem
        Boost::program_options
        Threads::Threads
)

# Installation configuration for cloud deployment
include(GNUInstallDirs)
install(TARGETS regulens
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Package configuration for different platforms
include(CPack)
set(CPACK_PACKAGE_NAME "regulens")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Agentic AI Compliance System")
set(CPACK_PACKAGE_VENDOR "Gaigentic AI")
set(CPACK_PACKAGE_CONTACT "support@gaigentic.ai")

# Platform-specific packaging
if(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "DEB;RPM")
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
elseif(WIN32)
    set(CPACK_GENERATOR "NSIS")
endif()

# Testing configuration
enable_testing()
add_test(NAME regulens_unit_tests COMMAND regulens_tests)

# Documentation generation (optional)
find_package(Doxygen)
if(DOXYGEN_FOUND)
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIZE
    )
endif()
