#ifndef DATA_QUALITY_HANDLERS_HPP
#define DATA_QUALITY_HANDLERS_HPP

#include <string>
#include <map>
#include <memory>
#include <vector>
#include "../database/postgresql_connection.hpp"
#include "../logging/structured_logger.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class DataQualityHandlers {
public:
    DataQualityHandlers(std::shared_ptr<regulens::PostgreSQLConnection> db_conn,
                       std::shared_ptr<regulens::StructuredLogger> logger);
    ~DataQualityHandlers() = default;

    // Data Quality Rules Management
    std::string list_quality_rules(const std::map<std::string, std::string>& headers);
    std::string create_quality_rule(const std::string& body, 
                                   const std::map<std::string, std::string>& headers);
    std::string get_quality_rule(const std::string& rule_id, 
                                const std::map<std::string, std::string>& headers);
    std::string update_quality_rule(const std::string& rule_id, 
                                   const std::string& body,
                                   const std::map<std::string, std::string>& headers);
    std::string delete_quality_rule(const std::string& rule_id, 
                                   const std::map<std::string, std::string>& headers);

    // Data Quality Checks
    std::string get_quality_checks(const std::map<std::string, std::string>& headers);
    std::string run_quality_check(const std::string& rule_id, 
                                 const std::map<std::string, std::string>& headers);
    std::string get_quality_dashboard(const std::map<std::string, std::string>& headers);
    std::string get_check_history(const std::map<std::string, std::string>& headers);

    // Data Quality Statistics
    std::string get_quality_metrics(const std::map<std::string, std::string>& headers);
    std::string get_quality_trends(const std::map<std::string, std::string>& headers);

private:
    std::shared_ptr<regulens::PostgreSQLConnection> db_conn_;
    std::shared_ptr<regulens::StructuredLogger> logger_;

    // Helper methods
    std::string extract_user_id_from_jwt(const std::map<std::string, std::string>& headers);
    std::string validate_json_input(const std::string& json_str);
    std::string generate_quality_score(int records_checked, int records_passed);
    std::string check_rule_condition(const std::string& rule_type, 
                                    const std::string& validation_logic,
                                    const std::string& data_source);
    
    // Data validation methods
    bool check_completeness_record(const regulens::PostgreSQLConnection::QueryResult& result, size_t row,
                                  const std::vector<std::string>& required_fields,
                                  const std::string& data_source);
    bool check_accuracy_record(const regulens::PostgreSQLConnection::QueryResult& result, size_t row,
                              const json& validation_config,
                              const std::string& data_source);
    bool check_consistency_record(const regulens::PostgreSQLConnection::QueryResult& result, size_t row,
                                 const json& validation_config,
                                 const std::string& data_source);
    bool check_timeliness_record(const regulens::PostgreSQLConnection::QueryResult& result, size_t row,
                                const json& validation_config,
                                const std::string& data_source);
    bool check_validity_record(const regulens::PostgreSQLConnection::QueryResult& result, size_t row,
                              const json& validation_config,
                              const std::string& data_source);
    
    // Utility methods
    std::string get_sample_failed_records(const std::string& data_source, 
                                         const std::string& validation_logic,
                                         int limit = 10);
    std::string calculate_quality_trends(const std::string& rule_id, int days = 30);
    std::string get_quality_summary_for_dashboard();
    
    // Utility functions
    std::string base64_decode(const std::string& encoded_string);
};

#endif // DATA_QUALITY_HANDLERS_HPP