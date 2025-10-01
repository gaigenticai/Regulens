# Shared utilities library

add_library(regulens_shared STATIC
    config/configuration_manager.cpp
    logging/structured_logger.cpp
    metrics/metrics_collector.cpp
    event_processor.cpp
    knowledge_base.cpp
)

target_include_directories(regulens_shared
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/config
        ${CMAKE_CURRENT_SOURCE_DIR}/logging
        ${CMAKE_CURRENT_SOURCE_DIR}/metrics
)

target_link_libraries(regulens_shared
    PUBLIC
        nlohmann_json::nlohmann_json
        spdlog::spdlog
        Boost::system
        Boost::filesystem
        Threads::Threads
)
