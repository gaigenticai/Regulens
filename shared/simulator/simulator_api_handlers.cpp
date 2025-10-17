/**
 * Regulatory Simulator API Handlers Implementation
 * Production-grade REST API endpoints for regulatory impact simulation
 */

#include "simulator_api_handlers.hpp"
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <algorithm>
#include <ctime>
#include <unordered_set>

namespace regulens {
namespace simulator {

namespace {

struct SimulatorCacheEntry {
    std::chrono::steady_clock::time_point expires_at;
    nlohmann::json payload;
};

std::mutex g_simulator_cache_mutex;
std::unordered_map<std::string, SimulatorCacheEntry> g_simulator_cache;
constexpr std::chrono::seconds kSimulatorAnalyticsTtl{180};

double json_to_double(const nlohmann::json& value, double fallback = 0.0) {
    try {
        if (value.is_number()) {
            return value.get<double>();
        }
        if (value.is_string()) {
            return std::stod(value.get<std::string>());
        }
    } catch (...) {
        return fallback;
    }
    return fallback;
}

int json_to_int(const nlohmann::json& value, int fallback = 0) {
    try {
        if (value.is_number_integer()) {
            return value.get<int>();
        }
        if (value.is_string()) {
            return std::stoi(value.get<std::string>());
        }
    } catch (...) {
        return fallback;
    }
    return fallback;
}

std::string make_cache_key(const std::string& scope, const std::string& user_id, const std::map<std::string, std::string>& filters) {
    std::ostringstream oss;
    oss << scope << ':' << user_id;
    for (const auto& [key, value] : filters) {
        oss << '|' << key << '=' << value;
    }
    return oss.str();
}

std::chrono::system_clock::time_point parse_timestamp(const std::string& timestamp) {
    if (timestamp.empty()) {
        return std::chrono::system_clock::now();
    }

    std::tm tm{};
    std::istringstream ss(timestamp);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (ss.fail()) {
        ss.clear();
        ss.str(timestamp);
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
        if (ss.fail()) {
            return std::chrono::system_clock::now();
        }
    }

#if defined(_WIN32)
    time_t tt = _mkgmtime(&tm);
#else
    time_t tt = timegm(&tm);
#endif
    if (tt == -1) {
        return std::chrono::system_clock::now();
    }
    return std::chrono::system_clock::from_time_t(tt);
}

std::vector<std::string> parse_text_array(const std::string& value) {
    std::vector<std::string> result;
    if (value.size() < 2) {
        return result;
    }

    std::string normalized = value;
    std::replace(normalized.begin(), normalized.end(), '{', '[');
    std::replace(normalized.begin(), normalized.end(), '}', ']');

    try {
        auto json_array = nlohmann::json::parse(normalized);
        if (json_array.is_array()) {
            for (const auto& item : json_array) {
                if (item.is_string()) {
                    result.push_back(item.get<std::string>());
                } else {
                    result.push_back(item.dump());
                }
            }
        }
    } catch (...) {}

    return result;
}

} // namespace

SimulatorAPIHandlers::SimulatorAPIHandlers(
    std::shared_ptr<PostgreSQLConnection> db_conn,
    StructuredLogger& logger,
    std::shared_ptr<RegulatorySimulator> simulator
) : db_conn_(db_conn), logger_(logger), simulator_(simulator) {

    if (!db_conn_) {
        throw std::runtime_error("Database connection is required for SimulatorAPIHandlers");
    }
    if (!simulator_) {
        throw std::runtime_error("Simulator is required for SimulatorAPIHandlers");
    }
}

std::string SimulatorAPIHandlers::handle_create_scenario(const std::string& request_body, const std::string& user_id) {
    try {
        nlohmann::json request = nlohmann::json::parse(request_body);

        // Validate required fields
        if (!request.contains("scenario_name")) {
            return create_error_response("scenario_name is required");
        }

        if (!request.contains("regulatory_changes")) {
            return create_error_response("regulatory_changes is required");
        }

        // Parse and create scenario
        SimulationScenario scenario = parse_scenario_request(request, user_id);
        auto created_scenario = simulator_->create_scenario(scenario, user_id);

        if (!created_scenario) {
            return create_error_response("Failed to create scenario", 500);
        }

        // Return formatted response
        nlohmann::json response_data = format_scenario_response(*created_scenario);
        return create_success_response(response_data, "Scenario created successfully");

    } catch (const nlohmann::json::exception& e) {
        logger_.log(LogLevel::ERROR, "Invalid JSON in handle_create_scenario: " + std::string(e.what()));
        return create_error_response("Invalid request format", 400);
    } catch (const std::exception& e) {
        logger_.log(LogLevel::ERROR, "Exception in handle_create_scenario: " + std::string(e.what()));
        return create_error_response("Internal server error", 500);
    }
}

std::string SimulatorAPIHandlers::handle_run_simulation(const std::string& request_body, const std::string& user_id) {
    try {
        // Check rate limiting
        if (!check_simulation_rate_limit(user_id)) {
            return create_error_response("Rate limit exceeded. Please try again later.", 429);
        }

        nlohmann::json request = nlohmann::json::parse(request_body);

        // Validate required fields
        if (!request.contains("scenario_id")) {
            return create_error_response("scenario_id is required");
        }

        // Validate scenario access
        if (!validate_scenario_access(request["scenario_id"], user_id)) {
            return create_error_response("Scenario not found or access denied", 404);
        }

        // Parse simulation request
        SimulationRequest sim_request = parse_simulation_request(request, user_id);

        // Run simulation
        std::string execution_id = simulator_->run_simulation(sim_request);

        // Record the attempt for rate limiting
        record_simulation_attempt(user_id);

        nlohmann::json response_data = {
            {"execution_id", execution_id},
            {"status", "running"},
            {"message", sim_request.async_execution ?
                      "Simulation started asynchronously" :
                      "Simulation completed synchronously"}
        };

        return create_success_response(response_data, "Simulation started successfully");

    } catch (const nlohmann::json::exception& e) {
        logger_.log(LogLevel::ERROR, "Invalid JSON in handle_run_simulation: " + std::string(e.what()));
        return create_error_response("Invalid request format", 400);
    } catch (const std::exception& e) {
        logger_.log(LogLevel::ERROR, "Exception in handle_run_simulation: " + std::string(e.what()));
        return create_error_response("Internal server error", 500);
    }
}

std::string SimulatorAPIHandlers::handle_get_execution_status(const std::string& execution_id, const std::string& user_id) {
    try {
        // Validate execution access
        if (!validate_execution_access(execution_id, user_id)) {
            return create_error_response("Execution not found or access denied", 404);
        }

        // Get execution status
        auto execution = simulator_->get_execution_status(execution_id);
        if (!execution) {
            return create_error_response("Execution not found", 404);
        }

        // Return formatted response
        nlohmann::json response_data = format_execution_response(*execution);
        return create_success_response(response_data);

    } catch (const std::exception& e) {
        logger_.log(LogLevel::ERROR, "Exception in handle_get_execution_status: " + std::string(e.what()));
        return create_error_response("Internal server error", 500);
    }
}

std::string SimulatorAPIHandlers::handle_get_simulation_result(const std::string& execution_id, const std::string& user_id) {
    try {
        // Validate execution access
        if (!validate_execution_access(execution_id, user_id)) {
            return create_error_response("Execution not found or access denied", 404);
        }

        // Check if execution is completed
        auto execution = simulator_->get_execution_status(execution_id);
        if (!execution || execution->execution_status != "completed") {
            return create_error_response("Simulation is not yet completed", 202);
        }

        // Get simulation result
        auto result = simulator_->get_simulation_result(execution_id);
        if (!result) {
            return create_error_response("Simulation result not found", 404);
        }

        // Return formatted response
        nlohmann::json response_data = format_result_response(*result);
        return create_success_response(response_data);

    } catch (const std::exception& e) {
        logger_.log(LogLevel::ERROR, "Exception in handle_get_simulation_result: " + std::string(e.what()));
        return create_error_response("Internal server error", 500);
    }
}

std::string SimulatorAPIHandlers::handle_get_scenarios(const std::string& user_id, const std::map<std::string, std::string>& query_params) {
    try {
        // Parse query parameters
        auto filters = parse_query_parameters(query_params);
        int limit = 50;
        int offset = 0;

        if (query_params.count("limit")) {
            limit = std::min(std::stoi(query_params.at("limit")), 100); // Max 100
        }
        if (query_params.count("offset")) {
            offset = std::stoi(query_params.at("offset"));
        }

        // Get scenarios
        std::vector<SimulationScenario> scenarios = query_scenarios_paginated(user_id, filters, limit, offset);

        // Format response
        nlohmann::json scenarios_array = nlohmann::json::array();
        for (const auto& scenario : scenarios) {
            scenarios_array.push_back(format_scenario_response(scenario));
        }

        nlohmann::json response = {
            {"scenarios", scenarios_array},
            {"count", scenarios.size()},
            {"limit", limit},
            {"offset", offset}
        };

        return create_success_response(response);

    } catch (const std::exception& e) {
        logger_.log(LogLevel::ERROR, "Exception in handle_get_scenarios: " + std::string(e.what()));
        return create_error_response("Internal server error", 500);
    }
}

std::string SimulatorAPIHandlers::handle_get_scenario(const std::string& scenario_id, const std::string& user_id) {
    try {
        // Validate access
        if (!validate_scenario_access(scenario_id, user_id)) {
            return create_error_response("Scenario not found or access denied", 404);
        }

        // Get scenario details
        auto scenario = simulator_->get_scenario(scenario_id);
        if (!scenario) {
            return create_error_response("Scenario not found", 404);
        }

        // Return formatted response
        nlohmann::json response_data = format_scenario_response(*scenario);
        return create_success_response(response_data);

    } catch (const std::exception& e) {
        logger_.log(LogLevel::ERROR, "Exception in handle_get_scenario: " + std::string(e.what()));
        return create_error_response("Internal server error", 500);
    }
}

std::string SimulatorAPIHandlers::handle_get_simulation_history(const std::string& user_id, const std::map<std::string, std::string>& query_params) {
    try {
        int limit = 50;
        int offset = 0;

        if (query_params.count("limit")) {
            limit = std::min(std::stoi(query_params.at("limit")), 200); // Max 200
        }
        if (query_params.count("offset")) {
            offset = std::stoi(query_params.at("offset"));
        }

        // Get simulation history
        std::vector<SimulationResult> results = simulator_->get_user_simulation_history(user_id, limit, offset);

        // Format response
        nlohmann::json results_array = nlohmann::json::array();
        for (const auto& result : results) {
            results_array.push_back(format_result_response(result));
        }

        nlohmann::json response = {
            {"simulations", results_array},
            {"count", results.size()},
            {"limit", limit},
            {"offset", offset}
        };

        return create_success_response(response);

    } catch (const std::exception& e) {
        logger_.log(LogLevel::ERROR, "Exception in handle_get_simulation_history: " + std::string(e.what()));
        return create_error_response("Internal server error", 500);
    }
}

std::string SimulatorAPIHandlers::handle_get_templates(const std::map<std::string, std::string>& query_params) {
    try {
        std::string category = query_params.count("category") ? query_params.at("category") : "";
        std::string jurisdiction = query_params.count("jurisdiction") ? query_params.at("jurisdiction") : "";

        // Get templates
        std::vector<SimulationTemplate> templates = simulator_->get_templates(category, jurisdiction);

        // Format response
        nlohmann::json templates_array = nlohmann::json::array();
        for (const auto& tmpl : templates) {
            templates_array.push_back(format_template_response(tmpl));
        }

        nlohmann::json response = {
            {"templates", templates_array},
            {"count", templates.size()}
        };

        if (!category.empty()) {
            response["category"] = category;
        }
        if (!jurisdiction.empty()) {
            response["jurisdiction"] = jurisdiction;
        }

        return create_success_response(response);

    } catch (const std::exception& e) {
        logger_.log(LogLevel::ERROR, "Exception in handle_get_templates: " + std::string(e.what()));
        return create_error_response("Internal server error", 500);
    }
}

std::string SimulatorAPIHandlers::handle_get_simulation_analytics(const std::string& user_id, const std::map<std::string, std::string>& query_params) {
    try {
        // Check cache first
        std::string cache_key = "simulator_analytics_" + user_id;
        if (query_params.count("time_range")) {
            cache_key += "_" + query_params.at("time_range");
        }

        auto cached_result = get_cached_analytics(cache_key, user_id);
        if (cached_result) {
            return create_success_response(*cached_result);
        }

        // Calculate analytics
        std::optional<std::string> time_range;
        if (query_params.count("time_range")) {
            time_range = query_params.at("time_range");
        }

        nlohmann::json analytics = simulator_->get_simulation_analytics(user_id, time_range);

        // Cache result
        cache_analytics_result(cache_key, user_id, analytics);

        return create_success_response(analytics);

    } catch (const std::exception& e) {
        logger_.log(LogLevel::ERROR, "Exception in handle_get_simulation_analytics: " + std::string(e.what()));
        return create_error_response("Internal server error", 500);
    }
}

SimulationScenario SimulatorAPIHandlers::parse_scenario_request(const nlohmann::json& request_json, const std::string& user_id) {
    SimulationScenario scenario;

    scenario.scenario_name = request_json["scenario_name"];
    scenario.description = request_json.value("description", "");
    scenario.scenario_type = request_json.value("scenario_type", "regulatory_change");
    scenario.regulatory_changes = request_json["regulatory_changes"];

    // Optional fields
    if (request_json.contains("impact_parameters")) {
        scenario.impact_parameters = request_json["impact_parameters"];
    }

    if (request_json.contains("baseline_data")) {
        scenario.baseline_data = request_json["baseline_data"];
    }

    if (request_json.contains("test_data")) {
        scenario.test_data = request_json["test_data"];
    }

    if (request_json.contains("tags") && request_json["tags"].is_array()) {
        for (const auto& tag : request_json["tags"]) {
            scenario.tags.push_back(tag);
        }
    }

    if (request_json.contains("metadata")) {
        scenario.metadata = request_json["metadata"];
    }

    scenario.created_by = user_id;
    scenario.estimated_runtime_seconds = request_json.value("estimated_runtime_seconds", 300);
    scenario.max_concurrent_simulations = request_json.value("max_concurrent_simulations", 1);
    scenario.is_template = request_json.value("is_template", false);
    scenario.is_active = true;

    return scenario;
}

SimulationRequest SimulatorAPIHandlers::parse_simulation_request(const nlohmann::json& request_json, const std::string& user_id) {
    SimulationRequest request;

    request.scenario_id = request_json["scenario_id"];
    request.user_id = user_id;
    request.async_execution = request_json.value("async_execution", true);
    request.priority = request_json.value("priority", 1);

    if (request_json.contains("custom_parameters")) {
        request.custom_parameters = request_json["custom_parameters"];
    }

    if (request_json.contains("test_data_override")) {
        request.test_data_override = request_json["test_data_override"];
    }

    return request;
}

nlohmann::json SimulatorAPIHandlers::format_scenario_response(const SimulationScenario& scenario) {
    nlohmann::json formatted = {
        {"scenario_id", scenario.scenario_id},
        {"scenario_name", scenario.scenario_name},
        {"description", scenario.description},
        {"scenario_type", scenario.scenario_type},
        {"regulatory_changes", scenario.regulatory_changes},
        {"created_by", scenario.created_by},
        {"is_template", scenario.is_template},
        {"is_active", scenario.is_active},
        {"estimated_runtime_seconds", scenario.estimated_runtime_seconds},
        {"max_concurrent_simulations", scenario.max_concurrent_simulations},
        {"created_at", std::chrono::duration_cast<std::chrono::seconds>(
            scenario.created_at.time_since_epoch()).count()},
        {"updated_at", std::chrono::duration_cast<std::chrono::seconds>(
            scenario.updated_at.time_since_epoch()).count()}
    };

    if (!scenario.tags.empty()) {
        formatted["tags"] = scenario.tags;
    }

    if (!scenario.metadata.empty()) {
        formatted["metadata"] = scenario.metadata;
    }

    return formatted;
}

nlohmann::json SimulatorAPIHandlers::format_execution_response(const SimulationExecution& execution) {
    nlohmann::json formatted = {
        {"execution_id", execution.execution_id},
        {"scenario_id", execution.scenario_id},
        {"user_id", execution.user_id},
        {"execution_status", execution.execution_status},
        {"progress_percentage", execution.progress_percentage},
        {"created_at", std::chrono::duration_cast<std::chrono::seconds>(
            execution.created_at.time_since_epoch()).count()}
    };

    if (execution.started_at) {
        formatted["started_at"] = std::chrono::duration_cast<std::chrono::seconds>(
            execution.started_at->time_since_epoch()).count();
    }

    if (execution.completed_at) {
        formatted["completed_at"] = std::chrono::duration_cast<std::chrono::seconds>(
            execution.completed_at->time_since_epoch()).count();
    }

    if (execution.cancelled_at) {
        formatted["cancelled_at"] = std::chrono::duration_cast<std::chrono::seconds>(
            execution.cancelled_at->time_since_epoch()).count();
    }

    if (execution.error_message) {
        formatted["error_message"] = *execution.error_message;
    }

    if (!execution.execution_parameters.empty()) {
        formatted["execution_parameters"] = execution.execution_parameters;
    }

    return formatted;
}

nlohmann::json SimulatorAPIHandlers::format_result_response(const SimulationResult& result) {
    nlohmann::json formatted = {
        {"result_id", result.result_id},
        {"execution_id", result.execution_id},
        {"scenario_id", result.scenario_id},
        {"user_id", result.user_id},
        {"result_type", result.result_type},
        {"impact_summary", result.impact_summary},
        {"detailed_results", result.detailed_results},
        {"affected_entities", result.affected_entities},
        {"recommendations", result.recommendations},
        {"risk_assessment", result.risk_assessment},
        {"cost_impact", result.cost_impact},
        {"compliance_impact", result.compliance_impact},
        {"operational_impact", result.operational_impact},
        {"created_at", std::chrono::duration_cast<std::chrono::seconds>(
            result.created_at.time_since_epoch()).count()}
    };

    if (!result.metadata.empty()) {
        formatted["metadata"] = result.metadata;
    }

    return formatted;
}

nlohmann::json SimulatorAPIHandlers::format_template_response(const SimulationTemplate& tmpl) {
    nlohmann::json formatted = {
        {"template_id", tmpl.template_id},
        {"template_name", tmpl.template_name},
        {"template_description", tmpl.template_description},
        {"category", tmpl.category},
        {"jurisdiction", tmpl.jurisdiction},
        {"regulatory_body", tmpl.regulatory_body},
        {"usage_count", tmpl.usage_count},
        {"success_rate", tmpl.success_rate},
        {"average_runtime_seconds", tmpl.average_runtime_seconds},
        {"is_active", tmpl.is_active},
        {"created_at", std::chrono::duration_cast<std::chrono::seconds>(
            tmpl.created_at.time_since_epoch()).count()}
    };

    if (!tmpl.tags.empty()) {
        formatted["tags"] = tmpl.tags;
    }

    return formatted;
}

bool SimulatorAPIHandlers::validate_scenario_access(const std::string& scenario_id, const std::string& user_id) {
    try {
        auto conn = db_conn_->get_connection();
        if (!conn) return false;

        const char* params[2] = {scenario_id.c_str(), user_id.c_str()};
        PGresult* result = PQexecParams(
            conn,
            "SELECT scenario_id FROM simulation_scenarios WHERE scenario_id = $1 AND (created_by = $2 OR is_template = true) AND is_active = true",
            2, nullptr, params, nullptr, nullptr, 0
        );

        bool valid = (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) > 0);
        PQclear(result);
        return valid;

    } catch (const std::exception& e) {
        logger_.log(LogLevel::ERROR, "Exception in validate_scenario_access: " + std::string(e.what()));
        return false;
    }
}

bool SimulatorAPIHandlers::validate_execution_access(const std::string& execution_id, const std::string& user_id) {
    try {
        auto conn = db_conn_->get_connection();
        if (!conn) return false;

        const char* params[2] = {execution_id.c_str(), user_id.c_str()};
        PGresult* result = PQexecParams(
            conn,
            "SELECT e.execution_id FROM simulation_executions e WHERE e.execution_id = $1 AND e.user_id = $2",
            2, nullptr, params, nullptr, nullptr, 0
        );

        bool valid = (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) > 0);
        PQclear(result);
        return valid;

    } catch (const std::exception& e) {
        logger_.log(LogLevel::ERROR, "Exception in validate_execution_access: " + std::string(e.what()));
        return false;
    }
}

std::string SimulatorAPIHandlers::create_error_response(const std::string& message, int status_code) {
    nlohmann::json response = {
        {"success", false},
        {"error", message},
        {"status_code", status_code},
        {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
    };
    return response.dump();
}

std::string SimulatorAPIHandlers::create_success_response(const nlohmann::json& data, const std::string& message) {
    nlohmann::json response = {
        {"success", true},
        {"data", data},
        {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
    };

    if (!message.empty()) {
        response["message"] = message;
    }

    return response.dump();
}

// Placeholder implementations for remaining methods
std::vector<SimulationScenario> SimulatorAPIHandlers::query_scenarios_paginated(const std::string& user_id,
                                                                              const std::map<std::string, std::string>& filters,
                                                                              int limit, int offset) {
    std::vector<SimulationScenario> scenarios;

    try {
        auto normalized_filters = parse_query_parameters(filters);
        int safe_limit = std::clamp(limit, 1, 100);
        int safe_offset = std::max(0, offset);

        std::ostringstream sql;
        sql << "SELECT scenario_id, scenario_name, description, scenario_type, "
               "regulatory_changes, impact_parameters, baseline_data, test_data, "
               "created_by, created_at, updated_at, is_template, is_active, tags, metadata, "
               "estimated_runtime_seconds, max_concurrent_simulations "
               "FROM simulation_scenarios WHERE created_by = $1";

        std::vector<std::string> params;
        params.push_back(user_id);
        int param_index = 2;

        if (normalized_filters.count("scenario_type")) {
            sql << " AND scenario_type = $" << param_index;
            params.push_back(normalized_filters["scenario_type"]);
            ++param_index;
        }

        if (normalized_filters.count("status")) {
            std::string status = normalized_filters["status"];
            if (status == "active") {
                sql << " AND is_active = true";
            } else if (status == "inactive") {
                sql << " AND is_active = false";
            }
        }

        if (normalized_filters.count("is_template")) {
            std::string flag = normalized_filters["is_template"];
            sql << " AND is_template = $" << param_index;
            params.push_back(flag == "true" ? "true" : "false");
            ++param_index;
        }

        if (normalized_filters.count("search")) {
            sql << " AND (scenario_name ILIKE $" << param_index << " OR description ILIKE $" << param_index << ")";
            params.push_back('%' + normalized_filters["search"] + '%');
            ++param_index;
        }

        if (normalized_filters.count("tag")) {
            sql << " AND tags @> ARRAY[$" << param_index << "]::text[]";
            params.push_back(normalized_filters["tag"]);
            ++param_index;
        }

        std::string sort_column = "created_at";
        if (normalized_filters.count("sort_by")) {
            const std::unordered_map<std::string, std::string> allowed = {
                {"created_at", "created_at"},
                {"updated_at", "updated_at"},
                {"name", "scenario_name"},
                {"runtime", "estimated_runtime_seconds"}
            };
            auto it = allowed.find(normalized_filters["sort_by"]);
            if (it != allowed.end()) {
                sort_column = it->second;
            }
        }

        std::string sort_direction = "DESC";
        if (normalized_filters.count("sort_direction")) {
            std::string direction = normalized_filters["sort_direction"];
            std::transform(direction.begin(), direction.end(), direction.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (direction == "asc") {
                sort_direction = "ASC";
            }
        }

        sql << " ORDER BY " << sort_column << ' ' << sort_direction;
        sql << " LIMIT " << safe_limit << " OFFSET " << safe_offset;

        auto rows = db_conn_->execute_query_multi(sql.str(), params);

        auto parse_json_field = [](const nlohmann::json& source,
                                   const std::string& key,
                                   const nlohmann::json& fallback = nlohmann::json::object()) {
            const auto raw = source.value(key, std::string{});
            if (raw.empty()) {
                return fallback;
            }
            try {
                return nlohmann::json::parse(raw);
            } catch (...) {
                return fallback;
            }
        };

        for (const auto& row : rows) {
            SimulationScenario scenario;
            scenario.scenario_id = row.value("scenario_id", "");
            scenario.scenario_name = row.value("scenario_name", "");
            scenario.description = row.value("description", "");
            scenario.scenario_type = row.value("scenario_type", "");
            scenario.created_by = row.value("created_by", "");
            scenario.is_template = row.value("is_template", "f") == "t";
            scenario.is_active = row.value("is_active", "t") == "t";
            scenario.estimated_runtime_seconds = row.contains("estimated_runtime_seconds") ? json_to_int(row["estimated_runtime_seconds"]) : 0;
            scenario.max_concurrent_simulations = row.contains("max_concurrent_simulations") ? json_to_int(row["max_concurrent_simulations"], 1) : 1;

            scenario.regulatory_changes = parse_json_field(row, "regulatory_changes");
            scenario.impact_parameters = parse_json_field(row, "impact_parameters");
            scenario.baseline_data = parse_json_field(row, "baseline_data");
            scenario.test_data = parse_json_field(row, "test_data");
            scenario.metadata = parse_json_field(row, "metadata");

            scenario.tags = parse_text_array(row.value("tags", ""));
            scenario.created_at = parse_timestamp(row.value("created_at", ""));
            scenario.updated_at = parse_timestamp(row.value("updated_at", ""));

            scenarios.push_back(std::move(scenario));
        }

    } catch (const std::exception& e) {
        logger_.log(LogLevel::ERROR, "Exception in query_scenarios_paginated: " + std::string(e.what()));
    }

    return scenarios;
}

std::vector<SimulationExecution> SimulatorAPIHandlers::query_user_executions(const std::string& user_id, int limit, int offset) {
    std::vector<SimulationExecution> executions;

    try {
        int safe_limit = std::clamp(limit, 1, 200);
        int safe_offset = std::max(0, offset);

        auto rows = db_conn_->execute_query_multi(
            "SELECT execution_id, scenario_id, user_id, execution_status, execution_parameters, "
            "started_at, completed_at, cancelled_at, error_message, progress_percentage, created_at, metadata "
            "FROM simulation_executions WHERE user_id = $1 ORDER BY created_at DESC LIMIT " + std::to_string(safe_limit) + " OFFSET " + std::to_string(safe_offset),
            {user_id}
        );

        executions.reserve(rows.size());
        for (const auto& row : rows) {
            SimulationExecution execution;
            execution.execution_id = row.value("execution_id", "");
            execution.scenario_id = row.value("scenario_id", "");
            execution.user_id = row.value("user_id", "");
            execution.execution_status = row.value("execution_status", "pending");
            execution.progress_percentage = row.contains("progress_percentage") ? json_to_double(row["progress_percentage"]) : 0.0;
            execution.created_at = parse_timestamp(row.value("created_at", ""));

            const auto execution_params_raw = row.value("execution_parameters", std::string{});
            if (!execution_params_raw.empty()) {
                try {
                    execution.execution_parameters = nlohmann::json::parse(execution_params_raw);
                } catch (...) {}
            }

            const auto metadata_raw = row.value("metadata", std::string{});
            if (!metadata_raw.empty()) {
                try {
                    execution.metadata = nlohmann::json::parse(metadata_raw);
                } catch (...) {}
            }

            auto started_at = row.value("started_at", "");
            if (!started_at.empty()) {
                execution.started_at = parse_timestamp(started_at);
            }

            auto completed_at = row.value("completed_at", "");
            if (!completed_at.empty()) {
                execution.completed_at = parse_timestamp(completed_at);
            }

            auto cancelled_at = row.value("cancelled_at", "");
            if (!cancelled_at.empty()) {
                execution.cancelled_at = parse_timestamp(cancelled_at);
            }

            const auto error_message_raw = row.value("error_message", std::string{});
            if (!error_message_raw.empty()) {
                execution.error_message = error_message_raw;
            }

            executions.push_back(std::move(execution));
        }

    } catch (const std::exception& e) {
        logger_.log(LogLevel::ERROR, "Exception in query_user_executions: " + std::string(e.what()));
    }

    return executions;
}

std::vector<SimulationResult> SimulatorAPIHandlers::query_simulation_results(const std::string& user_id, int limit, int offset) {
    std::vector<SimulationResult> results;

    try {
        int safe_limit = std::clamp(limit, 1, 200);
        int safe_offset = std::max(0, offset);

        auto rows = db_conn_->execute_query_multi(
            "SELECT result_id, execution_id, scenario_id, user_id, result_type, impact_summary, detailed_results, "
            "affected_entities, recommendations, risk_assessment, cost_impact, compliance_impact, operational_impact, created_at, metadata "
            "FROM simulation_results WHERE user_id = $1 ORDER BY created_at DESC LIMIT " + std::to_string(safe_limit) + " OFFSET " + std::to_string(safe_offset),
            {user_id}
        );

        results.reserve(rows.size());
        for (const auto& row : rows) {
            SimulationResult result;
            result.result_id = row.value("result_id", "");
            result.execution_id = row.value("execution_id", "");
            result.scenario_id = row.value("scenario_id", "");
            result.user_id = row.value("user_id", "");
            result.result_type = row.value("result_type", "impact_analysis");
            result.created_at = parse_timestamp(row.value("created_at", ""));

            auto parse_json_field = [](const nlohmann::json& row_json, const std::string& key) {
                const auto raw = row_json.value(key, std::string{});
                if (raw.empty()) {
                    return nlohmann::json::object();
                }
                try {
                    return nlohmann::json::parse(raw);
                } catch (...) {
                    return nlohmann::json::object();
                }
            };

            result.impact_summary = parse_json_field(row, "impact_summary");
            result.detailed_results = parse_json_field(row, "detailed_results");
            result.affected_entities = parse_json_field(row, "affected_entities");
            result.recommendations = parse_json_field(row, "recommendations");
            result.risk_assessment = parse_json_field(row, "risk_assessment");
            result.cost_impact = parse_json_field(row, "cost_impact");
            result.compliance_impact = parse_json_field(row, "compliance_impact");
            result.operational_impact = parse_json_field(row, "operational_impact");
            result.metadata = parse_json_field(row, "metadata");

            results.push_back(std::move(result));
        }

    } catch (const std::exception& e) {
        logger_.log(LogLevel::ERROR, "Exception in query_simulation_results: " + std::string(e.what()));
    }

    return results;
}

bool SimulatorAPIHandlers::validate_scenario_data(const nlohmann::json& scenario_data) {
    if (!scenario_data.is_object()) {
        logger_.log(LogLevel::WARN, "Scenario validation failed: payload is not an object");
        return false;
    }

    if (!scenario_data.contains("scenario_name") || !scenario_data["scenario_name"].is_string() || scenario_data["scenario_name"].get<std::string>().empty()) {
        logger_.log(LogLevel::WARN, "Scenario validation failed: scenario_name missing or invalid");
        return false;
    }

    if (!scenario_data.contains("regulatory_changes")) {
        logger_.log(LogLevel::WARN, "Scenario validation failed: regulatory_changes missing");
        return false;
    }

    if (!validate_regulatory_changes(scenario_data["regulatory_changes"])) {
        return false;
    }

    if (scenario_data.contains("scenario_type")) {
        static const std::unordered_set<std::string> allowed_types = {"regulatory_change", "market_change", "operational_change"};
        if (!scenario_data["scenario_type"].is_string() || !allowed_types.count(scenario_data["scenario_type"].get<std::string>())) {
            logger_.log(LogLevel::WARN, "Scenario validation failed: scenario_type is invalid");
            return false;
        }
    }

    if (scenario_data.contains("impact_parameters") && !validate_simulation_parameters(scenario_data["impact_parameters"])) {
        return false;
    }

    if (scenario_data.contains("tags")) {
        if (!scenario_data["tags"].is_array()) {
            logger_.log(LogLevel::WARN, "Scenario validation failed: tags must be an array");
            return false;
        }
        if (scenario_data["tags"].size() > 50) {
            logger_.log(LogLevel::WARN, "Scenario validation failed: too many tags provided");
            return false;
        }
    }

    return true;
}

bool SimulatorAPIHandlers::validate_regulatory_changes(const nlohmann::json& changes) {
    auto validate_change_object = [this](const nlohmann::json& change) {
        if (!change.is_object()) {
            logger_.log(LogLevel::WARN, "Regulatory change validation failed: change is not an object");
            return false;
        }

        static const std::unordered_set<std::string> required_fields = {"change_type", "jurisdiction", "description"};
        for (const auto& field : required_fields) {
            if (!change.contains(field) || change[field].is_null()) {
                logger_.log(LogLevel::WARN, "Regulatory change validation failed: missing field " + field);
                return false;
            }
        }

        if (!change["change_type"].is_string()) {
            logger_.log(LogLevel::WARN, "Regulatory change validation failed: change_type must be a string");
            return false;
        }

        static const std::unordered_set<std::string> allowed_change_types = {"addition", "modification", "repeal"};
        if (!allowed_change_types.count(change["change_type"].get<std::string>())) {
            logger_.log(LogLevel::WARN, "Regulatory change validation failed: change_type not allowed");
            return false;
        }

        if (!change["jurisdiction"].is_string()) {
            logger_.log(LogLevel::WARN, "Regulatory change validation failed: jurisdiction must be a string");
            return false;
        }

        if (!change["description"].is_string()) {
            logger_.log(LogLevel::WARN, "Regulatory change validation failed: description must be a string");
            return false;
        }

        if (change.contains("effective_date") && !change["effective_date"].is_string()) {
            logger_.log(LogLevel::WARN, "Regulatory change validation failed: effective_date must be a string");
            return false;
        }

        if (change.contains("severity") && change["severity"].is_string()) {
            static const std::unordered_set<std::string> allowed_severity = {"low", "medium", "high", "critical"};
            if (!allowed_severity.count(change["severity"].get<std::string>())) {
                logger_.log(LogLevel::WARN, "Regulatory change validation failed: severity not allowed");
                return false;
            }
        }

        return true;
    };

    if (changes.is_object()) {
        return validate_change_object(changes);
    }

    if (!changes.is_array() || changes.empty()) {
        logger_.log(LogLevel::WARN, "Regulatory change validation failed: changes should be a non-empty array");
        return false;
    }

    for (const auto& change : changes) {
        if (!validate_change_object(change)) {
            return false;
        }
    }

    return true;
}

bool SimulatorAPIHandlers::validate_simulation_parameters(const nlohmann::json& params) {
    if (!params.is_object()) {
        logger_.log(LogLevel::WARN, "Simulation parameters validation failed: parameters must be an object");
        return false;
    }

    if (params.contains("sensitivity")) {
        if (!params["sensitivity"].is_number()) {
            logger_.log(LogLevel::WARN, "Simulation parameters validation failed: sensitivity must be numeric");
            return false;
        }
        double sensitivity = params["sensitivity"].get<double>();
        if (sensitivity < 0.0 || sensitivity > 1.0) {
            logger_.log(LogLevel::WARN, "Simulation parameters validation failed: sensitivity out of range");
            return false;
        }
    }

    if (params.contains("impact_threshold")) {
        if (!params["impact_threshold"].is_number()) {
            logger_.log(LogLevel::WARN, "Simulation parameters validation failed: impact_threshold must be numeric");
            return false;
        }
        double threshold = params["impact_threshold"].get<double>();
        if (threshold < 0.0) {
            logger_.log(LogLevel::WARN, "Simulation parameters validation failed: impact_threshold cannot be negative");
            return false;
        }
    }

    if (params.contains("max_iterations")) {
        if (!params["max_iterations"].is_number_integer()) {
            logger_.log(LogLevel::WARN, "Simulation parameters validation failed: max_iterations must be integer");
            return false;
        }
        int max_iterations = params["max_iterations"].get<int>();
        if (max_iterations <= 0 || max_iterations > 10000) {
            logger_.log(LogLevel::WARN, "Simulation parameters validation failed: max_iterations out of range");
            return false;
        }
    }

    if (params.contains("confidence_threshold")) {
        if (!params["confidence_threshold"].is_number()) {
            logger_.log(LogLevel::WARN, "Simulation parameters validation failed: confidence_threshold must be numeric");
            return false;
        }
        double confidence = params["confidence_threshold"].get<double>();
        if (confidence < 0.0 || confidence > 1.0) {
            logger_.log(LogLevel::WARN, "Simulation parameters validation failed: confidence_threshold out of range");
            return false;
        }
    }

    return true;
}

std::map<std::string, std::string> SimulatorAPIHandlers::parse_query_parameters(const std::map<std::string, std::string>& query_params) {
    std::map<std::string, std::string> normalized;

    for (const auto& [key, value] : query_params) {
        if (value.empty()) {
            continue;
        }

        if (key == "scenario_type" || key == "status" || key == "search" || key == "tag") {
            normalized[key] = value;
        } else if (key == "is_template") {
            std::string lower = value;
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (lower == "true" || lower == "false") {
                normalized[key] = lower;
            }
        } else if (key == "sort_by") {
            normalized[key] = value;
        } else if (key == "sort_direction") {
            normalized[key] = value;
        } else if (key == "time_range") {
            normalized[key] = value;
        }
    }

    return normalized;
}

std::string SimulatorAPIHandlers::extract_user_id_from_scenario(const std::string& scenario_id) {
    try {
        auto row = db_conn_->execute_query_single(
            "SELECT created_by FROM simulation_scenarios WHERE scenario_id = $1",
            {scenario_id}
        );

        if (row) {
            return row->value("created_by", "");
        }

    } catch (const std::exception& e) {
        logger_.log(LogLevel::ERROR, "Failed to extract user from scenario: " + std::string(e.what()));
    }

    return "";
}

std::string SimulatorAPIHandlers::extract_user_id_from_execution(const std::string& execution_id) {
    try {
        auto row = db_conn_->execute_query_single(
            "SELECT user_id FROM simulation_executions WHERE execution_id = $1",
            {execution_id}
        );

        if (row) {
            return row->value("user_id", "");
        }

    } catch (const std::exception& e) {
        logger_.log(LogLevel::ERROR, "Failed to extract user from execution: " + std::string(e.what()));
    }

    return "";
}

bool SimulatorAPIHandlers::check_simulation_rate_limit(const std::string& user_id) {
    try {
        auto row = db_conn_->execute_query_single(
            "SELECT COUNT(*) AS recent_runs FROM simulation_executions WHERE user_id = $1 AND created_at >= NOW() - INTERVAL '10 minutes'",
            {user_id}
        );

        if (row && row->contains("recent_runs")) {
            int recent_runs = 0;
            try {
                recent_runs = std::stoi(row->value("recent_runs", "0"));
            } catch (...) {
                recent_runs = 0;
            }
            if (recent_runs >= 12) {
                return false;
            }
        }

        return true;

    } catch (const std::exception& e) {
        logger_.log(LogLevel::WARN, "Simulation rate limit check failed: " + std::string(e.what()));
        return true;
    }
}

void SimulatorAPIHandlers::record_simulation_attempt(const std::string& user_id) {
    try {
        nlohmann::json parameters = {{"user_id", user_id}};
        nlohmann::json result = {{"action", "simulation_attempt"}};

        db_conn_->execute_command(
            "INSERT INTO tool_usage_logs (tool_name, parameters, result, success, execution_time_ms) "
            "VALUES ($1, $2::jsonb, $3::jsonb, $4::boolean, $5)",
            {"simulation_engine", parameters.dump(), result.dump(), "true", "0"}
        );

    } catch (const std::exception& e) {
        logger_.log(LogLevel::WARN, "Failed to record simulation attempt: " + std::string(e.what()));
    }
}

std::optional<nlohmann::json> SimulatorAPIHandlers::get_cached_analytics(const std::string& cache_key, const std::string& user_id) {
    std::lock_guard<std::mutex> lock(g_simulator_cache_mutex);
    auto it = g_simulator_cache.find(cache_key + ':' + user_id);
    if (it == g_simulator_cache.end()) {
        return std::nullopt;
    }

    if (std::chrono::steady_clock::now() > it->second.expires_at) {
        g_simulator_cache.erase(it);
        return std::nullopt;
    }

    return it->second.payload;
}

void SimulatorAPIHandlers::cache_analytics_result(const std::string& cache_key, const std::string& user_id, const nlohmann::json& data, int ttl_seconds) {
    std::lock_guard<std::mutex> lock(g_simulator_cache_mutex);
    SimulatorCacheEntry entry;
    entry.payload = data;
    entry.expires_at = std::chrono::steady_clock::now() + std::chrono::seconds(ttl_seconds > 0 ? ttl_seconds : kSimulatorAnalyticsTtl.count());
    g_simulator_cache[cache_key + ':' + user_id] = std::move(entry);
}

void SimulatorAPIHandlers::record_api_metrics(const std::string& endpoint, const std::string& user_id, double response_time_ms, bool success) {
    try {
        db_conn_->execute_command(
            "INSERT INTO api_metrics_logs (endpoint, user_id, response_time_ms, success, recorded_at) VALUES ($1, $2, $3, $4, NOW())",
            {endpoint, user_id, std::to_string(response_time_ms), success ? "true" : "false"}
        );
    } catch (const std::exception& e) {
        logger_.log(LogLevel::WARN, "Failed to record API metrics: " + std::string(e.what()));
    }
}

void SimulatorAPIHandlers::update_scenario_usage_stats(const std::string& scenario_id) {
    try {
        db_conn_->execute_command(
            "UPDATE simulation_scenarios SET usage_count = COALESCE(usage_count, 0) + 1, last_used_at = NOW() WHERE scenario_id = $1",
            {scenario_id}
        );
    } catch (const std::exception& e) {
        logger_.log(LogLevel::WARN, "Failed to update scenario usage stats: " + std::string(e.what()));
    }
}

} // namespace simulator
} // namespace regulens
