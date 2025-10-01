# Agent implementations - TODO: Implement specific agent types

add_library(regulens_agents INTERFACE)

target_include_directories(regulens_agents
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(regulens_agents
    INTERFACE
        regulens_core
        regulens_shared
)
