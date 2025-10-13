# Shared utilities library

add_library(regulens_shared STATIC
    config/configuration_manager.cpp
    config/environment_validator.cpp
    database/postgresql_connection.cpp
    logging/structured_logger.cpp
    network/http_client.cpp
    event_processor.cpp
    knowledge_base.cpp
    knowledge_base/vector_knowledge_base.cpp
    regulatory_knowledge_base.cpp
    tool_integration/tool_interface.cpp
    visualization/decision_tree_visualizer.cpp
    agent_activity_feed.cpp
    human_ai_collaboration.cpp
    pattern_recognition.cpp
    feedback_incorporation.cpp
    error_handler.cpp
    llm/openai_client.cpp
    llm/anthropic_client.cpp
    llm/function_calling.cpp
    llm/compliance_functions.cpp
    llm/embeddings_client.cpp
    llm/streaming_handler.cpp
    risk_assessment.cpp
    decision_tree_optimizer.cpp
    metrics/metrics_collector.cpp
    memory/conversation_memory.cpp
    memory/learning_engine.cpp
    memory/memory_manager.cpp
    memory/case_based_reasoning.cpp
    # Inter-Agent Communication System - Requires proper pqxx integration implementation
    # TODO: Implement InterAgentCommunicator with PostgreSQLConnection interface
    tool_integration/tools/mcp_tool.cpp
    tool_integration/tools/email_tool.cpp
    tool_integration/tools/web_search_tool.cpp
    data_ingestion/data_ingestion_framework.cpp
    data_ingestion/sources/rest_api_source.cpp
    data_ingestion/sources/web_scraping_source.cpp
    data_ingestion/sources/database_source.cpp
    data_ingestion/pipelines/standard_ingestion_pipeline.cpp
    data_ingestion/storage/postgresql_storage_adapter.cpp
    data_ingestion/ingestion_metrics.cpp
    # Authentication module - JWT token parsing and validation (Rule 1 compliance - production-grade security)
    auth/jwt_parser.cpp
    auth/auth_helpers.cpp
    # API Handler modules - Production-grade business logic integration
    decisions/decision_api_handlers.cpp
    transactions/transaction_api_handlers.cpp
    patterns/pattern_api_handlers.cpp
    knowledge_base/knowledge_api_handlers.cpp
    knowledge_base/semantic_search_api_handlers.cpp
    llm/llm_api_handlers.cpp
    llm/chatbot_service.cpp
    llm/text_analysis_service.cpp
    llm/text_analysis_api_handlers.cpp
    llm/policy_generation_service.cpp
    llm/policy_generation_api_handlers.cpp
    config/dynamic_config_manager.cpp
    config/dynamic_config_api_handlers.cpp
    rules/advanced_rule_engine.cpp
    rules/advanced_rule_engine_api_handlers.cpp
    agentic_brain/consensus_engine.cpp
    agentic_brain/consensus_engine_api_handlers.cpp
    agentic_brain/message_translator.cpp
    agentic_brain/message_translator_api_handlers.cpp
    agentic_brain/communication_mediator.cpp
    agentic_brain/communication_mediator_api_handlers.cpp
    tools/tool_categories.cpp
    tools/tool_categories_api_handlers.cpp
    fraud_detection/fraud_api_handlers.cpp
    fraud_detection/fraud_scan_worker.cpp
    data_ingestion/ingestion_api_handlers.cpp
)

# Resilience module for fault tolerance (Rule 1 compliance - production-grade resilience)
add_subdirectory(resilience)

# Metrics module for monitoring and observability (Rule 1 compliance - enterprise monitoring)
add_subdirectory(metrics)

# Cache module for Redis-based distributed caching (Rule 1 compliance - enterprise performance)
add_subdirectory(cache)

# Web UI library for testing (Rule 6 compliance)
add_subdirectory(web_ui)

target_include_directories(regulens_shared
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/config
        ${CMAKE_CURRENT_SOURCE_DIR}/database
        ${CMAKE_CURRENT_SOURCE_DIR}/logging
        ${CMAKE_CURRENT_SOURCE_DIR}/models
        ${CMAKE_CURRENT_SOURCE_DIR}/network
        ${CMAKE_CURRENT_SOURCE_DIR}/knowledge_base
        ${CMAKE_CURRENT_SOURCE_DIR}/tool_integration
        ${CMAKE_CURRENT_SOURCE_DIR}/tool_integration/tools
        ${CMAKE_CURRENT_SOURCE_DIR}/data_ingestion
        ${CMAKE_CURRENT_SOURCE_DIR}/visualization
        ${CMAKE_CURRENT_SOURCE_DIR}/decisions
        ${CMAKE_CURRENT_SOURCE_DIR}/transactions
        ${CMAKE_CURRENT_SOURCE_DIR}/patterns
        ${CMAKE_CURRENT_SOURCE_DIR}/llm
        ${CMAKE_CURRENT_SOURCE_DIR}/fraud_detection
        ${CMAKE_CURRENT_SOURCE_DIR}/auth
)

target_link_libraries(regulens_shared
    PUBLIC
        Threads::Threads
        ${Boost_LIBRARIES}
        ${PostgreSQL_LIBRARIES}
        ${PQXX_LIBRARIES}
        CURL::libcurl
        spdlog::spdlog
        OpenSSL::SSL
        OpenSSL::Crypto
        resilience
        metrics
        cache
)
