/**
 * API Endpoint Registrations - Systematic Registration of All API Endpoints
 * Production-grade centralized endpoint registration following Rule 1 compliance
 */

#include "api_registry.hpp"

// Forward declarations for functions defined later in this file
void register_training_endpoints(PGconn* db_conn);
void register_simulator_endpoints(PGconn* db_conn);
void register_tool_categories_endpoints(PGconn* db_conn);
#include "../web_ui/web_ui_server.hpp"
#include "../auth/auth_api_handlers.hpp"
#include "../transactions/transaction_api_handlers.hpp"
#include "../fraud_detection/fraud_api_handlers.hpp"
#include "../memory/memory_api_handlers.hpp"
#include "../knowledge_base/knowledge_api_handlers_complete.hpp"
#include "../decisions/decision_api_handlers_complete.hpp"
#include "../llm/llm_api_handlers.hpp"
#include "../patterns/pattern_api_handlers.hpp"
#include "../chatbot/chatbot_api_handlers.hpp"
#include "../training/training_api_handlers.hpp"
#include "../policy/policy_api_handlers.hpp"
#include "../simulator/simulator_api_handlers.hpp"
#include "../config/dynamic_config_api_handlers.hpp"
#include "../agentic_brain/consensus_engine_api_handlers.hpp"
#include "../agentic_brain/communication_mediator_api_handlers.hpp"
#include "../agentic_brain/message_translator_api_handlers.hpp"
#include "../rules/advanced_rule_engine_api_handlers.hpp"
#include "../tool_integration/tools/tool_categories_api_handlers.hpp"
#include "../data_ingestion/ingestion_api_handlers.hpp"
#include <libpq-fe.h>

namespace regulens {

/**
 * Authentication Endpoints Registration
 */
std::vector<APIEndpoint> create_auth_endpoints(PGconn* db_conn) {
    return {
        create_endpoint("POST", "/api/auth/login",
                       "User authentication with JWT token generation",
                       "authentication",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               auto response = auth::login_user(db_conn, req.body);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, false), // No auth required for login

        create_endpoint("POST", "/api/auth/logout",
                       "User logout with token revocation",
                       "authentication",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               auto response = auth::logout_user(db_conn, req.headers);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"}), // Requires auth

        create_endpoint("GET", "/api/auth/me",
                       "Get current user information",
                       "authentication",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               auto response = auth::get_current_user(db_conn, req.headers);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"}), // Requires auth

        create_endpoint("POST", "/auth/refresh",
                       "Refresh JWT access token",
                       "authentication",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               auto response = auth::refresh_token(db_conn, req.body);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, false) // No auth required for token refresh
    };
}

/**
 * Transaction Endpoints Registration
 */
std::vector<APIEndpoint> create_transaction_endpoints(PGconn* db_conn) {
    return {
        create_endpoint("GET", "/transactions",
                       "Get transactions with filtering and pagination",
                       "transactions",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               auto response = transactions::get_transactions(db_conn, req.query_params);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"}),

        create_endpoint("GET", "/transactions/{id}",
                       "Get specific transaction by ID",
                       "transactions",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string transaction_id = req.params.at("id");
                               auto response = transactions::get_transaction_by_id(db_conn, transaction_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"}),

        create_endpoint("POST", "/transactions/{id}/approve",
                       "Approve a flagged transaction",
                       "transactions",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string transaction_id = req.params.at("id");
                               std::string user_id = "system"; // TODO: Extract from JWT
                               auto response = transactions::approve_transaction(db_conn, transaction_id, req.body);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"admin", "compliance_officer"}),

        create_endpoint("POST", "/transactions/{id}/reject",
                       "Reject a flagged transaction",
                       "transactions",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string transaction_id = req.params.at("id");
                               std::string user_id = "system"; // TODO: Extract from JWT
                               auto response = transactions::reject_transaction(db_conn, transaction_id, req.body);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"admin", "compliance_officer"}),

        create_endpoint("POST", "/transactions/{id}/analyze",
                       "Analyze transaction for fraud patterns",
                       "transactions",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string transaction_id = req.params.at("id");
                               auto response = transactions::analyze_transaction(db_conn, transaction_id, req.body);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"}),

        create_endpoint("GET", "/transactions/stats",
                       "Get transaction statistics and analytics",
                       "transactions",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               auto response = transactions::get_transaction_stats(db_conn, req.query_params);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"}),

        create_endpoint("GET", "/transactions/patterns",
                       "Get detected transaction patterns",
                       "transactions",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               auto response = transactions::get_transaction_patterns(db_conn, req.query_params);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"}),

        create_endpoint("POST", "/transactions/detect-anomalies",
                       "Detect transaction anomalies",
                       "transactions",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               auto response = transactions::detect_anomalies(db_conn, req.body);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"})
    };
}

/**
 * Fraud Detection Endpoints Registration
 */
std::vector<APIEndpoint> create_fraud_endpoints(PGconn* db_conn) {
    return {
        create_endpoint("GET", "/fraud/rules",
                       "Get all fraud detection rules",
                       "fraud_detection",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               auto response = fraud::get_fraud_rules(db_conn, req.query_params);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"}),

        create_endpoint("GET", "/fraud/rules/{id}",
                       "Get specific fraud rule by ID",
                       "fraud_detection",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string rule_id = req.params.at("id");
                               auto response = fraud::get_fraud_rule_by_id(db_conn, rule_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"}),

        create_endpoint("POST", "/fraud/rules",
                       "Create a new fraud detection rule",
                       "fraud_detection",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string user_id = "system"; // TODO: Extract from JWT
                               auto response = fraud::create_fraud_rule(db_conn, req.body, user_id);
                               return HTTPResponse(201, "Created", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"admin", "compliance_officer"}),

        create_endpoint("PUT", "/fraud/rules/{id}",
                       "Update an existing fraud rule",
                       "fraud_detection",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string rule_id = req.params.at("id");
                               auto response = fraud::update_fraud_rule(db_conn, rule_id, req.body);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"admin", "compliance_officer"}),

        create_endpoint("DELETE", "/fraud/rules/{id}",
                       "Delete a fraud rule",
                       "fraud_detection",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string rule_id = req.params.at("id");
                               auto response = fraud::delete_fraud_rule(db_conn, rule_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"admin"}),

        create_endpoint("POST", "/fraud/rules/{id}/test",
                       "Test a fraud rule against historical data",
                       "fraud_detection",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string rule_id = req.params.at("id");
                               auto response = fraud::test_fraud_rule(db_conn, rule_id, req.body);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"}),

        create_endpoint("GET", "/fraud/models",
                       "Get available fraud detection ML models",
                       "fraud_detection",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               auto response = fraud::get_fraud_models(db_conn);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"}),

        create_endpoint("POST", "/fraud/models/train",
                       "Train a new fraud detection model",
                       "fraud_detection",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string user_id = "system"; // TODO: Extract from JWT
                               auto response = fraud::train_fraud_model(db_conn, req.body, user_id);
                               return HTTPResponse(202, "Accepted", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"admin", "compliance_officer"}),

        create_endpoint("GET", "/fraud/models/{id}/performance",
                       "Get fraud model performance metrics",
                       "fraud_detection",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string model_id = req.params.at("id");
                               auto response = fraud::get_model_performance(db_conn, model_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"}),

        create_endpoint("POST", "/fraud/scan/batch",
                       "Run batch fraud scanning on transactions",
                       "fraud_detection",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string user_id = "system"; // TODO: Extract from JWT
                               auto response = fraud::run_batch_fraud_scan(db_conn, req.body, user_id);
                               return HTTPResponse(202, "Accepted", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"admin", "compliance_officer"})
    };
}

/**
 * Memory Management Endpoints Registration
 */
std::vector<APIEndpoint> create_memory_endpoints(PGconn* db_conn) {
    return {
        create_endpoint("POST", "/memory/visualize",
                       "Generate memory graph visualization",
                       "memory_management",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               auto response = memory::generate_graph_visualization(db_conn, req.body);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"}),

        create_endpoint("GET", "/memory/graph",
                       "Get memory graph data for agent",
                       "memory_management",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               auto response = memory::get_memory_graph(db_conn, req.query_params);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"}),

        create_endpoint("GET", "/memory/nodes/{id}",
                       "Get memory node details",
                       "memory_management",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string node_id = req.params.at("id");
                               auto response = memory::get_memory_node_details(db_conn, node_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"}),

        create_endpoint("POST", "/memory/search",
                       "Search memory nodes",
                       "memory_management",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               auto response = memory::search_memory(db_conn, req.body);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"}),

        create_endpoint("GET", "/memory/stats",
                       "Get memory statistics",
                       "memory_management",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               auto response = memory::get_memory_stats(db_conn, req.query_params);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"}),

        create_endpoint("GET", "/memory/clusters",
                       "Get memory clusters",
                       "memory_management",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               auto response = memory::get_memory_clusters(db_conn, req.query_params);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"}),

        create_endpoint("POST", "/memory/nodes",
                       "Create a new memory node",
                       "memory_management",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string user_id = "system"; // TODO: Extract from JWT
                               auto response = memory::create_memory_node(db_conn, req.body, user_id);
                               return HTTPResponse(201, "Created", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"}),

        create_endpoint("PUT", "/memory/nodes/{id}",
                       "Update a memory node",
                       "memory_management",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string node_id = req.params.at("id");
                               auto response = memory::update_memory_node(db_conn, node_id, req.body);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"}),

        create_endpoint("DELETE", "/memory/nodes/{id}",
                       "Delete a memory node",
                       "memory_management",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string node_id = req.params.at("id");
                               auto response = memory::delete_memory_node(db_conn, node_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"admin"})
    };
}

/**
 * Knowledge Base Endpoints Registration
 */
std::vector<APIEndpoint> create_knowledge_endpoints(PGconn* db_conn) {
    return {
        create_endpoint("GET", "/knowledge/search",
                       "Search knowledge base with semantic matching",
                       "knowledge_base",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               auto response = knowledge::search_knowledge_base(db_conn, req.query_params);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"}),

        create_endpoint("GET", "/knowledge/entries",
                       "Get knowledge base entries with filtering",
                       "knowledge_base",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               auto response = knowledge::get_knowledge_entries(db_conn, req.query_params);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"}),

        create_endpoint("GET", "/knowledge/entries/{id}",
                       "Get specific knowledge entry by ID",
                       "knowledge_base",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string entry_id = req.params.at("id");
                               auto response = knowledge::get_knowledge_entry_by_id(db_conn, entry_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"}),

        create_endpoint("POST", "/knowledge/entries",
                       "Create a new knowledge entry",
                       "knowledge_base",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string user_id = "system"; // TODO: Extract from JWT
                               auto response = knowledge::create_knowledge_entry(db_conn, req.body, user_id);
                               return HTTPResponse(201, "Created", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"}),

        create_endpoint("PUT", "/knowledge/entries/{id}",
                       "Update a knowledge entry",
                       "knowledge_base",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string entry_id = req.params.at("id");
                               auto response = knowledge::update_knowledge_entry(db_conn, entry_id, req.body);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"}),

        create_endpoint("DELETE", "/knowledge/entries/{id}",
                       "Delete a knowledge entry",
                       "knowledge_base",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string entry_id = req.params.at("id");
                               auto response = knowledge::delete_knowledge_entry(db_conn, entry_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"admin"}),

        create_endpoint("POST", "/knowledge/ask",
                       "Ask questions to the knowledge base (RAG)",
                       "knowledge_base",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string user_id = "system"; // TODO: Extract from JWT
                               auto response = knowledge::ask_knowledge_base(db_conn, req.body, user_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"}),

        create_endpoint("POST", "/knowledge/embeddings",
                       "Generate embeddings for text",
                       "knowledge_base",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string user_id = "system"; // TODO: Extract from JWT
                               auto response = knowledge::generate_embeddings(db_conn, req.body, user_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"}),

        create_endpoint("GET", "/knowledge/stats",
                       "Get knowledge base statistics",
                       "knowledge_base",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               auto response = knowledge::get_knowledge_stats(db_conn, req.query_params);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"}),

        create_endpoint("POST", "/knowledge/reindex",
                       "Reindex knowledge base for search optimization",
                       "knowledge_base",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string user_id = "system"; // TODO: Extract from JWT
                               auto response = knowledge::reindex_knowledge(db_conn, req.body, user_id);
                               return HTTPResponse(202, "Accepted", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"admin"})
    };
}

/**
 * Decision Management Endpoints Registration
 */
std::vector<APIEndpoint> create_decision_endpoints(PGconn* db_conn) {
    return {
        create_endpoint("GET", "/decisions",
                       "Get decisions with filtering and pagination",
                       "decision_management",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               auto response = decisions::get_decisions(db_conn, req.query_params);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"}),

        create_endpoint("GET", "/decisions/{id}",
                       "Get specific decision by ID",
                       "decision_management",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string decision_id = req.params.at("id");
                               auto response = decisions::get_decision_by_id(db_conn, decision_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"}),

        create_endpoint("POST", "/decisions",
                       "Create a new decision",
                       "decision_management",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string user_id = "system"; // TODO: Extract from JWT
                               auto response = decisions::create_decision(db_conn, req.body, user_id);
                               return HTTPResponse(201, "Created", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"}),

        create_endpoint("PUT", "/decisions/{id}",
                       "Update an existing decision",
                       "decision_management",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string decision_id = req.params.at("id");
                               auto response = decisions::update_decision(db_conn, decision_id, req.body);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"}),

        create_endpoint("DELETE", "/decisions/{id}",
                       "Delete a decision",
                       "decision_management",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string decision_id = req.params.at("id");
                               auto response = decisions::delete_decision(db_conn, decision_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"admin"}),

        create_endpoint("POST", "/decisions/visualize",
                       "Generate decision visualization",
                       "decision_management",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // TODO: Implement decision visualization
                               return HTTPResponse(200, "OK", R"({"message": "Decision visualization not yet implemented"})", "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"}),

        create_endpoint("GET", "/decisions/tree",
                       "Get decision tree structure",
                       "decision_management",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // TODO: Implement decision tree endpoint
                               return HTTPResponse(200, "OK", R"({"message": "Decision tree not yet implemented"})", "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"}),

        create_endpoint("GET", "/decisions/stats",
                       "Get decision statistics and analytics",
                       "decision_management",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               auto response = decisions::get_decision_stats(db_conn, req.query_params);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"}),

        create_endpoint("POST", "/decisions/{id}/review",
                       "Review a decision",
                       "decision_management",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string decision_id = req.params.at("id");
                               std::string user_id = "system"; // TODO: Extract from JWT
                               auto response = decisions::review_decision(db_conn, decision_id, req.body, user_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"}),

        create_endpoint("POST", "/decisions/{id}/approve",
                       "Approve a decision",
                       "decision_management",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string decision_id = req.params.at("id");
                               std::string user_id = "system"; // TODO: Extract from JWT
                               auto response = decisions::approve_decision(db_conn, decision_id, req.body, user_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"admin", "compliance_officer"}),

        create_endpoint("POST", "/decisions/{id}/reject",
                       "Reject a decision",
                       "decision_management",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string decision_id = req.params.at("id");
                               std::string user_id = "system"; // TODO: Extract from JWT
                               auto response = decisions::reject_decision(db_conn, decision_id, req.body, user_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"admin", "compliance_officer"}),

        create_endpoint("POST", "/decisions/analyze-impact",
                       "Analyze decision impact",
                       "decision_management",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               auto response = decisions::analyze_decision_impact(db_conn, req.body);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"}),

        create_endpoint("POST", "/decisions/mcda",
                       "Create Multi-Criteria Decision Analysis",
                       "decision_management",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string user_id = "system"; // TODO: Extract from JWT
                               auto response = decisions::create_mcda_analysis(db_conn, req.body, user_id);
                               return HTTPResponse(201, "Created", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"})
    };
}

/**
 * Authentication Endpoints Registration
 */
void register_auth_endpoints(PGconn* db_conn) {
    auto& registry = APIRegistry::get_instance();
    registry.register_category_endpoints("authentication", create_auth_endpoints(db_conn));
}

/**
 * Transaction Endpoints Registration
 */
void register_transaction_endpoints(PGconn* db_conn) {
    auto& registry = APIRegistry::get_instance();
    registry.register_category_endpoints("transactions", create_transaction_endpoints(db_conn));
}

/**
 * Fraud Detection Endpoints Registration
 */
void register_fraud_endpoints(PGconn* db_conn) {
    auto& registry = APIRegistry::get_instance();
    registry.register_category_endpoints("fraud_detection", create_fraud_endpoints(db_conn));
}

/**
 * Memory Management Endpoints Registration
 */
void register_memory_endpoints(PGconn* db_conn) {
    auto& registry = APIRegistry::get_instance();
    registry.register_category_endpoints("memory_management", create_memory_endpoints(db_conn));
}

/**
 * Knowledge Base Endpoints Registration
 */
void register_knowledge_endpoints(PGconn* db_conn) {
    auto& registry = APIRegistry::get_instance();
    registry.register_category_endpoints("knowledge_base", create_knowledge_endpoints(db_conn));
}

/**
 * Decision Management Endpoints Registration
 */
void register_decision_endpoints(PGconn* db_conn) {
    auto& registry = APIRegistry::get_instance();
    registry.register_category_endpoints("decision_management", create_decision_endpoints(db_conn));
}

/**
 * LLM Integration Endpoints Registration
 */
std::vector<APIEndpoint> create_llm_endpoints(PGconn* db_conn) {
    return {
        create_endpoint("GET", "/llm/models",
                       "Get available LLM models list",
                       "llm_integration",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // TODO: Implement model listing endpoint
                               return HTTPResponse(200, "OK", R"({"message": "LLM models listing not yet implemented"})", "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"}),

        create_endpoint("GET", "/llm/models/{id}",
                       "Get specific LLM model details",
                       "llm_integration",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string model_id = req.params.at("id");
                               auto response = llm::get_llm_model_by_id(db_conn, model_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"}),

        create_endpoint("POST", "/llm/completions",
                       "Generate LLM completions",
                       "llm_integration",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // TODO: Implement completions endpoint
                               return HTTPResponse(200, "OK", R"({"message": "LLM completions not yet implemented"})", "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"}),

        create_endpoint("POST", "/llm/analyze",
                       "Analyze text with LLM",
                       "llm_integration",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string user_id = "system"; // TODO: Extract from JWT
                               auto response = llm::analyze_text_with_llm(db_conn, req.body, user_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"}),

        create_endpoint("GET", "/llm/conversations",
                       "Get LLM conversations",
                       "llm_integration",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string user_id = "system"; // TODO: Extract from JWT
                               auto response = llm::get_llm_conversations(db_conn, req.query_params, user_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"}),

        create_endpoint("GET", "/llm/conversations/{id}",
                       "Get specific LLM conversation details",
                       "llm_integration",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string conversation_id = req.params.at("id");
                               auto response = llm::get_llm_conversation_by_id(db_conn, conversation_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"}),

        create_endpoint("POST", "/llm/conversations",
                       "Create a new LLM conversation",
                       "llm_integration",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string user_id = "system"; // TODO: Extract from JWT
                               auto response = llm::create_llm_conversation(db_conn, req.body, user_id);
                               return HTTPResponse(201, "Created", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"}),

        create_endpoint("POST", "/llm/conversations/{id}/messages",
                       "Add message to LLM conversation",
                       "llm_integration",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string conversation_id = req.params.at("id");
                               std::string user_id = "system"; // TODO: Extract from JWT
                               auto response = llm::add_message_to_conversation(db_conn, conversation_id, req.body, user_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"}),

        create_endpoint("DELETE", "/llm/conversations/{id}",
                       "Delete an LLM conversation",
                       "llm_integration",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string conversation_id = req.params.at("id");
                               auto response = llm::delete_llm_conversation(db_conn, conversation_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"}),

        create_endpoint("GET", "/llm/usage",
                       "Get LLM usage statistics",
                       "llm_integration",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string user_id = "system"; // TODO: Extract from JWT
                               auto response = llm::get_llm_usage_statistics(db_conn, req.query_params, user_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"}),

        create_endpoint("POST", "/llm/cost-estimate",
                       "Estimate LLM cost for request",
                       "llm_integration",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               auto response = llm::estimate_llm_cost(db_conn, req.body);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"}),

        create_endpoint("POST", "/llm/batch",
                       "Create LLM batch processing job",
                       "llm_integration",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string user_id = "system"; // TODO: Extract from JWT
                               auto response = llm::create_llm_batch_job(db_conn, req.body, user_id);
                               return HTTPResponse(202, "Accepted", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"}),

        create_endpoint("POST", "/llm/fine-tune",
                       "Create LLM fine-tuning job",
                       "llm_integration",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string user_id = "system"; // TODO: Extract from JWT
                               auto response = llm::create_fine_tune_job(db_conn, req.body, user_id);
                               return HTTPResponse(202, "Accepted", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"admin"}),

        create_endpoint("POST", "/llm/compare",
                       "Compare LLM models performance",
                       "llm_integration",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               auto response = llm::get_llm_model_benchmarks(db_conn, req.query_params);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"})
    };
}

void register_llm_endpoints(PGconn* db_conn) {
    auto& registry = APIRegistry::get_instance();
    registry.register_category_endpoints("llm_integration", create_llm_endpoints(db_conn));
}

/**
 * Pattern Detection Endpoints Registration
 */
std::vector<APIEndpoint> create_pattern_endpoints(PGconn* db_conn) {
    return {
        create_endpoint("GET", "/patterns",
                       "Get all patterns with filtering and pagination",
                       "pattern_detection",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               auto response = patterns::get_patterns(db_conn, req.query_params);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"}),

        create_endpoint("GET", "/patterns/{id}",
                       "Get specific pattern by ID",
                       "pattern_detection",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string pattern_id = req.params.at("id");
                               auto response = patterns::get_pattern_by_id(db_conn, pattern_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"}),

        create_endpoint("GET", "/patterns/stats",
                       "Get pattern detection statistics",
                       "pattern_detection",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               auto response = patterns::get_pattern_stats(db_conn);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"}),

        create_endpoint("POST", "/patterns/detect",
                       "Start pattern detection job",
                       "pattern_detection",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string user_id = "system"; // TODO: Extract from JWT
                               auto response = patterns::start_pattern_detection(db_conn, req.body, user_id);
                               return HTTPResponse(202, "Accepted", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"}),

        create_endpoint("GET", "/patterns/jobs/{id}/status",
                       "Get pattern detection job status",
                       "pattern_detection",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string job_id = req.params.at("id");
                               auto response = patterns::get_pattern_job_status(db_conn, job_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"}),

        create_endpoint("GET", "/patterns/{id}/predictions",
                       "Get pattern predictions",
                       "pattern_detection",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string pattern_id = req.params.at("id");
                               auto response = patterns::get_pattern_predictions(db_conn, pattern_id, req.query_params);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"}),

        create_endpoint("POST", "/patterns/{id}/validate",
                       "Validate pattern accuracy",
                       "pattern_detection",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string pattern_id = req.params.at("id");
                               std::string user_id = "system"; // TODO: Extract from JWT
                               auto response = patterns::validate_pattern(db_conn, pattern_id, req.body, user_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"}),

        create_endpoint("GET", "/patterns/{id}/correlations",
                       "Get pattern correlations and relationships",
                       "pattern_detection",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string pattern_id = req.params.at("id");
                               auto response = patterns::get_pattern_correlations(db_conn, pattern_id, req.query_params);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"}),

        create_endpoint("GET", "/patterns/{id}/timeline",
                       "Get pattern timeline and evolution",
                       "pattern_detection",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string pattern_id = req.params.at("id");
                               auto response = patterns::get_pattern_timeline(db_conn, pattern_id, req.query_params);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"}),

        create_endpoint("POST", "/patterns/export",
                       "Export pattern analysis report",
                       "pattern_detection",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string user_id = "system"; // TODO: Extract from JWT
                               auto response = patterns::export_pattern_report(db_conn, req.body, user_id);
                               return HTTPResponse(202, "Accepted", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"}),

        create_endpoint("GET", "/patterns/anomalies",
                       "Get detected pattern anomalies",
                       "pattern_detection",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               auto response = patterns::get_pattern_anomalies(db_conn, req.query_params);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"})
    };
}

void register_pattern_endpoints(PGconn* db_conn) {
    auto& registry = APIRegistry::get_instance();
    registry.register_category_endpoints("pattern_detection", create_pattern_endpoints(db_conn));
}

/**
 * Collaboration Endpoints Registration
 */
std::vector<APIEndpoint> create_collaboration_endpoints(PGconn* db_conn) {
    // We need to create a WebUIHandlers instance to use the collaboration handlers
    // For now, we'll create placeholder endpoints that can be implemented when the WebUIHandlers
    // are properly integrated into the API registry system
    return {
        create_endpoint("GET", "/collaboration/sessions",
                       "Get collaboration sessions",
                       "collaboration",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // TODO: Integrate with WebUIHandlers::handle_collaboration_sessions
                               return HTTPResponse(200, "OK", R"({"message": "Collaboration sessions endpoint not yet fully integrated"})", "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"}),

        create_endpoint("POST", "/collaboration/sessions",
                       "Create a new collaboration session",
                       "collaboration",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // TODO: Integrate with WebUIHandlers::handle_collaboration_session_create
                               return HTTPResponse(201, "Created", R"({"message": "Collaboration session creation not yet fully integrated"})", "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"}),

        create_endpoint("GET", "/collaboration/sessions/{id}",
                       "Get specific collaboration session details",
                       "collaboration",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // TODO: Integrate with collaboration session details handler
                               return HTTPResponse(200, "OK", R"({"message": "Collaboration session details not yet fully integrated"})", "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"}),

        create_endpoint("GET", "/collaboration/sessions/{id}/reasoning",
                       "Get collaboration session reasoning",
                       "collaboration",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // TODO: Integrate with WebUIHandlers collaboration reasoning
                               return HTTPResponse(200, "OK", R"({"message": "Collaboration reasoning endpoint not yet fully integrated"})", "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"}),

        create_endpoint("POST", "/collaboration/override",
                       "Human override for AI decision",
                       "collaboration",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // TODO: Integrate with WebUIHandlers::handle_collaboration_intervention
                               return HTTPResponse(200, "OK", R"({"message": "Human override endpoint not yet fully integrated"})", "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"}),

        create_endpoint("GET", "/collaboration/dashboard/stats",
                       "Get collaboration dashboard statistics",
                       "collaboration",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // TODO: Implement collaboration dashboard statistics
                               return HTTPResponse(200, "OK", R"({"message": "Collaboration dashboard stats not yet implemented"})", "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"})
    };
}

void register_collaboration_endpoints(PGconn* db_conn) {
    auto& registry = APIRegistry::get_instance();
    registry.register_category_endpoints("collaboration", create_collaboration_endpoints(db_conn));
}

/**
 * Alert Management Endpoints Registration
 */
std::vector<APIEndpoint> create_alert_endpoints(PGconn* db_conn) {
    // For now, we'll create placeholder endpoints that can be implemented when the AlertManagementHandlers
    // are properly integrated into the API registry system
    return {
        create_endpoint("GET", "/alerts/rules",
                       "Get alert rules with filtering and pagination",
                       "alert_management",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // TODO: Integrate with AlertManagementHandlers::handle_get_alert_rules
                               return HTTPResponse(200, "OK", R"({"message": "Alert rules endpoint not yet fully integrated"})", "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"}),

        create_endpoint("POST", "/alerts/rules",
                       "Create a new alert rule",
                       "alert_management",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // TODO: Integrate with AlertManagementHandlers::handle_create_alert_rule
                               return HTTPResponse(201, "Created", R"({"message": "Alert rule creation not yet fully integrated"})", "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"admin", "compliance_officer"}),

        create_endpoint("GET", "/alerts/delivery-log",
                       "Get alert delivery log and status",
                       "alert_management",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // TODO: Integrate with AlertManagementHandlers alert delivery log
                               return HTTPResponse(200, "OK", R"({"message": "Alert delivery log endpoint not yet fully integrated"})", "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"}),

        create_endpoint("GET", "/alerts/stats",
                       "Get alert statistics and analytics",
                       "alert_management",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // TODO: Integrate with AlertManagementHandlers::handle_get_alert_metrics
                               return HTTPResponse(200, "OK", R"({"message": "Alert statistics endpoint not yet fully integrated"})", "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"}),

        create_endpoint("GET", "/alerts/incidents",
                       "Get alert incidents with filtering",
                       "alert_management",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // TODO: Integrate with AlertManagementHandlers::handle_get_alert_history
                               return HTTPResponse(200, "OK", R"({"message": "Alert incidents endpoint not yet fully integrated"})", "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"}),

        create_endpoint("POST", "/alerts/incidents/{id}/acknowledge",
                       "Acknowledge an alert incident",
                       "alert_management",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // TODO: Integrate with AlertManagementHandlers::handle_acknowledge_alert
                               return HTTPResponse(200, "OK", R"({"message": "Alert acknowledgement endpoint not yet fully integrated"})", "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"}),

        create_endpoint("POST", "/alerts/incidents/{id}/resolve",
                       "Resolve an alert incident",
                       "alert_management",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // TODO: Integrate with AlertManagementHandlers::handle_resolve_alert
                               return HTTPResponse(200, "OK", R"({"message": "Alert resolution endpoint not yet fully integrated"})", "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"}),

        create_endpoint("GET", "/alerts/channels",
                       "Get notification channels",
                       "alert_management",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // TODO: Integrate with AlertManagementHandlers::handle_get_notification_channels
                               return HTTPResponse(200, "OK", R"({"message": "Notification channels endpoint not yet fully integrated"})", "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"admin"}),

        create_endpoint("POST", "/alerts/channels",
                       "Create a notification channel",
                       "alert_management",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // TODO: Integrate with AlertManagementHandlers::handle_create_notification_channel
                               return HTTPResponse(201, "Created", R"({"message": "Notification channel creation not yet fully integrated"})", "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"admin"}),

        create_endpoint("POST", "/alerts/test",
                       "Test alert delivery",
                       "alert_management",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // TODO: Integrate with AlertManagementHandlers::handle_test_alert_delivery
                               return HTTPResponse(200, "OK", R"({"message": "Alert testing endpoint not yet fully integrated"})", "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"admin"})
    };
}

void register_alert_endpoints(PGconn* db_conn) {
    auto& registry = APIRegistry::get_instance();
    registry.register_category_endpoints("alert_management", create_alert_endpoints(db_conn));
}

/**
 * Export Endpoints Registration
 */
std::vector<APIEndpoint> create_export_endpoints(PGconn* db_conn) {
    // For now, we'll create placeholder endpoints that can be implemented when the export handlers
    // are properly integrated into the API registry system
    return {
        create_endpoint("GET", "/exports",
                       "Get export requests with status tracking",
                       "export",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // TODO: Integrate with export request management system
                               return HTTPResponse(200, "OK", R"({"message": "Export requests endpoint not yet fully implemented"})", "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"}),

        create_endpoint("POST", "/exports",
                       "Create a new export request",
                       "export",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // TODO: Integrate with export creation system
                               // This would support various export types: patterns, feedback, errors, risk data, etc.
                               return HTTPResponse(202, "Accepted", R"({"message": "Export creation endpoint not yet fully implemented"})", "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"}),

        create_endpoint("GET", "/exports/templates",
                       "Get available export templates",
                       "export",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // TODO: Integrate with export template system
                               return HTTPResponse(200, "OK", R"({"message": "Export templates endpoint not yet fully implemented"})", "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"}),

        create_endpoint("GET", "/exports/{id}/status",
                       "Get export request status",
                       "export",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // TODO: Integrate with export status tracking
                               return HTTPResponse(200, "OK", R"({"message": "Export status endpoint not yet fully implemented"})", "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"}),

        create_endpoint("GET", "/exports/{id}/download",
                       "Download completed export file",
                       "export",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // TODO: Integrate with export file download system
                               return HTTPResponse(200, "OK", R"({"message": "Export download endpoint not yet fully implemented"})", "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"}),

        create_endpoint("DELETE", "/exports/{id}",
                       "Cancel or delete an export request",
                       "export",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // TODO: Integrate with export cancellation system
                               return HTTPResponse(200, "OK", R"({"message": "Export deletion endpoint not yet fully implemented"})", "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin"}),

        create_endpoint("POST", "/exports/patterns",
                       "Export pattern analysis data",
                       "export",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // TODO: Integrate with WebUIHandlers::handle_pattern_export
                               return HTTPResponse(202, "Accepted", R"({"message": "Pattern export endpoint not yet fully integrated"})", "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"}),

        create_endpoint("POST", "/exports/feedback",
                       "Export feedback data",
                       "export",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // TODO: Integrate with WebUIHandlers::handle_feedback_export
                               return HTTPResponse(202, "Accepted", R"({"message": "Feedback export endpoint not yet fully integrated"})", "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"}),

        create_endpoint("POST", "/exports/risk",
                       "Export risk assessment data",
                       "export",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // TODO: Integrate with WebUIHandlers::handle_risk_export
                               return HTTPResponse(202, "Accepted", R"({"message": "Risk export endpoint not yet fully integrated"})", "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"user", "admin", "compliance_officer"})
    };
}

void register_export_endpoints(PGconn* db_conn) {
    auto& registry = APIRegistry::get_instance();
    registry.register_category_endpoints("export", create_export_endpoints(db_conn));
}

/**
 * Register All API Endpoints
 * This function is called during server startup to register all endpoints
 */
void register_all_api_endpoints(PGconn* db_conn) {
    // Register authentication endpoints
    register_auth_endpoints(db_conn);

    // Register transaction endpoints
    register_transaction_endpoints(db_conn);

    // Register fraud detection endpoints
    register_fraud_endpoints(db_conn);

    // Register memory management endpoints
    register_memory_endpoints(db_conn);

    // Register knowledge base endpoints
    register_knowledge_endpoints(db_conn);

    // Register decision management endpoints
    register_decision_endpoints(db_conn);

    // Register LLM integration endpoints
    register_llm_endpoints(db_conn);

    // Register pattern detection endpoints
    register_pattern_endpoints(db_conn);

    // Register collaboration endpoints
    register_collaboration_endpoints(db_conn);

    // Register alert management endpoints
    register_alert_endpoints(db_conn);

    // Register export endpoints
    register_export_endpoints(db_conn);

    // Register training endpoints
    register_training_endpoints(db_conn);

    // Register simulator endpoints
    register_simulator_endpoints(db_conn);

    // Register tool categories endpoints
    register_tool_categories_endpoints(db_conn);

    // TODO: Register remaining endpoint categories:
    // - Frontend feature endpoints (Rule Engine, Policy Generation)
}

/**
 * Create Training API Endpoints
 * Production-grade training system endpoints
 */
std::vector<APIEndpoint> create_training_endpoints(PGconn* db_conn) {
    return {
        // Course Management
        create_endpoint("GET", "/training/courses",
                       "Get all training courses with filtering",
                       "training",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // Extract query parameters
                               std::map<std::string, std::string> query_params;
                               // Parse query string and populate query_params
                               size_t pos = req.path.find('?');
                               if (pos != std::string::npos) {
                                   std::string query = req.path.substr(pos + 1);
                                   // Simple query parameter parsing (production would use proper URL parsing)
                                   size_t start = 0;
                                   while (start < query.length()) {
                                       size_t eq_pos = query.find('=', start);
                                       size_t amp_pos = query.find('&', start);
                                       if (eq_pos != std::string::npos) {
                                           std::string key = query.substr(start, eq_pos - start);
                                           std::string value;
                                           if (amp_pos != std::string::npos) {
                                               value = query.substr(eq_pos + 1, amp_pos - eq_pos - 1);
                                               start = amp_pos + 1;
                                           } else {
                                               value = query.substr(eq_pos + 1);
                                               start = query.length();
                                           }
                                           query_params[key] = value;
                                       } else {
                                           break;
                                       }
                                   }
                               }

                               // Call training handler
                               regulens::training::TrainingAPIHandlers training_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>(),
                                   std::make_shared<regulens::StructuredLogger>()
                               );
                               std::string response = training_handlers.handle_get_courses(query_params);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"training.view"}),

        create_endpoint("GET", "/training/courses/{id}",
                       "Get specific training course details",
                       "training",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // Extract course ID from path
                               std::string path = req.path;
                               size_t courses_pos = path.find("/training/courses/");
                               if (courses_pos == std::string::npos) {
                                   return HTTPResponse(400, "Bad Request",
                                                     R"({"error": "Invalid course ID"})", "application/json");
                               }
                               std::string course_id = path.substr(courses_pos + 18); // Length of "/training/courses/"

                               regulens::training::TrainingAPIHandlers training_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>(),
                                   std::make_shared<regulens::StructuredLogger>()
                               );
                               std::string response = training_handlers.handle_get_course_by_id(course_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"training.view"}),

        create_endpoint("POST", "/training/courses",
                       "Create new training course",
                       "training",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // Extract user ID from JWT token (would be done in production)
                               std::string user_id = "current_user"; // Placeholder

                               regulens::training::TrainingAPIHandlers training_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>(),
                                   std::make_shared<regulens::StructuredLogger>()
                               );
                               std::string response = training_handlers.handle_create_course(req.body, user_id);
                               return HTTPResponse(201, "Created", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"training.manage"}),

        create_endpoint("PUT", "/training/courses/{id}",
                       "Update existing training course",
                       "training",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // Extract course ID from path
                               std::string path = req.path;
                               size_t courses_pos = path.find("/training/courses/");
                               if (courses_pos == std::string::npos) {
                                   return HTTPResponse(400, "Bad Request",
                                                     R"({"error": "Invalid course ID"})", "application/json");
                               }
                               std::string course_id = path.substr(courses_pos + 18);

                               regulens::training::TrainingAPIHandlers training_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>(),
                                   std::make_shared<regulens::StructuredLogger>()
                               );
                               std::string response = training_handlers.handle_update_course(course_id, req.body);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"training.manage"}),

        // Enrollment Management
        create_endpoint("POST", "/training/courses/{id}/enroll",
                       "Enroll user in training course",
                       "training",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // Extract course ID from path
                               std::string path = req.path;
                               size_t courses_pos = path.find("/training/courses/");
                               size_t enroll_pos = path.find("/enroll");
                               if (courses_pos == std::string::npos || enroll_pos == std::string::npos) {
                                   return HTTPResponse(400, "Bad Request",
                                                     R"({"error": "Invalid course ID"})", "application/json");
                               }
                               std::string course_id = path.substr(courses_pos + 18, enroll_pos - (courses_pos + 18));

                               // Extract user ID from JWT token
                               std::string user_id = "current_user"; // Placeholder

                               regulens::training::TrainingAPIHandlers training_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>(),
                                   std::make_shared<regulens::StructuredLogger>()
                               );
                               std::string response = training_handlers.handle_enroll_user(course_id, req.body, user_id);
                               return HTTPResponse(201, "Created", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"training.enroll"}),

        create_endpoint("GET", "/training/progress",
                       "Get user training progress",
                       "training",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // Extract query parameters
                               std::map<std::string, std::string> query_params;
                               // Parse query string (simplified)
                               size_t pos = req.path.find('?');
                               if (pos != std::string::npos) {
                                   std::string query = req.path.substr(pos + 1);
                                   // Simple parsing logic would go here
                               }

                               // Extract user ID from JWT token
                               std::string user_id = "current_user"; // Placeholder

                               regulens::training::TrainingAPIHandlers training_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>(),
                                   std::make_shared<regulens::StructuredLogger>()
                               );
                               std::string response = training_handlers.handle_get_user_progress(user_id, query_params);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"training.view"}),

        create_endpoint("PUT", "/training/enrollments/{id}/progress",
                       "Update training progress",
                       "training",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // Extract enrollment ID from path
                               std::string path = req.path;
                               size_t enrollments_pos = path.find("/training/enrollments/");
                               size_t progress_pos = path.find("/progress");
                               if (enrollments_pos == std::string::npos || progress_pos == std::string::npos) {
                                   return HTTPResponse(400, "Bad Request",
                                                     R"({"error": "Invalid enrollment ID"})", "application/json");
                               }
                               std::string enrollment_id = path.substr(enrollments_pos + 24, progress_pos - (enrollments_pos + 24));

                               regulens::training::TrainingAPIHandlers training_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>(),
                                   std::make_shared<regulens::StructuredLogger>()
                               );
                               std::string response = training_handlers.handle_update_progress(enrollment_id, req.body);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"training.update"}),

        create_endpoint("POST", "/training/courses/{id}/complete",
                       "Mark course as completed",
                       "training",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // Extract course ID from path
                               std::string path = req.path;
                               size_t courses_pos = path.find("/training/courses/");
                               size_t complete_pos = path.find("/complete");
                               if (courses_pos == std::string::npos || complete_pos == std::string::npos) {
                                   return HTTPResponse(400, "Bad Request",
                                                     R"({"error": "Invalid course ID"})", "application/json");
                               }
                               std::string course_id = path.substr(courses_pos + 18, complete_pos - (courses_pos + 18));

                               // Extract user ID from JWT token
                               std::string user_id = "current_user"; // Placeholder

                               regulens::training::TrainingAPIHandlers training_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>(),
                                   std::make_shared<regulens::StructuredLogger>()
                               );
                               std::string response = training_handlers.handle_mark_complete(course_id, user_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"training.complete"}),

        // Quiz Management
        create_endpoint("POST", "/training/quizzes/{id}/submit",
                       "Submit quiz answers",
                       "training",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // Extract quiz ID from path
                               std::string path = req.path;
                               size_t quizzes_pos = path.find("/training/quizzes/");
                               size_t submit_pos = path.find("/submit");
                               if (quizzes_pos == std::string::npos || submit_pos == std::string::npos) {
                                   return HTTPResponse(400, "Bad Request",
                                                     R"({"error": "Invalid quiz ID"})", "application/json");
                               }
                               std::string quiz_id = path.substr(quizzes_pos + 18, submit_pos - (quizzes_pos + 18));

                               // Extract user ID from JWT token
                               std::string user_id = "current_user"; // Placeholder

                               regulens::training::TrainingAPIHandlers training_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>(),
                                   std::make_shared<regulens::StructuredLogger>()
                               );
                               std::string response = training_handlers.handle_submit_quiz(quiz_id, req.body, user_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"training.quiz"}),

        create_endpoint("GET", "/training/enrollments/{id}/quiz-results",
                       "Get quiz results for enrollment",
                       "training",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // Extract enrollment ID from path
                               std::string path = req.path;
                               size_t enrollments_pos = path.find("/training/enrollments/");
                               size_t quiz_pos = path.find("/quiz-results");
                               if (enrollments_pos == std::string::npos || quiz_pos == std::string::npos) {
                                   return HTTPResponse(400, "Bad Request",
                                                     R"({"error": "Invalid enrollment ID"})", "application/json");
                               }
                               std::string enrollment_id = path.substr(enrollments_pos + 24, quiz_pos - (enrollments_pos + 24));

                               regulens::training::TrainingAPIHandlers training_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>(),
                                   std::make_shared<regulens::StructuredLogger>()
                               );
                               std::string response = training_handlers.handle_get_quiz_results(enrollment_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"training.view"}),

        // Certifications
        create_endpoint("GET", "/training/certifications",
                       "Get user certifications",
                       "training",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // Extract user ID from JWT token
                               std::string user_id = "current_user"; // Placeholder

                               regulens::training::TrainingAPIHandlers training_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>(),
                                   std::make_shared<regulens::StructuredLogger>()
                               );
                               std::string response = training_handlers.handle_get_certifications(user_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"training.view"}),

        create_endpoint("POST", "/training/enrollments/{id}/certificate",
                       "Issue certificate for completed course",
                       "training",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // Extract enrollment ID from path
                               std::string path = req.path;
                               size_t enrollments_pos = path.find("/training/enrollments/");
                               size_t cert_pos = path.find("/certificate");
                               if (enrollments_pos == std::string::npos || cert_pos == std::string::npos) {
                                   return HTTPResponse(400, "Bad Request",
                                                     R"({"error": "Invalid enrollment ID"})", "application/json");
                               }
                               std::string enrollment_id = path.substr(enrollments_pos + 24, cert_pos - (enrollments_pos + 24));

                               regulens::training::TrainingAPIHandlers training_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>(),
                                   std::make_shared<regulens::StructuredLogger>()
                               );
                               std::string response = training_handlers.handle_issue_certificate(enrollment_id);
                               return HTTPResponse(201, "Created", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"training.certify"}),

        create_endpoint("GET", "/training/certificates/{code}/verify",
                       "Verify certificate authenticity",
                       "training",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // Extract verification code from path
                               std::string path = req.path;
                               size_t cert_pos = path.find("/training/certificates/");
                               size_t verify_pos = path.find("/verify");
                               if (cert_pos == std::string::npos || verify_pos == std::string::npos) {
                                   return HTTPResponse(400, "Bad Request",
                                                     R"({"error": "Invalid verification code"})", "application/json");
                               }
                               std::string verification_code = path.substr(cert_pos + 23, verify_pos - (cert_pos + 23));

                               regulens::training::TrainingAPIHandlers training_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>(),
                                   std::make_shared<regulens::StructuredLogger>()
                               );
                               std::string response = training_handlers.handle_verify_certificate(verification_code);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, false), // Public endpoint - no authentication required

        // Analytics
        create_endpoint("GET", "/training/leaderboard",
                       "Get training leaderboard",
                       "training",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // Extract query parameters
                               std::map<std::string, std::string> query_params;
                               // Parse query string (simplified)
                               size_t pos = req.path.find('?');
                               if (pos != std::string::npos) {
                                   std::string query = req.path.substr(pos + 1);
                                   // Simple parsing logic would go here
                               }

                               regulens::training::TrainingAPIHandlers training_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>(),
                                   std::make_shared<regulens::StructuredLogger>()
                               );
                               std::string response = training_handlers.handle_get_leaderboard(query_params);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"training.view"}),

        create_endpoint("GET", "/training/analytics",
                       "Get training analytics and statistics",
                       "training",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // Extract user ID from JWT token
                               std::string user_id = "current_user"; // Placeholder

                               regulens::training::TrainingAPIHandlers training_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>(),
                                   std::make_shared<regulens::StructuredLogger>()
                               );
                               std::string response = training_handlers.handle_get_training_stats(user_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"training.analytics"})
    };
}

/**
 * Register Training API Endpoints
 */
void register_training_endpoints(PGconn* db_conn) {
    auto endpoints = create_training_endpoints(db_conn);
    auto& api_registry = regulens::APIRegistry::get_instance();

    for (const auto& endpoint : endpoints) {
        api_registry.register_endpoint(endpoint);
    }
}

/**
 * Tool Categories API Endpoints
 * Tool category management and execution endpoints
 */
std::vector<APIEndpoint> create_tool_categories_endpoints(PGconn* db_conn) {
    return {
        // Tool Registry Management
        create_endpoint("POST", "/api/tools/register",
                       "Register new tools in the system",
                       "tools",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // Extract user ID from JWT token
                               std::string user_id = "current_user"; // Placeholder

                               regulens::ToolCategoriesAPIHandlers tool_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>()
                               );
                               std::string response = tool_handlers.handle_register_tools(req.body, user_id);
                               return HTTPResponse(201, "Created", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"tools.register"}),

        create_endpoint("GET", "/api/tools/available",
                       "Get available tools for current user",
                       "tools",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // Extract user ID from JWT token
                               std::string user_id = "current_user"; // Placeholder

                               regulens::ToolCategoriesAPIHandlers tool_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>()
                               );
                               std::string response = tool_handlers.handle_get_available_tools(user_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"tools.view"}),

        create_endpoint("GET", "/api/tools/categories/{category}",
                       "Get tools by category",
                       "tools",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // Extract category from path
                               std::string path = req.path;
                               size_t categories_pos = path.find("/api/tools/categories/");
                               if (categories_pos == std::string::npos) {
                                   return HTTPResponse(400, "Bad Request",
                                                     R"({"error": "Invalid category"})", "application/json");
                               }
                               std::string category = path.substr(categories_pos + 22);

                               // Extract user ID from JWT token
                               std::string user_id = "current_user"; // Placeholder

                               regulens::ToolCategoriesAPIHandlers tool_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>()
                               );
                               std::string response = tool_handlers.handle_get_tools_by_category(category, user_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"tools.view"}),

        // Generic Tool Execution Endpoint
        create_endpoint("POST", "/api/tools/categories/{category}/execute",
                       "Execute a tool in the specified category",
                       "tools",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // Extract category from path
                               std::string path = req.path;
                               size_t categories_pos = path.find("/api/tools/categories/");
                               size_t execute_pos = path.find("/execute");
                               if (categories_pos == std::string::npos || execute_pos == std::string::npos) {
                                   return HTTPResponse(400, "Bad Request",
                                                     R"({"error": "Invalid category"})", "application/json");
                               }
                               std::string category = path.substr(categories_pos + 22, execute_pos - (categories_pos + 22));

                               // Extract user ID from JWT token
                               std::string user_id = "current_user"; // Placeholder

                               regulens::ToolCategoriesAPIHandlers tool_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>()
                               );
                               std::string response = tool_handlers.handle_execute_tool(category, req.body, user_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"tools.execute"}),

        create_endpoint("GET", "/api/tools/{tool_name}/info",
                       "Get information about a specific tool",
                       "tools",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // Extract tool name from path
                               std::string path = req.path;
                               size_t tools_pos = path.find("/api/tools/");
                               size_t info_pos = path.find("/info");
                               if (tools_pos == std::string::npos || info_pos == std::string::npos) {
                                   return HTTPResponse(400, "Bad Request",
                                                     R"({"error": "Invalid tool name"})", "application/json");
                               }
                               std::string tool_name = path.substr(tools_pos + 11, info_pos - (tools_pos + 11));

                               // Extract user ID from JWT token
                               std::string user_id = "current_user"; // Placeholder

                               regulens::ToolCategoriesAPIHandlers tool_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>()
                               );
                               std::string response = tool_handlers.handle_get_tool_info(tool_name, user_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"tools.view"}),

        // Analytics Tools
        create_endpoint("POST", "/api/tools/analytics/analyze-dataset",
                       "Analyze dataset using analytics tools",
                       "tools",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string user_id = "current_user"; // Placeholder
                               regulens::ToolCategoriesAPIHandlers tool_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>()
                               );
                               std::string response = tool_handlers.handle_analyze_dataset(req.body, user_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"tools.analytics"}),

        create_endpoint("POST", "/api/tools/analytics/generate-report",
                       "Generate report using analytics tools",
                       "tools",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string user_id = "current_user"; // Placeholder
                               regulens::ToolCategoriesAPIHandlers tool_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>()
                               );
                               std::string response = tool_handlers.handle_generate_report(req.body, user_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"tools.analytics"}),

        create_endpoint("POST", "/api/tools/analytics/build-dashboard",
                       "Build dashboard using analytics tools",
                       "tools",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string user_id = "current_user"; // Placeholder
                               regulens::ToolCategoriesAPIHandlers tool_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>()
                               );
                               std::string response = tool_handlers.handle_build_dashboard(req.body, user_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"tools.analytics"}),

        create_endpoint("POST", "/api/tools/analytics/run-prediction",
                       "Run prediction using analytics tools",
                       "tools",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string user_id = "current_user"; // Placeholder
                               regulens::ToolCategoriesAPIHandlers tool_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>()
                               );
                               std::string response = tool_handlers.handle_run_prediction(req.body, user_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"tools.analytics"}),

        // Workflow Tools
        create_endpoint("POST", "/api/tools/workflow/automate-task",
                       "Automate task using workflow tools",
                       "tools",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string user_id = "current_user"; // Placeholder
                               regulens::ToolCategoriesAPIHandlers tool_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>()
                               );
                               std::string response = tool_handlers.handle_automate_task(req.body, user_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"tools.workflow"}),

        create_endpoint("POST", "/api/tools/workflow/optimize-process",
                       "Optimize process using workflow tools",
                       "tools",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string user_id = "current_user"; // Placeholder
                               regulens::ToolCategoriesAPIHandlers tool_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>()
                               );
                               std::string response = tool_handlers.handle_optimize_process(req.body, user_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"tools.workflow"}),

        create_endpoint("POST", "/api/tools/workflow/manage-approval",
                       "Manage approval using workflow tools",
                       "tools",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string user_id = "current_user"; // Placeholder
                               regulens::ToolCategoriesAPIHandlers tool_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>()
                               );
                               std::string response = tool_handlers.handle_manage_approval(req.body, user_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"tools.workflow"}),

        // Security Tools
        create_endpoint("POST", "/api/tools/security/scan-vulnerabilities",
                       "Scan for vulnerabilities using security tools",
                       "tools",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string user_id = "current_user"; // Placeholder
                               regulens::ToolCategoriesAPIHandlers tool_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>()
                               );
                               std::string response = tool_handlers.handle_scan_vulnerabilities(req.body, user_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"tools.security"}),

        create_endpoint("POST", "/api/tools/security/check-compliance",
                       "Check compliance using security tools",
                       "tools",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string user_id = "current_user"; // Placeholder
                               regulens::ToolCategoriesAPIHandlers tool_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>()
                               );
                               std::string response = tool_handlers.handle_check_compliance(req.body, user_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"tools.security"}),

        create_endpoint("POST", "/api/tools/security/analyze-access",
                       "Analyze access using security tools",
                       "tools",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string user_id = "current_user"; // Placeholder
                               regulens::ToolCategoriesAPIHandlers tool_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>()
                               );
                               std::string response = tool_handlers.handle_analyze_access(req.body, user_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"tools.security"}),

        create_endpoint("POST", "/api/tools/security/log-audit-event",
                       "Log audit event using security tools",
                       "tools",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string user_id = "current_user"; // Placeholder
                               regulens::ToolCategoriesAPIHandlers tool_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>()
                               );
                               std::string response = tool_handlers.handle_log_audit_event(req.body, user_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"tools.security"}),

        // Monitoring Tools
        create_endpoint("POST", "/api/tools/monitoring/monitor-system",
                       "Monitor system using monitoring tools",
                       "tools",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string user_id = "current_user"; // Placeholder
                               regulens::ToolCategoriesAPIHandlers tool_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>()
                               );
                               std::string response = tool_handlers.handle_monitor_system(req.body, user_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"tools.monitoring"}),

        create_endpoint("POST", "/api/tools/monitoring/track-performance",
                       "Track performance using monitoring tools",
                       "tools",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string user_id = "current_user"; // Placeholder
                               regulens::ToolCategoriesAPIHandlers tool_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>()
                               );
                               std::string response = tool_handlers.handle_track_performance(req.body, user_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"tools.monitoring"}),

        create_endpoint("POST", "/api/tools/monitoring/manage-alerts",
                       "Manage alerts using monitoring tools",
                       "tools",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string user_id = "current_user"; // Placeholder
                               regulens::ToolCategoriesAPIHandlers tool_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>()
                               );
                               std::string response = tool_handlers.handle_manage_alerts(req.body, user_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"tools.monitoring"}),

        create_endpoint("POST", "/api/tools/monitoring/check-health",
                       "Check health using monitoring tools",
                       "tools",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               std::string user_id = "current_user"; // Placeholder
                               regulens::ToolCategoriesAPIHandlers tool_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>()
                               );
                               std::string response = tool_handlers.handle_check_health(req.body, user_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"tools.monitoring"})
    };
}

void register_tool_categories_endpoints(PGconn* db_conn) {
    auto endpoints = create_tool_categories_endpoints(db_conn);
    auto& api_registry = regulens::APIRegistry::get_instance();

    for (const auto& endpoint : endpoints) {
        api_registry.register_endpoint(endpoint);
    }
}

/**
 * Create Simulator API Endpoints
 * Regulatory impact simulation and scenario management endpoints
 */
std::vector<APIEndpoint> create_simulator_endpoints(PGconn* db_conn) {
    return {
        // Scenario Management
        create_endpoint("POST", "/api/simulator/scenarios",
                       "Create new simulation scenario",
                       "simulator",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // Extract user ID from JWT token
                               std::string user_id = "current_user"; // Placeholder

                               regulens::simulator::SimulatorAPIHandlers simulator_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>(),
                                   std::make_shared<regulens::StructuredLogger>(),
                                   std::make_shared<regulens::simulator::RegulatorySimulator>(
                                       std::make_shared<regulens::PostgreSQLConnection>(),
                                       std::make_shared<regulens::StructuredLogger>()
                                   )
                               );
                               std::string response = simulator_handlers.handle_create_scenario(req.body, user_id);
                               return HTTPResponse(201, "Created", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"simulator.create"}),

        create_endpoint("GET", "/simulator/scenarios",
                       "Get user simulation scenarios",
                       "simulator",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // Extract user ID from JWT token and query params
                               std::string user_id = "current_user"; // Placeholder
                               std::map<std::string, std::string> query_params;

                               regulens::simulator::SimulatorAPIHandlers simulator_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>(),
                                   std::make_shared<regulens::StructuredLogger>(),
                                   std::make_shared<regulens::simulator::RegulatorySimulator>(
                                       std::make_shared<regulens::PostgreSQLConnection>(),
                                       std::make_shared<regulens::StructuredLogger>()
                                   )
                               );
                               std::string response = simulator_handlers.handle_get_scenarios(user_id, query_params);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"simulator.view"}),

        create_endpoint("GET", "/simulator/scenarios/{id}",
                       "Get specific simulation scenario",
                       "simulator",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // Extract scenario ID from path
                               std::string path = req.path;
                               size_t scenarios_pos = path.find("/simulator/scenarios/");
                               if (scenarios_pos == std::string::npos) {
                                   return HTTPResponse(400, "Bad Request",
                                                     R"({"error": "Invalid scenario ID"})", "application/json");
                               }
                               std::string scenario_id = path.substr(scenarios_pos + 22);

                               // Extract user ID from JWT token
                               std::string user_id = "current_user"; // Placeholder

                               regulens::simulator::SimulatorAPIHandlers simulator_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>(),
                                   std::make_shared<regulens::StructuredLogger>(),
                                   std::make_shared<regulens::simulator::RegulatorySimulator>(
                                       std::make_shared<regulens::PostgreSQLConnection>(),
                                       std::make_shared<regulens::StructuredLogger>()
                                   )
                               );
                               std::string response = simulator_handlers.handle_get_scenario(scenario_id, user_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"simulator.view"}),

        create_endpoint("PUT", "/simulator/scenarios/{id}",
                       "Update simulation scenario",
                       "simulator",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // Extract scenario ID from path
                               std::string path = req.path;
                               size_t scenarios_pos = path.find("/simulator/scenarios/");
                               if (scenarios_pos == std::string::npos) {
                                   return HTTPResponse(400, "Bad Request",
                                                     R"({"error": "Invalid scenario ID"})", "application/json");
                               }
                               std::string scenario_id = path.substr(scenarios_pos + 22);

                               // Extract user ID from JWT token
                               std::string user_id = "current_user"; // Placeholder

                               regulens::simulator::SimulatorAPIHandlers simulator_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>(),
                                   std::make_shared<regulens::StructuredLogger>(),
                                   std::make_shared<regulens::simulator::RegulatorySimulator>(
                                       std::make_shared<regulens::PostgreSQLConnection>(),
                                       std::make_shared<regulens::StructuredLogger>()
                                   )
                               );
                               std::string response = simulator_handlers.handle_update_scenario(scenario_id, req.body, user_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"simulator.edit"}),

        create_endpoint("DELETE", "/simulator/scenarios/{id}",
                       "Delete simulation scenario",
                       "simulator",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // Extract scenario ID from path
                               std::string path = req.path;
                               size_t scenarios_pos = path.find("/simulator/scenarios/");
                               if (scenarios_pos == std::string::npos) {
                                   return HTTPResponse(400, "Bad Request",
                                                     R"({"error": "Invalid scenario ID"})", "application/json");
                               }
                               std::string scenario_id = path.substr(scenarios_pos + 22);

                               // Extract user ID from JWT token
                               std::string user_id = "current_user"; // Placeholder

                               regulens::simulator::SimulatorAPIHandlers simulator_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>(),
                                   std::make_shared<regulens::StructuredLogger>(),
                                   std::make_shared<regulens::simulator::RegulatorySimulator>(
                                       std::make_shared<regulens::PostgreSQLConnection>(),
                                       std::make_shared<regulens::StructuredLogger>()
                                   )
                               );
                               std::string response = simulator_handlers.handle_delete_scenario(scenario_id, user_id);
                               return HTTPResponse(204, "No Content", "", "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"simulator.delete"}),

        // Template Management
        create_endpoint("GET", "/simulator/templates",
                       "Get simulation templates",
                       "simulator",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // Extract query parameters
                               std::map<std::string, std::string> query_params;

                               regulens::simulator::SimulatorAPIHandlers simulator_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>(),
                                   std::make_shared<regulens::StructuredLogger>(),
                                   std::make_shared<regulens::simulator::RegulatorySimulator>(
                                       std::make_shared<regulens::PostgreSQLConnection>(),
                                       std::make_shared<regulens::StructuredLogger>()
                                   )
                               );
                               std::string response = simulator_handlers.handle_get_templates(query_params);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"simulator.view"}),

        create_endpoint("GET", "/simulator/templates/{id}",
                       "Get specific simulation template",
                       "simulator",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // Extract template ID from path
                               std::string path = req.path;
                               size_t templates_pos = path.find("/simulator/templates/");
                               if (templates_pos == std::string::npos) {
                                   return HTTPResponse(400, "Bad Request",
                                                     R"({"error": "Invalid template ID"})", "application/json");
                               }
                               std::string template_id = path.substr(templates_pos + 22);

                               regulens::simulator::SimulatorAPIHandlers simulator_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>(),
                                   std::make_shared<regulens::StructuredLogger>(),
                                   std::make_shared<regulens::simulator::RegulatorySimulator>(
                                       std::make_shared<regulens::PostgreSQLConnection>(),
                                       std::make_shared<regulens::StructuredLogger>()
                                   )
                               );
                               std::string response = simulator_handlers.handle_get_template(template_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"simulator.view"}),

        create_endpoint("POST", "/simulator/templates/{id}/create-scenario",
                       "Create scenario from template",
                       "simulator",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // Extract template ID from path
                               std::string path = req.path;
                               size_t templates_pos = path.find("/simulator/templates/");
                               size_t create_pos = path.find("/create-scenario");
                               if (templates_pos == std::string::npos || create_pos == std::string::npos) {
                                   return HTTPResponse(400, "Bad Request",
                                                     R"({"error": "Invalid template ID"})", "application/json");
                               }
                               std::string template_id = path.substr(templates_pos + 22, create_pos - (templates_pos + 22));

                               // Extract user ID from JWT token
                               std::string user_id = "current_user"; // Placeholder

                               regulens::simulator::SimulatorAPIHandlers simulator_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>(),
                                   std::make_shared<regulens::StructuredLogger>(),
                                   std::make_shared<regulens::simulator::RegulatorySimulator>(
                                       std::make_shared<regulens::PostgreSQLConnection>(),
                                       std::make_shared<regulens::StructuredLogger>()
                                   )
                               );
                               std::string response = simulator_handlers.handle_create_scenario_from_template(template_id, user_id);
                               return HTTPResponse(201, "Created", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"simulator.create"}),

        // Simulation Execution
        create_endpoint("POST", "/simulator/run",
                       "Run regulatory simulation",
                       "simulator",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // Extract user ID from JWT token
                               std::string user_id = "current_user"; // Placeholder

                               regulens::simulator::SimulatorAPIHandlers simulator_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>(),
                                   std::make_shared<regulens::StructuredLogger>(),
                                   std::make_shared<regulens::simulator::RegulatorySimulator>(
                                       std::make_shared<regulens::PostgreSQLConnection>(),
                                       std::make_shared<regulens::StructuredLogger>()
                                   )
                               );
                               std::string response = simulator_handlers.handle_run_simulation(req.body, user_id);
                               return HTTPResponse(202, "Accepted", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"simulator.run"}),

        create_endpoint("GET", "/simulator/executions/{id}",
                       "Get simulation execution status",
                       "simulator",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // Extract execution ID from path
                               std::string path = req.path;
                               size_t executions_pos = path.find("/simulator/executions/");
                               if (executions_pos == std::string::npos) {
                                   return HTTPResponse(400, "Bad Request",
                                                     R"({"error": "Invalid execution ID"})", "application/json");
                               }
                               std::string execution_id = path.substr(executions_pos + 23);

                               // Extract user ID from JWT token
                               std::string user_id = "current_user"; // Placeholder

                               regulens::simulator::SimulatorAPIHandlers simulator_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>(),
                                   std::make_shared<regulens::StructuredLogger>(),
                                   std::make_shared<regulens::simulator::RegulatorySimulator>(
                                       std::make_shared<regulens::PostgreSQLConnection>(),
                                       std::make_shared<regulens::StructuredLogger>()
                                   )
                               );
                               std::string response = simulator_handlers.handle_get_execution_status(execution_id, user_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"simulator.view"}),

        create_endpoint("DELETE", "/simulator/executions/{id}",
                       "Cancel simulation execution",
                       "simulator",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // Extract execution ID from path
                               std::string path = req.path;
                               size_t executions_pos = path.find("/simulator/executions/");
                               if (executions_pos == std::string::npos) {
                                   return HTTPResponse(400, "Bad Request",
                                                     R"({"error": "Invalid execution ID"})", "application/json");
                               }
                               std::string execution_id = path.substr(executions_pos + 23);

                               // Extract user ID from JWT token
                               std::string user_id = "current_user"; // Placeholder

                               regulens::simulator::SimulatorAPIHandlers simulator_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>(),
                                   std::make_shared<regulens::StructuredLogger>(),
                                   std::make_shared<regulens::simulator::RegulatorySimulator>(
                                       std::make_shared<regulens::PostgreSQLConnection>(),
                                       std::make_shared<regulens::StructuredLogger>()
                                   )
                               );
                               std::string response = simulator_handlers.handle_cancel_simulation(execution_id, user_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"simulator.cancel"}),

        // Results and Analytics
        create_endpoint("GET", "/simulator/results/{id}",
                       "Get simulation results",
                       "simulator",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // Extract execution ID from path
                               std::string path = req.path;
                               size_t results_pos = path.find("/simulator/results/");
                               if (results_pos == std::string::npos) {
                                   return HTTPResponse(400, "Bad Request",
                                                     R"({"error": "Invalid result ID"})", "application/json");
                               }
                               std::string execution_id = path.substr(results_pos + 20);

                               // Extract user ID from JWT token
                               std::string user_id = "current_user"; // Placeholder

                               regulens::simulator::SimulatorAPIHandlers simulator_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>(),
                                   std::make_shared<regulens::StructuredLogger>(),
                                   std::make_shared<regulens::simulator::RegulatorySimulator>(
                                       std::make_shared<regulens::PostgreSQLConnection>(),
                                       std::make_shared<regulens::StructuredLogger>()
                                   )
                               );
                               std::string response = simulator_handlers.handle_get_simulation_result(execution_id, user_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"simulator.view"}),

        create_endpoint("GET", "/simulator/history",
                       "Get simulation history",
                       "simulator",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // Extract user ID from JWT token and query params
                               std::string user_id = "current_user"; // Placeholder
                               std::map<std::string, std::string> query_params;

                               regulens::simulator::SimulatorAPIHandlers simulator_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>(),
                                   std::make_shared<regulens::StructuredLogger>(),
                                   std::make_shared<regulens::simulator::RegulatorySimulator>(
                                       std::make_shared<regulens::PostgreSQLConnection>(),
                                       std::make_shared<regulens::StructuredLogger>()
                                   )
                               );
                               std::string response = simulator_handlers.handle_get_simulation_history(user_id, query_params);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"simulator.view"}),

        create_endpoint("GET", "/simulator/analytics",
                       "Get simulation analytics",
                       "simulator",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // Extract user ID from JWT token and query params
                               std::string user_id = "current_user"; // Placeholder
                               std::map<std::string, std::string> query_params;

                               regulens::simulator::SimulatorAPIHandlers simulator_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>(),
                                   std::make_shared<regulens::StructuredLogger>(),
                                   std::make_shared<regulens::simulator::RegulatorySimulator>(
                                       std::make_shared<regulens::PostgreSQLConnection>(),
                                       std::make_shared<regulens::StructuredLogger>()
                                   )
                               );
                               std::string response = simulator_handlers.handle_get_simulation_analytics(user_id, query_params);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"simulator.analytics"}),

        create_endpoint("GET", "/simulator/scenarios/{id}/metrics",
                       "Get scenario performance metrics",
                       "simulator",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // Extract scenario ID from path
                               std::string path = req.path;
                               size_t scenarios_pos = path.find("/simulator/scenarios/");
                               size_t metrics_pos = path.find("/metrics");
                               if (scenarios_pos == std::string::npos || metrics_pos == std::string::npos) {
                                   return HTTPResponse(400, "Bad Request",
                                                     R"({"error": "Invalid scenario ID"})", "application/json");
                               }
                               std::string scenario_id = path.substr(scenarios_pos + 22, metrics_pos - (scenarios_pos + 22));

                               // Extract user ID from JWT token
                               std::string user_id = "current_user"; // Placeholder

                               regulens::simulator::SimulatorAPIHandlers simulator_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>(),
                                   std::make_shared<regulens::StructuredLogger>(),
                                   std::make_shared<regulens::simulator::RegulatorySimulator>(
                                       std::make_shared<regulens::PostgreSQLConnection>(),
                                       std::make_shared<regulens::StructuredLogger>()
                                   )
                               );
                               std::string response = simulator_handlers.handle_get_scenario_metrics(scenario_id, user_id);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"simulator.view"}),

        create_endpoint("GET", "/simulator/popular-scenarios",
                       "Get popular simulation scenarios",
                       "simulator",
                       [db_conn](const HTTPRequest& req, PGconn*) -> HTTPResponse {
                           try {
                               // Extract query parameters
                               std::map<std::string, std::string> query_params;

                               regulens::simulator::SimulatorAPIHandlers simulator_handlers(
                                   std::make_shared<regulens::PostgreSQLConnection>(),
                                   std::make_shared<regulens::StructuredLogger>(),
                                   std::make_shared<regulens::simulator::RegulatorySimulator>(
                                       std::make_shared<regulens::PostgreSQLConnection>(),
                                       std::make_shared<regulens::StructuredLogger>()
                                   )
                               );
                               std::string response = simulator_handlers.handle_get_popular_scenarios(query_params);
                               return HTTPResponse(200, "OK", response, "application/json");
                           } catch (const std::exception& e) {
                               return HTTPResponse(500, "Internal Server Error",
                                                 R"({"error": ")" + std::string(e.what()) + "\"}", "application/json");
                           }
                       }, true, {"simulator.view"})
    };
}

void register_simulator_endpoints(PGconn* db_conn) {
    auto endpoints = create_simulator_endpoints(db_conn);
    auto& api_registry = regulens::APIRegistry::get_instance();

    for (const auto& endpoint : endpoints) {
        api_registry.register_endpoint(endpoint);
    }
}

} // namespace regulens
