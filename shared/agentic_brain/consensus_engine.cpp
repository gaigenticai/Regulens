#include "consensus_engine.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <uuid/uuid.h>
#include <spdlog/spdlog.h>
#include <random>

namespace regulens {

ConsensusEngine::ConsensusEngine(
    std::shared_ptr<PostgreSQLConnection> db_conn,
    std::shared_ptr<StructuredLogger> logger
) : db_conn_(db_conn), logger_(logger) {

    if (!db_conn_) {
        throw std::runtime_error("Database connection is required for ConsensusEngine");
    }

    spdlog::info("ConsensusEngine initialized with default algorithm: MAJORITY");
}

ConsensusEngine::~ConsensusEngine() {
    spdlog::info("ConsensusEngine shutting down");
}

// Consensus process management
std::string ConsensusEngine::initiate_consensus(const ConsensusConfiguration& config) {
    if (!validate_consensus_config(config)) {
        throw std::runtime_error("Invalid consensus configuration");
    }

    std::string consensus_id = generate_consensus_id();

    std::lock_guard<std::mutex> lock(consensus_mutex_);

    // Initialize the consensus process
    active_consensus_[consensus_id] = config;

    // Create first voting round
    VotingRound initial_round;
    initial_round.round_number = 1;
    initial_round.topic = config.topic;
    initial_round.description = config.description;
    initial_round.state = ConsensusState::COLLECTING_OPINIONS;
    initial_round.started_at = std::chrono::system_clock::now();

    consensus_rounds_[consensus_id] = {initial_round};

    // Store in database
    store_consensus_session(consensus_id, config);

    spdlog::info("Consensus process initiated: {} with {} participants using {} algorithm",
                consensus_id, config.participants.size(), static_cast<int>(config.algorithm));

    if (logger_) {
        nlohmann::json log_data;
        log_data["consensus_id"] = consensus_id;
        log_data["topic"] = config.topic;
        log_data["participant_count"] = config.participants.size();
        log_data["algorithm"] = static_cast<int>(config.algorithm);
        logger_->log("consensus_initiated", log_data, spdlog::level::info);
    }

    return consensus_id;
}

ConsensusResult ConsensusEngine::get_consensus_result(const std::string& consensus_id) {
    std::lock_guard<std::mutex> lock(consensus_mutex_);

    // Check if it's already completed
    auto completed_it = completed_consensus_.find(consensus_id);
    if (completed_it != completed_consensus_.end()) {
        return completed_it->second;
    }

    // Check if it's still active
    auto active_it = active_consensus_.find(consensus_id);
    if (active_it == active_consensus_.end()) {
        ConsensusResult result;
        result.consensus_id = consensus_id;
        result.success = false;
        result.error_message = "Consensus process not found";
        return result;
    }

    // Try to calculate consensus
    return calculate_consensus(consensus_id);
}

ConsensusState ConsensusEngine::get_consensus_state(const std::string& consensus_id) {
    std::lock_guard<std::mutex> lock(consensus_mutex_);

    auto active_it = active_consensus_.find(consensus_id);
    if (active_it != active_consensus_.end()) {
        auto rounds_it = consensus_rounds_.find(consensus_id);
        if (rounds_it != consensus_rounds_.end() && !rounds_it->second.empty()) {
            return rounds_it->second.back().state;
        }
        return ConsensusState::INITIALIZING;
    }

    return ConsensusState::CANCELLED;
}

// Agent opinion management
bool ConsensusEngine::submit_opinion(const std::string& consensus_id, const AgentOpinion& opinion) {
    std::lock_guard<std::mutex> lock(consensus_mutex_);

    auto active_it = active_consensus_.find(consensus_id);
    if (active_it == active_consensus_.end()) {
        spdlog::warn("Attempted to submit opinion for non-existent consensus: {}", consensus_id);
        return false;
    }

    // Validate the opinion
    if (!validate_agent_opinion(opinion, active_it->second)) {
        spdlog::warn("Invalid opinion submitted for consensus: {}", consensus_id);
        return false;
    }

    // Add to current round
    auto rounds_it = consensus_rounds_.find(consensus_id);
    if (rounds_it == consensus_rounds_.end() || rounds_it->second.empty()) {
        spdlog::error("No active rounds for consensus: {}", consensus_id);
        return false;
    }

    VotingRound& current_round = rounds_it->second.back();

    // Check if agent already submitted opinion for this round
    auto existing_opinion = std::find_if(current_round.opinions.begin(), current_round.opinions.end(),
                                        [&opinion](const AgentOpinion& o) {
                                            return o.agent_id == opinion.agent_id;
                                        });

    if (existing_opinion != current_round.opinions.end()) {
        // Update existing opinion
        *existing_opinion = opinion;
    } else {
        // Add new opinion
        current_round.opinions.push_back(opinion);
    }

    // Store in database
    store_agent_opinion(consensus_id, opinion);

    spdlog::info("Opinion submitted for consensus {} by agent {}: confidence={}",
                consensus_id, opinion.agent_id, opinion.confidence_score);

    if (logger_) {
        nlohmann::json log_data;
        log_data["consensus_id"] = consensus_id;
        log_data["agent_id"] = opinion.agent_id;
        log_data["decision"] = opinion.decision;
        log_data["confidence_score"] = opinion.confidence_score;
        log_data["round_number"] = opinion.round_number;
        logger_->log("opinion_submitted", log_data, spdlog::level::info);
    }

    return true;
}

std::vector<AgentOpinion> ConsensusEngine::get_agent_opinions(const std::string& consensus_id, int round_number) {
    std::lock_guard<std::mutex> lock(consensus_mutex_);

    auto rounds_it = consensus_rounds_.find(consensus_id);
    if (rounds_it == consensus_rounds_.end()) {
        return {};
    }

    if (round_number == -1) {
        // Return opinions from all rounds
        std::vector<AgentOpinion> all_opinions;
        for (const auto& round : rounds_it->second) {
            all_opinions.insert(all_opinions.end(), round.opinions.begin(), round.opinions.end());
        }
        return all_opinions;
    } else {
        // Return opinions from specific round
        for (const auto& round : rounds_it->second) {
            if (round.round_number == round_number) {
                return round.opinions;
            }
        }
    }

    return {};
}

bool ConsensusEngine::update_opinion(const std::string& consensus_id, const std::string& agent_id, const AgentOpinion& updated_opinion) {
    std::lock_guard<std::mutex> lock(consensus_mutex_);

    auto rounds_it = consensus_rounds_.find(consensus_id);
    if (rounds_it == consensus_rounds_.end()) {
        return false;
    }

    // Find and update the opinion
    for (auto& round : rounds_it->second) {
        auto opinion_it = std::find_if(round.opinions.begin(), round.opinions.end(),
                                      [&agent_id](const AgentOpinion& o) {
                                          return o.agent_id == agent_id;
                                      });
        if (opinion_it != round.opinions.end()) {
            *opinion_it = updated_opinion;
            // Update in database
            update_agent_opinion(consensus_id, updated_opinion);
            return true;
        }
    }

    return false;
}

// Voting and decision making
bool ConsensusEngine::start_voting_round(const std::string& consensus_id) {
    std::lock_guard<std::mutex> lock(consensus_mutex_);

    auto active_it = active_consensus_.find(consensus_id);
    if (active_it == active_consensus_.end()) {
        return false;
    }

    auto rounds_it = consensus_rounds_.find(consensus_id);
    if (rounds_it == consensus_rounds_.end() || rounds_it->second.empty()) {
        return false;
    }

    VotingRound& current_round = rounds_it->second.back();

    // End current round if it's not already ended
    if (current_round.ended_at == std::chrono::system_clock::time_point{}) {
        current_round.ended_at = std::chrono::system_clock::now();
        current_round.state = ConsensusState::VOTING;
    }

    // Start new voting round
    VotingRound new_round;
    new_round.round_number = current_round.round_number + 1;
    new_round.topic = current_round.topic;
    new_round.description = "Round " + std::to_string(new_round.round_number) + " voting";
    new_round.state = ConsensusState::VOTING;
    new_round.started_at = std::chrono::system_clock::now();

    rounds_it->second.push_back(new_round);

    spdlog::info("Voting round {} started for consensus {}", new_round.round_number, consensus_id);

    return true;
}

bool ConsensusEngine::end_voting_round(const std::string& consensus_id) {
    std::lock_guard<std::mutex> lock(consensus_mutex_);

    auto rounds_it = consensus_rounds_.find(consensus_id);
    if (rounds_it == consensus_rounds_.end() || rounds_it->second.empty()) {
        return false;
    }

    VotingRound& current_round = rounds_it->second.back();

    if (current_round.ended_at != std::chrono::system_clock::time_point{}) {
        // Round already ended
        return true;
    }

    current_round.ended_at = std::chrono::system_clock::now();
    current_round.state = ConsensusState::DISCUSSING;

    // Count votes
    for (const auto& opinion : current_round.opinions) {
        current_round.vote_counts[opinion.decision]++;
    }

    spdlog::info("Voting round {} ended for consensus {}", current_round.round_number, consensus_id);

    return true;
}

ConsensusResult ConsensusEngine::calculate_consensus(const std::string& consensus_id) {
    std::lock_guard<std::mutex> lock(consensus_mutex_);

    auto active_it = active_consensus_.find(consensus_id);
    if (active_it == active_consensus_.end()) {
    ConsensusResult result;
        result.consensus_id = consensus_id;
        result.success = false;
        result.error_message = "Consensus process not found";
        return result;
    }

    auto rounds_it = consensus_rounds_.find(consensus_id);
    if (rounds_it == consensus_rounds_.end()) {
        ConsensusResult result;
        result.consensus_id = consensus_id;
        result.success = false;
        result.error_message = "No voting rounds found";
        return result;
    }

    const ConsensusConfiguration& config = active_it->second;
    const std::vector<VotingRound>& rounds = rounds_it->second;

    // Use the appropriate algorithm
    ConsensusResult result;
    result.consensus_id = consensus_id;
    result.topic = config.topic;
    result.algorithm_used = config.algorithm;

    try {
        switch (config.algorithm) {
            case VotingAlgorithm::UNANIMOUS:
                result = run_unanimous_voting(rounds, config);
                break;
            case VotingAlgorithm::MAJORITY:
                result = run_majority_voting(rounds, config);
                break;
            case VotingAlgorithm::WEIGHTED_MAJORITY:
                result = run_weighted_majority_voting(rounds, config);
                break;
            case VotingAlgorithm::RANKED_CHOICE:
                result = run_ranked_choice_voting(rounds, config);
                break;
            case VotingAlgorithm::QUORUM:
                result = run_quorum_voting(rounds, config);
                break;
            default:
                result = run_majority_voting(rounds, config);
            break;
        }

        result.rounds_used = rounds.size();
        result.total_participants = config.participants.size();

        // Store completed consensus
        if (result.success) {
            completed_consensus_[consensus_id] = result;
            active_consensus_.erase(active_it);
        }

        // Update database
        update_consensus_result(consensus_id, result);

        spdlog::info("Consensus calculated for {}: success={}, decision='{}'",
                    consensus_id, result.success, result.final_decision);

    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string("Consensus calculation failed: ") + e.what();
        spdlog::error("Consensus calculation failed for {}: {}", consensus_id, e.what());
    }

    return result;
}

// Agent management
bool ConsensusEngine::register_agent(const Agent& agent) {
    try {
        // Validate agent
        if (agent.agent_id.empty() || agent.name.empty()) {
            return false;
        }

        // Store in database
        const char* query = R"(
            INSERT INTO consensus_agents (
                agent_id, name, role, voting_weight, domain_expertise,
                confidence_threshold, is_active, last_active
            ) VALUES ($1, $2, $3, $4, $5, $6, $7, NOW())
            ON CONFLICT (agent_id) DO UPDATE SET
                name = EXCLUDED.name,
                role = EXCLUDED.role,
                voting_weight = EXCLUDED.voting_weight,
                domain_expertise = EXCLUDED.domain_expertise,
                confidence_threshold = EXCLUDED.confidence_threshold,
                is_active = EXCLUDED.is_active,
                last_active = NOW()
        )";

        std::string role_str;
        switch (agent.role) {
            case AgentRole::EXPERT: role_str = "EXPERT"; break;
            case AgentRole::REVIEWER: role_str = "REVIEWER"; break;
            case AgentRole::DECISION_MAKER: role_str = "DECISION_MAKER"; break;
            case AgentRole::FACILITATOR: role_str = "FACILITATOR"; break;
            case AgentRole::OBSERVER: role_str = "OBSERVER"; break;
        }

        const char* param_values[7] = {
            agent.agent_id.c_str(),
            agent.name.c_str(),
            role_str.c_str(),
            std::to_string(agent.voting_weight).c_str(),
            agent.domain_expertise.c_str(),
            std::to_string(agent.confidence_threshold).c_str(),
            agent.is_active ? "true" : "false"
        };

        PGresult* pg_result = PQexecParams(db_conn_->get_connection(), query, 7, NULL, param_values, NULL, NULL, 0);

        if (PQresultStatus(pg_result) != PGRES_COMMAND_OK) {
            spdlog::error("Failed to register agent: {}", PQerrorMessage(db_conn_->get_connection()));
            PQclear(pg_result);
            return false;
        }

        PQclear(pg_result);

        spdlog::info("Agent registered: {} ({})", agent.name, agent.agent_id);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to register agent: {}", e.what());
        return false;
    }
}

bool ConsensusEngine::update_agent(const std::string& agent_id, const Agent& updated_agent) {
    return register_agent(updated_agent); // Reuse the same logic
}

std::optional<Agent> ConsensusEngine::get_agent(const std::string& agent_id) {
    try {
        const char* query = "SELECT agent_id, name, role, voting_weight, domain_expertise, "
                           "confidence_threshold, is_active, last_active FROM consensus_agents WHERE agent_id = $1";

        const char* param_values[1] = {agent_id.c_str()};
        PGresult* pg_result = PQexecParams(db_conn_->get_connection(), query, 1, NULL, param_values, NULL, NULL, 0);

        if (PQresultStatus(pg_result) != PGRES_TUPLES_OK || PQntuples(pg_result) == 0) {
            PQclear(pg_result);
            return std::nullopt;
        }

        Agent agent;
        agent.agent_id = PQgetvalue(pg_result, 0, 0);
        agent.name = PQgetvalue(pg_result, 0, 1);

        std::string role_str = PQgetvalue(pg_result, 0, 2);
        if (role_str == "EXPERT") agent.role = AgentRole::EXPERT;
        else if (role_str == "REVIEWER") agent.role = AgentRole::REVIEWER;
        else if (role_str == "DECISION_MAKER") agent.role = AgentRole::DECISION_MAKER;
        else if (role_str == "FACILITATOR") agent.role = AgentRole::FACILITATOR;
        else agent.role = AgentRole::OBSERVER;

        agent.voting_weight = std::stod(PQgetvalue(pg_result, 0, 3) ? PQgetvalue(pg_result, 0, 3) : "1.0");
        agent.domain_expertise = PQgetvalue(pg_result, 0, 4) ? PQgetvalue(pg_result, 0, 4) : "";
        agent.confidence_threshold = std::stod(PQgetvalue(pg_result, 0, 5) ? PQgetvalue(pg_result, 0, 5) : "0.7");
        agent.is_active = std::string(PQgetvalue(pg_result, 0, 6)) == "t";

        PQclear(pg_result);
        return agent;

    } catch (const std::exception& e) {
        spdlog::error("Failed to get agent {}: {}", agent_id, e.what());
        return std::nullopt;
    }
}

std::vector<Agent> ConsensusEngine::get_active_agents() {
    std::vector<Agent> agents;

    try {
        const char* query = "SELECT agent_id, name, role, voting_weight, domain_expertise, "
                           "confidence_threshold, is_active, last_active FROM consensus_agents WHERE is_active = true";

        PGresult* pg_result = PQexec(db_conn_->get_connection(), query);

        if (PQresultStatus(pg_result) != PGRES_TUPLES_OK) {
            spdlog::error("Failed to get active agents: {}", PQerrorMessage(db_conn_->get_connection()));
            PQclear(pg_result);
            return agents;
        }

        int num_rows = PQntuples(pg_result);
        for (int i = 0; i < num_rows; ++i) {
            Agent agent;
            agent.agent_id = PQgetvalue(pg_result, i, 0);
            agent.name = PQgetvalue(pg_result, i, 1);

            std::string role_str = PQgetvalue(pg_result, i, 2);
            if (role_str == "EXPERT") agent.role = AgentRole::EXPERT;
            else if (role_str == "REVIEWER") agent.role = AgentRole::REVIEWER;
            else if (role_str == "DECISION_MAKER") agent.role = AgentRole::DECISION_MAKER;
            else if (role_str == "FACILITATOR") agent.role = AgentRole::FACILITATOR;
            else agent.role = AgentRole::OBSERVER;

            agent.voting_weight = std::stod(PQgetvalue(pg_result, i, 3) ? PQgetvalue(pg_result, i, 3) : "1.0");
            agent.domain_expertise = PQgetvalue(pg_result, i, 4) ? PQgetvalue(pg_result, i, 4) : "";
            agent.confidence_threshold = std::stod(PQgetvalue(pg_result, i, 5) ? PQgetvalue(pg_result, i, 5) : "0.7");
            agent.is_active = true;

            agents.push_back(agent);
        }

        PQclear(pg_result);

    } catch (const std::exception& e) {
        spdlog::error("Failed to get active agents: {}", e.what());
    }

    return agents;
}

bool ConsensusEngine::deactivate_agent(const std::string& agent_id) {
    try {
        const char* query = "UPDATE consensus_agents SET is_active = false, last_active = NOW() WHERE agent_id = $1";

        const char* param_values[1] = {agent_id.c_str()};
        PGresult* pg_result = PQexecParams(db_conn_->get_connection(), query, 1, NULL, param_values, NULL, NULL, 0);

        if (PQresultStatus(pg_result) != PGRES_COMMAND_OK) {
            spdlog::error("Failed to deactivate agent: {}", PQerrorMessage(db_conn_->get_connection()));
            PQclear(pg_result);
            return false;
        }

        PQclear(pg_result);

        spdlog::info("Agent deactivated: {}", agent_id);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to deactivate agent {}: {}", agent_id, e.what());
        return false;
    }
}

// Algorithm implementations
ConsensusResult ConsensusEngine::run_unanimous_voting(const std::vector<VotingRound>& rounds, const ConsensusConfiguration& config) {
    ConsensusResult result;
    result.consensus_id = config.consensus_id;
    result.topic = config.topic;
    result.algorithm_used = VotingAlgorithm::UNANIMOUS;

    if (rounds.empty()) {
        result.success = false;
        result.error_message = "No voting rounds available";
        return result;
    }

    const VotingRound& final_round = rounds.back();

    // Check if all opinions agree
    if (final_round.opinions.empty()) {
        result.success = false;
        result.error_message = "No opinions submitted";
        return result;
    }

    std::string first_decision = final_round.opinions[0].decision;
    bool unanimous = true;

    for (const auto& opinion : final_round.opinions) {
        if (opinion.decision != first_decision) {
            unanimous = false;
            break;
        }
    }

    if (unanimous) {
        result.success = true;
        result.final_decision = first_decision;
        result.confidence_level = ConsensusDecisionConfidence::VERY_HIGH;
        result.agreement_percentage = 1.0;
    } else {
        result.success = false;
        result.error_message = "No unanimous agreement reached";
    }

    return result;
}

ConsensusResult ConsensusEngine::run_majority_voting(const std::vector<VotingRound>& rounds, const ConsensusConfiguration& config) {
    ConsensusResult result;
    result.consensus_id = config.consensus_id;
    result.topic = config.topic;
    result.algorithm_used = VotingAlgorithm::MAJORITY;

    if (rounds.empty()) {
        result.success = false;
        result.error_message = "No voting rounds available";
        return result;
    }

    const VotingRound& final_round = rounds.back();

    // Count votes
    std::unordered_map<std::string, int> vote_counts;
    for (const auto& opinion : final_round.opinions) {
        vote_counts[opinion.decision]++;
    }

    if (vote_counts.empty()) {
        result.success = false;
        result.error_message = "No votes recorded";
        return result;
    }

    // Find the majority decision
    auto majority_it = std::max_element(vote_counts.begin(), vote_counts.end(),
                                       [](const auto& a, const auto& b) {
                                           return a.second < b.second;
                                       });

    int total_votes = final_round.opinions.size();
    int majority_votes = majority_it->second;
    double agreement_percentage = static_cast<double>(majority_votes) / total_votes;

    result.final_decision = majority_it->first;
    result.agreement_percentage = agreement_percentage;
    result.success = agreement_percentage > config.consensus_threshold;

    if (result.success) {
        if (agreement_percentage >= 0.9) result.confidence_level = DecisionConfidence::VERY_HIGH;
        else if (agreement_percentage >= 0.7) result.confidence_level = DecisionConfidence::HIGH;
        else if (agreement_percentage >= 0.5) result.confidence_level = DecisionConfidence::MEDIUM;
        else result.confidence_level = DecisionConfidence::LOW;
    }

    return result;
}

ConsensusResult ConsensusEngine::run_weighted_majority_voting(const std::vector<VotingRound>& rounds, const ConsensusConfiguration& config) {
    ConsensusResult result;
    result.consensus_id = config.consensus_id;
    result.topic = config.topic;
    result.algorithm_used = VotingAlgorithm::WEIGHTED_MAJORITY;

    if (rounds.empty()) {
        result.success = false;
        result.error_message = "No voting rounds available";
        return result;
    }

    const VotingRound& final_round = rounds.back();

    // Count weighted votes
    std::unordered_map<std::string, double> weighted_vote_counts;

    for (const auto& opinion : final_round.opinions) {
        // Get agent voting weight (default to 1.0 if not found)
        double weight = 1.0;
        if (auto agent_opt = get_agent(opinion.agent_id)) {
            weight = agent_opt->voting_weight;
        }

        weighted_vote_counts[opinion.decision] += weight * opinion.confidence_score;
    }

    if (weighted_vote_counts.empty()) {
        result.success = false;
        result.error_message = "No weighted votes recorded";
        return result;
    }

    // Find the weighted majority decision
    auto majority_it = std::max_element(weighted_vote_counts.begin(), weighted_vote_counts.end(),
                                       [](const auto& a, const auto& b) {
                                           return a.second < b.second;
                                       });

    double total_weighted_votes = 0.0;
    for (const auto& [decision, weight] : weighted_vote_counts) {
        total_weighted_votes += weight;
    }

    double majority_weighted_votes = majority_it->second;
    double agreement_percentage = majority_weighted_votes / total_weighted_votes;

    result.final_decision = majority_it->first;
    result.agreement_percentage = agreement_percentage;
    result.success = agreement_percentage > config.consensus_threshold;

    if (result.success) {
        if (agreement_percentage >= 0.8) result.confidence_level = DecisionConfidence::VERY_HIGH;
        else if (agreement_percentage >= 0.6) result.confidence_level = DecisionConfidence::HIGH;
        else if (agreement_percentage >= 0.4) result.confidence_level = DecisionConfidence::MEDIUM;
        else result.confidence_level = DecisionConfidence::LOW;
    }

    return result;
}

ConsensusResult ConsensusEngine::run_ranked_choice_voting(const std::vector<VotingRound>& rounds, const ConsensusConfiguration& config) {
    ConsensusResult result;
    result.consensus_id = config.consensus_id;
    result.topic = config.topic;
    result.algorithm_used = VotingAlgorithm::RANKED_CHOICE;

    // Simplified implementation - in production, this would be more complex
    // For now, fall back to majority voting
    return run_majority_voting(rounds, config);
}

ConsensusResult ConsensusEngine::run_quorum_voting(const std::vector<VotingRound>& rounds, const ConsensusConfiguration& config) {
    ConsensusResult result;
    result.consensus_id = config.consensus_id;
    result.topic = config.topic;
    result.algorithm_used = VotingAlgorithm::QUORUM;

    if (rounds.empty()) {
        result.success = false;
        result.error_message = "No voting rounds available";
        return result;
    }

    const VotingRound& final_round = rounds.back();

    // Check if quorum is met
    size_t required_quorum = config.participants.size() / 2 + 1; // Majority quorum
    if (final_round.opinions.size() < required_quorum) {
        result.success = false;
        result.error_message = "Quorum not met: " + std::to_string(final_round.opinions.size()) +
                              " participants, " + std::to_string(required_quorum) + " required";
        return result;
    }

    // Use majority voting among quorum
    return run_majority_voting(rounds, config);
}

// Conflict resolution
std::vector<std::string> ConsensusEngine::identify_conflicts(const std::vector<AgentOpinion>& opinions) {
    std::vector<std::string> conflicts;

    if (opinions.size() < 2) {
        return conflicts;
    }

    // Group opinions by decision
    std::unordered_map<std::string, std::vector<const AgentOpinion*>> decision_groups;

    for (const auto& opinion : opinions) {
        decision_groups[opinion.decision].push_back(&opinion);
    }

    // Find decisions with significant disagreement
    for (const auto& [decision, group_opinions] : decision_groups) {
        if (group_opinions.size() < opinions.size() * 0.3) { // Less than 30% agreement
            conflicts.push_back("Low agreement on decision: " + decision);
        }

        // Check for conflicting concerns
        std::set<std::string> all_concerns;
        for (const auto* opinion : group_opinions) {
            all_concerns.insert(opinion->concerns.begin(), opinion->concerns.end());
        }

        if (all_concerns.size() > 2) {
            conflicts.push_back("Multiple concerns for decision: " + decision);
        }
    }

    return conflicts;
}

nlohmann::json ConsensusEngine::suggest_resolution_strategies(const std::vector<AgentOpinion>& opinions) {
    nlohmann::json strategies = nlohmann::json::array();

    auto conflicts = identify_conflicts(opinions);

    if (conflicts.empty()) {
        strategies.push_back({
            {"strategy", "no_action_needed"},
            {"description", "No significant conflicts detected"},
            {"confidence", 0.9}
        });
        return strategies;
    }

    // Suggest strategies based on conflict types
    for (const auto& conflict : conflicts) {
        if (conflict.find("Low agreement") != std::string::npos) {
            strategies.push_back({
                {"strategy", "additional_round"},
                {"description", "Conduct another voting round with more discussion"},
                {"confidence", 0.8}
            });
        } else if (conflict.find("Multiple concerns") != std::string::npos) {
            strategies.push_back({
                {"strategy", "expert_arbitration"},
                {"description", "Bring in domain expert to resolve technical concerns"},
                {"confidence", 0.7}
            });
        }
    }

    return strategies;
}

bool ConsensusEngine::resolve_conflict(const std::string& consensus_id, const std::string& resolution_strategy) {
    std::lock_guard<std::mutex> lock(consensus_mutex_);

    auto active_it = active_consensus_.find(consensus_id);
    if (active_it == active_consensus_.end()) {
        return false;
    }

    // Update the consensus configuration with resolution strategy
    active_it->second.custom_rules["conflict_resolution"] = resolution_strategy;

    spdlog::info("Conflict resolution applied to consensus {}: {}", consensus_id, resolution_strategy);

    if (logger_) {
        nlohmann::json log_data;
        log_data["consensus_id"] = consensus_id;
        log_data["resolution_strategy"] = resolution_strategy;
        logger_->log("conflict_resolved", log_data, spdlog::level::info);
    }

    return true;
}

// Analytics and monitoring
std::unordered_map<std::string, int> ConsensusEngine::get_consensus_statistics() {
    std::unordered_map<std::string, int> stats;

    // This would typically query the database for historical statistics
    // For now, return basic in-memory stats
    std::lock_guard<std::mutex> lock(consensus_mutex_);

    stats["active_consensus"] = active_consensus_.size();
    stats["completed_consensus"] = completed_consensus_.size();

    int total_rounds = 0;
    for (const auto& [id, rounds] : consensus_rounds_) {
        total_rounds += rounds.size();
    }
    stats["total_rounds"] = total_rounds;

    return stats;
}

std::vector<std::pair<std::string, double>> ConsensusEngine::get_agent_performance_metrics() {
    std::vector<std::pair<std::string, double>> metrics;

    // This would analyze historical agent performance
    // For now, return basic metrics for active agents
    auto agents = get_active_agents();

    for (const auto& agent : agents) {
        // Calculate basic performance score
        double performance_score = agent.voting_weight * agent.confidence_threshold;
        metrics.emplace_back(agent.agent_id, performance_score);
    }

    return metrics;
}

double ConsensusEngine::calculate_decision_accuracy(const std::string& consensus_id, bool actual_outcome) {
    // This would compare consensus decisions with actual outcomes
    // For now, return a placeholder implementation
    auto result = get_consensus_result(consensus_id);
    if (!result.success) {
        return 0.0;
    }

    // Simplified accuracy calculation based on confidence level
    switch (result.confidence_level) {
        case DecisionConfidence::VERY_HIGH: return actual_outcome ? 0.9 : 0.1;
        case DecisionConfidence::HIGH: return actual_outcome ? 0.8 : 0.2;
        case DecisionConfidence::MEDIUM: return actual_outcome ? 0.7 : 0.3;
        case DecisionConfidence::LOW: return actual_outcome ? 0.6 : 0.4;
        default: return 0.5;
    }
}

// Configuration and optimization
void ConsensusEngine::set_default_algorithm(VotingAlgorithm algorithm) {
    default_algorithm_ = algorithm;
    spdlog::info("Default consensus algorithm set to: {}", static_cast<int>(algorithm));
}

void ConsensusEngine::set_max_rounds(int max_rounds) {
    default_max_rounds_ = max_rounds;
    spdlog::info("Default max rounds set to: {}", max_rounds);
}

void ConsensusEngine::set_timeout_per_round(std::chrono::minutes timeout) {
    default_timeout_ = timeout;
    spdlog::info("Default timeout per round set to: {} minutes", timeout.count());
}

void ConsensusEngine::optimize_for_scenario(const std::string& scenario_type) {
    if (scenario_type == "high_stakes") {
        set_default_algorithm(VotingAlgorithm::UNANIMOUS);
        default_consensus_threshold_ = 0.9;
        spdlog::info("Optimized for high-stakes scenarios");
    } else if (scenario_type == "time_critical") {
        set_default_algorithm(VotingAlgorithm::MAJORITY);
        default_consensus_threshold_ = 0.6;
        set_timeout_per_round(std::chrono::minutes(5));
        spdlog::info("Optimized for time-critical scenarios");
    } else if (scenario_type == "expert_driven") {
        set_default_algorithm(VotingAlgorithm::WEIGHTED_MAJORITY);
        default_consensus_threshold_ = 0.7;
        spdlog::info("Optimized for expert-driven scenarios");
    }
}

// Private helper methods

std::string ConsensusEngine::generate_consensus_id() {
    uuid_t uuid;
    uuid_generate(uuid);

    char uuid_str[37];
    uuid_unparse(uuid, uuid_str);

    return std::string("consensus_") + uuid_str;
}

std::string ConsensusEngine::generate_agent_id() {
    uuid_t uuid;
    uuid_generate(uuid);

    char uuid_str[37];
    uuid_unparse(uuid, uuid_str);

    return std::string("agent_") + uuid_str;
}

bool ConsensusEngine::validate_consensus_config(const ConsensusConfiguration& config) {
    if (config.topic.empty() || config.participants.empty()) {
        return false;
    }

    if (config.consensus_threshold < 0.0 || config.consensus_threshold > 1.0) {
        return false;
    }

    if (config.max_rounds < 1) {
        return false;
    }

    return true;
}

bool ConsensusEngine::validate_agent_opinion(const AgentOpinion& opinion, const ConsensusConfiguration& config) {
    if (opinion.agent_id.empty() || opinion.decision.empty()) {
        return false;
    }

    if (opinion.confidence_score < 0.0 || opinion.confidence_score > 1.0) {
        return false;
    }

    // Check if agent is authorized to participate
    if (std::find(config.participants.begin(), config.participants.end(), opinion.agent_id) == config.participants.end()) {
        return false;
    }

    return true;
}

ConsensusDecisionConfidence ConsensusEngine::calculate_confidence_level(double agreement_percentage, int rounds_used) {
    if (agreement_percentage >= 0.9 && rounds_used <= 2) return ConsensusDecisionConfidence::VERY_HIGH;
    if (agreement_percentage >= 0.7 && rounds_used <= 3) return ConsensusDecisionConfidence::HIGH;
    if (agreement_percentage >= 0.5) return ConsensusDecisionConfidence::MEDIUM;
    if (agreement_percentage >= 0.3) return ConsensusDecisionConfidence::LOW;
    return ConsensusDecisionConfidence::LOW; // Changed from VERY_LOW to LOW since VERY_LOW doesn't exist
}

// Database operations (simplified implementations)
void ConsensusEngine::store_consensus_session(const std::string& consensus_id, const ConsensusConfiguration& config) {
    // Implementation would store consensus session in database
    spdlog::debug("Storing consensus session: {}", consensus_id);
}

void ConsensusEngine::store_agent_opinion(const std::string& consensus_id, const AgentOpinion& opinion) {
    // Implementation would store agent opinion in database
    spdlog::debug("Storing agent opinion for consensus {} by agent {}", consensus_id, opinion.agent_id);
}

void ConsensusEngine::update_agent_opinion(const std::string& consensus_id, const AgentOpinion& opinion) {
    // Implementation would update agent opinion in database
    spdlog::debug("Updating agent opinion for consensus {} by agent {}", consensus_id, opinion.agent_id);
}

void ConsensusEngine::update_consensus_result(const std::string& consensus_id, const ConsensusResult& result) {
    // Implementation would update consensus result in database
    spdlog::debug("Updating consensus result for: {}", consensus_id);
}

} // namespace regulens