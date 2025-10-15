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
    add_compile_options(/W4 /permissive- /Zc:__cplusplus)
else()
    # Disable warnings as errors to allow building with warnings for now
    add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-c++23-attribute-extensions -Wno-unused-parameter -Wno-vexing-parse)
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
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)

# Find system-installed libraries (now available via homebrew)
find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)

# Add nlohmann_json include directory for all targets
include_directories(${nlohmann_json_INCLUDE_DIRS})
include_directories(/opt/homebrew/include)

# PostgreSQL for database connectivity - Cross-platform detection
# Support multiple installation methods for cloud deployment
if(DEFINED ENV{POSTGRESQL_ROOT})
    # Custom PostgreSQL installation path (for cloud/on-prem deployments)
    set(PostgreSQL_ROOT $ENV{POSTGRESQL_ROOT})
    find_path(PostgreSQL_INCLUDE_DIR libpq-fe.h
        PATHS ${PostgreSQL_ROOT}/include
        NO_DEFAULT_PATH
    )
    find_library(PostgreSQL_LIBRARY pq
        PATHS ${PostgreSQL_ROOT}/lib
        NO_DEFAULT_PATH
    )
    if(PostgreSQL_INCLUDE_DIR AND PostgreSQL_LIBRARY)
        set(PostgreSQL_FOUND TRUE)
        set(PostgreSQL_INCLUDE_DIRS ${PostgreSQL_INCLUDE_DIR})
        set(PostgreSQL_LIBRARIES ${PostgreSQL_LIBRARY})
        include_directories(${PostgreSQL_INCLUDE_DIRS})
        message(STATUS "Using custom PostgreSQL installation at ${PostgreSQL_ROOT}")
    endif()
elseif(UNIX AND NOT APPLE)
    # Linux: Use standard system paths or pkg-config
    pkg_check_modules(PostgreSQL_PKG libpq)
    if(PostgreSQL_PKG_FOUND)
        set(PostgreSQL_FOUND TRUE)
        set(PostgreSQL_INCLUDE_DIRS ${PostgreSQL_PKG_INCLUDE_DIRS})
        set(PostgreSQL_LIBRARIES ${PostgreSQL_PKG_LIBRARIES})
        include_directories(${PostgreSQL_INCLUDE_DIRS})
        link_directories(${PostgreSQL_PKG_LIBRARY_DIRS})
    else()
        # Fallback to standard Linux paths
        find_path(PostgreSQL_INCLUDE_DIR libpq-fe.h
            PATHS /usr/include /usr/local/include /usr/include/postgresql
        )
        find_library(PostgreSQL_LIBRARY pq
            PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu
        )
        if(PostgreSQL_INCLUDE_DIR AND PostgreSQL_LIBRARY)
            set(PostgreSQL_FOUND TRUE)
            set(PostgreSQL_INCLUDE_DIRS ${PostgreSQL_INCLUDE_DIR})
            set(PostgreSQL_LIBRARIES ${PostgreSQL_LIBRARY})
            include_directories(${PostgreSQL_INCLUDE_DIRS})
        endif()
    endif()
else()
    # macOS/Windows: Use CMake find_package (may find Homebrew on macOS)
    find_package(PostgreSQL REQUIRED)
    if(PostgreSQL_FOUND)
        include_directories(${PostgreSQL_INCLUDE_DIRS})
        link_directories(${PostgreSQL_LIBRARY_DIRS})
        set(PostgreSQL_LIBRARIES ${PostgreSQL_LIBRARIES})
    endif()
endif()

if(NOT PostgreSQL_FOUND)
    message(FATAL_ERROR "PostgreSQL not found. Please install PostgreSQL development libraries or set POSTGRESQL_ROOT environment variable for custom installations.")
endif()

# libpqxx for C++ PostgreSQL client - Cross-platform detection
if(DEFINED ENV{PQXX_ROOT})
    # Custom libpqxx installation path (for cloud/on-prem deployments)
    set(PQXX_ROOT $ENV{PQXX_ROOT})
    find_path(PQXX_INCLUDE_DIR pqxx/pqxx
        PATHS ${PQXX_ROOT}/include
        NO_DEFAULT_PATH
    )
    find_library(PQXX_LIBRARY pqxx
        PATHS ${PQXX_ROOT}/lib
        NO_DEFAULT_PATH
    )
    if(PQXX_INCLUDE_DIR AND PQXX_LIBRARY)
        set(PQXX_FOUND TRUE)
        set(PQXX_INCLUDE_DIRS ${PQXX_INCLUDE_DIR})
        set(PQXX_LIBRARIES ${PQXX_LIBRARY})
        include_directories(${PQXX_INCLUDE_DIRS})
        link_directories(${PQXX_ROOT}/lib)
        message(STATUS "Using custom libpqxx installation at ${PQXX_ROOT}")
    endif()
elseif(UNIX AND NOT APPLE)
    # Linux: Use standard system paths
    pkg_check_modules(PQXX_PKG libpqxx)
    if(PQXX_PKG_FOUND)
        set(PQXX_FOUND TRUE)
        set(PQXX_INCLUDE_DIRS ${PQXX_PKG_INCLUDE_DIRS})
        set(PQXX_LIBRARIES ${PQXX_PKG_LIBRARIES})
        include_directories(${PQXX_INCLUDE_DIRS})
        link_directories(${PQXX_PKG_LIBRARY_DIRS})
    else()
        # Fallback to standard Linux paths
        find_path(PQXX_INCLUDE_DIR pqxx/pqxx
            PATHS /usr/include /usr/local/include
        )
        find_library(PQXX_LIBRARY pqxx
            PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu
        )
        if(PQXX_INCLUDE_DIR AND PQXX_LIBRARY)
            set(PQXX_FOUND TRUE)
            set(PQXX_INCLUDE_DIRS ${PQXX_INCLUDE_DIR})
            set(PQXX_LIBRARIES ${PQXX_LIBRARY})
            include_directories(${PQXX_INCLUDE_DIRS})
        endif()
    endif()
else()
    # macOS/Windows: Use pkg-config
    pkg_check_modules(PQXX REQUIRED libpqxx)
    include_directories(${PQXX_INCLUDE_DIRS})
    link_directories(${PQXX_LIBRARY_DIRS})
endif()

if(NOT PQXX_FOUND)
    message(FATAL_ERROR "libpqxx not found. Please install libpqxx development libraries or set PQXX_ROOT environment variable for custom installations.")
endif()

# Boost for enterprise-grade utilities
find_package(Boost 1.78 QUIET COMPONENTS
    system
    filesystem
    program_options
    date_time
    regex
)

if(NOT Boost_FOUND)
    message(WARNING "Boost not found. Some features will be limited.")
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/shared
    ${CMAKE_CURRENT_SOURCE_DIR}/core
)

# Subdirectories for modular components
add_subdirectory(shared)
add_subdirectory(core)
add_subdirectory(data_ingestion)
# add_subdirectory(regulatory_monitor)  # Temporarily disabled due to missing dependencies
add_subdirectory(agents)
# Infrastructure (K8s operator) temporarily disabled for core build
# Temporarily disabled due to missing prometheus_client.hpp dependency
# add_subdirectory(infrastructure)
add_subdirectory(shared/audit)
add_subdirectory(shared/event_system)
add_subdirectory(shared/tool_integration)
add_subdirectory(shared/agentic_brain)
add_subdirectory(tests)

# Create alias libraries for easier linking
add_library(regulens::shared ALIAS regulens_shared)
add_library(regulens::core ALIAS regulens_core)
add_library(regulens::agents ALIAS regulens_agents)

# Main executable - Production Agentic AI Compliance System
add_executable(regulens
    main.cpp
)

# Link the main executable
target_link_libraries(regulens
    PRIVATE
        regulens_shared
        regulens_regulatory_monitor
        regulens_web_ui
        regulens_audit
        regulens_agents
        Threads::Threads
        CURL::libcurl
        ${Boost_LIBRARIES}
)

# Installation configuration for cloud deployment
include(GNUInstallDirs)
install(TARGETS regulens
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Package configuration for different platforms
include(CPack)
