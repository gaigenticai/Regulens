# Agentic brain library

add_library(regulens_agentic_brain STATIC
    agentic_orchestrator.cpp
    consensus_engine.cpp
)

target_include_directories(regulens_agentic_brain
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(regulens_agentic_brain
    PUBLIC
        regulens_shared
        Threads::Threads
)
