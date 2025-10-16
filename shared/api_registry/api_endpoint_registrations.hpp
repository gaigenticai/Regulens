/**
 * API Endpoint Registrations Header
 * Declarations for all API endpoint registration functions
 */

#ifndef REGULENS_API_ENDPOINT_REGISTRATIONS_HPP
#define REGULENS_API_ENDPOINT_REGISTRATIONS_HPP

#include <libpq-fe.h>

namespace regulens {

/**
 * Register all authentication API endpoints
 */
void register_auth_endpoints(PGconn* db_conn);

/**
 * Register all transaction API endpoints
 */
void register_transaction_endpoints(PGconn* db_conn);

/**
 * Register all fraud detection API endpoints
 */
void register_fraud_endpoints(PGconn* db_conn);

/**
 * Register all memory management API endpoints
 */
void register_memory_endpoints(PGconn* db_conn);

/**
 * Register all knowledge base API endpoints
 */
void register_knowledge_endpoints(PGconn* db_conn);

/**
 * Register all decision management API endpoints
 */
void register_decision_endpoints(PGconn* db_conn);

/**
 * Register all LLM integration API endpoints
 */
void register_llm_endpoints(PGconn* db_conn);

/**
 * Register all pattern detection API endpoints
 */
void register_pattern_endpoints(PGconn* db_conn);

/**
 * Register all collaboration API endpoints
 */
void register_collaboration_endpoints(PGconn* db_conn);

/**
 * Register all alert management API endpoints
 */
void register_alert_endpoints(PGconn* db_conn);

/**
 * Register all export API endpoints
 */
void register_export_endpoints(PGconn* db_conn);

/**
 * Register all training API endpoints
 */
void register_training_endpoints(PGconn* db_conn);

/**
 * Create training API endpoints
 */
std::vector<APIEndpoint> create_training_endpoints(PGconn* db_conn);

/**
 * Simulator API Endpoints
 * Regulatory impact simulation and scenario management
 */
std::vector<APIEndpoint> create_simulator_endpoints(PGconn* db_conn);
void register_simulator_endpoints(PGconn* db_conn);

/**
 * Tool Categories API Endpoints
 * Tool category management and execution endpoints
 */
std::vector<APIEndpoint> create_tool_categories_endpoints(PGconn* db_conn);
void register_tool_categories_endpoints(PGconn* db_conn);

/**
 * Register all API endpoints
 * This function is called during server startup to register all endpoints
 */
void register_all_api_endpoints(PGconn* db_conn);

} // namespace regulens

#endif // REGULENS_API_ENDPOINT_REGISTRATIONS_HPP
