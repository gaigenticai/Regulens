# Agentic brain library

add_library(regulens_agentic_brain STATIC
    agentic_orchestrator.cpp
    consensus_engine.cpp
    consensus_engine_api_handlers.cpp
    advanced_rule_engine.cpp
    advanced_rule_engine_api_handlers.cpp
    communication_mediator.cpp
    communication_mediator_api_handlers.cpp
    message_translator.cpp
    message_translator_api_handlers.cpp
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
