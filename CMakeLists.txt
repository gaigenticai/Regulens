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
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wno-c++23-attribute-extensions)
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

# Find system-installed libraries (now available via homebrew)
find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)

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
add_subdirectory(regulatory_monitor)
add_subdirectory(agents)
add_subdirectory(infrastructure)
add_subdirectory(shared/audit)
add_subdirectory(shared/event_system)
add_subdirectory(shared/tool_integration)
add_subdirectory(shared/agentic_brain)
add_subdirectory(tests)

# Create alias libraries for easier linking
add_library(regulens::shared ALIAS regulens_shared)
add_library(regulens::core ALIAS regulens_core)
add_library(regulens::agents ALIAS regulens_agents)

# Main executable
add_executable(regulens
    main.cpp
)

# Regulatory Monitoring demonstration executable
add_executable(regulatory_monitoring_demo
    core/agent/simple_orchestrator_demo.cpp
)

# Basic UI demonstration executable
add_executable(basic_ui_demo
    regulatory_monitor/regulatory_monitor_ui_demo.cpp
)

# Standalone UI demonstration executable (no external dependencies)
add_executable(standalone_ui_demo
    regulatory_monitor/regulatory_monitor_standalone_ui_demo.cpp
)

# Real Agentic AI demonstration executable
add_executable(real_agent_demo
    agents/real_agent_demo.cpp
)

# Standalone Real Agentic AI demonstration executable
add_executable(standalone_real_agent_demo
    standalone_real_agent_demo.cpp
)

# Enterprise UI demonstration executable
add_executable(enterprise_ui_demo
    regulatory_monitor/enterprise_ui_demo.cpp
)

add_executable(audit_intelligence_ui_demo
    agents/audit_intelligence/audit_intelligence_ui_demo.cpp
)

# Regulatory Assessor Agent UI demonstration executable (Rule 6 compliant)
add_executable(regulatory_assessor_ui_demo
    agents/regulatory_assessor/regulatory_assessor_demo.cpp
)

# Transaction Guardian Agent UI demonstration executable (Rule 6 compliant)
add_executable(transaction_guardian_ui_demo
    agents/transaction_guardian/transaction_guardian_demo.cpp
)

# Production Regulatory Monitoring demonstration executable
add_executable(production_regulatory_demo
    regulatory_monitor/production_regulatory_demo.cpp
)

# Complete Regulatory Monitoring & API demonstration executable
add_executable(complete_regulatory_demo
    regulatory_monitor/complete_regulatory_demo.cpp
)

# Simple UI demonstration executable
add_executable(simple_ui_demo
    regulatory_monitor/simple_ui_demo.cpp
)

target_link_libraries(regulens
    PRIVATE
        regulens_shared
        Threads::Threads
        CURL::libcurl
        ${Boost_LIBRARIES}
)

target_link_libraries(basic_ui_demo
    PRIVATE
        regulens_web_ui
        regulens_shared
        Threads::Threads
        ${Boost_LIBRARIES}
)

target_link_libraries(standalone_ui_demo
    PRIVATE
        Threads::Threads
)

target_link_libraries(real_agent_demo
    PRIVATE
        regulens_agents
        regulens_shared
        Threads::Threads
        CURL::libcurl
)

target_link_libraries(standalone_real_agent_demo
    PRIVATE
        regulens_shared
        Threads::Threads
        CURL::libcurl
)

target_link_libraries(enterprise_ui_demo
    PRIVATE
        Threads::Threads
)

target_link_libraries(audit_intelligence_ui_demo
    PRIVATE
        regulens_agents
        regulens_web_ui
        Threads::Threads
)

target_link_libraries(regulatory_assessor_ui_demo
    PRIVATE
        regulens_agents
        regulens_web_ui
        Threads::Threads
)

target_link_libraries(production_regulatory_demo
    PRIVATE
        regulens_regulatory_monitor
        regulens_shared
        Threads::Threads
)

target_link_libraries(complete_regulatory_demo
    PRIVATE
        regulens_regulatory_monitor
        regulens_shared
        Threads::Threads
)

target_link_libraries(simple_ui_demo
    PRIVATE
        Threads::Threads
)

# Data Ingestion Framework demonstration executable
add_executable(data_ingestion_demo
    data_ingestion_demo.cpp
)

target_link_libraries(data_ingestion_demo
    PRIVATE
        regulens_shared
        Threads::Threads
)

# Vector Knowledge Base demonstration executable
add_executable(vector_knowledge_base_demo
    vector_knowledge_base_demo.cpp
)

target_link_libraries(vector_knowledge_base_demo
    PRIVATE
        regulens_shared
        Threads::Threads
)

# Decision Audit & Explanation System demonstration executable
add_executable(decision_audit_demo
    decision_audit_demo.cpp
)

target_link_libraries(decision_audit_demo
    PRIVATE
        regulens_shared
        regulens_audit
        Threads::Threads
)

# Event-Driven Architecture demonstration executable
add_executable(event_driven_demo
    event_driven_demo.cpp
)

target_link_libraries(event_driven_demo
    PRIVATE
        regulens_shared
        regulens_event_system
        Threads::Threads
)

# Tool Integration Layer demonstration executable
add_executable(tool_integration_demo
    tool_integration_demo.cpp
)

target_link_libraries(tool_integration_demo
    PRIVATE
        regulens_shared
        regulens_tool_integration
        Threads::Threads
)

# Agent-Tool Integration demonstration executable
add_executable(agent_tool_integration_demo
    agent_tool_integration_demo.cpp
)

target_link_libraries(agent_tool_integration_demo
    PRIVATE
        regulens_shared
        regulens_agentic_brain
        regulens_tool_integration
        regulens_event_system
        Threads::Threads
)

# Advanced Agent Capabilities demonstration executable
add_executable(advanced_agent_demo
    advanced_agent_demo.cpp
)

target_link_libraries(advanced_agent_demo
    PRIVATE
        regulens_shared
        regulens_agentic_brain
        regulens_tool_integration
        regulens_event_system
        Threads::Threads
)

# Function Calling Integration demonstration executable
add_executable(function_calling_demo
    function_calling_demo.cpp
)

target_link_libraries(function_calling_demo
    PRIVATE
    regulens_shared
    Threads::Threads
)

# Embeddings Integration demonstration executable
add_executable(embeddings_demo
    embeddings_demo.cpp
)

target_link_libraries(embeddings_demo
    PRIVATE
    regulens_shared
    Threads::Threads
)

# Multi-Agent Communication Demo executable
add_executable(multi_agent_demo
    multi_agent_demo.cpp
)

target_link_libraries(multi_agent_demo
    PRIVATE
    regulens_shared
    Threads::Threads
)

add_executable(memory_system_demo
    memory_system_demo.cpp
)

target_link_libraries(memory_system_demo
    PRIVATE
    regulens_shared
    Threads::Threads
)

# Level 3 Capabilities Test executable
add_executable(level3_capabilities_test
    level3_capabilities_test.cpp
)

target_link_libraries(level3_capabilities_test
    PRIVATE
        regulens_shared
        regulens_agentic_brain
        regulens_tool_integration
        regulens_event_system
        Threads::Threads
)

# Level 4 Capabilities Test executable
add_executable(level4_capabilities_test
    level4_capabilities_test.cpp
)

target_link_libraries(level4_capabilities_test
    PRIVATE
        regulens_shared
        regulens_agentic_brain
        regulens_tool_integration
        regulens_event_system
        Threads::Threads
)

# Level 4 Real Implementation Demo
add_executable(level4_real_demo
    level4_real_demo.cpp
)

target_link_libraries(level4_real_demo
    PRIVATE
        regulens_shared
        Threads::Threads
)

# MCP Autonomous Integration Test
add_executable(mcp_autonomous_integration_test
    mcp_autonomous_integration_test.cpp
)

target_link_libraries(mcp_autonomous_integration_test
    PRIVATE
        regulens_shared
        regulens_agentic_brain
        regulens_tool_integration
        regulens_event_system
        Threads::Threads
)

# MCP Autonomous Integration Test (Simple)
add_executable(mcp_autonomous_integration_test_simple
    mcp_autonomous_integration_test_simple.cpp
)

target_link_libraries(mcp_autonomous_integration_test_simple
    PRIVATE
        regulens_shared
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
