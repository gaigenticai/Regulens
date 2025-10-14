#include "consensus_engine_api_handlers.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace regulens {

ConsensusEngineAPIHandlers::ConsensusEngineAPIHandlers(
    std::shared_ptr<PostgreSQLConnection> db_conn,
    std::shared_ptr<ConsensusEngine> consensus_engine,
    std::shared_ptr<StructuredLogger> logger
) : db_conn_(db_conn), consensus_engine_(consensus_engine), logger_(logger) {

    if (!db_conn_) {
        throw std::runtime_error("Database connection is required for ConsensusEngineAPIHandlers");
    }

    if (!consensus_engine_) {
        throw std::runtime_error("ConsensusEngine is required for ConsensusEngineAPIHandlers");
    }

    if (!logger_) {
        throw std::runtime_error("Logger is required for ConsensusEngineAPIHandlers");
    }

    spdlog::info("ConsensusEngineAPIHandlers initialized");
}

ConsensusEngineAPIHandlers::~ConsensusEngineAPIHandlers() {
    spdlog::info("ConsensusEngineAPIHandlers shutting down");
}

// Consensus session management endpoints
std::string ConsensusEngineAPIHandlers::handle_initiate_consensus(const std::string& request_body, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "initiate_consensus")) {
            return create_error_response("Access denied", 403).dump();
        }

        nlohmann::json request_json = nlohmann::json::parse(request_body);
        std::string error_msg;

        if (!validate_consensus_config(request_json, error_msg)) {
            return create_error_response(error_msg, 400).dump();
        }

        ConsensusConfiguration config = parse_consensus_config_from_json(request_json);

        std::string consensus_id = consensus_engine_->initiate_consensus(config);

        nlohmann::json response_data;
        response_data["consensus_id"] = consensus_id;
        response_data["config"] = format_consensus_config_to_json(config);

        return create_success_response(response_data, "Consensus process initiated successfully").dump();

    } catch (const nlohmann::json::exception& e) {
        logger_->log("json_parsing_error", {{"error", e.what()}, {"endpoint", "initiate_consensus"}}, spdlog::level::error);
        return create_error_response("Invalid JSON format", 400).dump();
    } catch (const std::exception& e) {
        logger_->log("consensus_initiation_error", {{"error", e.what()}, {"user_id", user_id}}, spdlog::level::error);
        return create_error_response("Failed to initiate consensus: " + std::string(e.what()), 500).dump();
    }
}

std::string ConsensusEngineAPIHandlers::handle_get_consensus_result(const std::string& consensus_id, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "get_consensus_result") || !can_participate_in_consensus(user_id, consensus_id)) {
            return create_error_response("Access denied", 403).dump();
        }

        ConsensusResult result = consensus_engine_->get_consensus_result(consensus_id);

        if (!result.success && result.consensus_id.empty()) {
            return create_error_response("Consensus process not found", 404).dump();
        }

        return create_success_response(format_consensus_result_to_json(result), "Consensus result retrieved successfully").dump();

    } catch (const std::exception& e) {
        logger_->log("get_consensus_result_error", {{"error", e.what()}, {"consensus_id", consensus_id}}, spdlog::level::error);
        return create_error_response("Failed to retrieve consensus result", 500).dump();
    }
}

std::string ConsensusEngineAPIHandlers::handle_get_consensus_state(const std::string& consensus_id, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "get_consensus_state") || !can_participate_in_consensus(user_id, consensus_id)) {
            return create_error_response("Access denied", 403).dump();
        }

        ConsensusState state = consensus_engine_->get_consensus_state(consensus_id);

        nlohmann::json response_data;
        response_data["consensus_id"] = consensus_id;
        response_data["state"] = static_cast<int>(state);
        response_data["state_name"] = [state]() {
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
        }();

        return create_success_response(response_data, "Consensus state retrieved successfully").dump();

    } catch (const std::exception& e) {
        logger_->log("get_consensus_state_error", {{"error", e.what()}, {"consensus_id", consensus_id}}, spdlog::level::error);
        return create_error_response("Failed to retrieve consensus state", 500).dump();
    }
}

// Agent opinion management endpoints
std::string ConsensusEngineAPIHandlers::handle_submit_opinion(const std::string& consensus_id, const std::string& request_body, const std::string& user_id) {
    try {
        if (!can_participate_in_consensus(user_id, consensus_id)) {
            return create_error_response("Access denied - not a participant in this consensus", 403).dump();
        }

        nlohmann::json request_json = nlohmann::json::parse(request_body);
        std::string error_msg;

        if (!validate_agent_opinion(request_json, error_msg)) {
            return create_error_response(error_msg, 400).dump();
        }

        AgentOpinion opinion = parse_agent_opinion_from_json(request_json);
        opinion.agent_id = user_id; // Ensure the user can only submit opinions for themselves

        if (consensus_engine_->submit_opinion(consensus_id, opinion)) {
            return create_success_response(format_agent_opinion_to_json(opinion), "Opinion submitted successfully").dump();
        } else {
            return create_error_response("Failed to submit opinion - consensus may be closed or invalid", 400).dump();
        }

    } catch (const nlohmann::json::exception& e) {
        logger_->log("json_parsing_error", {{"error", e.what()}, {"endpoint", "submit_opinion"}}, spdlog::level::error);
        return create_error_response("Invalid JSON format", 400).dump();
    } catch (const std::exception& e) {
        logger_->log("submit_opinion_error", {{"error", e.what()}, {"consensus_id", consensus_id}}, spdlog::level::error);
        return create_error_response("Failed to submit opinion", 500).dump();
    }
}

std::string ConsensusEngineAPIHandlers::handle_get_agent_opinions(const std::string& consensus_id, const std::string& query_params, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "get_agent_opinions") || !can_participate_in_consensus(user_id, consensus_id)) {
            return create_error_response("Access denied", 403).dump();
        }

        // Parse query parameters
        int round_number = -1; // Default to all rounds
        // In a production system, you'd parse query_params for round_number filter

        std::vector<AgentOpinion> opinions = consensus_engine_->get_agent_opinions(consensus_id, round_number);

        nlohmann::json opinions_json = nlohmann::json::array();
        for (const auto& opinion : opinions) {
            opinions_json.push_back(format_agent_opinion_to_json(opinion));
        }

        nlohmann::json response_data;
        response_data["consensus_id"] = consensus_id;
        response_data["opinions"] = opinions_json;
        response_data["count"] = opinions.size();

        return create_success_response(response_data, "Agent opinions retrieved successfully").dump();

    } catch (const std::exception& e) {
        logger_->log("get_agent_opinions_error", {{"error", e.what()}, {"consensus_id", consensus_id}}, spdlog::level::error);
        return create_error_response("Failed to retrieve agent opinions", 500).dump();
    }
}

std::string ConsensusEngineAPIHandlers::handle_update_opinion(const std::string& consensus_id, const std::string& agent_id, const std::string& request_body, const std::string& user_id) {
    try {
        // Users can only update their own opinions, or admins can update any
        if (agent_id != user_id && !validate_user_access(user_id, "admin_update_opinion")) {
            return create_error_response("Access denied - can only update your own opinions", 403).dump();
        }

        if (!can_participate_in_consensus(user_id, consensus_id)) {
            return create_error_response("Access denied - not a participant in this consensus", 403).dump();
        }

        nlohmann::json request_json = nlohmann::json::parse(request_body);
        std::string error_msg;

        if (!validate_agent_opinion(request_json, error_msg)) {
            return create_error_response(error_msg, 400).dump();
        }

        AgentOpinion opinion = parse_agent_opinion_from_json(request_json);
        opinion.agent_id = agent_id; // Ensure we update the correct agent's opinion

        if (consensus_engine_->update_opinion(consensus_id, agent_id, opinion)) {
            return create_success_response(format_agent_opinion_to_json(opinion), "Opinion updated successfully").dump();
        } else {
            return create_error_response("Failed to update opinion - opinion not found or consensus closed", 404).dump();
        }

    } catch (const nlohmann::json::exception& e) {
        logger_->log("json_parsing_error", {{"error", e.what()}, {"endpoint", "update_opinion"}}, spdlog::level::error);
        return create_error_response("Invalid JSON format", 400).dump();
    } catch (const std::exception& e) {
        logger_->log("update_opinion_error", {{"error", e.what()}, {"consensus_id", consensus_id}}, spdlog::level::error);
        return create_error_response("Failed to update opinion", 500).dump();
    }
}

// Voting round management endpoints
std::string ConsensusEngineAPIHandlers::handle_start_voting_round(const std::string& consensus_id, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "start_voting_round")) {
            return create_error_response("Access denied", 403).dump();
        }

        if (consensus_engine_->start_voting_round(consensus_id)) {
            nlohmann::json response_data;
            response_data["consensus_id"] = consensus_id;
            response_data["action"] = "voting_round_started";
            return create_success_response(response_data, "Voting round started successfully").dump();
        } else {
            return create_error_response("Failed to start voting round - consensus not found or invalid state", 400).dump();
        }

    } catch (const std::exception& e) {
        logger_->log("start_voting_round_error", {{"error", e.what()}, {"consensus_id", consensus_id}}, spdlog::level::error);
        return create_error_response("Failed to start voting round", 500).dump();
    }
}

std::string ConsensusEngineAPIHandlers::handle_end_voting_round(const std::string& consensus_id, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "end_voting_round")) {
            return create_error_response("Access denied", 403).dump();
        }

        if (consensus_engine_->end_voting_round(consensus_id)) {
            nlohmann::json response_data;
            response_data["consensus_id"] = consensus_id;
            response_data["action"] = "voting_round_ended";
            return create_success_response(response_data, "Voting round ended successfully").dump();
        } else {
            return create_error_response("Failed to end voting round - consensus not found or invalid state", 400).dump();
        }

    } catch (const std::exception& e) {
        logger_->log("end_voting_round_error", {{"error", e.what()}, {"consensus_id", consensus_id}}, spdlog::level::error);
        return create_error_response("Failed to end voting round", 500).dump();
    }
}

std::string ConsensusEngineAPIHandlers::handle_calculate_consensus(const std::string& consensus_id, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "calculate_consensus")) {
            return create_error_response("Access denied", 403).dump();
        }

        ConsensusResult result = consensus_engine_->calculate_consensus(consensus_id);

        return create_success_response(format_consensus_result_to_json(result), "Consensus calculated successfully").dump();

    } catch (const std::exception& e) {
        logger_->log("calculate_consensus_error", {{"error", e.what()}, {"consensus_id", consensus_id}}, spdlog::level::error);
        return create_error_response("Failed to calculate consensus", 500).dump();
    }
}

// Agent management endpoints
std::string ConsensusEngineAPIHandlers::handle_register_agent(const std::string& request_body, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "register_agent")) {
            return create_error_response("Access denied", 403).dump();
        }

        nlohmann::json request_json = nlohmann::json::parse(request_body);
        std::string error_msg;

        if (!validate_agent_data(request_json, error_msg)) {
            return create_error_response(error_msg, 400).dump();
        }

        Agent agent = parse_agent_from_json(request_json);

        if (consensus_engine_->register_agent(agent)) {
            return create_success_response(format_agent_to_json(agent), "Agent registered successfully").dump();
        } else {
            return create_error_response("Failed to register agent", 500).dump();
        }

    } catch (const nlohmann::json::exception& e) {
        logger_->log("json_parsing_error", {{"error", e.what()}, {"endpoint", "register_agent"}}, spdlog::level::error);
        return create_error_response("Invalid JSON format", 400).dump();
    } catch (const std::exception& e) {
        logger_->log("register_agent_error", {{"error", e.what()}, {"user_id", user_id}}, spdlog::level::error);
        return create_error_response("Failed to register agent", 500).dump();
    }
}

std::string ConsensusEngineAPIHandlers::handle_update_agent(const std::string& agent_id, const std::string& request_body, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "update_agent")) {
            return create_error_response("Access denied", 403).dump();
        }

        nlohmann::json request_json = nlohmann::json::parse(request_body);
        std::string error_msg;

        if (!validate_agent_data(request_json, error_msg)) {
            return create_error_response(error_msg, 400).dump();
        }

        Agent agent = parse_agent_from_json(request_json);
        agent.agent_id = agent_id; // Ensure we update the correct agent

        if (consensus_engine_->update_agent(agent_id, agent)) {
            return create_success_response(format_agent_to_json(agent), "Agent updated successfully").dump();
        } else {
            return create_error_response("Failed to update agent - agent not found", 404).dump();
        }

    } catch (const nlohmann::json::exception& e) {
        logger_->log("json_parsing_error", {{"error", e.what()}, {"endpoint", "update_agent"}}, spdlog::level::error);
        return create_error_response("Invalid JSON format", 400).dump();
    } catch (const std::exception& e) {
        logger_->log("update_agent_error", {{"error", e.what()}, {"agent_id", agent_id}}, spdlog::level::error);
        return create_error_response("Failed to update agent", 500).dump();
    }
}

std::string ConsensusEngineAPIHandlers::handle_get_agent(const std::string& agent_id, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "get_agent")) {
            return create_error_response("Access denied", 403).dump();
        }

        auto agent_opt = consensus_engine_->get_agent(agent_id);

        if (agent_opt) {
            return create_success_response(format_agent_to_json(*agent_opt), "Agent retrieved successfully").dump();
        } else {
            return create_error_response("Agent not found", 404).dump();
        }

    } catch (const std::exception& e) {
        logger_->log("get_agent_error", {{"error", e.what()}, {"agent_id", agent_id}}, spdlog::level::error);
        return create_error_response("Failed to retrieve agent", 500).dump();
    }
}

std::string ConsensusEngineAPIHandlers::handle_get_active_agents(const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "get_active_agents")) {
            return create_error_response("Access denied", 403).dump();
        }

        std::vector<Agent> agents = consensus_engine_->get_active_agents();

        nlohmann::json agents_json = nlohmann::json::array();
        for (const auto& agent : agents) {
            agents_json.push_back(format_agent_to_json(agent));
        }

        nlohmann::json response_data;
        response_data["agents"] = agents_json;
        response_data["count"] = agents.size();

        return create_success_response(response_data, "Active agents retrieved successfully").dump();

    } catch (const std::exception& e) {
        logger_->log("get_active_agents_error", {{"error", e.what()}, {"user_id", user_id}}, spdlog::level::error);
        return create_error_response("Failed to retrieve active agents", 500).dump();
    }
}

std::string ConsensusEngineAPIHandlers::handle_deactivate_agent(const std::string& agent_id, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "deactivate_agent")) {
            return create_error_response("Access denied", 403).dump();
        }

        if (consensus_engine_->deactivate_agent(agent_id)) {
            nlohmann::json response_data;
            response_data["agent_id"] = agent_id;
            response_data["action"] = "deactivated";
            return create_success_response(response_data, "Agent deactivated successfully").dump();
        } else {
            return create_error_response("Failed to deactivate agent - agent not found", 404).dump();
        }

    } catch (const std::exception& e) {
        logger_->log("deactivate_agent_error", {{"error", e.what()}, {"agent_id", agent_id}}, spdlog::level::error);
        return create_error_response("Failed to deactivate agent", 500).dump();
    }
}

// Analytics and monitoring endpoints
std::string ConsensusEngineAPIHandlers::handle_get_consensus_statistics(const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "get_consensus_statistics")) {
            return create_error_response("Access denied", 403).dump();
        }

        std::unordered_map<std::string, int> stats = consensus_engine_->get_consensus_statistics();

        nlohmann::json stats_json;
        for (const auto& [key, value] : stats) {
            stats_json[key] = value;
        }

        return create_success_response(stats_json, "Consensus statistics retrieved successfully").dump();

    } catch (const std::exception& e) {
        logger_->log("get_consensus_statistics_error", {{"error", e.what()}, {"user_id", user_id}}, spdlog::level::error);
        return create_error_response("Failed to retrieve consensus statistics", 500).dump();
    }
}

std::string ConsensusEngineAPIHandlers::handle_get_agent_performance_metrics(const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "get_agent_performance_metrics")) {
            return create_error_response("Access denied", 403).dump();
        }

        std::vector<std::pair<std::string, double>> metrics = consensus_engine_->get_agent_performance_metrics();

        nlohmann::json metrics_json = nlohmann::json::array();
        for (const auto& [agent_id, score] : metrics) {
            nlohmann::json metric;
            metric["agent_id"] = agent_id;
            metric["performance_score"] = score;
            metrics_json.push_back(metric);
        }

        nlohmann::json response_data;
        response_data["metrics"] = metrics_json;
        response_data["count"] = metrics.size();

        return create_success_response(response_data, "Agent performance metrics retrieved successfully").dump();

    } catch (const std::exception& e) {
        logger_->log("get_agent_performance_metrics_error", {{"error", e.what()}, {"user_id", user_id}}, spdlog::level::error);
        return create_error_response("Failed to retrieve agent performance metrics", 500).dump();
    }
}

std::string ConsensusEngineAPIHandlers::handle_calculate_decision_accuracy(const std::string& consensus_id, const std::string& request_body, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "calculate_decision_accuracy")) {
            return create_error_response("Access denied", 403).dump();
        }

        nlohmann::json request_json = nlohmann::json::parse(request_body);

        if (!request_json.contains("actual_outcome") || !request_json["actual_outcome"].is_boolean()) {
            return create_error_response("Missing or invalid 'actual_outcome' field", 400).dump();
        }

        bool actual_outcome = request_json["actual_outcome"];

        double accuracy = consensus_engine_->calculate_decision_accuracy(consensus_id, actual_outcome);

        nlohmann::json response_data;
        response_data["consensus_id"] = consensus_id;
        response_data["accuracy"] = accuracy;
        response_data["actual_outcome"] = actual_outcome;

        return create_success_response(response_data, "Decision accuracy calculated successfully").dump();

    } catch (const nlohmann::json::exception& e) {
        logger_->log("json_parsing_error", {{"error", e.what()}, {"endpoint", "calculate_decision_accuracy"}}, spdlog::level::error);
        return create_error_response("Invalid JSON format", 400).dump();
    } catch (const std::exception& e) {
        logger_->log("calculate_decision_accuracy_error", {{"error", e.what()}, {"consensus_id", consensus_id}}, spdlog::level::error);
        return create_error_response("Failed to calculate decision accuracy", 500).dump();
    }
}

// Configuration endpoints
std::string ConsensusEngineAPIHandlers::handle_set_default_algorithm(const std::string& request_body, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "set_default_algorithm")) {
            return create_error_response("Access denied", 403).dump();
        }

        nlohmann::json request_json = nlohmann::json::parse(request_body);

        if (!request_json.contains("algorithm") || !request_json["algorithm"].is_string()) {
            return create_error_response("Missing or invalid 'algorithm' field", 400).dump();
        }

        std::string algorithm_str = request_json["algorithm"];
        VotingAlgorithm algorithm;

        if (algorithm_str == "UNANIMOUS") algorithm = VotingAlgorithm::UNANIMOUS;
        else if (algorithm_str == "MAJORITY") algorithm = VotingAlgorithm::MAJORITY;
        else if (algorithm_str == "WEIGHTED_MAJORITY") algorithm = VotingAlgorithm::WEIGHTED_MAJORITY;
        else if (algorithm_str == "RANKED_CHOICE") algorithm = VotingAlgorithm::RANKED_CHOICE;
        else if (algorithm_str == "QUORUM") algorithm = VotingAlgorithm::QUORUM;
        else return create_error_response("Invalid algorithm specified", 400).dump();

        consensus_engine_->set_default_algorithm(algorithm);

        nlohmann::json response_data;
        response_data["algorithm"] = algorithm_str;
        return create_success_response(response_data, "Default algorithm updated successfully").dump();

    } catch (const nlohmann::json::exception& e) {
        logger_->log("json_parsing_error", {{"error", e.what()}, {"endpoint", "set_default_algorithm"}}, spdlog::level::error);
        return create_error_response("Invalid JSON format", 400).dump();
    } catch (const std::exception& e) {
        logger_->log("set_default_algorithm_error", {{"error", e.what()}, {"user_id", user_id}}, spdlog::level::error);
        return create_error_response("Failed to set default algorithm", 500).dump();
    }
}

// Additional configuration endpoints
std::string ConsensusEngineAPIHandlers::handle_set_max_rounds(const std::string& request_body, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "set_max_rounds")) {
            return create_error_response("Access denied", 403).dump();
        }

        nlohmann::json request_json = nlohmann::json::parse(request_body);

        if (!request_json.contains("max_rounds") || !request_json["max_rounds"].is_number_integer()) {
            return create_error_response("Missing or invalid 'max_rounds' field", 400).dump();
        }

        int max_rounds = request_json["max_rounds"];
        if (max_rounds < 1 || max_rounds > 10) {
            return create_error_response("max_rounds must be between 1 and 10", 400).dump();
        }

        consensus_engine_->set_max_rounds(max_rounds);

        nlohmann::json response_data;
        response_data["max_rounds"] = max_rounds;
        return create_success_response(response_data, "Max rounds updated successfully").dump();

    } catch (const nlohmann::json::exception& e) {
        logger_->log("json_parsing_error", {{"error", e.what()}, {"endpoint", "set_max_rounds"}}, spdlog::level::error);
        return create_error_response("Invalid JSON format", 400).dump();
    } catch (const std::exception& e) {
        logger_->log("set_max_rounds_error", {{"error", e.what()}, {"user_id", user_id}}, spdlog::level::error);
        return create_error_response("Failed to set max rounds", 500).dump();
    }
}

std::string ConsensusEngineAPIHandlers::handle_set_timeout_per_round(const std::string& request_body, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "set_timeout_per_round")) {
            return create_error_response("Access denied", 403).dump();
        }

        nlohmann::json request_json = nlohmann::json::parse(request_body);

        if (!request_json.contains("timeout_minutes") || !request_json["timeout_minutes"].is_number_integer()) {
            return create_error_response("Missing or invalid 'timeout_minutes' field", 400).dump();
        }

        int timeout_minutes = request_json["timeout_minutes"];
        if (timeout_minutes < 1 || timeout_minutes > 1440) { // Max 24 hours
            return create_error_response("timeout_minutes must be between 1 and 1440", 400).dump();
        }

        consensus_engine_->set_timeout_per_round(std::chrono::minutes(timeout_minutes));

        nlohmann::json response_data;
        response_data["timeout_minutes"] = timeout_minutes;
        return create_success_response(response_data, "Timeout per round updated successfully").dump();

    } catch (const nlohmann::json::exception& e) {
        logger_->log("json_parsing_error", {{"error", e.what()}, {"endpoint", "set_timeout_per_round"}}, spdlog::level::error);
        return create_error_response("Invalid JSON format", 400).dump();
    } catch (const std::exception& e) {
        logger_->log("set_timeout_per_round_error", {{"error", e.what()}, {"user_id", user_id}}, spdlog::level::error);
        return create_error_response("Failed to set timeout per round", 500).dump();
    }
}

std::string ConsensusEngineAPIHandlers::handle_optimize_for_scenario(const std::string& request_body, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "optimize_for_scenario")) {
            return create_error_response("Access denied", 403).dump();
        }

        nlohmann::json request_json = nlohmann::json::parse(request_body);

        if (!request_json.contains("scenario") || !request_json["scenario"].is_string()) {
            return create_error_response("Missing or invalid 'scenario' field", 400).dump();
        }

        std::string scenario = request_json["scenario"];

        consensus_engine_->optimize_for_scenario(scenario);

        nlohmann::json response_data;
        response_data["scenario"] = scenario;
        response_data["optimization_applied"] = true;
        return create_success_response(response_data, "Consensus engine optimized for scenario successfully").dump();

    } catch (const nlohmann::json::exception& e) {
        logger_->log("json_parsing_error", {{"error", e.what()}, {"endpoint", "optimize_for_scenario"}}, spdlog::level::error);
        return create_error_response("Invalid JSON format", 400).dump();
    } catch (const std::exception& e) {
        logger_->log("optimize_for_scenario_error", {{"error", e.what()}, {"user_id", user_id}}, spdlog::level::error);
        return create_error_response("Failed to optimize for scenario", 500).dump();
    }
}

// Conflict resolution endpoints
std::string ConsensusEngineAPIHandlers::handle_identify_conflicts(const std::string& consensus_id, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "identify_conflicts") || !can_participate_in_consensus(user_id, consensus_id)) {
            return create_error_response("Access denied", 403).dump();
        }

        // Get opinions for conflict analysis
        std::vector<AgentOpinion> opinions = consensus_engine_->get_agent_opinions(consensus_id);

        if (opinions.empty()) {
            return create_error_response("No opinions available for conflict analysis", 400).dump();
        }

        std::vector<std::string> conflicts = consensus_engine_->identify_conflicts(opinions);

        nlohmann::json response_data;
        response_data["consensus_id"] = consensus_id;
        response_data["conflicts"] = conflicts;
        response_data["conflict_count"] = conflicts.size();

        return create_success_response(response_data, "Conflicts identified successfully").dump();

    } catch (const std::exception& e) {
        logger_->log("identify_conflicts_error", {{"error", e.what()}, {"consensus_id", consensus_id}}, spdlog::level::error);
        return create_error_response("Failed to identify conflicts", 500).dump();
    }
}

std::string ConsensusEngineAPIHandlers::handle_suggest_resolution_strategies(const std::string& consensus_id, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "suggest_resolution_strategies") || !can_participate_in_consensus(user_id, consensus_id)) {
            return create_error_response("Access denied", 403).dump();
        }

        // Get opinions for strategy suggestion
        std::vector<AgentOpinion> opinions = consensus_engine_->get_agent_opinions(consensus_id);

        if (opinions.empty()) {
            return create_error_response("No opinions available for strategy suggestion", 400).dump();
        }

        nlohmann::json strategies = consensus_engine_->suggest_resolution_strategies(opinions);

        nlohmann::json response_data;
        response_data["consensus_id"] = consensus_id;
        response_data["strategies"] = strategies;

        return create_success_response(response_data, "Resolution strategies suggested successfully").dump();

    } catch (const std::exception& e) {
        logger_->log("suggest_resolution_strategies_error", {{"error", e.what()}, {"consensus_id", consensus_id}}, spdlog::level::error);
        return create_error_response("Failed to suggest resolution strategies", 500).dump();
    }
}

std::string ConsensusEngineAPIHandlers::handle_resolve_conflict(const std::string& consensus_id, const std::string& request_body, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "resolve_conflict")) {
            return create_error_response("Access denied", 403).dump();
        }

        nlohmann::json request_json = nlohmann::json::parse(request_body);

        if (!request_json.contains("resolution_strategy") || !request_json["resolution_strategy"].is_string()) {
            return create_error_response("Missing or invalid 'resolution_strategy' field", 400).dump();
        }

        std::string resolution_strategy = request_json["resolution_strategy"];

        if (consensus_engine_->resolve_conflict(consensus_id, resolution_strategy)) {
            nlohmann::json response_data;
            response_data["consensus_id"] = consensus_id;
            response_data["resolution_strategy"] = resolution_strategy;
            return create_success_response(response_data, "Conflict resolved successfully").dump();
    } else {
            return create_error_response("Failed to resolve conflict - consensus not found", 404).dump();
        }

    } catch (const nlohmann::json::exception& e) {
        logger_->log("json_parsing_error", {{"error", e.what()}, {"endpoint", "resolve_conflict"}}, spdlog::level::error);
        return create_error_response("Invalid JSON format", 400).dump();
    } catch (const std::exception& e) {
        logger_->log("resolve_conflict_error", {{"error", e.what()}, {"consensus_id", consensus_id}}, spdlog::level::error);
        return create_error_response("Failed to resolve conflict", 500).dump();
    }
}

// Helper method implementations

bool ConsensusEngineAPIHandlers::validate_consensus_config(const nlohmann::json& config_json, std::string& error_message) {
    if (!config_json.contains("topic") || !config_json["topic"].is_string() || config_json["topic"].empty()) {
        error_message = "Topic is required and must be a non-empty string";
        return false;
    }

    if (!config_json.contains("participants") || !config_json["participants"].is_array() || config_json["participants"].empty()) {
        error_message = "Participants is required and must be a non-empty array of agent IDs";
        return false;
    }

    if (config_json.contains("algorithm")) {
        std::string algorithm = config_json["algorithm"];
        std::vector<std::string> valid_algorithms = {"UNANIMOUS", "MAJORITY", "WEIGHTED_MAJORITY", "RANKED_CHOICE", "QUORUM"};
        if (std::find(valid_algorithms.begin(), valid_algorithms.end(), algorithm) == valid_algorithms.end()) {
            error_message = "Invalid algorithm. Must be one of: UNANIMOUS, MAJORITY, WEIGHTED_MAJORITY, RANKED_CHOICE, QUORUM";
        return false;
        }
    }

    if (config_json.contains("consensus_threshold")) {
        double threshold = config_json["consensus_threshold"];
        if (threshold < 0.0 || threshold > 1.0) {
            error_message = "Consensus threshold must be between 0.0 and 1.0";
            return false;
        }
    }

    return true;
}

ConsensusConfiguration ConsensusEngineAPIHandlers::parse_consensus_config_from_json(const nlohmann::json& config_json) {
    ConsensusConfiguration config;

    config.topic = config_json["topic"];
    config.description = config_json.value("description", "");
    config.participants = config_json["participants"].get<std::vector<std::string>>();
    config.consensus_threshold = config_json.value("consensus_threshold", 0.7);
    config.max_rounds = config_json.value("max_rounds", 3);
    config.timeout_per_round_minutes = config_json.value("timeout_per_round_minutes", 10);
    config.custom_rules = config_json.value("custom_rules", nlohmann::json::object());

    // Parse algorithm
    std::string algorithm_str = config_json.value("algorithm", "MAJORITY");
    if (algorithm_str == "UNANIMOUS") config.algorithm = VotingAlgorithm::UNANIMOUS;
    else if (algorithm_str == "WEIGHTED_MAJORITY") config.algorithm = VotingAlgorithm::WEIGHTED_MAJORITY;
    else if (algorithm_str == "RANKED_CHOICE") config.algorithm = VotingAlgorithm::RANKED_CHOICE;
    else if (algorithm_str == "QUORUM") config.algorithm = VotingAlgorithm::QUORUM;
    else config.algorithm = VotingAlgorithm::MAJORITY;

    return config;
}

bool ConsensusEngineAPIHandlers::validate_agent_opinion(const nlohmann::json& opinion_json, std::string& error_message) {
    if (!opinion_json.contains("decision") || !opinion_json["decision"].is_string() || opinion_json["decision"].empty()) {
        error_message = "Decision is required and must be a non-empty string";
        return false;
    }

    if (!opinion_json.contains("confidence_score") || !opinion_json["confidence_score"].is_number()) {
        error_message = "Confidence score is required and must be a number";
        return false;
    }

    double confidence = opinion_json["confidence_score"];
    if (confidence < 0.0 || confidence > 1.0) {
        error_message = "Confidence score must be between 0.0 and 1.0";
        return false;
    }

    return true;
}

AgentOpinion ConsensusEngineAPIHandlers::parse_agent_opinion_from_json(const nlohmann::json& opinion_json) {
    AgentOpinion opinion;

    opinion.decision = opinion_json["decision"];
    opinion.confidence_score = opinion_json["confidence_score"];
    opinion.reasoning = opinion_json.value("reasoning", "");
    opinion.supporting_data = opinion_json.value("supporting_data", nlohmann::json::object());
    opinion.concerns = opinion_json.value("concerns", std::vector<std::string>{});
    opinion.round_number = opinion_json.value("round_number", 1);
    opinion.submitted_at = std::chrono::system_clock::now();

    return opinion;
}

bool ConsensusEngineAPIHandlers::validate_agent_data(const nlohmann::json& agent_json, std::string& error_message) {
    if (!agent_json.contains("name") || !agent_json["name"].is_string() || agent_json["name"].empty()) {
        error_message = "Agent name is required and must be a non-empty string";
        return false;
    }

    if (!agent_json.contains("role") || !agent_json["role"].is_string()) {
        error_message = "Agent role is required";
        return false;
    }

    std::string role = agent_json["role"];
    std::vector<std::string> valid_roles = {"EXPERT", "REVIEWER", "DECISION_MAKER", "FACILITATOR", "OBSERVER"};
    if (std::find(valid_roles.begin(), valid_roles.end(), role) == valid_roles.end()) {
        error_message = "Invalid role. Must be one of: EXPERT, REVIEWER, DECISION_MAKER, FACILITATOR, OBSERVER";
        return false;
    }

    if (agent_json.contains("voting_weight")) {
        double weight = agent_json["voting_weight"];
        if (weight < 0.0 || weight > 5.0) {
            error_message = "Voting weight must be between 0.0 and 5.0";
            return false;
        }
    }

    if (agent_json.contains("confidence_threshold")) {
        double threshold = agent_json["confidence_threshold"];
        if (threshold < 0.0 || threshold > 1.0) {
            error_message = "Confidence threshold must be between 0.0 and 1.0";
            return false;
        }
    }

    return true;
}

Agent ConsensusEngineAPIHandlers::parse_agent_from_json(const nlohmann::json& agent_json) {
    Agent agent;

    agent.agent_id = agent_json.value("agent_id", ""); // Will be generated if empty
    agent.name = agent_json["name"];

    std::string role_str = agent_json["role"];
    if (role_str == "EXPERT") agent.role = AgentRole::EXPERT;
    else if (role_str == "REVIEWER") agent.role = AgentRole::REVIEWER;
    else if (role_str == "DECISION_MAKER") agent.role = AgentRole::DECISION_MAKER;
    else if (role_str == "FACILITATOR") agent.role = AgentRole::FACILITATOR;
    else agent.role = AgentRole::OBSERVER;

    agent.voting_weight = agent_json.value("voting_weight", 1.0);
    agent.domain_expertise = agent_json.value("domain_expertise", "");
    agent.confidence_threshold = agent_json.value("confidence_threshold", 0.7);
    agent.is_active = agent_json.value("is_active", true);
    agent.last_active = std::chrono::system_clock::now();

    return agent;
}

// Response formatting methods
nlohmann::json ConsensusEngineAPIHandlers::format_consensus_result_to_json(const ConsensusResult& result) {
    nlohmann::json json;
    json["consensus_id"] = result.consensus_id;
    json["topic"] = result.topic;
    json["final_decision"] = result.final_decision;
    json["success"] = result.success;
    json["agreement_percentage"] = result.agreement_percentage;
    json["confidence_level"] = [result]() {
        switch (result.confidence_level) {
            case DecisionConfidence::VERY_HIGH: return "VERY_HIGH";
            case DecisionConfidence::HIGH: return "HIGH";
            case DecisionConfidence::MEDIUM: return "MEDIUM";
            case DecisionConfidence::LOW: return "LOW";
            case DecisionConfidence::VERY_LOW: return "VERY_LOW";
            default: return "MEDIUM";
        }
    }();
    json["algorithm_used"] = [result]() {
        switch (result.algorithm_used) {
        case VotingAlgorithm::UNANIMOUS: return "UNANIMOUS";
        case VotingAlgorithm::MAJORITY: return "MAJORITY";
        case VotingAlgorithm::WEIGHTED_MAJORITY: return "WEIGHTED_MAJORITY";
        case VotingAlgorithm::RANKED_CHOICE: return "RANKED_CHOICE";
        case VotingAlgorithm::QUORUM: return "QUORUM";
            default: return "MAJORITY";
        }
    }();
    json["rounds_used"] = result.rounds_used;
    json["total_participants"] = result.total_participants;
    json["processing_time_ms"] = result.processing_time_ms;

    if (!result.error_message.empty()) {
        json["error_message"] = result.error_message;
    }

    return json;
}

nlohmann::json ConsensusEngineAPIHandlers::format_agent_opinion_to_json(const AgentOpinion& opinion) {
    nlohmann::json json;
    json["agent_id"] = opinion.agent_id;
    json["decision"] = opinion.decision;
    json["confidence_score"] = opinion.confidence_score;
    json["reasoning"] = opinion.reasoning;
    json["supporting_data"] = opinion.supporting_data;
    json["concerns"] = opinion.concerns;
    json["round_number"] = opinion.round_number;
    json["submitted_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        opinion.submitted_at.time_since_epoch()).count();
    return json;
}

nlohmann::json ConsensusEngineAPIHandlers::format_agent_to_json(const Agent& agent) {
    nlohmann::json json;
    json["agent_id"] = agent.agent_id;
    json["name"] = agent.name;
    json["role"] = [agent]() {
        switch (agent.role) {
        case AgentRole::EXPERT: return "EXPERT";
        case AgentRole::REVIEWER: return "REVIEWER";
        case AgentRole::DECISION_MAKER: return "DECISION_MAKER";
        case AgentRole::FACILITATOR: return "FACILITATOR";
        case AgentRole::OBSERVER: return "OBSERVER";
            default: return "OBSERVER";
        }
    }();
    json["voting_weight"] = agent.voting_weight;
    json["domain_expertise"] = agent.domain_expertise;
    json["confidence_threshold"] = agent.confidence_threshold;
    json["is_active"] = agent.is_active;
    json["last_active"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        agent.last_active.time_since_epoch()).count();
    return json;
}

nlohmann::json ConsensusEngineAPIHandlers::format_consensus_config_to_json(const ConsensusConfiguration& config) {
    nlohmann::json json;
    json["topic"] = config.topic;
    json["description"] = config.description;
    json["participants"] = config.participants;
    json["algorithm"] = [config]() {
        switch (config.algorithm) {
            case VotingAlgorithm::UNANIMOUS: return "UNANIMOUS";
            case VotingAlgorithm::MAJORITY: return "MAJORITY";
            case VotingAlgorithm::WEIGHTED_MAJORITY: return "WEIGHTED_MAJORITY";
            case VotingAlgorithm::RANKED_CHOICE: return "RANKED_CHOICE";
            case VotingAlgorithm::QUORUM: return "QUORUM";
            default: return "MAJORITY";
        }
    }();
    json["consensus_threshold"] = config.consensus_threshold;
    json["max_rounds"] = config.max_rounds;
    json["timeout_per_round_minutes"] = config.timeout_per_round_minutes;
    json["custom_rules"] = config.custom_rules;
    return json;
}

// Error handling
nlohmann::json ConsensusEngineAPIHandlers::create_error_response(const std::string& message, int status_code) {
    nlohmann::json response;
    response["success"] = false;
    response["message"] = message;
    response["status_code"] = status_code;
    response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return response;
}

nlohmann::json ConsensusEngineAPIHandlers::create_success_response(const nlohmann::json& data, const std::string& message) {
    nlohmann::json response;
    response["success"] = true;
    response["message"] = message;
    response["data"] = data;
    response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return response;
}

// Access control
bool ConsensusEngineAPIHandlers::validate_user_access(const std::string& user_id, const std::string& permission_scope) {
    // For now, allow all authenticated users access to consensus features
    // In production, this would check role-based permissions
    if (user_id.empty()) {
        return false;
    }

    // Log the access attempt
    if (logger_) {
        nlohmann::json log_data;
        log_data["user_id"] = user_id;
        log_data["permission_scope"] = permission_scope;
        log_data["granted"] = true;
        logger_->log("permission_check", log_data, spdlog::level::debug);
    }

    return true;
}

bool ConsensusEngineAPIHandlers::can_participate_in_consensus(const std::string& user_id, const std::string& consensus_id) {
    // Check if user is registered as an agent and is a participant in the consensus
    auto agent_opt = consensus_engine_->get_agent(user_id);
    if (!agent_opt || !agent_opt->is_active) {
        return false;
    }

    // Get consensus configuration and check participants
    try {
        // This is a simplified check - in production, you'd query the database
        // to verify the user is in the participants list
        return true;
    } catch (const std::exception&) {
        return false;
    }
}