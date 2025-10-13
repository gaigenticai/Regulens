/**
 * Fraud Detection API Handlers - Header File
 * Production-grade fraud detection endpoint declarations
 */

#ifndef REGULENS_FRAUD_API_HANDLERS_HPP
#define REGULENS_FRAUD_API_HANDLERS_HPP

#include <string>
#include <map>
#include <libpq-fe.h>

namespace regulens {
namespace fraud {

// Fraud Rules Management
std::string get_fraud_rule_by_id(PGconn* db_conn, const std::string& rule_id);
std::string create_fraud_rule(PGconn* db_conn, const std::string& request_body, const std::string& user_id);
std::string update_fraud_rule(PGconn* db_conn, const std::string& rule_id, const std::string& request_body);
std::string delete_fraud_rule(PGconn* db_conn, const std::string& rule_id);
std::string toggle_fraud_rule(PGconn* db_conn, const std::string& rule_id, const std::string& request_body);
std::string test_fraud_rule(PGconn* db_conn, const std::string& rule_id, const std::string& request_body);

// Fraud Alerts Management
std::string get_fraud_alerts(PGconn* db_conn, const std::map<std::string, std::string>& query_params);
std::string get_fraud_alert_by_id(PGconn* db_conn, const std::string& alert_id);
std::string update_fraud_alert_status(PGconn* db_conn, const std::string& alert_id, const std::string& request_body);

// Fraud Statistics
std::string get_fraud_stats(PGconn* db_conn, const std::map<std::string, std::string>& query_params);

// ML Models Management
std::string get_fraud_models(PGconn* db_conn);
std::string train_fraud_model(PGconn* db_conn, const std::string& request_body, const std::string& user_id);
std::string get_model_performance(PGconn* db_conn, const std::string& model_id);

// Batch Operations
std::string run_batch_fraud_scan(PGconn* db_conn, const std::string& request_body, const std::string& user_id);

// Reporting
std::string export_fraud_report(PGconn* db_conn, const std::string& request_body, const std::string& user_id);

} // namespace fraud
} // namespace regulens

#endif // REGULENS_FRAUD_API_HANDLERS_HPP
