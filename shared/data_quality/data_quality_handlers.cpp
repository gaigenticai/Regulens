#include "data_quality_handlers.hpp"
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <regex>

using json = nlohmann::json;

DataQualityHandlers::DataQualityHandlers(std::shared_ptr<regulens::PostgreSQLConnection> db_conn,
                                       std::shared_ptr<regulens::StructuredLogger> logger)
    : db_conn_(db_conn), logger_(logger) {
}

std::string DataQualityHandlers::extract_user_id_from_jwt(const std::map<std::string, std::string>& headers) {
    try {
        // Look for Authorization header
        auto auth_it = headers.find("authorization");
        if (auth_it == headers.end()) {
            auth_it = headers.find("Authorization");
        }
        
        if (auth_it == headers.end()) {
            logger_->log(regulens::LogLevel::WARN, "No Authorization header found in request");
            return "";
        }
        
        std::string auth_header = auth_it->second;
        
        // Extract Bearer token
        if (auth_header.substr(0, 7) != "Bearer ") {
            logger_->log(regulens::LogLevel::WARN, "Invalid Authorization header format, expected Bearer token");
            return "";
        }
        
        std::string token = auth_header.substr(7);
        
        // In a production system, this would verify the JWT signature and extract claims
        // For now, we'll implement a simple extraction of the user_id claim
        
        // Parse JWT payload (between the two dots)
        size_t first_dot = token.find('.');
        if (first_dot == std::string::npos) {
            logger_->log(regulens::LogLevel::WARN, "Invalid JWT format: missing first dot");
            return "";
        }
        
        size_t second_dot = token.find('.', first_dot + 1);
        if (second_dot == std::string::npos) {
            logger_->log(regulens::LogLevel::WARN, "Invalid JWT format: missing second dot");
            return "";
        }
        
        // Extract and decode payload
        std::string payload = token.substr(first_dot + 1, second_dot - first_dot - 1);
        
        // Base64 decode (simple implementation)
        std::string decoded_payload;
        try {
            // Add padding if needed
            while (payload.length() % 4) {
                payload += '=';
            }
            
            // Base64 decode
            decoded_payload = base64_decode(payload);
        } catch (const std::exception& e) {
            logger_->log(regulens::LogLevel::WARN, "Failed to decode JWT payload: " + std::string(e.what()));
            return "";
        }
        
        // Parse JSON payload
        json payload_json = json::parse(decoded_payload);
        
        if (payload_json.contains("user_id")) {
            return payload_json["user_id"];
        } else if (payload_json.contains("sub")) {
            return payload_json["sub"];
        } else {
            logger_->log(regulens::LogLevel::WARN, "No user_id or sub claim found in JWT payload");
            return "";
        }
        
    } catch (const json::exception& e) {
        logger_->log(regulens::LogLevel::ERROR, "JSON parsing error in extract_user_id_from_jwt: " + std::string(e.what()));
        return "";
    } catch (const std::exception& e) {
        logger_->log(regulens::LogLevel::ERROR, "Exception in extract_user_id_from_jwt: " + std::string(e.what()));
        return "";
    }
}

std::string DataQualityHandlers::validate_json_input(const std::string& json_str) {
    try {
        json::parse(json_str);
        return "";
    } catch (const json::parse_error& e) {
        return "Invalid JSON: " + std::string(e.what());
    }
}

std::string DataQualityHandlers::generate_quality_score(int records_checked, int records_passed) {
    if (records_checked == 0) {
        return "0.00";
    }
    
    double score = (static_cast<double>(records_passed) / records_checked) * 100.0;
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << score;
    return ss.str();
}

std::string DataQualityHandlers::list_quality_rules(const std::map<std::string, std::string>& headers) {
    try {
        std::string user_id = extract_user_id_from_jwt(headers);
        if (user_id.empty()) {
            return R"({"error": "Authentication required"})";
        }
        
        const char* query = "SELECT rule_id, rule_name, data_source, rule_type, validation_logic, "
                           "severity, is_enabled, created_at FROM data_quality_rules ORDER BY created_at DESC";
        
        auto result = db_conn_->execute_query(query, {});
        if (result.rows.empty()) {
            return R"({"error": "Database query failed"})";
        }
        
        json rules = json::array();
        
        for (const auto& row : result.rows) {
            json rule;
            rule["rule_id"] = row.at("rule_id");
            rule["rule_name"] = row.at("rule_name");
            rule["data_source"] = row.at("data_source");
            rule["rule_type"] = row.at("rule_type");
            rule["validation_logic"] = json::parse(row.at("validation_logic"));
            rule["severity"] = row.at("severity");
            rule["is_enabled"] = row.at("is_enabled") == "t";
            rule["created_at"] = row.at("created_at");
            rules.push_back(rule);
        }
        
        json response = {
            {"success", true},
            {"data", rules}
        };
        
        return response.dump();
    } catch (const std::exception& e) {
        logger_->error("Exception in list_quality_rules: " + std::string(e.what()), "DataQualityHandlers", "list_quality_rules");
        return R"({"error": "Internal server error"})";
    }
}

std::string DataQualityHandlers::create_quality_rule(const std::string& body, 
                                                   const std::map<std::string, std::string>& headers) {
    try {
        std::string user_id = extract_user_id_from_jwt(headers);
        if (user_id.empty()) {
            return R"({"error": "Authentication required"})";
        }
        
        std::string validation_error = validate_json_input(body);
        if (!validation_error.empty()) {
            return R"({"error": ")" + validation_error + R"("})";
        }
        
        json request = json::parse(body);
        
        // Validate required fields
        if (!request.contains("rule_name") || !request.contains("data_source") || 
            !request.contains("rule_type") || !request.contains("validation_logic") || 
            !request.contains("severity")) {
            return R"({"error": "Missing required fields: rule_name, data_source, rule_type, validation_logic, severity"})";
        }
        
        const char* query = "INSERT INTO data_quality_rules (rule_name, data_source, rule_type, "
                           "validation_logic, severity, is_enabled) VALUES ($1, $2, $3, $4, $5, $6) "
                           "RETURNING rule_id";
        
        std::string validation_logic_str = request["validation_logic"].dump();
        bool is_enabled = request.value("is_enabled", true);
        
        std::vector<std::string> params = {
            request["rule_name"].get<std::string>(),
            request["data_source"].get<std::string>(),
            request["rule_type"].get<std::string>(),
            validation_logic_str,
            request["severity"].get<std::string>(),
            is_enabled ? "t" : "f"
        };
        
        auto result = db_conn_->execute_query(query, params);
        
        if (result.rows.empty()) {
            return R"({"error": "Database query failed"})";
        }
        
        std::string rule_id = result.rows[0].at("rule_id");
        
        logger_->info("Data quality rule created: " + rule_id + " by user: " + user_id, "DataQualityHandlers", "create_quality_rule");
        
        json response = {
            {"success", true},
            {"data", {
                {"rule_id", rule_id},
                {"message", "Data quality rule created successfully"}
            }}
        };
        
        return response.dump();
    } catch (const std::exception& e) {
        logger_->error("Exception in create_quality_rule: " + std::string(e.what()), "DataQualityHandlers", "create_quality_rule");
        return R"({"error": "Internal server error"})";
    }
}

std::string DataQualityHandlers::get_quality_rule(const std::string& rule_id, 
                                                const std::map<std::string, std::string>& headers) {
    try {
        std::string user_id = extract_user_id_from_jwt(headers);
        if (user_id.empty()) {
            return R"({"error": "Authentication required"})";
        }
        
        const char* query = "SELECT rule_id, rule_name, data_source, rule_type, validation_logic, "
                           "severity, is_enabled, created_at FROM data_quality_rules WHERE rule_id = $1";
        
        std::vector<std::string> params = {rule_id};
        auto result = db_conn_->execute_query(query, params);
        
        if (result.rows.empty()) {
            return R"({"error": "Data quality rule not found"})";
        }
        
        json rule;
        rule["rule_id"] = result.rows[0].at("rule_id");
        rule["rule_name"] = result.rows[0].at("rule_name");
        rule["data_source"] = result.rows[0].at("data_source");
        rule["rule_type"] = result.rows[0].at("rule_type");
        rule["validation_logic"] = json::parse(result.rows[0].at("validation_logic"));
        rule["severity"] = result.rows[0].at("severity");
        rule["is_enabled"] = result.rows[0].at("is_enabled") == "t";
        rule["created_at"] = result.rows[0].at("created_at");
        
        json response = {
            {"success", true},
            {"data", rule}
        };
        
        return response.dump();
    } catch (const std::exception& e) {
        logger_->error("Exception in get_quality_rule: " + std::string(e.what()), "DataQualityHandlers", "get_quality_rule");
        return R"({"error": "Internal server error"})";
    }
}

std::string DataQualityHandlers::update_quality_rule(const std::string& rule_id, 
                                                   const std::string& body,
                                                   const std::map<std::string, std::string>& headers) {
    try {
        std::string user_id = extract_user_id_from_jwt(headers);
        if (user_id.empty()) {
            return R"({"error": "Authentication required"})";
        }
        
        std::string validation_error = validate_json_input(body);
        if (!validation_error.empty()) {
            return R"({"error": ")" + validation_error + R"("})";
        }
        
        json request = json::parse(body);
        
        // Check if rule exists
        const char* check_query = "SELECT rule_id FROM data_quality_rules WHERE rule_id = $1";
        std::vector<std::string> check_params = {rule_id};
        auto check_result = db_conn_->execute_query(check_query, check_params);
        
        if (check_result.rows.empty()) {
            return R"({"error": "Data quality rule not found"})";
        }
        
        // Build dynamic update query
        std::vector<std::string> set_clauses;
        std::vector<const char*> params;
        std::vector<std::string> param_values;
        
        if (request.contains("rule_name")) {
            set_clauses.push_back("rule_name = $" + std::to_string(params.size() + 1));
            param_values.push_back(request["rule_name"].get<std::string>());
            params.push_back(param_values.back().c_str());
        }
        
        if (request.contains("data_source")) {
            set_clauses.push_back("data_source = $" + std::to_string(params.size() + 1));
            param_values.push_back(request["data_source"].get<std::string>());
            params.push_back(param_values.back().c_str());
        }
        
        if (request.contains("rule_type")) {
            set_clauses.push_back("rule_type = $" + std::to_string(params.size() + 1));
            param_values.push_back(request["rule_type"].get<std::string>());
            params.push_back(param_values.back().c_str());
        }
        
        if (request.contains("validation_logic")) {
            set_clauses.push_back("validation_logic = $" + std::to_string(params.size() + 1));
            param_values.push_back(request["validation_logic"].dump());
            params.push_back(param_values.back().c_str());
        }
        
        if (request.contains("severity")) {
            set_clauses.push_back("severity = $" + std::to_string(params.size() + 1));
            param_values.push_back(request["severity"].get<std::string>());
            params.push_back(param_values.back().c_str());
        }
        
        if (request.contains("is_enabled")) {
            set_clauses.push_back("is_enabled = $" + std::to_string(params.size() + 1));
            param_values.push_back(request["is_enabled"].get<bool>() ? "t" : "f");
            params.push_back(param_values.back().c_str());
        }
        
        if (set_clauses.empty()) {
            return R"({"error": "No fields to update"})";
        }
        
        std::string query_str = "UPDATE data_quality_rules SET " + 
                               std::accumulate(set_clauses.begin(), set_clauses.end(), std::string(),
                                             [](const std::string& a, const std::string& b) {
                                                 return a.empty() ? b : a + ", " + b;
                                             }) +
                               " WHERE rule_id = $" + std::to_string(params.size() + 1);
        
        param_values.push_back(rule_id);
        params.push_back(param_values.back().c_str());
        
        std::vector<std::string> string_params;
        for (const auto& param : params) {
            string_params.push_back(param);
        }
        auto result = db_conn_->execute_query(query_str.c_str(), string_params);
        
        if (result.rows.empty()) {
            return R"({"error": "Database query failed"})";
        }
        
        logger_->info("Data quality rule updated: " + rule_id + " by user: " + user_id, "DataQualityHandlers", "update_quality_rule");
        
        json response = {
            {"success", true},
            {"data", {
                {"rule_id", rule_id},
                {"message", "Data quality rule updated successfully"}
            }}
        };
        
        return response.dump();
    } catch (const std::exception& e) {
        logger_->error("Exception in update_quality_rule: " + std::string(e.what()), "DataQualityHandlers", "update_quality_rule");
        return R"({"error": "Internal server error"})";
    }
}

std::string DataQualityHandlers::delete_quality_rule(const std::string& rule_id, 
                                                   const std::map<std::string, std::string>& headers) {
    try {
        std::string user_id = extract_user_id_from_jwt(headers);
        if (user_id.empty()) {
            return R"({"error": "Authentication required"})";
        }
        
        // Check if rule exists
        const char* check_query = "SELECT rule_id FROM data_quality_rules WHERE rule_id = $1";
        std::vector<std::string> check_params = {rule_id};
        auto check_result = db_conn_->execute_query(check_query, check_params);
        
        if (check_result.rows.empty()) {
            return R"({"error": "Data quality rule not found"})";
        }
        
        // Delete the rule
        const char* query = "DELETE FROM data_quality_rules WHERE rule_id = $1";
        auto result = db_conn_->execute_query(query, check_params);
        
        if (result.rows.empty()) {
            return R"({"error": "Database query failed"})";
        }
        
        logger_->info("Data quality rule deleted: " + rule_id + " by user: " + user_id, "DataQualityHandlers", "delete_quality_rule");
        
        json response = {
            {"success", true},
            {"data", {
                {"rule_id", rule_id},
                {"message", "Data quality rule deleted successfully"}
            }}
        };
        
        return response.dump();
    } catch (const std::exception& e) {
        logger_->error("Exception in delete_quality_rule: " + std::string(e.what()), "DataQualityHandlers", "delete_quality_rule");
        return R"({"error": "Internal server error"})";
    }
}

std::string DataQualityHandlers::get_quality_checks(const std::map<std::string, std::string>& headers) {
    try {
        std::string user_id = extract_user_id_from_jwt(headers);
        if (user_id.empty()) {
            return R"({"error": "Authentication required"})";
        }
        
        const char* query = "SELECT c.check_id, c.rule_id, r.rule_name, r.data_source, r.rule_type, "
                           "c.check_timestamp, c.records_checked, c.records_passed, c.records_failed, "
                           "c.quality_score, c.execution_time_ms, c.status "
                           "FROM data_quality_checks c "
                           "JOIN data_quality_rules r ON c.rule_id = r.rule_id "
                           "ORDER BY c.check_timestamp DESC LIMIT 100";
        
        auto result = db_conn_->execute_query(query, {});
        if (result.rows.empty()) {
            json checks = json::array();
            return R"({"success": true, "data": []})";
        }
        
        json checks = json::array();
        
        for (const auto& row : result.rows) {
            json check;
            check["check_id"] = row.at("check_id");
            check["rule_id"] = row.at("rule_id");
            check["rule_name"] = row.at("rule_name");
            check["data_source"] = row.at("data_source");
            check["rule_type"] = row.at("rule_type");
            check["check_timestamp"] = row.at("check_timestamp");
            check["records_checked"] = std::stoi(row.at("records_checked"));
            check["records_passed"] = std::stoi(row.at("records_passed"));
            check["records_failed"] = std::stoi(row.at("records_failed"));
            check["quality_score"] = std::stod(row.at("quality_score"));
            check["execution_time_ms"] = std::stoi(row.at("execution_time_ms"));
            check["status"] = row.at("status");
            checks.push_back(check);
        }
        
        json response = {
            {"success", true},
            {"data", checks}
        };
        
        return response.dump();
    } catch (const std::exception& e) {
        logger_->error("Exception in get_quality_checks: " + std::string(e.what()), "DataQualityHandlers", "get_quality_checks");
        return R"({"error": "Internal server error"})";
    }
}

std::string DataQualityHandlers::run_quality_check(const std::string& rule_id, 
                                                 const std::map<std::string, std::string>& headers) {
    try {
        std::string user_id = extract_user_id_from_jwt(headers);
        if (user_id.empty()) {
            return R"({"error": "Authentication required"})";
        }
        
        // Get rule details
        const char* rule_query = "SELECT rule_id, rule_name, data_source, rule_type, validation_logic, "
                                "severity FROM data_quality_rules WHERE rule_id = $1 AND is_enabled = true";
        
        std::vector<std::string> rule_params = {rule_id};
        auto rule_result = db_conn_->execute_query(rule_query, rule_params);
        
        if (rule_result.rows.empty()) {
            return R"({"error": "Enabled data quality rule not found"})";
        }
        
        std::string rule_name = rule_result.rows[0].at("rule_name");
        std::string data_source = rule_result.rows[0].at("data_source");
        std::string rule_type = rule_result.rows[0].at("rule_type");
        std::string validation_logic = rule_result.rows[0].at("validation_logic");
        std::string severity = rule_result.rows[0].at("severity");
        
        // Record start time
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Execute the quality check based on rule type
        std::string check_result = check_rule_condition(rule_type, validation_logic, data_source);
        
        // Record end time
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        // Parse the check result
        json result_json = json::parse(check_result);
        int records_checked = result_json["records_checked"];
        int records_passed = result_json["records_passed"];
        int records_failed = result_json["records_failed"];
        std::string quality_score_str = generate_quality_score(records_checked, records_passed);
        double quality_score = std::stod(quality_score_str);
        
        // Determine status based on quality score
        std::string status;
        if (quality_score >= 90.0) {
            status = "passed";
        } else if (quality_score >= 70.0) {
            status = "warning";
        } else {
            status = "failed";
        }
        
        // Get sample failed records
        std::string failed_records = get_sample_failed_records(data_source, validation_logic);
        
        // Insert check result
        const char* insert_query = "INSERT INTO data_quality_checks (rule_id, check_timestamp, "
                                  "records_checked, records_passed, records_failed, quality_score, "
                                  "failed_records, execution_time_ms, status) "
                                  "VALUES ($1, NOW(), $2, $3, $4, $5, $6, $7, $8) "
                                  "RETURNING check_id";
        
        std::vector<std::string> insert_params = {
            rule_id,
            std::to_string(records_checked),
            std::to_string(records_passed),
            std::to_string(records_failed),
            quality_score_str,
            failed_records,
            std::to_string(duration.count()),
            status
        };
        
        auto insert_result = db_conn_->execute_query(insert_query, insert_params);
        
        if (insert_result.rows.empty()) {
            return R"({"error": "Database query failed"})";
        }
        
        std::string check_id = insert_result.rows[0].at("check_id");
        
        logger_->info("Data quality check executed: " + check_id + " for rule: " + rule_name +
                    " with score: " + quality_score_str + " by user: " + user_id, "DataQualityHandlers", "run_quality_check");
        
        json response = {
            {"success", true},
            {"data", {
                {"check_id", check_id},
                {"rule_id", rule_id},
                {"rule_name", rule_name},
                {"records_checked", records_checked},
                {"records_passed", records_passed},
                {"records_failed", records_failed},
                {"quality_score", quality_score},
                {"status", status},
                {"execution_time_ms", duration.count()},
                {"message", "Data quality check completed successfully"}
            }}
        };
        
        return response.dump();
    } catch (const std::exception& e) {
        logger_->error("Exception in run_quality_check: " + std::string(e.what()), "DataQualityHandlers", "run_quality_check");
        return R"({"error": "Internal server error"})";
    }
}

std::string DataQualityHandlers::get_quality_dashboard(const std::map<std::string, std::string>& headers) {
    try {
        std::string user_id = extract_user_id_from_jwt(headers);
        if (user_id.empty()) {
            return R"({"error": "Authentication required"})";
        }
        
        // Get overall quality metrics
        const char* metrics_query = "SELECT "
                                   "COUNT(*) as total_checks, "
                                   "AVG(quality_score) as avg_quality_score, "
                                   "COUNT(CASE WHEN status = 'passed' THEN 1 END) as passed_checks, "
                                   "COUNT(CASE WHEN status = 'warning' THEN 1 END) as warning_checks, "
                                   "COUNT(CASE WHEN status = 'failed' THEN 1 END) as failed_checks "
                                   "FROM data_quality_checks WHERE check_timestamp > NOW() - INTERVAL '24 hours'";
        
        auto metrics_result = db_conn_->execute_query(metrics_query, {});
        if (metrics_result.rows.empty()) {
            json metrics = {
                {"total_checks", 0},
                {"avg_quality_score", 0.0},
                {"passed_checks", 0},
                {"warning_checks", 0},
                {"failed_checks", 0}
            };
            
            json response = {
                {"success", true},
                {"data", {
                    {"summary", metrics},
                    {"by_source", json::array()},
                    {"recent_failures", json::array()}
                }}
            };
            return response.dump();
        }
        
        json metrics;
        metrics["total_checks"] = std::stoi(metrics_result.rows[0].at("total_checks"));
        metrics["avg_quality_score"] = std::stod(metrics_result.rows[0].at("avg_quality_score"));
        metrics["passed_checks"] = std::stoi(metrics_result.rows[0].at("passed_checks"));
        metrics["warning_checks"] = std::stoi(metrics_result.rows[0].at("warning_checks"));
        metrics["failed_checks"] = std::stoi(metrics_result.rows[0].at("failed_checks"));
        
        // Get quality by data source
        const char* source_query = "SELECT r.data_source, COUNT(*) as checks, AVG(c.quality_score) as avg_score "
                                  "FROM data_quality_checks c "
                                  "JOIN data_quality_rules r ON c.rule_id = r.rule_id "
                                  "WHERE c.check_timestamp > NOW() - INTERVAL '24 hours' "
                                  "GROUP BY r.data_source ORDER BY avg_score DESC";
        
        auto source_result = db_conn_->execute_query(source_query, {});
        
        json by_source = json::array();
        
        for (const auto& row : source_result.rows) {
            json source;
            source["data_source"] = row.at("data_source");
            source["checks"] = std::stoi(row.at("checks"));
            source["avg_score"] = std::stod(row.at("avg_score"));
            by_source.push_back(source);
        }
        
        // Get recent failed checks
        const char* failed_query = "SELECT c.check_id, c.rule_id, r.rule_name, r.data_source, "
                                  "c.check_timestamp, c.quality_score "
                                  "FROM data_quality_checks c "
                                  "JOIN data_quality_rules r ON c.rule_id = r.rule_id "
                                  "WHERE c.status = 'failed' AND c.check_timestamp > NOW() - INTERVAL '24 hours' "
                                  "ORDER BY c.check_timestamp DESC LIMIT 10";
        
        auto failed_result = db_conn_->execute_query(failed_query, {});
        
        json recent_failures = json::array();
        
        for (const auto& row : failed_result.rows) {
            json failure;
            failure["check_id"] = row.at("check_id");
            failure["rule_id"] = row.at("rule_id");
            failure["rule_name"] = row.at("rule_name");
            failure["data_source"] = row.at("data_source");
            failure["check_timestamp"] = row.at("check_timestamp");
            failure["quality_score"] = std::stod(row.at("quality_score"));
            recent_failures.push_back(failure);
        }
        
        json response = {
            {"success", true},
            {"data", {
                {"summary", metrics},
                {"by_source", by_source},
                {"recent_failures", recent_failures}
            }}
        };
        
        return response.dump();
    } catch (const std::exception& e) {
        logger_->error("Exception in get_quality_dashboard: " + std::string(e.what()), "DataQualityHandlers", "get_quality_dashboard");
        return R"({"error": "Internal server error"})";
    }
}

std::string DataQualityHandlers::get_check_history(const std::map<std::string, std::string>& headers) {
    try {
        std::string user_id = extract_user_id_from_jwt(headers);
        if (user_id.empty()) {
            return R"({"error": "Authentication required"})";
        }
        
        const char* query = "SELECT DATE(check_timestamp) as check_date, "
                           "COUNT(*) as total_checks, "
                           "AVG(quality_score) as avg_score, "
                           "COUNT(CASE WHEN status = 'passed' THEN 1 END) as passed, "
                           "COUNT(CASE WHEN status = 'warning' THEN 1 END) as warning, "
                           "COUNT(CASE WHEN status = 'failed' THEN 1 END) as failed "
                           "FROM data_quality_checks "
                           "WHERE check_timestamp > NOW() - INTERVAL '30 days' "
                           "GROUP BY DATE(check_timestamp) ORDER BY check_date DESC";
        
        auto result = db_conn_->execute_query(query, {});
        if (result.rows.empty()) {
            json history = json::array();
            return R"({"success": true, "data": []})";
        }
        
        json history = json::array();
        
        for (const auto& row : result.rows) {
            json day;
            day["date"] = row.at("check_date");
            day["total_checks"] = std::stoi(row.at("total_checks"));
            day["avg_score"] = std::stod(row.at("avg_score"));
            day["passed"] = std::stoi(row.at("passed"));
            day["warning"] = std::stoi(row.at("warning"));
            day["failed"] = std::stoi(row.at("failed"));
            history.push_back(day);
        }
        
        json response = {
            {"success", true},
            {"data", history}
        };
        
        return response.dump();
    } catch (const std::exception& e) {
        logger_->error("Exception in get_check_history: " + std::string(e.what()), "DataQualityHandlers", "get_check_history");
        return R"({"error": "Internal server error"})";
    }
}

std::string DataQualityHandlers::get_quality_metrics(const std::map<std::string, std::string>& headers) {
    try {
        std::string user_id = extract_user_id_from_jwt(headers);
        if (user_id.empty()) {
            return R"({"error": "Authentication required"})";
        }
        
        // Get overall metrics
        const char* overall_query = "SELECT "
                                  "COUNT(DISTINCT rule_id) as total_rules, "
                                  "COUNT(CASE WHEN is_enabled THEN 1 END) as enabled_rules, "
                                  "COUNT(*) as total_checks, "
                                  "AVG(quality_score) as overall_avg_score "
                                  "FROM data_quality_rules r "
                                  "LEFT JOIN data_quality_checks c ON r.rule_id = c.rule_id";
        
        auto overall_result = db_conn_->execute_query(overall_query, {});
        if (overall_result.rows.empty()) {
            return R"({"error": "Database query failed"})";
        }
        
        json overall;
        overall["total_rules"] = std::stoi(overall_result.rows[0].at("total_rules"));
        overall["enabled_rules"] = std::stoi(overall_result.rows[0].at("enabled_rules"));
        overall["total_checks"] = std::stoi(overall_result.rows[0].at("total_checks"));
        overall["overall_avg_score"] = std::stod(overall_result.rows[0].at("overall_avg_score"));
        
        // Get metrics by rule type
        const char* type_query = "SELECT rule_type, COUNT(*) as rule_count, "
                                "AVG(c.quality_score) as avg_score "
                                "FROM data_quality_rules r "
                                "LEFT JOIN data_quality_checks c ON r.rule_id = c.rule_id "
                                "GROUP BY rule_type ORDER BY rule_count DESC";
        
        auto type_result = db_conn_->execute_query(type_query, {});
        
        json by_type = json::array();
        
        for (const auto& row : type_result.rows) {
            json type;
            type["rule_type"] = row.at("rule_type");
            type["rule_count"] = std::stoi(row.at("rule_count"));
            type["avg_score"] = std::stod(row.at("avg_score"));
            by_type.push_back(type);
        }
        
        json response = {
            {"success", true},
            {"data", {
                {"overall", overall},
                {"by_type", by_type}
            }}
        };
        
        return response.dump();
    } catch (const std::exception& e) {
        logger_->error("Exception in get_quality_metrics: " + std::string(e.what()), "DataQualityHandlers", "get_quality_metrics");
        return R"({"error": "Internal server error"})";
    }
}

std::string DataQualityHandlers::get_quality_trends(const std::map<std::string, std::string>& headers) {
    try {
        std::string user_id = extract_user_id_from_jwt(headers);
        if (user_id.empty()) {
            return R"({"error": "Authentication required"})";
        }
        
        const char* query = "SELECT DATE(check_timestamp) as check_date, "
                           "r.rule_type, AVG(c.quality_score) as avg_score "
                           "FROM data_quality_checks c "
                           "JOIN data_quality_rules r ON c.rule_id = r.rule_id "
                           "WHERE c.check_timestamp > NOW() - INTERVAL '30 days' "
                           "GROUP BY DATE(check_timestamp), r.rule_type "
                           "ORDER BY check_date DESC, r.rule_type";
        
        auto result = db_conn_->execute_query(query, {});
        
        json trends = json::array();
        
        for (const auto& row : result.rows) {
            json trend;
            trend["date"] = row.at("check_date");
            trend["rule_type"] = row.at("rule_type");
            trend["avg_score"] = std::stod(row.at("avg_score"));
            trends.push_back(trend);
        }
        
        json response = {
            {"success", true},
            {"data", trends}
        };
        
        return response.dump();
    } catch (const std::exception& e) {
        logger_->error("Exception in get_quality_trends: " + std::string(e.what()), "DataQualityHandlers", "get_quality_trends");
        return R"({"error": "Internal server error"})";
    }
}

std::string DataQualityHandlers::check_rule_condition(const std::string& rule_type,
                                                    const std::string& validation_logic,
                                                    const std::string& data_source) {
    try {
        if (!db_conn_->is_connected()) {
            logger_->error("Database connection failed in check_rule_condition", "DataQualityHandlers", "check_rule_condition");
            return R"({"records_checked": 0, "records_passed": 0, "records_failed": 0})";
        }
        
        json validation_config = json::parse(validation_logic);
        int records_checked = 0;
        int records_passed = 0;
        int records_failed = 0;
        
        // Build appropriate query based on data source
        std::string query;
        std::vector<std::string> required_fields;
        
        if (data_source == "transactions") {
            query = "SELECT transaction_id, amount, currency, status, created_at FROM transactions";
            if (validation_config.contains("required_fields")) {
                for (const auto& field : validation_config["required_fields"]) {
                    required_fields.push_back(field.get<std::string>());
                }
            }
        } else if (data_source == "customers") {
            query = "SELECT customer_id, name, email, phone, created_at FROM customers";
            if (validation_config.contains("required_fields")) {
                for (const auto& field : validation_config["required_fields"]) {
                    required_fields.push_back(field.get<std::string>());
                }
            }
        } else if (data_source == "regulatory_changes") {
            query = "SELECT change_id, regulation_id, description, effective_date, status FROM regulatory_changes";
            if (validation_config.contains("required_fields")) {
                for (const auto& field : validation_config["required_fields"]) {
                    required_fields.push_back(field.get<std::string>());
                }
            }
        } else {
            logger_->error("Unknown data source: " + data_source, "DataQualityHandlers", "check_rule_condition");
            return R"({"records_checked": 0, "records_passed": 0, "records_failed": 0})";
        }
        
        // Add time filtering if specified
        if (validation_config.contains("time_filter_hours")) {
            int hours = validation_config["time_filter_hours"];
            query += " WHERE created_at > NOW() - INTERVAL '" + std::to_string(hours) + " hours'";
        }
        
        auto result = db_conn_->execute_query(query, {});
        records_checked = result.rows.size();
        
        // Check each record based on rule type
        for (size_t i = 0; i < result.rows.size(); i++) {
            bool record_passed = true;
            
            if (rule_type == "completeness") {
                record_passed = check_completeness_record(result, i, required_fields, data_source);
            } else if (rule_type == "accuracy") {
                record_passed = check_accuracy_record(result, i, validation_config, data_source);
            } else if (rule_type == "consistency") {
                record_passed = check_consistency_record(result, i, validation_config, data_source);
            } else if (rule_type == "timeliness") {
                record_passed = check_timeliness_record(result, i, validation_config, data_source);
            } else if (rule_type == "validity") {
                record_passed = check_validity_record(result, i, validation_config, data_source);
            }
            
            if (record_passed) {
                records_passed++;
            } else {
                records_failed++;
            }
        }
        
        json result_data = {
            {"records_checked", records_checked},
            {"records_passed", records_passed},
            {"records_failed", records_failed}
        };
        
        return result_data.dump();
    } catch (const std::exception& e) {
        logger_->error("Exception in check_rule_condition: " + std::string(e.what()), "DataQualityHandlers", "check_rule_condition");
        return R"({"records_checked": 0, "records_passed": 0, "records_failed": 0})";
    }
}

bool DataQualityHandlers::check_completeness_record(const regulens::PostgreSQLConnection::QueryResult& result, size_t row,
                                                   const std::vector<std::string>& required_fields,
                                                   const std::string& data_source) {
    // Check if all required fields are present and not null
    for (const auto& field : required_fields) {
        bool field_exists = false;
        bool field_is_null = false;
        std::string field_value;
        
        // Check field in result based on data source
        if (data_source == "transactions") {
            if (field == "transaction_id") {
                field_exists = true;
                field_value = result.rows[row].at("transaction_id");
                field_is_null = field_value.empty() || field_value == "NULL";
            } else if (field == "amount") {
                field_exists = true;
                field_value = result.rows[row].at("amount");
                field_is_null = field_value.empty() || field_value == "NULL";
            } else if (field == "currency") {
                field_exists = true;
                field_value = result.rows[row].at("currency");
                field_is_null = field_value.empty() || field_value == "NULL";
            } else if (field == "status") {
                field_exists = true;
                field_value = result.rows[row].at("status");
                field_is_null = field_value.empty() || field_value == "NULL";
            }
        } else if (data_source == "customers") {
            if (field == "customer_id") {
                field_exists = true;
                field_value = result.rows[row].at("customer_id");
                field_is_null = field_value.empty() || field_value == "NULL";
            } else if (field == "name") {
                field_exists = true;
                field_value = result.rows[row].at("name");
                field_is_null = field_value.empty() || field_value == "NULL";
            } else if (field == "email") {
                field_exists = true;
                field_value = result.rows[row].at("email");
                field_is_null = field_value.empty() || field_value == "NULL";
            } else if (field == "phone") {
                field_exists = true;
                field_value = result.rows[row].at("phone");
                field_is_null = field_value.empty() || field_value == "NULL";
            }
        } else if (data_source == "regulatory_changes") {
            if (field == "change_id") {
                field_exists = true;
                field_value = result.rows[row].at("change_id");
                field_is_null = field_value.empty() || field_value == "NULL";
            } else if (field == "regulation_id") {
                field_exists = true;
                field_value = result.rows[row].at("regulation_id");
                field_is_null = field_value.empty() || field_value == "NULL";
            } else if (field == "description") {
                field_exists = true;
                field_value = result.rows[row].at("description");
                field_is_null = field_value.empty() || field_value == "NULL";
            } else if (field == "effective_date") {
                field_exists = true;
                field_value = result.rows[row].at("effective_date");
                field_is_null = field_value.empty() || field_value == "NULL";
            } else if (field == "status") {
                field_exists = true;
                field_value = result.rows[row].at("status");
                field_is_null = field_value.empty() || field_value == "NULL";
            }
        }
        
        if (field_exists && field_is_null) {
            return false; // Field is null or empty
        }
    }
    
    return true;
}

bool DataQualityHandlers::check_accuracy_record(const regulens::PostgreSQLConnection::QueryResult& result, size_t row,
                                               const json& validation_config,
                                               const std::string& data_source) {
    // Check data accuracy based on validation rules
    try {
        if (data_source == "transactions") {
            if (validation_config.contains("amount_range")) {
                double amount = std::stod(result.rows[row].at("amount"));
                double min_amount = validation_config["amount_range"]["min"];
                double max_amount = validation_config["amount_range"]["max"];
                
                if (amount < min_amount || amount > max_amount) {
                    return false;
                }
            }
            
            if (validation_config.contains("valid_currencies")) {
                std::string currency = result.rows[row].at("currency");
                bool valid_currency = false;
                for (const auto& valid_curr : validation_config["valid_currencies"]) {
                    if (currency == valid_curr.get<std::string>()) {
                        valid_currency = true;
                        break;
                    }
                }
                if (!valid_currency) {
                    return false;
                }
            }
            
            if (validation_config.contains("valid_statuses")) {
                std::string status = result.rows[row].at("status");
                bool valid_status = false;
                for (const auto& valid_stat : validation_config["valid_statuses"]) {
                    if (status == valid_stat.get<std::string>()) {
                        valid_status = true;
                        break;
                    }
                }
                if (!valid_status) {
                    return false;
                }
            }
        } else if (data_source == "customers") {
            if (validation_config.contains("email_format")) {
                std::string email = result.rows[row].at("email");
                std::regex email_regex(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})");
                if (!std::regex_match(email, email_regex)) {
                    return false;
                }
            }
            
            if (validation_config.contains("phone_format")) {
                std::string phone = result.rows[row].at("phone");
                std::regex phone_regex(R"(\+?[\d\s\-\(\)]{10,})");
                if (!std::regex_match(phone, phone_regex)) {
                    return false;
                }
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        logger_->error("Exception in check_accuracy_record: " + std::string(e.what()), "DataQualityHandlers", "check_accuracy_record");
        return false;
    }
}

bool DataQualityHandlers::check_consistency_record(const regulens::PostgreSQLConnection::QueryResult& result, size_t row,
                                                 const json& validation_config,
                                                 const std::string& data_source) {
    // Check data consistency across related records
    try {
        if (data_source == "transactions") {
            std::string transaction_id = result.rows[row].at("transaction_id");
            std::string status = result.rows[row].at("status");
            
            // Check if transaction status is consistent with related records
            if (validation_config.contains("status_consistency")) {
                const char* check_query = "SELECT COUNT(*) FROM transaction_logs "
                                        "WHERE transaction_id = $1 AND status != $2";
                
                std::vector<std::string> params = {transaction_id, status};
                auto check_result = db_conn_->execute_query(check_query, params);
                
                if (!check_result.rows.empty()) {
                    int inconsistent_count = std::stoi(check_result.rows[0].at("count"));
                    
                    if (inconsistent_count > 0) {
                        return false;
                    }
                }
            }
        } else if (data_source == "customers") {
            std::string customer_id = result.rows[row].at("customer_id");
            std::string email = result.rows[row].at("email");
            
            // Check if customer email is unique
            if (validation_config.contains("email_uniqueness")) {
                const char* check_query = "SELECT COUNT(*) FROM customers "
                                        "WHERE email = $1 AND customer_id != $2";
                
                std::vector<std::string> params = {email, customer_id};
                auto check_result = db_conn_->execute_query(check_query, params);
                
                if (!check_result.rows.empty()) {
                    int duplicate_count = std::stoi(check_result.rows[0].at("count"));
                    
                    if (duplicate_count > 0) {
                        return false;
                    }
                }
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        logger_->error("Exception in check_consistency_record: " + std::string(e.what()), "DataQualityHandlers", "check_consistency_record");
        return false;
    }
}

bool DataQualityHandlers::check_timeliness_record(const regulens::PostgreSQLConnection::QueryResult& result, size_t row,
                                                const json& validation_config,
                                                const std::string& data_source) {
    // Check if data is updated within expected timeframes
    try {
        if (validation_config.contains("max_age_hours")) {
            int max_age_hours = validation_config["max_age_hours"];
            
            if (data_source == "transactions") {
                std::string created_at_str = result.rows[row].at("created_at");
                
                // Parse timestamp and check age
                std::tm tm = {};
                std::istringstream ss(created_at_str);
                ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
                
                if (ss.fail()) {
                    return false; // Invalid timestamp format
                }
                
                auto record_time = std::chrono::system_clock::from_time_t(std::mktime(&tm));
                auto now = std::chrono::system_clock::now();
                auto age = std::chrono::duration_cast<std::chrono::hours>(now - record_time).count();
                
                if (age > max_age_hours) {
                    return false;
                }
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        logger_->error("Exception in check_timeliness_record: " + std::string(e.what()), "DataQualityHandlers", "check_timeliness_record");
        return false;
    }
}

bool DataQualityHandlers::check_validity_record(const regulens::PostgreSQLConnection::QueryResult& result, size_t row,
                                               const json& validation_config,
                                               const std::string& data_source) {
    // Check if data conforms to expected formats and value ranges
    try {
        if (data_source == "transactions") {
            if (validation_config.contains("amount_precision")) {
                int max_precision = validation_config["amount_precision"];
                std::string amount_str = result.rows[row].at("amount");
                
                // Check decimal precision
                size_t decimal_pos = amount_str.find('.');
                if (decimal_pos != std::string::npos) {
                    int precision = amount_str.length() - decimal_pos - 1;
                    if (precision > max_precision) {
                        return false;
                    }
                }
            }
        } else if (data_source == "customers") {
            if (validation_config.contains("name_min_length")) {
                int min_length = validation_config["name_min_length"];
                std::string name = result.rows[row].at("name");
                
                if (name.length() < min_length) {
                    return false;
                }
            }
        } else if (data_source == "regulatory_changes") {
            if (validation_config.contains("description_min_length")) {
                int min_length = validation_config["description_min_length"];
                std::string description = result.rows[row].at("description");
                
                if (description.length() < min_length) {
                    return false;
                }
            }
            
            if (validation_config.contains("valid_statuses")) {
                std::string status = result.rows[row].at("status");
                bool valid_status = false;
                for (const auto& valid_stat : validation_config["valid_statuses"]) {
                    if (status == valid_stat.get<std::string>()) {
                        valid_status = true;
                        break;
                    }
                }
                if (!valid_status) {
                    return false;
                }
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        logger_->error("Exception in check_validity_record: " + std::string(e.what()), "DataQualityHandlers", "check_validity_record");
        return false;
    }
}

std::string DataQualityHandlers::get_sample_failed_records(const std::string& data_source,
                                                         const std::string& validation_logic,
                                                         int limit) {
    try {
        if (!db_conn_->is_connected()) {
            logger_->error("Database connection failed in get_sample_failed_records", "DataQualityHandlers", "get_sample_failed_records");
            return "[]";
        }
        
        json validation_config = json::parse(validation_logic);
        json samples = json::array();
        std::string query;
        
        // Build query to get sample records from the data source
        if (data_source == "transactions") {
            query = "SELECT transaction_id, amount, currency, status, created_at FROM transactions";
        } else if (data_source == "customers") {
            query = "SELECT customer_id, name, email, phone, created_at FROM customers";
        } else if (data_source == "regulatory_changes") {
            query = "SELECT change_id, regulation_id, description, effective_date, status FROM regulatory_changes";
        } else {
            return "[]";
        }
        
        query += " LIMIT " + std::to_string(limit * 2); // Get more records to find failed ones
        
        auto result = db_conn_->execute_query(query, {});
        std::string rule_type = validation_config.value("rule_type", "completeness");
        
        for (size_t i = 0; i < result.rows.size() && samples.size() < static_cast<size_t>(limit); i++) {
            bool record_passed = true;
            std::vector<std::string> required_fields;
            
            if (validation_config.contains("required_fields")) {
                for (const auto& field : validation_config["required_fields"]) {
                    required_fields.push_back(field.get<std::string>());
                }
            }
            
            if (rule_type == "completeness") {
                record_passed = check_completeness_record(result, i, required_fields, data_source);
            } else if (rule_type == "accuracy") {
                record_passed = check_accuracy_record(result, i, validation_config, data_source);
            } else if (rule_type == "consistency") {
                record_passed = check_consistency_record(result, i, validation_config, data_source);
            } else if (rule_type == "timeliness") {
                record_passed = check_timeliness_record(result, i, validation_config, data_source);
            } else if (rule_type == "validity") {
                record_passed = check_validity_record(result, i, validation_config, data_source);
            }
            
            if (!record_passed) {
                json sample;
                
                if (data_source == "transactions") {
                    sample["record_id"] = result.rows[i].at("transaction_id");
                    sample["amount"] = result.rows[i].at("amount");
                    sample["currency"] = result.rows[i].at("currency");
                    sample["status"] = result.rows[i].at("status");
                    sample["created_at"] = result.rows[i].at("created_at");
                } else if (data_source == "customers") {
                    sample["record_id"] = result.rows[i].at("customer_id");
                    sample["name"] = result.rows[i].at("name");
                    sample["email"] = result.rows[i].at("email");
                    sample["phone"] = result.rows[i].at("phone");
                    sample["created_at"] = result.rows[i].at("created_at");
                } else if (data_source == "regulatory_changes") {
                    sample["record_id"] = result.rows[i].at("change_id");
                    sample["regulation_id"] = result.rows[i].at("regulation_id");
                    sample["description"] = result.rows[i].at("description");
                    sample["effective_date"] = result.rows[i].at("effective_date");
                    sample["status"] = result.rows[i].at("status");
                }
                
                sample["error"] = "Failed " + rule_type + " check";
                samples.push_back(sample);
            }
        }
        
        return samples.dump();
    } catch (const std::exception& e) {
        logger_->error("Exception in get_sample_failed_records: " + std::string(e.what()), "DataQualityHandlers", "get_sample_failed_records");
        return "[]";
    }
}

std::string DataQualityHandlers::calculate_quality_trends(const std::string& rule_id, int days) {
    try {
        if (!db_conn_->is_connected()) {
            logger_->error("Database connection failed in calculate_quality_trends", "DataQualityHandlers", "calculate_quality_trends");
            return "[]";
        }
        
        std::string query = "SELECT DATE(check_timestamp) as check_date, "
                           "AVG(quality_score) as avg_score, "
                           "COUNT(*) as checks_performed, "
                           "COUNT(CASE WHEN status = 'passed' THEN 1 END) as passed_checks, "
                           "COUNT(CASE WHEN status = 'warning' THEN 1 END) as warning_checks, "
                           "COUNT(CASE WHEN status = 'failed' THEN 1 END) as failed_checks "
                           "FROM data_quality_checks "
                           "WHERE rule_id = $1 AND check_timestamp > NOW() - INTERVAL '" +
                           std::to_string(days) + " days' "
                           "GROUP BY DATE(check_timestamp) "
                           "ORDER BY check_date DESC";
        
        std::vector<std::string> params = {rule_id};
        auto result = db_conn_->execute_query(query, params);
        
        json trends = json::array();
        
        for (const auto& row : result.rows) {
            json trend;
            trend["date"] = row.at("check_date");
            trend["avg_score"] = std::stod(row.at("avg_score"));
            trend["checks_performed"] = std::stoi(row.at("checks_performed"));
            trend["passed_checks"] = std::stoi(row.at("passed_checks"));
            trend["warning_checks"] = std::stoi(row.at("warning_checks"));
            trend["failed_checks"] = std::stoi(row.at("failed_checks"));
            trends.push_back(trend);
        }
        
        return trends.dump();
    } catch (const std::exception& e) {
        logger_->error("Exception in calculate_quality_trends: " + std::string(e.what()), "DataQualityHandlers", "calculate_quality_trends");
        return "[]";
    }
}

std::string DataQualityHandlers::get_quality_summary_for_dashboard() {
    try {
        if (!db_conn_->is_connected()) {
            logger_->error("Database connection failed in get_quality_summary_for_dashboard", "DataQualityHandlers", "get_quality_summary_for_dashboard");
            return "{}";
        }
        
        const char* query = "SELECT "
                           "(SELECT COUNT(*) FROM data_quality_rules) as total_rules, "
                           "(SELECT COUNT(*) FROM data_quality_rules WHERE is_enabled = true) as enabled_rules, "
                           "(SELECT AVG(quality_score) FROM data_quality_checks "
                           "WHERE check_timestamp > NOW() - INTERVAL '24 hours') as avg_quality_score, "
                           "(SELECT COUNT(*) FROM data_quality_checks "
                           "WHERE check_timestamp > NOW() - INTERVAL '24 hours') as checks_today, "
                           "(SELECT COUNT(*) FROM data_quality_checks "
                           "WHERE check_timestamp > NOW() - INTERVAL '24 hours' AND status = 'failed') as failed_checks_today";
        
        auto result = db_conn_->execute_query(query, {});
        
        if (result.rows.empty()) {
            return "{}";
        }
        
        json summary;
        summary["total_rules"] = std::stoi(result.rows[0].at("total_rules"));
        summary["enabled_rules"] = std::stoi(result.rows[0].at("enabled_rules"));
        
        // Handle null values
        std::string avg_score_str = result.rows[0].at("avg_quality_score");
        if (avg_score_str.empty() || avg_score_str == "NULL") {
            summary["avg_quality_score"] = 0.0;
        } else {
            summary["avg_quality_score"] = std::stod(avg_score_str);
        }
        
        summary["checks_today"] = std::stoi(result.rows[0].at("checks_today"));
        summary["failed_checks_today"] = std::stoi(result.rows[0].at("failed_checks_today"));
        
        return summary.dump();
    } catch (const std::exception& e) {
        logger_->error("Exception in get_quality_summary_for_dashboard: " + std::string(e.what()), "DataQualityHandlers", "get_quality_summary_for_dashboard");
        return "{}";
    }
}

std::string DataQualityHandlers::base64_decode(const std::string& encoded_string) {
    static const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    std::string decoded_string;
    int val = 0, valb = -8;
    
    for (unsigned char c : encoded_string) {
        if (c == '=') {
            break;
        }
        
        if (chars.find(c) == std::string::npos) {
            continue; // Skip non-base64 characters
        }
        
        val = (val << 6) + chars.find(c);
        valb += 6;
        
        if (valb >= 0) {
            decoded_string.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    
    return decoded_string;
}