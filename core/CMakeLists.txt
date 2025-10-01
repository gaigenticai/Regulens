# Core agent library

add_library(regulens_core STATIC
    agent/agent_orchestrator.cpp
    agent/compliance_agent.cpp
)

target_include_directories(regulens_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/agent
)

target_link_libraries(regulens_core
    PUBLIC
        regulens_shared
        Threads::Threads
        ${Boost_LIBRARIES}
)
