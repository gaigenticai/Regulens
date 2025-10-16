/**
 * Decision Management API Handlers - Complete Implementation Header File
 * Production-grade decision management endpoint declarations
 * Implements CRUD operations for decisions and decision analytics
 */

#ifndef REGULENS_DECISION_API_HANDLERS_COMPLETE_HPP
#define REGULENS_DECISION_API_HANDLERS_COMPLETE_HPP

#include <string>
#include <map>
#include <libpq-fe.h>

namespace regulens {
namespace decisions {

// Decision CRUD Operations
std::string get_decisions(PGconn* db_conn, const std::map<std::string, std::string>& query_params);
std::string get_decision_by_id(PGconn* db_conn, const std::string& decision_id);
std::string create_decision(PGconn* db_conn, const std::string& request_body, const std::string& user_id);
std::string update_decision(PGconn* db_conn, const std::string& decision_id, const std::string& request_body);
std::string delete_decision(PGconn* db_conn, const std::string& decision_id);

// Decision Analytics
std::string get_decision_stats(PGconn* db_conn, const std::map<std::string, std::string>& query_params);
std::string get_decision_outcomes(PGconn* db_conn, const std::map<std::string, std::string>& query_params);
std::string get_decision_timeline(PGconn* db_conn, const std::map<std::string, std::string>& query_params);

// Decision Review and Approval
std::string review_decision(PGconn* db_conn, const std::string& decision_id, const std::string& request_body, const std::string& user_id);
std::string approve_decision(PGconn* db_conn, const std::string& decision_id, const std::string& request_body, const std::string& user_id);
std::string reject_decision(PGconn* db_conn, const std::string& decision_id, const std::string& request_body, const std::string& user_id);

// Decision Templates
std::string get_decision_templates(PGconn* db_conn, const std::map<std::string, std::string>& query_params);
std::string create_decision_from_template(PGconn* db_conn, const std::string& request_body, const std::string& user_id);

// Decision Impact Analysis
std::string analyze_decision_impact(PGconn* db_conn, const std::string& request_body);
std::string get_decision_impact_report(PGconn* db_conn, const std::string& decision_id);

// Multi-Criteria Decision Analysis (MCDA)
std::string create_mcda_analysis(PGconn* db_conn, const std::string& request_body, const std::string& user_id);
std::string get_mcda_analysis(PGconn* db_conn, const std::string& analysis_id);
std::string update_mcda_criteria(PGconn* db_conn, const std::string& analysis_id, const std::string& request_body);
std::string evaluate_mcda_alternatives(PGconn* db_conn, const std::string& analysis_id, const std::string& request_body);

// Helper functions
std::string calculate_decision_confidence(PGconn* db_conn, const std::string& decision_id);
std::string generate_decision_summary(PGconn* db_conn, const std::string& decision_id);
std::vector<std::string> get_decision_stakeholders(PGconn* db_conn, const std::string& decision_id);

} // namespace decisions
} // namespace regulens

#endif // REGULENS_DECISION_API_HANDLERS_COMPLETE_HPP