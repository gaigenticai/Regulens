#include "alert_management_handlers.hpp"
#include <nlohmann/json.hpp>
#include <random>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <algorithm>
#include <curl/curl.h>
#include <regex>

using json = nlohmann::json;

namespace regulens {
namespace alerts {

// Callback function for cURL HTTP requests
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

AlertManagementHandlers::AlertManagementHandlers(
    std::shared_ptr<PostgreSQLConnection> db_conn,
    std::shared_ptr<StructuredLogger> logger
) : db_conn_(db_conn), logger_(logger) {}

std::string AlertManagementHandlers::handle_get_alert_rules(const std::map<std::string, std::string>& query_params) {
    try {
        std::string rule_type = query_params.count("type") ? query_params.at("type") : "";
        std::string severity = query_params.count("severity") ? query_params.at("severity") : "";
        std::string is_enabled = query_params.count("enabled") ? query_params.at("enabled") : "";
        int limit = query_params.count("limit") ? std::stoi(query_params.at("limit")) : 50;
        int offset = query_params.count("offset") ? std::stoi(query_params.at("offset")) : 0;

        std::stringstream query;
        query << "SELECT rule_id, rule_name, description, rule_type, severity, condition, "
              << "notification_channels, notification_config, cooldown_minutes, is_enabled, "
              << "created_by, created_at, updated_at, last_triggered_at "
              << "FROM alert_rules WHERE 1=1";

        std::vector<std::string> params;
        int param_count = 0;

        if (!rule_type.empty()) {
            query << " AND rule_type = $" << (++param_count);
            params.push_back(rule_type);
        }

        if (!severity.empty()) {
            query << " AND severity = $" << (++param_count);
            params.push_back(severity);
        }

        if (!is_enabled.empty()) {
            query << " AND is_enabled = $" << (++param_count);
            params.push_back(is_enabled);
        }

        query << " ORDER BY created_at DESC LIMIT $" << (++param_count) << " OFFSET $" << (++param_count);
        params.push_back(std::to_string(limit));
        params.push_back(std::to_string(offset));

        auto result = db_conn_->execute_query(query.str(), params);
        
        json rules = json::array();
        for (const auto& row : result.rows) {
            json rule = {
                {"rule_id", row.at("rule_id")},
                {"rule_name", row.at("rule_name")},
                {"description", row.at("description")},
                {"rule_type", row.at("rule_type")},
                {"severity", row.at("severity")},
                {"condition", json::parse(row.at("condition"))},
                {"notification_channels", json::parse(row.at("notification_channels"))},
                {"cooldown_minutes", std::stoi(row.at("cooldown_minutes"))},
                {"is_enabled", row.at("is_enabled") == "t"},
                {"created_at", row.at("created_at")},
                {"updated_at", row.at("updated_at")}
            };
            
            if (row.count("notification_config") && !row.at("notification_config").empty()) {
                rule["notification_config"] = json::parse(row.at("notification_config"));
            }
            if (row.count("created_by") && !row.at("created_by").empty()) {
                rule["created_by"] = row.at("created_by");
            }
            if (row.count("last_triggered_at") && !row.at("last_triggered_at").empty()) {
                rule["last_triggered_at"] = row.at("last_triggered_at");
            }
            
            rules.push_back(rule);
        }

        // Get total count for pagination
        std::stringstream count_query;
        count_query << "SELECT COUNT(*) FROM alert_rules WHERE 1=1";
        
        std::vector<std::string> count_params;
        int count_param_count = 0;
        
        if (!rule_type.empty()) {
            count_query << " AND rule_type = $" << (++count_param_count);
            count_params.push_back(rule_type);
        }
        
        if (!severity.empty()) {
            count_query << " AND severity = $" << (++count_param_count);
            count_params.push_back(severity);
        }
        
        if (!is_enabled.empty()) {
            count_query << " AND is_enabled = $" << (++count_param_count);
            count_params.push_back(is_enabled);
        }

        auto count_result = db_conn_->execute_query(count_query.str(), count_params);
        int total_count = 0;
        if (!count_result.rows.empty()) {
            total_count = std::stoi(count_result.rows[0].at("count"));
        }

        json response = {
            {"rules", rules},
            {"pagination", {
                {"total", total_count},
                {"limit", limit},
                {"offset", offset},
                {"hasMore", (offset + rules.size()) < total_count}
            }}
        };

        return response.dump();

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_get_alert_rules: " + std::string(e.what()));
        return R"({"error": "Internal server error"})";
    }
}

std::string AlertManagementHandlers::handle_get_alert_rule_by_id(const std::string& rule_id) {
    try {
        auto conn = db_conn_->get_connection();
        if (!conn) {
            return R"({"error": "Database connection failed"})";
        }

        const char* params[1] = {rule_id.c_str()};
        PGresult* result = PQexecParams(
            conn,
            "SELECT rule_id, rule_name, description, rule_type, severity, condition, "
            "notification_channels, notification_config, cooldown_minutes, is_enabled, "
            "created_by, created_at, updated_at, last_triggered_at "
            "FROM alert_rules WHERE rule_id = $1",
            1, nullptr, params, nullptr, nullptr, 0
        );

        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            std::string error = PQerrorMessage(conn);
            PQclear(result);
            logger_->log(LogLevel::ERROR, "Failed to fetch alert rule: " + error);
            return R"({"error": "Failed to fetch alert rule"})";
        }

        if (PQntuples(result) == 0) {
            PQclear(result);
            return R"({"error": "Alert rule not found"})";
        }

        json rule = serialize_alert_rule(result, 0);
        PQclear(result);

        return rule.dump();

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_get_alert_rule_by_id: " + std::string(e.what()));
        return R"({"error": "Internal server error"})";
    }
}

std::string AlertManagementHandlers::handle_create_alert_rule(const std::string& request_body, const std::string& user_id) {
    try {
        json request = json::parse(request_body);

        // Validate required fields
        if (!request.contains("rule_name") || !request.contains("rule_type") || 
            !request.contains("severity") || !request.contains("condition")) {
            return R"({"error": "Missing required fields: rule_name, rule_type, severity, condition"})";
        }

        // Validate alert condition
        if (!validate_alert_condition(request["condition"])) {
            return R"({"error": "Invalid alert condition format"})";
        }

        auto conn = db_conn_->get_connection();
        if (!conn) {
            return R"({"error": "Database connection failed"})";
        }

        std::string rule_name = request["rule_name"];
        std::string description = request.value("description", "");
        std::string rule_type = request["rule_type"];
        std::string severity = request["severity"];
        json condition = request["condition"];
        json notification_channels = request.value("notification_channels", json::array());
        json notification_config = request.value("notification_config", json::object());
        int cooldown_minutes = request.value("cooldown_minutes", 5);
        std::string cooldown_str = std::to_string(cooldown_minutes);

        const char* params[9] = {
            rule_name.c_str(),
            description.c_str(),
            rule_type.c_str(),
            severity.c_str(),
            condition.dump().c_str(),
            notification_channels.dump().c_str(),
            notification_config.dump().c_str(),
            cooldown_str.c_str(),
            user_id.c_str()
        };

        PGresult* result = PQexecParams(
            conn,
            "INSERT INTO alert_rules (rule_name, description, rule_type, severity, "
            "condition, notification_channels, notification_config, cooldown_minutes, created_by) "
            "VALUES ($1, $2, $3, $4, $5::jsonb, $6::jsonb, $7::jsonb, $8, $9) "
            "RETURNING rule_id, created_at",
            9, nullptr, params, nullptr, nullptr, 0
        );

        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            std::string error = PQerrorMessage(conn);
            PQclear(result);
            logger_->log(LogLevel::ERROR, "Failed to create alert rule: " + error);
            return R"({"error": "Failed to create alert rule"})";
        }

        json response = {
            {"rule_id", PQgetvalue(result, 0, 0)},
            {"rule_name", rule_name},
            {"description", description},
            {"rule_type", rule_type},
            {"severity", severity},
            {"condition", condition},
            {"notification_channels", notification_channels},
            {"notification_config", notification_config},
            {"cooldown_minutes", cooldown_minutes},
            {"is_enabled", true},
            {"created_at", PQgetvalue(result, 0, 1)},
            {"created_by", user_id}
        };

        PQclear(result);
        return response.dump();

    } catch (const json::exception& e) {
        logger_->log(LogLevel::ERROR, "JSON parsing error in handle_create_alert_rule: " + std::string(e.what()));
        return R"({"error": "Invalid JSON format"})";
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_create_alert_rule: " + std::string(e.what()));
        return R"({"error": "Internal server error"})";
    }
}

std::string AlertManagementHandlers::handle_update_alert_rule(const std::string& rule_id, const std::string& request_body) {
    try {
        json request = json::parse(request_body);
        
        auto conn = db_conn_->get_connection();
        if (!conn) {
            return R"({"error": "Database connection failed"})";
        }

        std::vector<std::string> updates;
        std::vector<std::string> params;
        int param_count = 0;

        if (request.contains("rule_name")) {
            updates.push_back("rule_name = $" + std::to_string(++param_count));
            params.push_back(request["rule_name"]);
        }
        if (request.contains("description")) {
            updates.push_back("description = $" + std::to_string(++param_count));
            params.push_back(request["description"]);
        }
        if (request.contains("rule_type")) {
            updates.push_back("rule_type = $" + std::to_string(++param_count));
            params.push_back(request["rule_type"]);
        }
        if (request.contains("severity")) {
            updates.push_back("severity = $" + std::to_string(++param_count));
            params.push_back(request["severity"]);
        }
        if (request.contains("condition")) {
            if (!validate_alert_condition(request["condition"])) {
                return R"({"error": "Invalid alert condition format"})";
            }
            updates.push_back("condition = $" + std::to_string(++param_count) + "::jsonb");
            params.push_back(request["condition"].dump());
        }
        if (request.contains("notification_channels")) {
            updates.push_back("notification_channels = $" + std::to_string(++param_count) + "::jsonb");
            params.push_back(request["notification_channels"].dump());
        }
        if (request.contains("notification_config")) {
            updates.push_back("notification_config = $" + std::to_string(++param_count) + "::jsonb");
            params.push_back(request["notification_config"].dump());
        }
        if (request.contains("cooldown_minutes")) {
            updates.push_back("cooldown_minutes = $" + std::to_string(++param_count));
            params.push_back(std::to_string(request["cooldown_minutes"].get<int>()));
        }
        if (request.contains("is_enabled")) {
            updates.push_back("is_enabled = $" + std::to_string(++param_count));
            params.push_back(request["is_enabled"] ? "true" : "false");
        }

        if (updates.empty()) {
            return R"({"error": "No fields to update"})";
        }

        updates.push_back("updated_at = CURRENT_TIMESTAMP");

        std::string query = "UPDATE alert_rules SET " + updates[0];
        for (size_t i = 1; i < updates.size(); i++) {
            query += ", " + updates[i];
        }
        query += " WHERE rule_id = $" + std::to_string(++param_count);
        query += " RETURNING rule_id, rule_name, updated_at";

        params.push_back(rule_id);

        std::vector<const char*> param_values;
        for (const auto& p : params) {
            param_values.push_back(p.c_str());
        }

        PGresult* result = PQexecParams(
            conn,
            query.c_str(),
            param_values.size(),
            nullptr,
            param_values.data(),
            nullptr,
            nullptr,
            0
        );

        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            std::string error = PQerrorMessage(conn);
            PQclear(result);
            logger_->log(LogLevel::ERROR, "Failed to update alert rule: " + error);
            return R"({"error": "Failed to update alert rule"})";
        }

        if (PQntuples(result) == 0) {
            PQclear(result);
            return R"({"error": "Alert rule not found"})";
        }

        json response = {
            {"rule_id", PQgetvalue(result, 0, 0)},
            {"rule_name", PQgetvalue(result, 0, 1)},
            {"updated_at", PQgetvalue(result, 0, 2)},
            {"message", "Alert rule updated successfully"}
        };

        PQclear(result);
        return response.dump();

    } catch (const json::exception& e) {
        logger_->log(LogLevel::ERROR, "JSON parsing error in handle_update_alert_rule: " + std::string(e.what()));
        return R"({"error": "Invalid JSON format"})";
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_update_alert_rule: " + std::string(e.what()));
        return R"({"error": "Internal server error"})";
    }
}

std::string AlertManagementHandlers::handle_delete_alert_rule(const std::string& rule_id) {
    try {
        auto conn = db_conn_->get_connection();
        if (!conn) {
            return R"({"error": "Database connection failed"})";
        }

        // Check if there are active incidents for this rule
        const char* check_params[1] = {rule_id.c_str()};
        PGresult* check_result = PQexecParams(
            conn,
            "SELECT COUNT(*) FROM alert_incidents WHERE rule_id = $1 AND status IN ('active', 'acknowledged')",
            1, nullptr, check_params, nullptr, nullptr, 0
        );

        int active_incidents = 0;
        if (PQresultStatus(check_result) == PGRES_TUPLES_OK && PQntuples(check_result) > 0) {
            active_incidents = std::stoi(PQgetvalue(check_result, 0, 0));
        }
        PQclear(check_result);

        if (active_incidents > 0) {
            return R"({"error": "Cannot delete rule with active incidents. Resolve or acknowledge incidents first."})";
        }

        // Delete the rule
        PGresult* result = PQexecParams(
            conn,
            "DELETE FROM alert_rules WHERE rule_id = $1 RETURNING rule_name",
            1, nullptr, check_params, nullptr, nullptr, 0
        );

        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            std::string error = PQerrorMessage(conn);
            PQclear(result);
            logger_->log(LogLevel::ERROR, "Failed to delete alert rule: " + error);
            return R"({"error": "Failed to delete alert rule"})";
        }

        if (PQntuples(result) == 0) {
            PQclear(result);
            return R"({"error": "Alert rule not found"})";
        }

        json response = {
            {"rule_id", rule_id},
            {"rule_name", PQgetvalue(result, 0, 0)},
            {"message", "Alert rule deleted successfully"}
        };

        PQclear(result);
        return response.dump();

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_delete_alert_rule: " + std::string(e.what()));
        return R"({"error": "Internal server error"})";
    }
}

std::string AlertManagementHandlers::handle_get_alert_history(const std::map<std::string, std::string>& query_params) {
    try {
        auto conn = db_conn_->get_connection();
        if (!conn) {
            return R"({"error": "Database connection failed"})";
        }

        std::string status = query_params.count("status") ? query_params.at("status") : "";
        std::string severity = query_params.count("severity") ? query_params.at("severity") : "";
        std::string rule_id = query_params.count("rule_id") ? query_params.at("rule_id") : "";
        int limit = query_params.count("limit") ? std::stoi(query_params.at("limit")) : 50;
        int offset = query_params.count("offset") ? std::stoi(query_params.at("offset")) : 0;

        std::stringstream query;
        query << "SELECT i.incident_id, i.rule_id, i.severity, i.title, i.message, i.incident_data, "
              << "i.triggered_at, i.acknowledged_at, i.acknowledged_by, i.resolved_at, i.resolved_by, "
              << "i.resolution_notes, i.status, i.notification_status, r.rule_name "
              << "FROM alert_incidents i "
              << "LEFT JOIN alert_rules r ON i.rule_id = r.rule_id "
              << "WHERE 1=1";

        std::vector<std::string> params;
        int param_count = 0;

        if (!status.empty()) {
            query << " AND i.status = $" << (++param_count);
            params.push_back(status);
        }

        if (!severity.empty()) {
            query << " AND i.severity = $" << (++param_count);
            params.push_back(severity);
        }

        if (!rule_id.empty()) {
            query << " AND i.rule_id = $" << (++param_count);
            params.push_back(rule_id);
        }

        query << " ORDER BY i.triggered_at DESC LIMIT $" << (++param_count) << " OFFSET $" << (++param_count);
        params.push_back(std::to_string(limit));
        params.push_back(std::to_string(offset));

        std::vector<const char*> param_values;
        for (const auto& p : params) {
            param_values.push_back(p.c_str());
        }

        PGresult* result = PQexecParams(
            conn,
            query.str().c_str(),
            param_count,
            nullptr,
            param_values.data(),
            nullptr,
            nullptr,
            0
        );

        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            std::string error = PQerrorMessage(conn);
            PQclear(result);
            logger_->log(LogLevel::ERROR, "Failed to fetch alert history: " + error);
            return R"({"error": "Failed to fetch alert history"})";
        }

        json incidents = json::array();
        int num_rows = PQntuples(result);

        for (int i = 0; i < num_rows; i++) {
            json incident = serialize_alert_incident(result, i);
            incident["rule_name"] = !PQgetisnull(result, i, 14) ? PQgetvalue(result, i, 14) : "";
            incidents.push_back(incident);
        }

        PQclear(result);

        // Get total count for pagination
        std::stringstream count_query;
        count_query << "SELECT COUNT(*) FROM alert_incidents WHERE 1=1";
        
        std::vector<std::string> count_params;
        int count_param_count = 0;
        
        if (!status.empty()) {
            count_query << " AND status = $" << (++count_param_count);
            count_params.push_back(status);
        }
        
        if (!severity.empty()) {
            count_query << " AND severity = $" << (++count_param_count);
            count_params.push_back(severity);
        }
        
        if (!rule_id.empty()) {
            count_query << " AND rule_id = $" << (++count_param_count);
            count_params.push_back(rule_id);
        }

        std::vector<const char*> count_param_values;
        for (const auto& p : count_params) {
            count_param_values.push_back(p.c_str());
        }

        PGresult* count_result = PQexecParams(
            conn,
            count_query.str().c_str(),
            count_param_count,
            nullptr,
            count_param_values.data(),
            nullptr,
            nullptr,
            0
        );

        int total_count = 0;
        if (PQresultStatus(count_result) == PGRES_TUPLES_OK && PQntuples(count_result) > 0) {
            total_count = std::stoi(PQgetvalue(count_result, 0, 0));
        }
        PQclear(count_result);

        json response = {
            {"incidents", incidents},
            {"pagination", {
                {"total", total_count},
                {"limit", limit},
                {"offset", offset},
                {"hasMore", (offset + num_rows) < total_count}
            }}
        };

        return response.dump();

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_get_alert_history: " + std::string(e.what()));
        return R"({"error": "Internal server error"})";
    }
}

std::string AlertManagementHandlers::handle_acknowledge_alert(const std::string& incident_id, const std::string& request_body, const std::string& user_id) {
    try {
        json request = json::parse(request_body);
        std::string notes = request.value("notes", "");
        
        auto conn = db_conn_->get_connection();
        if (!conn) {
            return R"({"error": "Database connection failed"})";
        }

        const char* params[3] = {
            user_id.c_str(),
            notes.c_str(),
            incident_id.c_str()
        };

        PGresult* result = PQexecParams(
            conn,
            "UPDATE alert_incidents SET status = 'acknowledged', acknowledged_at = CURRENT_TIMESTAMP, "
            "acknowledged_by = $1, resolution_notes = $2 WHERE incident_id = $3 AND status = 'active' "
            "RETURNING incident_id, title, acknowledged_at",
            3, nullptr, params, nullptr, nullptr, 0
        );

        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            std::string error = PQerrorMessage(conn);
            PQclear(result);
            logger_->log(LogLevel::ERROR, "Failed to acknowledge alert: " + error);
            return R"({"error": "Failed to acknowledge alert"})";
        }

        if (PQntuples(result) == 0) {
            PQclear(result);
            return R"({"error": "Alert incident not found or already acknowledged/resolved"})";
        }

        json response = {
            {"incident_id", PQgetvalue(result, 0, 0)},
            {"title", PQgetvalue(result, 0, 1)},
            {"acknowledged_at", PQgetvalue(result, 0, 2)},
            {"acknowledged_by", user_id},
            {"status", "acknowledged"},
            {"message", "Alert acknowledged successfully"}
        };

        PQclear(result);
        return response.dump();

    } catch (const json::exception& e) {
        logger_->log(LogLevel::ERROR, "JSON parsing error in handle_acknowledge_alert: " + std::string(e.what()));
        return R"({"error": "Invalid JSON format"})";
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_acknowledge_alert: " + std::string(e.what()));
        return R"({"error": "Internal server error"})";
    }
}

std::string AlertManagementHandlers::handle_resolve_alert(const std::string& incident_id, const std::string& request_body, const std::string& user_id) {
    try {
        json request = json::parse(request_body);
        std::string resolution_notes = request.value("resolution_notes", "");
        
        auto conn = db_conn_->get_connection();
        if (!conn) {
            return R"({"error": "Database connection failed"})";
        }

        const char* params[3] = {
            user_id.c_str(),
            resolution_notes.c_str(),
            incident_id.c_str()
        };

        PGresult* result = PQexecParams(
            conn,
            "UPDATE alert_incidents SET status = 'resolved', resolved_at = CURRENT_TIMESTAMP, "
            "resolved_by = $1, resolution_notes = $2 WHERE incident_id = $3 AND status IN ('active', 'acknowledged') "
            "RETURNING incident_id, title, resolved_at",
            3, nullptr, params, nullptr, nullptr, 0
        );

        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            std::string error = PQerrorMessage(conn);
            PQclear(result);
            logger_->log(LogLevel::ERROR, "Failed to resolve alert: " + error);
            return R"({"error": "Failed to resolve alert"})";
        }

        if (PQntuples(result) == 0) {
            PQclear(result);
            return R"({"error": "Alert incident not found or already resolved"})";
        }

        json response = {
            {"incident_id", PQgetvalue(result, 0, 0)},
            {"title", PQgetvalue(result, 0, 1)},
            {"resolved_at", PQgetvalue(result, 0, 2)},
            {"resolved_by", user_id},
            {"status", "resolved"},
            {"message", "Alert resolved successfully"}
        };

        PQclear(result);
        return response.dump();

    } catch (const json::exception& e) {
        logger_->log(LogLevel::ERROR, "JSON parsing error in handle_resolve_alert: " + std::string(e.what()));
        return R"({"error": "Invalid JSON format"})";
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_resolve_alert: " + std::string(e.what()));
        return R"({"error": "Internal server error"})";
    }
}

std::string AlertManagementHandlers::handle_get_notification_channels(const std::map<std::string, std::string>& query_params) {
    try {
        auto conn = db_conn_->get_connection();
        if (!conn) {
            return R"({"error": "Database connection failed"})";
        }

        std::string channel_type = query_params.count("type") ? query_params.at("type") : "";
        std::string is_enabled = query_params.count("enabled") ? query_params.at("enabled") : "";
        int limit = query_params.count("limit") ? std::stoi(query_params.at("limit")) : 50;
        int offset = query_params.count("offset") ? std::stoi(query_params.at("offset")) : 0;

        std::stringstream query;
        query << "SELECT channel_id, channel_type, channel_name, configuration, is_enabled, "
              << "last_tested_at, test_status, created_at "
              << "FROM notification_channels WHERE 1=1";

        std::vector<std::string> params;
        int param_count = 0;

        if (!channel_type.empty()) {
            query << " AND channel_type = $" << (++param_count);
            params.push_back(channel_type);
        }

        if (!is_enabled.empty()) {
            query << " AND is_enabled = $" << (++param_count);
            params.push_back(is_enabled);
        }

        query << " ORDER BY created_at DESC LIMIT $" << (++param_count) << " OFFSET $" << (++param_count);
        params.push_back(std::to_string(limit));
        params.push_back(std::to_string(offset));

        std::vector<const char*> param_values;
        for (const auto& p : params) {
            param_values.push_back(p.c_str());
        }

        PGresult* result = PQexecParams(
            conn,
            query.str().c_str(),
            param_count,
            nullptr,
            param_values.data(),
            nullptr,
            nullptr,
            0
        );

        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            std::string error = PQerrorMessage(conn);
            PQclear(result);
            logger_->log(LogLevel::ERROR, "Failed to fetch notification channels: " + error);
            return R"({"error": "Failed to fetch notification channels"})";
        }

        json channels = json::array();
        int num_rows = PQntuples(result);

        for (int i = 0; i < num_rows; i++) {
            channels.push_back(serialize_notification_channel(result, i));
        }

        PQclear(result);

        // Get total count for pagination
        std::stringstream count_query;
        count_query << "SELECT COUNT(*) FROM notification_channels WHERE 1=1";
        
        std::vector<std::string> count_params;
        int count_param_count = 0;
        
        if (!channel_type.empty()) {
            count_query << " AND channel_type = $" << (++count_param_count);
            count_params.push_back(channel_type);
        }
        
        if (!is_enabled.empty()) {
            count_query << " AND is_enabled = $" << (++count_param_count);
            count_params.push_back(is_enabled);
        }

        std::vector<const char*> count_param_values;
        for (const auto& p : count_params) {
            count_param_values.push_back(p.c_str());
        }

        PGresult* count_result = PQexecParams(
            conn,
            count_query.str().c_str(),
            count_param_count,
            nullptr,
            count_param_values.data(),
            nullptr,
            nullptr,
            0
        );

        int total_count = 0;
        if (PQresultStatus(count_result) == PGRES_TUPLES_OK && PQntuples(count_result) > 0) {
            total_count = std::stoi(PQgetvalue(count_result, 0, 0));
        }
        PQclear(count_result);

        json response = {
            {"channels", channels},
            {"pagination", {
                {"total", total_count},
                {"limit", limit},
                {"offset", offset},
                {"hasMore", (offset + num_rows) < total_count}
            }}
        };

        return response.dump();

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_get_notification_channels: " + std::string(e.what()));
        return R"({"error": "Internal server error"})";
    }
}

std::string AlertManagementHandlers::handle_create_notification_channel(const std::string& request_body, const std::string& user_id) {
    try {
        json request = json::parse(request_body);

        // Validate required fields
        if (!request.contains("channel_name") || !request.contains("channel_type") || 
            !request.contains("configuration")) {
            return R"({"error": "Missing required fields: channel_name, channel_type, configuration"})";
        }

        // Validate notification config based on channel type
        if (!validate_notification_config(request["configuration"], request["channel_type"])) {
            return R"({"error": "Invalid configuration for channel type"})";
        }

        auto conn = db_conn_->get_connection();
        if (!conn) {
            return R"({"error": "Database connection failed"})";
        }

        std::string channel_name = request["channel_name"];
        std::string channel_type = request["channel_type"];
        json configuration = request["configuration"];

        const char* params[3] = {
            channel_type.c_str(),
            channel_name.c_str(),
            configuration.dump().c_str()
        };

        PGresult* result = PQexecParams(
            conn,
            "INSERT INTO notification_channels (channel_type, channel_name, configuration) "
            "VALUES ($1, $2, $3::jsonb) RETURNING channel_id, created_at",
            3, nullptr, params, nullptr, nullptr, 0
        );

        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            std::string error = PQerrorMessage(conn);
            PQclear(result);
            logger_->log(LogLevel::ERROR, "Failed to create notification channel: " + error);
            return R"({"error": "Failed to create notification channel"})";
        }

        json response = {
            {"channel_id", PQgetvalue(result, 0, 0)},
            {"channel_name", channel_name},
            {"channel_type", channel_type},
            {"configuration", configuration},
            {"is_enabled", true},
            {"created_at", PQgetvalue(result, 0, 1)},
            {"message", "Notification channel created successfully"}
        };

        PQclear(result);
        return response.dump();

    } catch (const json::exception& e) {
        logger_->log(LogLevel::ERROR, "JSON parsing error in handle_create_notification_channel: " + std::string(e.what()));
        return R"({"error": "Invalid JSON format"})";
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_create_notification_channel: " + std::string(e.what()));
        return R"({"error": "Internal server error"})";
    }
}

std::string AlertManagementHandlers::handle_update_notification_channel(const std::string& channel_id, const std::string& request_body) {
    try {
        json request = json::parse(request_body);
        
        auto conn = db_conn_->get_connection();
        if (!conn) {
            return R"({"error": "Database connection failed"})";
        }

        std::vector<std::string> updates;
        std::vector<std::string> params;
        int param_count = 0;

        if (request.contains("channel_name")) {
            updates.push_back("channel_name = $" + std::to_string(++param_count));
            params.push_back(request["channel_name"]);
        }
        if (request.contains("configuration")) {
            // Validate notification config if channel_type is known
            if (request.contains("channel_type")) {
                if (!validate_notification_config(request["configuration"], request["channel_type"])) {
                    return R"({"error": "Invalid configuration for channel type"})";
                }
            }
            updates.push_back("configuration = $" + std::to_string(++param_count) + "::jsonb");
            params.push_back(request["configuration"].dump());
        }
        if (request.contains("is_enabled")) {
            updates.push_back("is_enabled = $" + std::to_string(++param_count));
            params.push_back(request["is_enabled"] ? "true" : "false");
        }

        if (updates.empty()) {
            return R"({"error": "No fields to update"})";
        }

        std::string query = "UPDATE notification_channels SET " + updates[0];
        for (size_t i = 1; i < updates.size(); i++) {
            query += ", " + updates[i];
        }
        query += " WHERE channel_id = $" + std::to_string(++param_count);
        query += " RETURNING channel_id, channel_name, updated_at";

        params.push_back(channel_id);

        std::vector<const char*> param_values;
        for (const auto& p : params) {
            param_values.push_back(p.c_str());
        }

        PGresult* result = PQexecParams(
            conn,
            query.c_str(),
            param_values.size(),
            nullptr,
            param_values.data(),
            nullptr,
            nullptr,
            0
        );

        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            std::string error = PQerrorMessage(conn);
            PQclear(result);
            logger_->log(LogLevel::ERROR, "Failed to update notification channel: " + error);
            return R"({"error": "Failed to update notification channel"})";
        }

        if (PQntuples(result) == 0) {
            PQclear(result);
            return R"({"error": "Notification channel not found"})";
        }

        json response = {
            {"channel_id", PQgetvalue(result, 0, 0)},
            {"channel_name", PQgetvalue(result, 0, 1)},
            {"message", "Notification channel updated successfully"}
        };

        PQclear(result);
        return response.dump();

    } catch (const json::exception& e) {
        logger_->log(LogLevel::ERROR, "JSON parsing error in handle_update_notification_channel: " + std::string(e.what()));
        return R"({"error": "Invalid JSON format"})";
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_update_notification_channel: " + std::string(e.what()));
        return R"({"error": "Internal server error"})";
    }
}

std::string AlertManagementHandlers::handle_delete_notification_channel(const std::string& channel_id) {
    try {
        auto conn = db_conn_->get_connection();
        if (!conn) {
            return R"({"error": "Database connection failed"})";
        }

        // Check if channel is being used by active alert rules
        const char* check_params[1] = {channel_id.c_str()};
        PGresult* check_result = PQexecParams(
            conn,
            "SELECT COUNT(*) FROM alert_rules WHERE notification_channels::jsonb ? $1",
            1, nullptr, check_params, nullptr, nullptr, 0
        );

        int rules_using_channel = 0;
        if (PQresultStatus(check_result) == PGRES_TUPLES_OK && PQntuples(check_result) > 0) {
            rules_using_channel = std::stoi(PQgetvalue(check_result, 0, 0));
        }
        PQclear(check_result);

        if (rules_using_channel > 0) {
            return R"({"error": "Cannot delete channel that is being used by alert rules. Update rules first."})";
        }

        // Delete the channel
        PGresult* result = PQexecParams(
            conn,
            "DELETE FROM notification_channels WHERE channel_id = $1 RETURNING channel_name",
            1, nullptr, check_params, nullptr, nullptr, 0
        );

        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            std::string error = PQerrorMessage(conn);
            PQclear(result);
            logger_->log(LogLevel::ERROR, "Failed to delete notification channel: " + error);
            return R"({"error": "Failed to delete notification channel"})";
        }

        if (PQntuples(result) == 0) {
            PQclear(result);
            return R"({"error": "Notification channel not found"})";
        }

        json response = {
            {"channel_id", channel_id},
            {"channel_name", PQgetvalue(result, 0, 0)},
            {"message", "Notification channel deleted successfully"}
        };

        PQclear(result);
        return response.dump();

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_delete_notification_channel: " + std::string(e.what()));
        return R"({"error": "Internal server error"})";
    }
}

std::string AlertManagementHandlers::handle_test_notification_channel(const std::string& channel_id) {
    try {
        auto conn = db_conn_->get_connection();
        if (!conn) {
            return R"({"error": "Database connection failed"})";
        }

        // Get channel details
        const char* params[1] = {channel_id.c_str()};
        PGresult* result = PQexecParams(
            conn,
            "SELECT channel_id, channel_type, channel_name, configuration "
            "FROM notification_channels WHERE channel_id = $1",
            1, nullptr, params, nullptr, nullptr, 0
        );

        if (PQresultStatus(result) != PGRES_TUPLES_OK || PQntuples(result) == 0) {
            PQclear(result);
            return R"({"error": "Notification channel not found"})";
        }

        json channel = serialize_notification_channel(result, 0);
        PQclear(result);

        // Create test alert data
        json test_alert = {
            {"title", "Test Alert from Regulens"},
            {"message", "This is a test notification to verify your channel configuration."},
            {"severity", "low"},
            {"timestamp", std::to_string(std::time(nullptr))},
            {"incident_id", "test-" + std::to_string(std::time(nullptr))}
        };

        bool test_success = false;
        std::string error_message;

        // Test based on channel type
        std::string channel_type = channel["channel_type"];
        if (channel_type == "email") {
            test_success = send_email_notification(channel["configuration"], test_alert);
        } else if (channel_type == "webhook") {
            test_success = send_webhook_notification(channel["configuration"], test_alert);
        } else if (channel_type == "slack") {
            test_success = send_slack_notification(channel["configuration"], test_alert);
        } else {
            error_message = "Unsupported channel type for testing: " + channel_type;
        }

        // Update channel test status
        const char* update_params[3] = {
            test_success ? "success" : "failed",
            test_success ? "" : error_message.c_str(),
            channel_id.c_str()
        };

        PGresult* update_result = PQexecParams(
            conn,
            "UPDATE notification_channels SET last_tested_at = CURRENT_TIMESTAMP, "
            "test_status = $1 WHERE channel_id = $3",
            3, nullptr, update_params, nullptr, nullptr, 0
        );
        PQclear(update_result);

        json response = {
            {"channel_id", channel_id},
            {"channel_name", channel["channel_name"]},
            {"channel_type", channel_type},
            {"test_success", test_success},
            {"tested_at", std::to_string(std::time(nullptr))},
            {"message", test_success ? "Test notification sent successfully" : "Test notification failed: " + error_message}
        };

        return response.dump();

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_test_notification_channel: " + std::string(e.what()));
        return R"({"error": "Internal server error"})";
    }
}

std::string AlertManagementHandlers::handle_test_alert_delivery(const std::string& request_body, const std::string& user_id) {
    try {
        json request = json::parse(request_body);

        // Validate required fields
        if (!request.contains("rule_id") || !request.contains("test_data")) {
            return R"({"error": "Missing required fields: rule_id, test_data"})";
        }

        std::string rule_id = request["rule_id"];
        json test_data = request["test_data"];

        auto conn = db_conn_->get_connection();
        if (!conn) {
            return R"({"error": "Database connection failed"})";
        }

        // Get rule details
        const char* params[1] = {rule_id.c_str()};
        PGresult* result = PQexecParams(
            conn,
            "SELECT rule_id, rule_name, rule_type, severity, condition, "
            "notification_channels, notification_config FROM alert_rules WHERE rule_id = $1",
            1, nullptr, params, nullptr, nullptr, 0
        );

        if (PQresultStatus(result) != PGRES_TUPLES_OK || PQntuples(result) == 0) {
            PQclear(result);
            return R"({"error": "Alert rule not found"})";
        }

        json rule = serialize_alert_rule(result, 0);
        PQclear(result);

        // Create test incident
        std::string test_title = generate_alert_title(rule, test_data);
        std::string test_message = generate_alert_message(rule, test_data);

        const char* incident_params[6] = {
            rule_id.c_str(),
            rule["severity"].get<std::string>().c_str(),
            test_title.c_str(),
            test_message.c_str(),
            test_data.dump().c_str(),
            "true"  // Mark as test
        };

        PGresult* incident_result = PQexecParams(
            conn,
            "INSERT INTO alert_incidents (rule_id, severity, title, message, incident_data, status) "
            "VALUES ($1, $2, $3, $4, $5::jsonb, $6) RETURNING incident_id, triggered_at",
            6, nullptr, incident_params, nullptr, nullptr, 0
        );

        if (PQresultStatus(incident_result) != PGRES_TUPLES_OK) {
            std::string error = PQerrorMessage(conn);
            PQclear(incident_result);
            logger_->log(LogLevel::ERROR, "Failed to create test incident: " + error);
            return R"({"error": "Failed to create test incident"})";
        }

        std::string incident_id = PQgetvalue(incident_result, 0, 0);
        PQclear(incident_result);

        // Send test notifications
        try {
            send_alert_notifications(incident_id, rule);
        } catch (const std::exception& e) {
            logger_->log(LogLevel::ERROR, "Failed to send test notifications: " + std::string(e.what()));
        }

        json response = {
            {"incident_id", incident_id},
            {"rule_id", rule_id},
            {"rule_name", rule["rule_name"]},
            {"title", test_title},
            {"message", test_message},
            {"test_data", test_data},
            {"triggered_at", std::to_string(std::time(nullptr))},
            {"message", "Test alert created and notifications sent"}
        };

        return response.dump();

    } catch (const json::exception& e) {
        logger_->log(LogLevel::ERROR, "JSON parsing error in handle_test_alert_delivery: " + std::string(e.what()));
        return R"({"error": "Invalid JSON format"})";
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_test_alert_delivery: " + std::string(e.what()));
        return R"({"error": "Internal server error"})";
    }
}

std::string AlertManagementHandlers::handle_get_alert_metrics(const std::map<std::string, std::string>& query_params) {
    try {
        auto conn = db_conn_->get_connection();
        if (!conn) {
            return R"({"error": "Database connection failed"})";
        }

        std::string time_range = query_params.count("timeRange") ? query_params.at("timeRange") : "24h";

        // Determine time filter based on range
        std::string time_filter;
        if (time_range == "1h") {
            time_filter = "AND triggered_at >= CURRENT_TIMESTAMP - INTERVAL '1 hour'";
        } else if (time_range == "24h") {
            time_filter = "AND triggered_at >= CURRENT_TIMESTAMP - INTERVAL '24 hours'";
        } else if (time_range == "7d") {
            time_filter = "AND triggered_at >= CURRENT_TIMESTAMP - INTERVAL '7 days'";
        } else if (time_range == "30d") {
            time_filter = "AND triggered_at >= CURRENT_TIMESTAMP - INTERVAL '30 days'";
        }

        // Get overall metrics
        PGresult* result = PQexecParams(
            conn,
            ("SELECT "
             "COUNT(*) as total_incidents, "
             "COUNT(*) FILTER (WHERE severity = 'critical') as critical_incidents, "
             "COUNT(*) FILTER (WHERE severity = 'high') as high_incidents, "
             "COUNT(*) FILTER (WHERE severity = 'medium') as medium_incidents, "
             "COUNT(*) FILTER (WHERE severity = 'low') as low_incidents, "
             "COUNT(*) FILTER (WHERE status = 'active') as active_incidents, "
             "COUNT(*) FILTER (WHERE status = 'acknowledged') as acknowledged_incidents, "
             "COUNT(*) FILTER (WHERE status = 'resolved') as resolved_incidents, "
             "AVG(CASE WHEN resolved_at IS NOT NULL THEN EXTRACT(EPOCH FROM (resolved_at - triggered_at))/60 END) as avg_resolution_time_minutes "
             "FROM alert_incidents WHERE 1=1 " + time_filter).c_str(),
            0, nullptr, nullptr, nullptr, nullptr, 0
        );

        json metrics = json::object();
        if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) > 0) {
            metrics["total_incidents"] = std::stoi(PQgetvalue(result, 0, 0));
            metrics["critical_incidents"] = std::stoi(PQgetvalue(result, 0, 1));
            metrics["high_incidents"] = std::stoi(PQgetvalue(result, 0, 2));
            metrics["medium_incidents"] = std::stoi(PQgetvalue(result, 0, 3));
            metrics["low_incidents"] = std::stoi(PQgetvalue(result, 0, 4));
            metrics["active_incidents"] = std::stoi(PQgetvalue(result, 0, 5));
            metrics["acknowledged_incidents"] = std::stoi(PQgetvalue(result, 0, 6));
            metrics["resolved_incidents"] = std::stoi(PQgetvalue(result, 0, 7));
            
            if (!PQgetisnull(result, 0, 8)) {
                metrics["avg_resolution_time_minutes"] = std::stod(PQgetvalue(result, 0, 8));
            }
        }
        PQclear(result);

        // Get rule-specific metrics
        PGresult* rule_result = PQexecParams(
            conn,
            ("SELECT "
             "r.rule_id, r.rule_name, r.rule_type, r.severity, "
             "COUNT(i.incident_id) as incident_count, "
             "MAX(i.triggered_at) as last_triggered "
             "FROM alert_rules r "
             "LEFT JOIN alert_incidents i ON r.rule_id = i.rule_id "
             "WHERE 1=1 " + time_filter + " "
             "GROUP BY r.rule_id, r.rule_name, r.rule_type, r.severity "
             "ORDER BY incident_count DESC LIMIT 10").c_str(),
            0, nullptr, nullptr, nullptr, nullptr, 0
        );

        json rule_metrics = json::array();
        if (PQresultStatus(rule_result) == PGRES_TUPLES_OK) {
            int num_rows = PQntuples(rule_result);
            for (int i = 0; i < num_rows; i++) {
                json rule_metric = {
                    {"rule_id", PQgetvalue(rule_result, i, 0)},
                    {"rule_name", PQgetvalue(rule_result, i, 1)},
                    {"rule_type", PQgetvalue(rule_result, i, 2)},
                    {"severity", PQgetvalue(rule_result, i, 3)},
                    {"incident_count", std::stoi(PQgetvalue(rule_result, i, 4))}
                };
                
                if (!PQgetisnull(rule_result, i, 5)) {
                    rule_metric["last_triggered"] = PQgetvalue(rule_result, i, 5);
                }
                
                rule_metrics.push_back(rule_metric);
            }
        }
        PQclear(rule_result);

        // Get notification metrics
        std::string query = "SELECT "
                             "c.channel_type, "
                             "COUNT(n.notification_id) as total_sent, "
                             "COUNT(*) FILTER (WHERE n.delivery_status = 'delivered') as delivered, "
                             "COUNT(*) FILTER (WHERE n.delivery_status = 'failed') as failed "
                             "FROM notification_channels c "
                             "LEFT JOIN alert_notifications n ON c.channel_id = n.channel_id "
                             "WHERE n.sent_at >= CURRENT_TIMESTAMP - INTERVAL '24 hours' OR n.sent_at IS NULL "
                             "GROUP BY c.channel_type";

        PGresult* notif_result = PQexecParams(
            conn,
            query.c_str(),
            0, nullptr, nullptr, nullptr, nullptr, 0
        );

        json notification_metrics = json::array();
        if (PQresultStatus(notif_result) == PGRES_TUPLES_OK) {
            int num_rows = PQntuples(notif_result);
            for (int i = 0; i < num_rows; i++) {
                json notif_metric = {
                    {"channel_type", PQgetvalue(notif_result, i, 0)},
                    {"total_sent", std::stoi(PQgetvalue(notif_result, i, 1))},
                    {"delivered", std::stoi(PQgetvalue(notif_result, i, 2))},
                    {"failed", std::stoi(PQgetvalue(notif_result, i, 3))}
                };
                
                int total = notif_metric["total_sent"];
                if (total > 0) {
                    notif_metric["delivery_rate"] = (static_cast<double>(notif_metric["delivered"]) / total) * 100.0;
                } else {
                    notif_metric["delivery_rate"] = 0.0;
                }
                
                notification_metrics.push_back(notif_metric);
            }
        }
        PQclear(notif_result);

        json response = {
            {"metrics", metrics},
            {"rule_metrics", rule_metrics},
            {"notification_metrics", notification_metrics},
            {"time_range", time_range},
            {"generated_at", std::to_string(std::time(nullptr))}
        };

        return response.dump();

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_get_alert_metrics: " + std::string(e.what()));
        return R"({"error": "Internal server error"})";
    }
}

// Helper methods implementation
json AlertManagementHandlers::serialize_alert_rule(PGresult* result, int row) {
    json rule = {
        {"rule_id", PQgetvalue(result, row, 0)},
        {"rule_name", PQgetvalue(result, row, 1)},
        {"description", PQgetvalue(result, row, 2)},
        {"rule_type", PQgetvalue(result, row, 3)},
        {"severity", PQgetvalue(result, row, 4)},
        {"condition", json::parse(PQgetvalue(result, row, 5))},
        {"notification_channels", json::parse(PQgetvalue(result, row, 6))},
        {"cooldown_minutes", std::stoi(PQgetvalue(result, row, 8))},
        {"is_enabled", std::string(PQgetvalue(result, row, 9)) == "t"},
        {"created_at", PQgetvalue(result, row, 11)},
        {"updated_at", PQgetvalue(result, row, 12)}
    };

    if (!PQgetisnull(result, row, 7)) {
        rule["notification_config"] = json::parse(PQgetvalue(result, row, 7));
    }
    if (!PQgetisnull(result, row, 10)) {
        rule["created_by"] = PQgetvalue(result, row, 10);
    }
    if (!PQgetisnull(result, row, 13)) {
        rule["last_triggered_at"] = PQgetvalue(result, row, 13);
    }

    return rule;
}

json AlertManagementHandlers::serialize_alert_incident(PGresult* result, int row) {
    json incident = {
        {"incident_id", PQgetvalue(result, row, 0)},
        {"rule_id", PQgetvalue(result, row, 1)},
        {"severity", PQgetvalue(result, row, 2)},
        {"title", PQgetvalue(result, row, 3)},
        {"message", PQgetvalue(result, row, 4)},
        {"incident_data", json::parse(PQgetvalue(result, row, 5))},
        {"triggered_at", PQgetvalue(result, row, 6)},
        {"status", PQgetvalue(result, row, 12)}
    };

    if (!PQgetisnull(result, row, 7)) {
        incident["acknowledged_at"] = PQgetvalue(result, row, 7);
    }
    if (!PQgetisnull(result, row, 8)) {
        incident["acknowledged_by"] = PQgetvalue(result, row, 8);
    }
    if (!PQgetisnull(result, row, 9)) {
        incident["resolved_at"] = PQgetvalue(result, row, 9);
    }
    if (!PQgetisnull(result, row, 10)) {
        incident["resolved_by"] = PQgetvalue(result, row, 10);
    }
    if (!PQgetisnull(result, row, 11)) {
        incident["resolution_notes"] = PQgetvalue(result, row, 11);
    }
    if (!PQgetisnull(result, row, 13)) {
        incident["notification_status"] = json::parse(PQgetvalue(result, row, 13));
    }

    return incident;
}

json AlertManagementHandlers::serialize_notification_channel(PGresult* result, int row) {
    json channel = {
        {"channel_id", PQgetvalue(result, row, 0)},
        {"channel_type", PQgetvalue(result, row, 1)},
        {"channel_name", PQgetvalue(result, row, 2)},
        {"configuration", json::parse(PQgetvalue(result, row, 3))},
        {"is_enabled", std::string(PQgetvalue(result, row, 4)) == "t"},
        {"created_at", PQgetvalue(result, row, 7)}
    };

    if (!PQgetisnull(result, row, 5)) {
        channel["last_tested_at"] = PQgetvalue(result, row, 5);
    }
    if (!PQgetisnull(result, row, 6)) {
        channel["test_status"] = PQgetvalue(result, row, 6);
    }

    return channel;
}

json AlertManagementHandlers::serialize_alert_notification(PGresult* result, int row) {
    json notification = {
        {"notification_id", PQgetvalue(result, row, 0)},
        {"incident_id", PQgetvalue(result, row, 1)},
        {"channel_id", PQgetvalue(result, row, 2)},
        {"sent_at", PQgetvalue(result, row, 3)},
        {"delivery_status", PQgetvalue(result, row, 4)},
        {"retry_count", std::stoi(PQgetvalue(result, row, 5))}
    };

    if (!PQgetisnull(result, row, 6)) {
        notification["error_message"] = PQgetvalue(result, row, 6);
    }
    if (!PQgetisnull(result, row, 7)) {
        notification["next_retry_at"] = PQgetvalue(result, row, 7);
    }

    return notification;
}

std::string AlertManagementHandlers::extract_user_id_from_jwt(const std::map<std::string, std::string>& headers) {
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
            logger_->log(LogLevel::WARN, "Failed to decode JWT payload: " + std::string(e.what()));
            return "";
        }
        
        // Parse JSON payload
        json payload_json = json::parse(decoded_payload);
        
        if (payload_json.contains("user_id")) {
            return payload_json["user_id"];
        } else if (payload_json.contains("sub")) {
            return payload_json["sub"];
        } else {
            logger_->log(LogLevel::WARN, "No user_id or sub claim found in JWT payload");
            return "";
        }
        
    } catch (const json::exception& e) {
        logger_->log(LogLevel::ERROR, "JSON parsing error in extract_user_id_from_jwt: " + std::string(e.what()));
        return "";
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in extract_user_id_from_jwt: " + std::string(e.what()));
        return "";
    }
}

bool AlertManagementHandlers::validate_json_schema(const nlohmann::json& data, const std::string& schema_type) {
    // This would implement JSON schema validation based on the schema type
    // For now, return true - this should be implemented with proper validation
    return true;
}

bool AlertManagementHandlers::validate_alert_condition(const nlohmann::json& condition) {
    // Basic validation for alert condition
    if (!condition.is_object()) {
        return false;
    }
    
    if (!condition.contains("metric") || !condition.contains("operator") || !condition.contains("threshold")) {
        return false;
    }
    
    std::string op = condition["operator"];
    if (op != "gt" && op != "gte" && op != "lt" && op != "lte" && op != "eq" && op != "ne") {
        return false;
    }
    
    return true;
}

bool AlertManagementHandlers::validate_notification_config(const nlohmann::json& config, const std::string& channel_type) {
    if (!config.is_object()) {
        return false;
    }
    
    if (channel_type == "email") {
        return config.contains("recipients") && config.contains("subject");
    } else if (channel_type == "webhook") {
        return config.contains("url");
    } else if (channel_type == "slack") {
        return config.contains("webhook_url");
    }
    
    return false;
}

std::string AlertManagementHandlers::generate_alert_title(const nlohmann::json& rule, const nlohmann::json& incident_data) {
    std::string rule_name = rule["rule_name"];
    std::string severity = rule["severity"];
    
    std::string upper_severity = severity;
    std::transform(upper_severity.begin(), upper_severity.end(), upper_severity.begin(), ::toupper);
    return "[" + upper_severity + "] " + rule_name;
}

std::string AlertManagementHandlers::generate_alert_message(const nlohmann::json& rule, const nlohmann::json& incident_data) {
    std::string rule_name = rule["rule_name"];
    std::string metric = rule["condition"]["metric"];
    std::string operator_str = rule["condition"]["operator"];
    std::string threshold = std::to_string(rule["condition"]["threshold"].get<double>());
    
    std::string message = "Alert: " + rule_name + "\n";
    message += "Condition: " + metric + " " + operator_str + " " + threshold + "\n";
    
    if (incident_data.contains("current_value")) {
        message += "Current value: " + std::to_string(incident_data["current_value"].get<double>()) + "\n";
    }
    
    return message;
}

void AlertManagementHandlers::evaluate_alert_rules() {
    // This method would be called by the background evaluation engine
    // Implementation would check all enabled rules against current metrics
    // and trigger alerts when conditions are met
}

bool AlertManagementHandlers::check_rule_cooldown(const std::string& rule_id) {
    auto conn = db_conn_->get_connection();
    if (!conn) {
        return true; // Fail safe - don't trigger if can't check
    }
    
    const char* params[1] = {rule_id.c_str()};
    PGresult* result = PQexecParams(
        conn,
        "SELECT cooldown_minutes, last_triggered_at FROM alert_rules WHERE rule_id = $1 AND is_enabled = true",
        1, nullptr, params, nullptr, nullptr, 0
    );
    
    if (PQresultStatus(result) != PGRES_TUPLES_OK || PQntuples(result) == 0) {
        PQclear(result);
        return true; // Rule not found or disabled
    }
    
    int cooldown_minutes = std::stoi(PQgetvalue(result, 0, 0));
    
    if (PQgetisnull(result, 0, 1)) {
        PQclear(result);
        return false; // Never triggered before
    }
    
    std::string last_triggered_str = PQgetvalue(result, 0, 1);
    PQclear(result);
    
    // Parse timestamp and check if cooldown period has passed
    // This is a simplified check - in production, proper timestamp parsing would be needed
    std::time_t last_triggered = std::stol(last_triggered_str);
    std::time_t now = std::time(nullptr);
    double seconds_since_trigger = difftime(now, last_triggered);
    double cooldown_seconds = cooldown_minutes * 60.0;
    
    return seconds_since_trigger < cooldown_seconds;
}

void AlertManagementHandlers::update_rule_last_triggered(const std::string& rule_id) {
    auto conn = db_conn_->get_connection();
    if (!conn) {
        return;
    }
    
    const char* params[1] = {rule_id.c_str()};
    PGresult* result = PQexecParams(
        conn,
        "UPDATE alert_rules SET last_triggered_at = CURRENT_TIMESTAMP WHERE rule_id = $1",
        1, nullptr, params, nullptr, nullptr, 0
    );
    
    PQclear(result);
}

void AlertManagementHandlers::create_alert_incident(const std::string& rule_id, const nlohmann::json& rule, const nlohmann::json& incident_data) {
    auto conn = db_conn_->get_connection();
    if (!conn) {
        return;
    }
    
    std::string title = generate_alert_title(rule, incident_data);
    std::string message = generate_alert_message(rule, incident_data);
    std::string severity_str = rule["severity"].get<std::string>();
    std::string incident_data_str = incident_data.dump();

    const char* params[5] = {
        rule_id.c_str(),
        severity_str.c_str(),
        title.c_str(),
        message.c_str(),
        incident_data_str.c_str()
    };
    
    PGresult* result = PQexecParams(
        conn,
        "INSERT INTO alert_incidents (rule_id, severity, title, message, incident_data) "
        "VALUES ($1, $2, $3, $4, $5::jsonb) RETURNING incident_id",
        5, nullptr, params, nullptr, nullptr, 0
    );
    
    if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) > 0) {
        std::string incident_id = PQgetvalue(result, 0, 0);
        PQclear(result);
        
        // Send notifications for this incident
        send_alert_notifications(incident_id, rule);
        
        // Update rule last triggered time
        update_rule_last_triggered(rule_id);
    } else {
        PQclear(result);
    }
}

void AlertManagementHandlers::send_alert_notifications(const std::string& incident_id, const nlohmann::json& rule) {
    // Get incident details
    auto conn = db_conn_->get_connection();
    if (!conn) {
        return;
    }
    
    const char* params[1] = {incident_id.c_str()};
    PGresult* result = PQexecParams(
        conn,
        "SELECT incident_id, title, message, severity, triggered_at FROM alert_incidents WHERE incident_id = $1",
        1, nullptr, params, nullptr, nullptr, 0
    );
    
    if (PQresultStatus(result) != PGRES_TUPLES_OK || PQntuples(result) == 0) {
        PQclear(result);
        return;
    }
    
    json incident = {
        {"incident_id", PQgetvalue(result, 0, 0)},
        {"title", PQgetvalue(result, 0, 1)},
        {"message", PQgetvalue(result, 0, 2)},
        {"severity", PQgetvalue(result, 0, 3)},
        {"triggered_at", PQgetvalue(result, 0, 4)}
    };
    PQclear(result);
    
    // Get notification channels for this rule
    json notification_channels = rule["notification_channels"];
    json notification_config = rule.value("notification_config", json::object());
    
    // Send notifications to each channel
    for (const auto& channel_id : notification_channels) {
        std::string channel_id_str = channel_id.get<std::string>();
        
        // Get channel details
        const char* channel_params[1] = {channel_id_str.c_str()};
        PGresult* channel_result = PQexecParams(
            conn,
            "SELECT channel_id, channel_type, configuration FROM notification_channels WHERE channel_id = $1 AND is_enabled = true",
            1, nullptr, channel_params, nullptr, nullptr, 0
        );
        
        if (PQresultStatus(channel_result) != PGRES_TUPLES_OK || PQntuples(channel_result) == 0) {
            PQclear(channel_result);
            continue;
        }
        
        json channel = {
            {"channel_id", PQgetvalue(channel_result, 0, 0)},
            {"channel_type", PQgetvalue(channel_result, 0, 1)},
            {"configuration", json::parse(PQgetvalue(channel_result, 0, 2))}
        };
        PQclear(channel_result);
        
        // Send notification based on channel type
        bool success = false;
        std::string error_message;
        
        try {
            std::string channel_type = channel["channel_type"];
            if (channel_type == "email") {
                success = send_email_notification(channel["configuration"], incident);
            } else if (channel_type == "webhook") {
                success = send_webhook_notification(channel["configuration"], incident);
            } else if (channel_type == "slack") {
                success = send_slack_notification(channel["configuration"], incident);
            } else {
                error_message = "Unsupported channel type: " + channel_type;
            }
        } catch (const std::exception& e) {
            error_message = e.what();
        }
        
        // Log notification attempt
        log_notification_attempt(incident_id, channel_id_str, success ? "sent" : "failed", error_message);
    }
}

bool AlertManagementHandlers::send_email_notification(const nlohmann::json& config, const nlohmann::json& alert_data) {
    try {
        // Get SMTP configuration from environment variables
        const char* smtp_host = std::getenv("SMTP_HOST");
        const char* smtp_port = std::getenv("SMTP_PORT");
        const char* smtp_username = std::getenv("SMTP_USERNAME");
        const char* smtp_password = std::getenv("SMTP_PASSWORD");
        const char* smtp_use_tls = std::getenv("SMTP_USE_TLS");
        
        if (!smtp_host || !smtp_port || !smtp_username || !smtp_password) {
            logger_->log(LogLevel::ERROR, "Missing SMTP configuration for email notifications");
            return false;
        }
        
        // Get email configuration from alert rule
        if (!config.contains("recipients") || !config.contains("subject")) {
            logger_->log(LogLevel::ERROR, "Missing email configuration: recipients and subject required");
            return false;
        }
        
        std::string recipients_str = config["recipients"];
        std::string subject = config["subject"];
        
        // Build email content
        std::string email_body = "Alert: " + alert_data["title"].get<std::string>() + "\n\n";
        email_body += "Message: " + alert_data["message"].get<std::string>() + "\n";
        email_body += "Severity: " + alert_data["severity"].get<std::string>() + "\n";
        email_body += "Incident ID: " + alert_data["incident_id"].get<std::string>() + "\n";
        
        // Initialize cURL for sending email via SMTP
        CURL *curl = curl_easy_init();
        if (!curl) {
            logger_->log(LogLevel::ERROR, "Failed to initialize cURL for email notification");
            return false;
        }
        
        // Set SMTP options
        std::string smtp_url = "smtp://" + std::string(smtp_host) + ":" + std::string(smtp_port);
        curl_easy_setopt(curl, CURLOPT_URL, smtp_url.c_str());
        curl_easy_setopt(curl, CURLOPT_USERNAME, smtp_username);
        curl_easy_setopt(curl, CURLOPT_PASSWORD, smtp_password);
        
        if (smtp_use_tls && std::string(smtp_use_tls) == "true") {
            curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
        }
        
        // Set email headers and content
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, ("To: " + recipients_str).c_str());
        headers = curl_slist_append(headers, ("From: " + std::string(smtp_username)).c_str());
        headers = curl_slist_append(headers, ("Subject: " + subject).c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        
        // Set email content
        curl_easy_setopt(curl, CURLOPT_MAIL_FROM, smtp_username);
        
        // Set recipients
        struct curl_slist *recipients = NULL;
        std::istringstream recipients_stream(recipients_str);
        std::string recipient;
        while (std::getline(recipients_stream, recipient, ',')) {
            // Trim whitespace
            recipient.erase(0, recipient.find_first_not_of(" \t"));
            recipient.erase(recipient.find_last_not_of(" \t") + 1);
            recipients = curl_slist_append(recipients, recipient.c_str());
        }
        curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
        
        // Set email payload
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, [](void *ptr, size_t size, size_t nmemb, void *userp) -> size_t {
            std::string *email_body = static_cast<std::string*>(userp);
            size_t max_write = size * nmemb;
            size_t to_write = std::min(email_body->length(), max_write);
            if (to_write > 0) {
                memcpy(ptr, email_body->c_str(), to_write);
                email_body->erase(0, to_write);
            }
            return to_write;
        });
        curl_easy_setopt(curl, CURLOPT_READDATA, &email_body);
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
        
        // Send the email
        CURLcode res = curl_easy_perform(curl);
        
        // Clean up
        curl_slist_free_all(headers);
        curl_slist_free_all(recipients);
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK) {
            logger_->log(LogLevel::ERROR, "Failed to send email notification: " + std::string(curl_easy_strerror(res)));
            return false;
        }
        
        logger_->log(LogLevel::INFO, "Email notification sent successfully to: " + recipients_str);
        return true;
        
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in send_email_notification: " + std::string(e.what()));
        return false;
    }
}

bool AlertManagementHandlers::send_webhook_notification(const nlohmann::json& config, const nlohmann::json& alert_data) {
    std::string url = config["url"];
    
    // Prepare JSON payload
    json payload = {
        {"alert", alert_data},
        {"timestamp", std::to_string(std::time(nullptr))}
    };
    
    // Initialize cURL
    CURL *curl = curl_easy_init();
    if (!curl) {
        return false;
    }
    
    // Set cURL options
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.dump().c_str());
    
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // Set callback for response
    std::string response_string;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
    
    // Perform the request
    CURLcode res = curl_easy_perform(curl);
    
    // Clean up
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    
    if (res != CURLE_OK) {
        logger_->log(LogLevel::ERROR, "Webhook notification failed: " + std::string(curl_easy_strerror(res)));
        return false;
    }
    
    logger_->log(LogLevel::INFO, "Webhook notification sent to: " + url);
    return true;
}

bool AlertManagementHandlers::send_slack_notification(const nlohmann::json& config, const nlohmann::json& alert_data) {
    std::string webhook_url = config["webhook_url"];
    
    // Prepare Slack message payload
    json payload = {
        {"text", alert_data["title"]},
        {"attachments", json::array({
            {
                {"color", alert_data["severity"] == "critical" ? "danger" : 
                        alert_data["severity"] == "high" ? "warning" : "good"},
                {"fields", json::array({
                    {{"title", "Message"}, {"value", alert_data["message"]}, {"short", false}},
                    {{"title", "Severity"}, {"value", alert_data["severity"]}, {"short", true}},
                    {{"title", "Incident ID"}, {"value", alert_data["incident_id"]}, {"short", true}}
                })}
            }
        })}
    };
    
    // Initialize cURL
    CURL *curl = curl_easy_init();
    if (!curl) {
        return false;
    }
    
    // Set cURL options
    curl_easy_setopt(curl, CURLOPT_URL, webhook_url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.dump().c_str());
    
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // Set callback for response
    std::string response_string;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
    
    // Perform the request
    CURLcode res = curl_easy_perform(curl);
    
    // Clean up
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    
    if (res != CURLE_OK) {
        logger_->log(LogLevel::ERROR, "Slack notification failed: " + std::string(curl_easy_strerror(res)));
        return false;
    }
    
    logger_->log(LogLevel::INFO, "Slack notification sent for alert: " + alert_data["title"].get<std::string>());
    return true;
}

void AlertManagementHandlers::log_notification_attempt(const std::string& incident_id, const std::string& channel_id, 
                                                      const std::string& status, const std::string& error_message) {
    auto conn = db_conn_->get_connection();
    if (!conn) {
        return;
    }
    
    const char* params[4] = {
        incident_id.c_str(),
        channel_id.c_str(),
        status.c_str(),
        error_message.c_str()
    };
    
    PGresult* result = PQexecParams(
        conn,
        "INSERT INTO alert_notifications (incident_id, channel_id, delivery_status, error_message) "
        "VALUES ($1, $2, $3, $4)",
        4, nullptr, params, nullptr, nullptr, 0
    );
    
    PQclear(result);
}

void AlertManagementHandlers::retry_failed_notifications() {
    auto conn = db_conn_->get_connection();
    if (!conn) {
        return;
    }
    
    // Get failed notifications that are ready for retry
    PGresult* result = PQexecParams(
        conn,
        "SELECT n.notification_id, n.incident_id, n.channel_id, n.retry_count "
        "FROM alert_notifications n "
        "JOIN alert_incidents i ON n.incident_id = i.incident_id "
        "WHERE n.delivery_status = 'failed' "
        "AND (n.next_retry_at IS NULL OR n.next_retry_at <= CURRENT_TIMESTAMP) "
        "AND n.retry_count < 5 "
        "AND i.status != 'resolved' "
        "ORDER BY n.sent_at ASC LIMIT 10",
        0, nullptr, nullptr, nullptr, nullptr, 0
    );
    
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        PQclear(result);
        return;
    }
    
    int num_rows = PQntuples(result);
    for (int i = 0; i < num_rows; i++) {
        std::string notification_id = PQgetvalue(result, i, 0);
        std::string incident_id = PQgetvalue(result, i, 1);
        std::string channel_id = PQgetvalue(result, i, 2);
        int retry_count = std::stoi(PQgetvalue(result, i, 3));
        
        // Schedule retry with exponential backoff
        schedule_notification_retry(notification_id, retry_count + 1);
    }
    
    PQclear(result);
}

void AlertManagementHandlers::schedule_notification_retry(const std::string& notification_id, int retry_count) {
    auto conn = db_conn_->get_connection();
    if (!conn) {
        return;
    }
    
    // Calculate next retry time with exponential backoff (2^retry_count minutes, max 2 hours)
    int delay_minutes = std::min(1 << retry_count, 120);
    std::string retry_count_str = std::to_string(retry_count);

    const char* params[2] = {
        retry_count_str.c_str(),
        notification_id.c_str()
    };

    std::string query = "UPDATE alert_notifications SET retry_count = $1, next_retry_at = CURRENT_TIMESTAMP + INTERVAL '" +
                       std::to_string(delay_minutes) + " minutes', delivery_status = 'pending' WHERE notification_id = $2";

    PGresult* result = PQexecParams(
        conn,
        query.c_str(),
        2, nullptr, params, nullptr, nullptr, 0
    );
    
    PQclear(result);
}

std::string AlertManagementHandlers::base64_decode(const std::string& encoded_string) {
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

} // namespace alerts
} // namespace regulens