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

# PostgreSQL for database connectivity
include_directories(/opt/homebrew/include/postgresql@14)
link_directories(/opt/homebrew/lib/postgresql@14)
set(PostgreSQL_LIBRARIES pq)

# libpqxx for C++ PostgreSQL client
pkg_check_modules(PQXX REQUIRED libpqxx)
include_directories(${PQXX_INCLUDE_DIRS})
link_directories(${PQXX_LIBRARY_DIRS})

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
    /opt/homebrew/include
)

# Subdirectories for modular components
add_subdirectory(shared)
add_subdirectory(core)
add_subdirectory(data_ingestion)
add_subdirectory(regulatory_monitor)
add_subdirectory(agents)
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
