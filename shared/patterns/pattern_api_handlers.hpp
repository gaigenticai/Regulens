/**
 * Pattern Analysis API Handlers - Header File
 * Production-grade pattern detection endpoint declarations
 * Uses PatternRecognitionEngine for actual pattern analysis
 */

#ifndef REGULENS_PATTERN_API_HANDLERS_HPP
#define REGULENS_PATTERN_API_HANDLERS_HPP

#include <string>
#include <map>
#include <memory>
#include <libpq-fe.h>
#include "../pattern_recognition.hpp"

namespace regulens {
namespace patterns {

// Initialize pattern recognition engine (should be called at startup)
bool initialize_pattern_engine(
    std::shared_ptr<PatternRecognitionEngine> engine
);

// Get shared pattern engine instance
std::shared_ptr<PatternRecognitionEngine> get_pattern_engine();

// Pattern Retrieval and Management
std::string get_patterns(
    PGconn* db_conn, 
    const std::map<std::string, std::string>& query_params
);

std::string get_pattern_by_id(
    PGconn* db_conn,
    const std::string& pattern_id
);

std::string get_pattern_stats(PGconn* db_conn);

// Pattern Detection
std::string start_pattern_detection(
    PGconn* db_conn,
    const std::string& request_body,
    const std::string& user_id
);

std::string get_pattern_job_status(
    PGconn* db_conn,
    const std::string& job_id
);

// Pattern Predictions
std::string get_pattern_predictions(
    PGconn* db_conn,
    const std::string& pattern_id,
    const std::map<std::string, std::string>& query_params
);

// Pattern Validation
std::string validate_pattern(
    PGconn* db_conn,
    const std::string& pattern_id,
    const std::string& request_body,
    const std::string& user_id
);

// Pattern Correlations
std::string get_pattern_correlations(
    PGconn* db_conn,
    const std::string& pattern_id,
    const std::map<std::string, std::string>& query_params
);

// Pattern Timeline
std::string get_pattern_timeline(
    PGconn* db_conn,
    const std::string& pattern_id,
    const std::map<std::string, std::string>& query_params
);

// Pattern Export
std::string export_pattern_report(
    PGconn* db_conn,
    const std::string& request_body,
    const std::string& user_id
);

// Pattern Anomalies
std::string get_pattern_anomalies(
    PGconn* db_conn,
    const std::map<std::string, std::string>& query_params
);

} // namespace patterns
} // namespace regulens

#endif // REGULENS_PATTERN_API_HANDLERS_HPP
