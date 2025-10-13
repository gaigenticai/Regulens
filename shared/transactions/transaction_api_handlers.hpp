/**
 * Transaction Analysis API Handlers - Header File
 * Production-grade transaction analysis endpoint declarations
 * Integrates fraud detection, pattern recognition, and anomaly detection
 */

#ifndef REGULENS_TRANSACTION_API_HANDLERS_HPP
#define REGULENS_TRANSACTION_API_HANDLERS_HPP

#include <string>
#include <map>
#include <memory>
#include <libpq-fe.h>
#include "../pattern_recognition.hpp"
#include "../fraud_detection/fraud_api_handlers.hpp"

namespace regulens {
namespace transactions {

// Initialize transaction analysis engines (should be called at startup)
bool initialize_transaction_engines(
    std::shared_ptr<PatternRecognitionEngine> pattern_engine
);

// Get shared engine instances
std::shared_ptr<PatternRecognitionEngine> get_pattern_engine();

// Transaction Analysis
std::string analyze_transaction(
    PGconn* db_conn,
    const std::string& transaction_id,
    const std::string& request_body,
    const std::string& user_id
);

// Fraud Analysis Retrieval
std::string get_transaction_fraud_analysis(
    PGconn* db_conn,
    const std::string& transaction_id
);

// Pattern Detection
std::string get_transaction_patterns(
    PGconn* db_conn,
    const std::map<std::string, std::string>& query_params
);

// Anomaly Detection
std::string detect_transaction_anomalies(
    PGconn* db_conn,
    const std::string& request_body,
    const std::string& user_id
);

// Metrics
std::string get_transaction_metrics(
    PGconn* db_conn,
    const std::map<std::string, std::string>& query_params
);

} // namespace transactions
} // namespace regulens

#endif // REGULENS_TRANSACTION_API_HANDLERS_HPP

