/**
 * Transaction API Handlers - Header File
 * Production-grade transaction management endpoint declarations
 * Implements CRUD operations, fraud analysis, and statistics
 */

#ifndef REGULENS_TRANSACTION_API_HANDLERS_HPP
#define REGULENS_TRANSACTION_API_HANDLERS_HPP

#include <string>
#include <map>
#include <libpq-fe.h>

namespace regulens {
namespace transactions {

// Transaction CRUD operations
std::string get_transactions(PGconn* db_conn, const std::map<std::string, std::string>& query_params);
std::string get_transaction_by_id(PGconn* db_conn, const std::string& transaction_id);
std::string create_transaction(PGconn* db_conn, const std::string& request_body, const std::string& user_id);
std::string update_transaction(PGconn* db_conn, const std::string& transaction_id, const std::string& request_body);
std::string delete_transaction(PGconn* db_conn, const std::string& transaction_id);

// Transaction analysis and fraud detection
std::string analyze_transaction(PGconn* db_conn, const std::string& transaction_id, const std::string& request_body);
std::string get_fraud_analysis(PGconn* db_conn, const std::string& transaction_id);
std::string approve_transaction(PGconn* db_conn, const std::string& transaction_id, const std::string& user_id, const std::string& request_body);
std::string reject_transaction(PGconn* db_conn, const std::string& transaction_id, const std::string& user_id, const std::string& request_body);

// Transaction patterns and anomalies
std::string get_transaction_patterns(PGconn* db_conn, const std::map<std::string, std::string>& query_params);
std::string detect_anomalies(PGconn* db_conn, const std::string& request_body);

// Transaction statistics and metrics
std::string get_transaction_stats(PGconn* db_conn, const std::map<std::string, std::string>& query_params);
std::string get_transaction_metrics(PGconn* db_conn, const std::map<std::string, std::string>& query_params);

// Helper functions
double calculate_transaction_risk_score(PGconn* db_conn, const std::string& transaction_id);
std::string generate_transaction_analysis(PGconn* db_conn, const std::string& transaction_id);
bool is_high_risk_transaction(PGconn* db_conn, const std::string& transaction_id);

} // namespace transactions
} // namespace regulens

#endif // REGULENS_TRANSACTION_API_HANDLERS_HPP
