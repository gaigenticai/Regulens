# Collaboration Module - Real-Time Agent Streaming
# Production-grade collaboration and WebSocket streaming

# Agent Streaming Service
add_library(agent_streaming
    agent_streaming.cpp
    agent_streaming.hpp
)

target_link_libraries(agent_streaming
    connection_pool
    structured_logger
    nlohmann_json::nlohmann_json
    PostgreSQL::PostgreSQL
)

target_include_directories(agent_streaming PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Export for parent CMakeLists
set(COLLABORATION_LIBRARIES agent_streaming PARENT_SCOPE)

