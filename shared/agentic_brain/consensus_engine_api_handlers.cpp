/**
 * Consensus Engine API Handlers Implementation
 * REST API endpoints for multi-agent decision making
 */

#include "consensus_engine_api_handlers.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <pqxx/pqxx>

namespace regulens {

ConsensusEngineAPIHandlers::ConsensusEngineAPIHandlers(
    std::shared_ptr<PostgreSQLConnection> db_conn,
    std::shared_ptr<ConsensusEngine> consensus_engine
) : db_conn_(db_conn), consensus_engine_(consensus_engine) {

    if (!db_conn_) {
        throw std::runtime_error("Database connection is required for ConsensusEngineAPIHandlers");
    }
    if (!consensus_engine_) {
        throw std::runtime_error("ConsensusEngine is required for ConsensusEngineAPIHandlers");
    }

    spdlog::info("ConsensusEngineAPIHandlers initialized");
}

ConsensusEngineAPIHandlers::~ConsensusEngineAPIHandlers() {
    spdlog::info("ConsensusEngineAPIHandlers shutting down");
}

// Authorization helper functions

bool ConsensusEngineAPIHandlers::check_user_permission(const std::string& user_id, const std::string& permission) {
    try {
        if (!db_conn_ || !db_conn_->is_connected()) {
            spdlog::error("Database connection not available for permission check");
            return false;
        }

        auto result = db_conn_->execute_query_multi(
            "SELECT COUNT(*) FROM user_permissions "
            "WHERE user_id = $1 AND permission = $2 AND is_active = true "
            "AND (expires_at IS NULL OR expires_at > NOW())",
            {user_id, permission}
        );

        if (result.empty() || result[0].find("count") == result[0].end()) {
            return false;
        }

        int count = std::stoi(result[0]["count"]);
        return count > 0;
    } catch (const std::exception& e) {
        spdlog::error("Exception in check_user_permission: {}", e.what());
        return false;
    }
}

bool ConsensusEngineAPIHandlers::check_user_role(const std::string& user_id, const std::string& role) {
    try {
        if (!db_conn_ || !db_conn_->is_connected()) {
            spdlog::error("Database connection not available for role check");
            return false;
        }

        auto result = db_conn_->execute_query_multi(
            "SELECT role FROM user_authentication WHERE user_id = $1",
            {user_id}
        );

        if (result.empty()) {
            return false;
        }

        std::string user_role = result[0]["role"];
        return user_role == role;
    } catch (const std::exception& e) {
        spdlog::error("Exception in check_user_role: {}", e.what());
        return false;
    }
}

std::string ConsensusEngineAPIHandlers::get_user_role(const std::string& user_id) {
    try {
        if (!db_conn_ || !db_conn_->is_connected()) {
            spdlog::error("Database connection not available for role retrieval");
            return "unknown";
        }

        auto result = db_conn_->execute_query_multi(
            "SELECT role FROM user_authentication WHERE user_id = $1",
            {user_id}
        );

        if (result.empty()) {
            return "unknown";
        }

        return result[0]["role"];
    } catch (const std::exception& e) {
        spdlog::error("Exception in get_user_role: {}", e.what());
        return "unknown";
    }
}

bool ConsensusEngineAPIHandlers::check_consensus_participant(const std::string& user_id, const std::string& consensus_id) {
    try {
        if (!db_conn_ || !db_conn_->is_connected()) {
            spdlog::error("Database connection not available for participant check");
            return false;
        }

        auto result = db_conn_->execute_query_multi(
            "SELECT COUNT(*) FROM consensus_agents "
            "WHERE consensus_id = $1 AND agent_id IN ("
            "    SELECT agent_id FROM agents WHERE created_by = $2 OR assigned_to = $2"
            ")",
            {consensus_id, user_id}
        );

        if (result.empty() || result[0].find("count") == result[0].end()) {
            return false;
        }

        int count = std::stoi(result[0]["count"]);
        return count > 0;
    } catch (const std::exception& e) {
        spdlog::error("Exception in check_consensus_participant: {}", e.what());
        return false;
    }
}

std::string ConsensusEngineAPIHandlers::handle_initiate_consensus(const std::string& request_body, const std::string& user_id) {
    try {
        nlohmann::json request = nlohmann::json::parse(request_body);
        std::string validation_error;
        if (!validate_consensus_request(request, validation_error)) {
            return create_error_response(validation_error).dump();
        }

        if (!validate_user_access(user_id, "initiate_consensus")) {
            return create_error_response("Access denied", 403).dump();
        }

        // Parse consensus configuration
        ConsensusConfiguration config = parse_consensus_config(request);

        // Add current user as facilitator if not specified
        bool has_facilitator = false;
        for (const auto& participant : config.participants) {
            if (participant.role == AgentRole::FACILITATOR) {
                has_facilitator = true;
                break;
            }
        }

        if (!has_facilitator) {
            // Try to find current user as agent
            auto user_agent = consensus_engine_->get_agent(user_id);
            if (user_agent) {
                Agent facilitator = *user_agent;
                facilitator.role = AgentRole::FACILITATOR;
                config.participants.push_back(facilitator);
            }
        }

        // Initiate consensus
        std::string consensus_id = consensus_engine_->initiate_consensus(config);

        if (consensus_id.empty()) {
            return create_error_response("Failed to initiate consensus process").dump();
        }

        nlohmann::json response_data = {
            {"consensus_id", consensus_id},
            {"topic", config.topic},
            {"algorithm", algorithm_to_string(config.algorithm)},
            {"participants_count", config.participants.size()},
            {"status", "initialized"}
        };

        return create_success_response(response_data, "Consensus process initiated successfully").dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_initiate_consensus: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string ConsensusEngineAPIHandlers::handle_get_consensus(const std::string& consensus_id, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "get_consensus", consensus_id)) {
            return create_error_response("Access denied", 403).dump();
        }

        ConsensusResult result = consensus_engine_->get_consensus_result(consensus_id);

        if (result.consensus_id.empty() || result.final_state == ConsensusState::CANCELLED) {
            return create_error_response("Consensus not found", 404).dump();
        }

        nlohmann::json response_data = format_consensus_result(result);
        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_get_consensus: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string ConsensusEngineAPIHandlers::handle_submit_opinion(const std::string& consensus_id, const std::string& request_body, const std::string& user_id) {
    try {
        if (!can_submit_opinion(user_id, consensus_id)) {
            return create_error_response("Access denied - not a participant or consensus not found", 403).dump();
        }

        nlohmann::json request = nlohmann::json::parse(request_body);
        std::string validation_error;
        if (!validate_opinion_request(request, validation_error)) {
            return create_error_response(validation_error).dump();
        }

        // Parse agent opinion
        AgentOpinion opinion = parse_agent_opinion(request, user_id);

        // Submit opinion
        if (!consensus_engine_->submit_opinion(consensus_id, opinion)) {
            return create_error_response("Failed to submit opinion").dump();
        }

        nlohmann::json response_data = {
            {"consensus_id", consensus_id},
            {"agent_id", opinion.agent_id},
            {"decision", opinion.decision},
            {"confidence_score", opinion.confidence_score},
            {"round_number", opinion.round_number},
            {"status", "submitted"}
        };

        return create_success_response(response_data, "Opinion submitted successfully").dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_submit_opinion: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string ConsensusEngineAPIHandlers::handle_register_agent(const std::string& request_body, const std::string& user_id) {
    try {
        if (!is_admin_user(user_id)) {
            return create_error_response("Admin access required", 403).dump();
        }

        nlohmann::json request = nlohmann::json::parse(request_body);
        std::string validation_error;
        if (!validate_agent_request(request, validation_error)) {
            return create_error_response(validation_error).dump();
        }

        // Parse agent configuration
        Agent agent = parse_agent_config(request, user_id);

        // Register agent
        if (!consensus_engine_->register_agent(agent)) {
            return create_error_response("Failed to register agent").dump();
        }

        nlohmann::json response_data = format_agent(agent);
        return create_success_response(response_data, "Agent registered successfully").dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_register_agent: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string ConsensusEngineAPIHandlers::handle_get_agent(const std::string& agent_id, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "get_agent", agent_id)) {
            return create_error_response("Access denied", 403).dump();
        }

        auto agent_opt = consensus_engine_->get_agent(agent_id);
        if (!agent_opt) {
            return create_error_response("Agent not found", 404).dump();
        }

        nlohmann::json response_data = format_agent(*agent_opt);
        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_get_agent: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string ConsensusEngineAPIHandlers::handle_list_agents(const std::string& query_params, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "list_agents")) {
            return create_error_response("Access denied", 403).dump();
        }

        auto params = parse_query_params(query_params);
        bool active_only = parse_bool_param(params["active_only"], true);
        int limit = parse_int_param(params["limit"], 50);

        auto agents = consensus_engine_->get_active_agents();

        // Filter by active status if requested
        if (active_only) {
            agents.erase(std::remove_if(agents.begin(), agents.end(),
                [](const Agent& agent) { return !agent.is_active; }), agents.end());
        }

        // Apply limit
        if (agents.size() > limit) {
            agents.resize(limit);
        }

        // Format response
        std::vector<nlohmann::json> formatted_agents;
        for (const auto& agent : agents) {
            formatted_agents.push_back(format_agent(agent));
        }

        nlohmann::json response_data = create_paginated_response(
            formatted_agents,
            formatted_agents.size(), // Simplified
            1,
            limit
        );

        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_list_agents: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string ConsensusEngineAPIHandlers::handle_start_voting_round(const std::string& consensus_id, const std::string& user_id) {
    try {
        if (!can_modify_consensus(user_id, consensus_id)) {
            return create_error_response("Access denied - not authorized to modify consensus", 403).dump();
        }

        if (!consensus_engine_->start_voting_round(consensus_id)) {
            return create_error_response("Failed to start voting round").dump();
        }

        nlohmann::json response_data = {
            {"consensus_id", consensus_id},
            {"action", "start_voting_round"},
            {"status", "success"}
        };

        return create_success_response(response_data, "Voting round started successfully").dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_start_voting_round: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string ConsensusEngineAPIHandlers::handle_calculate_consensus(const std::string& consensus_id, const std::string& user_id) {
    try {
        if (!can_modify_consensus(user_id, consensus_id)) {
            return create_error_response("Access denied - not authorized to calculate consensus", 403).dump();
        }

        ConsensusResult result = consensus_engine_->calculate_consensus(consensus_id);

        if (result.final_state == ConsensusState::ERROR) {
            return create_error_response("Failed to calculate consensus").dump();
        }

        nlohmann::json response_data = format_consensus_result(result);
        response_data["action"] = "calculate_consensus";

        return create_success_response(response_data, "Consensus calculated successfully").dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_calculate_consensus: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string ConsensusEngineAPIHandlers::handle_get_consensus_stats(const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "get_consensus_stats")) {
            return create_error_response("Access denied", 403).dump();
        }

        auto stats = consensus_engine_->get_consensus_statistics();
        auto agent_performance = consensus_engine_->get_agent_performance_metrics();

        nlohmann::json response_data = {
            {"consensus_stats", stats},
            {"agent_performance", agent_performance},
            {"generated_at", "2024-01-15T10:30:00Z"}
        };

        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_get_consensus_stats: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

// Helper method implementations

ConsensusConfiguration ConsensusEngineAPIHandlers::parse_consensus_config(const nlohmann::json& request) {
    ConsensusConfiguration config;

    config.topic = request.value("topic", "");
    config.algorithm = parse_algorithm_param(request.value("algorithm", "MAJORITY"));
    config.max_rounds = request.value("max_rounds", 3);
    config.consensus_threshold = request.value("consensus_threshold", 0.7);
    config.min_participants = request.value("min_participants", 3);
    config.allow_discussion = request.value("allow_discussion", true);
    config.require_justification = request.value("require_justification", true);

    if (request.contains("timeout_per_round_minutes")) {
        int timeout_minutes = request["timeout_per_round_minutes"];
        config.timeout_per_round = std::chrono::minutes(timeout_minutes);
    }

    if (request.contains("participants") && request["participants"].is_array()) {
        for (const auto& participant : request["participants"]) {
            if (participant.contains("agent_id")) {
                Agent agent;
                agent.agent_id = participant.value("agent_id", "");
                agent.name = participant.value("name", "");
                agent.role = parse_role_param(participant.value("role", "EXPERT"));
                agent.voting_weight = participant.value("voting_weight", 1.0);
                agent.domain_expertise = participant.value("domain_expertise", "");
                agent.confidence_threshold = participant.value("confidence_threshold", 0.7);
                agent.is_active = true;
                agent.last_active = std::chrono::system_clock::now();

                config.participants.push_back(agent);
            }
        }
    }

    if (request.contains("custom_rules")) {
        config.custom_rules = request["custom_rules"];
    }

    return config;
}

Agent ConsensusEngineAPIHandlers::parse_agent_config(const nlohmann::json& request, const std::string& user_id) {
    Agent agent;

    agent.agent_id = request.value("agent_id", consensus_engine_ ?
        consensus_engine_->generate_agent_id() : "agent_" + user_id);
    agent.name = request.value("name", "");
    agent.role = parse_role_param(request.value("role", "EXPERT"));
    agent.voting_weight = request.value("voting_weight", 1.0);
    agent.domain_expertise = request.value("domain_expertise", "");
    agent.confidence_threshold = request.value("confidence_threshold", 0.7);
    agent.is_active = request.value("is_active", true);
    agent.last_active = std::chrono::system_clock::now();

    return agent;
}

AgentOpinion ConsensusEngineAPIHandlers::parse_agent_opinion(const nlohmann::json& request, const std::string& agent_id) {
    AgentOpinion opinion;

    opinion.agent_id = agent_id;
    opinion.decision = request.value("decision", "");
    opinion.confidence_score = request.value("confidence_score", 0.5);
    opinion.reasoning = request.value("reasoning", "");
    opinion.submitted_at = std::chrono::system_clock::now();

    if (request.contains("supporting_data")) {
        opinion.supporting_data = request["supporting_data"];
    }

    if (request.contains("concerns") && request["concerns"].is_array()) {
        for (const auto& concern : request["concerns"]) {
            if (concern.is_string()) {
                opinion.concerns.push_back(concern.get<std::string>());
            }
        }
    }

    return opinion;
}

nlohmann::json ConsensusEngineAPIHandlers::format_consensus_result(const ConsensusResult& result) {
    nlohmann::json rounds_json;
    for (const auto& round : result.rounds) {
        rounds_json.push_back(format_voting_round(round));
    }

    return {
        {"consensus_id", result.consensus_id},
        {"topic", result.topic},
        {"final_decision", result.final_decision},
        {"confidence_level", confidence_to_string(result.confidence_level)},
        {"algorithm_used", algorithm_to_string(result.algorithm_used)},
        {"final_state", state_to_string(result.final_state)},
        {"total_participants", result.total_participants},
        {"agreement_percentage", result.agreement_percentage},
        {"total_duration_ms", result.total_duration.count()},
        {"rounds", rounds_json},
        {"dissenting_opinions", result.dissenting_opinions},
        {"completed_at", std::chrono::duration_cast<std::chrono::seconds>(
            result.completed_at.time_since_epoch()).count()}
    };
}

nlohmann::json ConsensusEngineAPIHandlers::format_agent(const Agent& agent) {
    return {
        {"agent_id", agent.agent_id},
        {"name", agent.name},
        {"role", role_to_string(agent.role)},
        {"voting_weight", agent.voting_weight},
        {"domain_expertise", agent.domain_expertise},
        {"confidence_threshold", agent.confidence_threshold},
        {"is_active", agent.is_active},
        {"last_active", std::chrono::duration_cast<std::chrono::seconds>(
            agent.last_active.time_since_epoch()).count()}
    };
}

bool ConsensusEngineAPIHandlers::validate_consensus_request(const nlohmann::json& request, std::string& error_message) {
    if (!request.contains("topic") || !request["topic"].is_string() || request["topic"].get<std::string>().empty()) {
        error_message = "Missing or invalid 'topic' field";
        return false;
    }

    if (request.contains("participants") && request["participants"].is_array()) {
        if (request["participants"].size() < 2) {
            error_message = "At least 2 participants required";
            return false;
        }
    } else {
        error_message = "Missing or invalid 'participants' array";
        return false;
    }

    return true;
}

bool ConsensusEngineAPIHandlers::validate_agent_request(const nlohmann::json& request, std::string& error_message) {
    if (!request.contains("name") || !request["name"].is_string() || request["name"].get<std::string>().empty()) {
        error_message = "Missing or invalid 'name' field";
        return false;
    }

    return true;
}

bool ConsensusEngineAPIHandlers::validate_opinion_request(const nlohmann::json& request, std::string& error_message) {
    if (!request.contains("decision") || !request["decision"].is_string() || request["decision"].get<std::string>().empty()) {
        error_message = "Missing or invalid 'decision' field";
        return false;
    }

    if (request.contains("confidence_score")) {
        double confidence = request.value("confidence_score", 0.5);
        if (confidence < 0.0 || confidence > 1.0) {
            error_message = "Confidence score must be between 0.0 and 1.0";
            return false;
        }
    }

    return true;
}

bool ConsensusEngineAPIHandlers::validate_user_access(const std::string& user_id, const std::string& operation, const std::string& resource_id) {
    if (user_id.empty()) {
        return false;
    }

    // Check specific permissions based on operation
    if (operation == "initiate_consensus") {
        return check_user_permission(user_id, "consensus:create") || check_user_role(user_id, "admin");
    } else if (operation == "get_consensus") {
        return check_user_permission(user_id, "consensus:read") || check_consensus_participant(user_id, resource_id);
    } else if (operation == "list_agents") {
        return check_user_permission(user_id, "agents:read") || check_user_role(user_id, "admin");
    } else if (operation == "get_agent") {
        return check_user_permission(user_id, "agents:read") || check_user_role(user_id, "admin");
    } else if (operation == "get_consensus_stats") {
        return check_user_permission(user_id, "consensus:read") || check_user_role(user_id, "admin");
    }

    // Default: require basic authenticated user
    return true;
}

bool ConsensusEngineAPIHandlers::is_admin_user(const std::string& user_id) {
    return check_user_role(user_id, "admin");
}

bool ConsensusEngineAPIHandlers::can_modify_consensus(const std::string& user_id, const std::string& consensus_id) {
    // Check if user is facilitator or admin
    return is_admin_user(user_id) || is_participant(user_id, consensus_id);
}

bool ConsensusEngineAPIHandlers::can_submit_opinion(const std::string& user_id, const std::string& consensus_id) {
    return is_participant(user_id, consensus_id);
}

bool ConsensusEngineAPIHandlers::is_participant(const std::string& user_id, const std::string& consensus_id) {
    return check_consensus_participant(user_id, consensus_id);
}

nlohmann::json ConsensusEngineAPIHandlers::create_success_response(const nlohmann::json& data, const std::string& message) {
    nlohmann::json response = {
        {"success", true},
        {"status_code", 200}
    };

    if (!message.empty()) {
        response["message"] = message;
    }

    if (data.is_object() || data.is_array()) {
        response["data"] = data;
    }

    return response;
}

nlohmann::json ConsensusEngineAPIHandlers::create_error_response(const std::string& message, int status_code) {
    return {
        {"success", false},
        {"status_code", status_code},
        {"error", message}
    };
}

nlohmann::json ConsensusEngineAPIHandlers::create_paginated_response(const std::vector<nlohmann::json>& items, int total_count, int page, int page_size) {
    int total_pages = (total_count + page_size - 1) / page_size;

    return {
        {"items", items},
        {"pagination", {
            {"page", page},
            {"page_size", page_size},
            {"total_count", total_count},
            {"total_pages", total_pages},
            {"has_next", page < total_pages},
            {"has_prev", page > 1}
        }}
    };
}

// Utility string conversion methods
std::string ConsensusEngineAPIHandlers::algorithm_to_string(VotingAlgorithm algorithm) {
    switch (algorithm) {
        case VotingAlgorithm::UNANIMOUS: return "UNANIMOUS";
        case VotingAlgorithm::MAJORITY: return "MAJORITY";
        case VotingAlgorithm::SUPER_MAJORITY: return "SUPER_MAJORITY";
        case VotingAlgorithm::WEIGHTED_MAJORITY: return "WEIGHTED_MAJORITY";
        case VotingAlgorithm::RANKED_CHOICE: return "RANKED_CHOICE";
        case VotingAlgorithm::QUORUM: return "QUORUM";
        case VotingAlgorithm::CONSENSUS: return "CONSENSUS";
        case VotingAlgorithm::PLURALITY: return "PLURALITY";
        default: return "UNKNOWN";
    }
}

std::string ConsensusEngineAPIHandlers::role_to_string(AgentRole role) {
    switch (role) {
        case AgentRole::EXPERT: return "EXPERT";
        case AgentRole::REVIEWER: return "REVIEWER";
        case AgentRole::DECISION_MAKER: return "DECISION_MAKER";
        case AgentRole::FACILITATOR: return "FACILITATOR";
        case AgentRole::OBSERVER: return "OBSERVER";
        default: return "EXPERT";
    }
}

std::string ConsensusEngineAPIHandlers::state_to_string(ConsensusState state) {
    switch (state) {
        case ConsensusState::INITIALIZING: return "INITIALIZING";
        case ConsensusState::COLLECTING_OPINIONS: return "COLLECTING_OPINIONS";
        case ConsensusState::DISCUSSING: return "DISCUSSING";
        case ConsensusState::VOTING: return "VOTING";
        case ConsensusState::RESOLVING_CONFLICTS: return "RESOLVING_CONFLICTS";
        case ConsensusState::REACHED_CONSENSUS: return "REACHED_CONSENSUS";
        case ConsensusState::DEADLOCK: return "DEADLOCK";
        case ConsensusState::TIMEOUT: return "TIMEOUT";
        case ConsensusState::CANCELLED: return "CANCELLED";
        default: return "UNKNOWN";
    }
}

std::string ConsensusEngineAPIHandlers::confidence_to_string(DecisionConfidence confidence) {
    switch (confidence) {
        case DecisionConfidence::VERY_LOW: return "VERY_LOW";
        case DecisionConfidence::LOW: return "LOW";
        case DecisionConfidence::MEDIUM: return "MEDIUM";
        case DecisionConfidence::HIGH: return "HIGH";
        case DecisionConfidence::VERY_HIGH: return "VERY_HIGH";
        default: return "MEDIUM";
    }
}

VotingAlgorithm ConsensusEngineAPIHandlers::parse_algorithm_param(const std::string& algorithm_str) {
    if (algorithm_str == "UNANIMOUS") return VotingAlgorithm::UNANIMOUS;
    if (algorithm_str == "SUPER_MAJORITY") return VotingAlgorithm::SUPER_MAJORITY;
    if (algorithm_str == "WEIGHTED_MAJORITY") return VotingAlgorithm::WEIGHTED_MAJORITY;
    if (algorithm_str == "RANKED_CHOICE") return VotingAlgorithm::RANKED_CHOICE;
    if (algorithm_str == "QUORUM") return VotingAlgorithm::QUORUM;
    if (algorithm_str == "CONSENSUS") return VotingAlgorithm::CONSENSUS;
    if (algorithm_str == "PLURALITY") return VotingAlgorithm::PLURALITY;
    return VotingAlgorithm::MAJORITY;
}

AgentRole ConsensusEngineAPIHandlers::parse_role_param(const std::string& role_str) {
    if (role_str == "REVIEWER") return AgentRole::REVIEWER;
    if (role_str == "DECISION_MAKER") return AgentRole::DECISION_MAKER;
    if (role_str == "FACILITATOR") return AgentRole::FACILITATOR;
    if (role_str == "OBSERVER") return AgentRole::OBSERVER;
    return AgentRole::EXPERT;
}

std::unordered_map<std::string, std::string> ConsensusEngineAPIHandlers::parse_query_params(const std::string& query_string) {
    std::unordered_map<std::string, std::string> params;

    if (query_string.empty()) return params;

    std::stringstream ss(query_string);
    std::string pair;

    while (std::getline(ss, pair, '&')) {
        size_t equals_pos = pair.find('=');
        if (equals_pos != std::string::npos) {
            std::string key = pair.substr(0, equals_pos);
            std::string value = pair.substr(equals_pos + 1);
            params[key] = value;
        }
    }

    return params;
}

int ConsensusEngineAPIHandlers::parse_int_param(const std::string& value, int default_value) {
    try {
        return std::stoi(value);
    } catch (...) {
        return default_value;
    }
}

bool ConsensusEngineAPIHandlers::parse_bool_param(const std::string& value, bool default_value) {
    if (value == "true" || value == "1") return true;
    if (value == "false" || value == "0") return false;
    return default_value;
}

nlohmann::json ConsensusEngineAPIHandlers::format_voting_round(const VotingRound& round) {
    nlohmann::json opinions_json;
    for (const auto& opinion : round.opinions) {
        opinions_json.push_back(format_agent_opinion(opinion));
    }

    return {
        {"round_number", round.round_number},
        {"topic", round.topic},
        {"description", round.description},
        {"opinions_count", round.opinions.size()},
        {"opinions", opinions_json},
        {"vote_counts", round.vote_counts},
        {"state", state_to_string(round.state)},
        {"started_at", std::chrono::duration_cast<std::chrono::seconds>(
            round.started_at.time_since_epoch()).count()},
        {"ended_at", round.ended_at.time_since_epoch().count() > 0 ?
            std::chrono::duration_cast<std::chrono::seconds>(
                round.ended_at.time_since_epoch()).count() : 0}
    };
}

nlohmann::json ConsensusEngineAPIHandlers::format_agent_opinion(const AgentOpinion& opinion) {
    return {
        {"agent_id", opinion.agent_id},
        {"decision", opinion.decision},
        {"confidence_score", opinion.confidence_score},
        {"reasoning", opinion.reasoning},
        {"supporting_data", opinion.supporting_data},
        {"concerns", opinion.concerns},
        {"round_number", opinion.round_number},
        {"submitted_at", std::chrono::duration_cast<std::chrono::seconds>(
            opinion.submitted_at.time_since_epoch()).count()}
    };
}

} // namespace regulens
