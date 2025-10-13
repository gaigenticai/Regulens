#include "consensus_engine.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <uuid/uuid.h>

namespace regulens {

ConsensusEngine::ConsensusEngine(PGconn* db_conn)
    : db_conn_(db_conn) {
}

ConsensusEngine::~ConsensusEngine() {
}

std::string ConsensusEngine::consensus_type_to_string(ConsensusType type) {
    switch (type) {
        case ConsensusType::UNANIMOUS: return "unanimous";
        case ConsensusType::MAJORITY: return "majority";
        case ConsensusType::SUPERMAJORITY: return "supermajority";
        case ConsensusType::WEIGHTED_VOTING: return "weighted_voting";
        case ConsensusType::RANKED_CHOICE: return "ranked_choice";
        case ConsensusType::BAYESIAN: return "bayesian";
        default: return "majority";
    }
}

ConsensusType ConsensusEngine::string_to_consensus_type(const std::string& str) {
    if (str == "unanimous") return ConsensusType::UNANIMOUS;
    if (str == "majority") return ConsensusType::MAJORITY;
    if (str == "supermajority") return ConsensusType::SUPERMAJORITY;
    if (str == "weighted_voting") return ConsensusType::WEIGHTED_VOTING;
    if (str == "ranked_choice") return ConsensusType::RANKED_CHOICE;
    if (str == "bayesian") return ConsensusType::BAYESIAN;
    return ConsensusType::MAJORITY;
}

std::string ConsensusEngine::consensus_status_to_string(ConsensusStatus status) {
    switch (status) {
        case ConsensusStatus::OPEN: return "open";
        case ConsensusStatus::CLOSED: return "closed";
        case ConsensusStatus::REACHED: return "reached";
        case ConsensusStatus::FAILED: return "failed";
        case ConsensusStatus::TIMEOUT: return "timeout";
        default: return "open";
    }
}

ConsensusStatus ConsensusEngine::string_to_consensus_status(const std::string& str) {
    if (str == "open") return ConsensusStatus::OPEN;
    if (str == "closed") return ConsensusStatus::CLOSED;
    if (str == "reached") return ConsensusStatus::REACHED;
    if (str == "failed") return ConsensusStatus::FAILED;
    if (str == "timeout") return ConsensusStatus::TIMEOUT;
    return ConsensusStatus::OPEN;
}

std::optional<std::string> ConsensusEngine::start_session(
    const std::string& topic,
    const std::vector<std::string>& participant_agent_ids,
    ConsensusType type,
    const nlohmann::json& parameters) {

    // Validate inputs
    if (topic.empty() || participant_agent_ids.empty()) {
        return std::nullopt;
    }

    // Generate session ID
    uuid_t uuid;
    uuid_generate_random(uuid);
    char session_id_str[37];
    uuid_unparse_lower(uuid, session_id_str);
    std::string session_id(session_id_str);

    // Extract parameters
    double threshold = parameters.value("threshold", 0.5);  // For majority/supermajority
    int deadline_minutes = parameters.value("deadline_minutes", 60);
    std::string description = parameters.value("description", "");

    // Adjust threshold for supermajority
    if (type == ConsensusType::SUPERMAJORITY && threshold == 0.5) {
        threshold = 0.67;  // Default to 2/3
    }

    int required_votes = participant_agent_ids.size();
    std::string type_str = consensus_type_to_string(type);
    std::string threshold_str = std::to_string(threshold);
    std::string required_str = std::to_string(required_votes);
    std::string deadline_str = std::to_string(deadline_minutes);

    const char* params[7] = {
        session_id.c_str(),
        topic.c_str(),
        description.c_str(),
        type_str.c_str(),
        threshold_str.c_str(),
        required_str.c_str(),
        deadline_str.c_str()
    };

    PGresult* result = PQexecParams(db_conn_,
        "INSERT INTO consensus_sessions "
        "(session_id, topic, description, consensus_type, threshold, required_votes, deadline) "
        "VALUES ($1, $2, $3, $4::consensus_type, $5, $6, NOW() + INTERVAL '$7 minutes') "
        "RETURNING session_id",
        7, NULL, params, NULL, NULL, 0);

    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        PQclear(result);
        return std::nullopt;
    }

    PQclear(result);
    return session_id;
}

bool ConsensusEngine::contribute_vote(
    const std::string& session_id,
    const std::string& agent_id,
    const nlohmann::json& vote_value,
    double confidence,
    const std::string& reasoning) {

    // Check if session is open
    const char* check_params[] = {session_id.c_str()};
    PGresult* check_result = PQexecParams(db_conn_,
        "SELECT status FROM consensus_sessions WHERE session_id = $1",
        1, NULL, check_params, NULL, NULL, 0);

    if (PQresultStatus(check_result) != PGRES_TUPLES_OK || PQntuples(check_result) == 0) {
        PQclear(check_result);
        return false;
    }

    std::string status = PQgetvalue(check_result, 0, 0);
    PQclear(check_result);

    if (status != "open") {
        return false;  // Session closed
    }

    // Generate vote ID
    uuid_t uuid;
    uuid_generate_random(uuid);
    char vote_id_str[37];
    uuid_unparse_lower(uuid, vote_id_str);
    std::string vote_id(vote_id_str);

    // Insert vote
    std::string vote_json = vote_value.dump();
    std::string confidence_str = std::to_string(confidence);

    const char* params[6] = {
        vote_id.c_str(),
        session_id.c_str(),
        agent_id.c_str(),
        vote_json.c_str(),
        confidence_str.c_str(),
        reasoning.c_str()
    };

    PGresult* result = PQexecParams(db_conn_,
        "INSERT INTO consensus_votes "
        "(vote_id, session_id, agent_id, vote_value, confidence, reasoning) "
        "VALUES ($1, $2, $3, $4::jsonb, $5, $6) "
        "ON CONFLICT (session_id, agent_id) DO UPDATE SET "
        "  vote_value = EXCLUDED.vote_value, "
        "  confidence = EXCLUDED.confidence, "
        "  reasoning = EXCLUDED.reasoning, "
        "  cast_at = NOW()",
        6, NULL, params, NULL, NULL, 0);

    bool success = PQresultStatus(result) == PGRES_COMMAND_OK;
    PQclear(result);

    if (!success) return false;

    // Update vote count
    const char* update_params[] = {session_id.c_str()};
    PGresult* update_result = PQexecParams(db_conn_,
        "UPDATE consensus_sessions "
        "SET current_votes = (SELECT COUNT(*) FROM consensus_votes WHERE session_id = $1) "
        "WHERE session_id = $1",
        1, NULL, update_params, NULL, NULL, 0);
    PQclear(update_result);

    return true;
}

std::vector<ConsensusVote> ConsensusEngine::get_votes(const std::string& session_id) {
    std::vector<ConsensusVote> votes;

    const char* params[] = {session_id.c_str()};

    PGresult* result = PQexecParams(db_conn_,
        "SELECT vote_id, session_id, agent_id, vote_value, confidence, reasoning, cast_at "
        "FROM consensus_votes "
        "WHERE session_id = $1 "
        "ORDER BY cast_at ASC",
        1, NULL, params, NULL, NULL, 0);

    if (PQresultStatus(result) == PGRES_TUPLES_OK) {
        int num_rows = PQntuples(result);
        for (int i = 0; i < num_rows; i++) {
            ConsensusVote vote;
            vote.vote_id = PQgetvalue(result, i, 0);
            vote.session_id = PQgetvalue(result, i, 1);
            vote.agent_id = PQgetvalue(result, i, 2);

            std::string vote_json = PQgetvalue(result, i, 3);
            try {
                vote.vote_value = nlohmann::json::parse(vote_json);
            } catch (...) {
                vote.vote_value = nlohmann::json::object();
            }

            vote.confidence = std::stod(PQgetvalue(result, i, 4));
            vote.reasoning = PQgetvalue(result, i, 5);
            vote.cast_at = PQgetvalue(result, i, 6);

            votes.push_back(vote);
        }
    }

    PQclear(result);
    return votes;
}

ConsensusResult ConsensusEngine::calculate_unanimous(const std::vector<ConsensusVote>& votes) {
    ConsensusResult result;

    if (votes.empty()) {
        result.consensus_reached = false;
        result.reasoning = "No votes cast";
        result.confidence = 0.0;
        return result;
    }

    // For unanimous, all votes must be identical
    nlohmann::json first_vote = votes[0].vote_value;
    bool all_agree = true;
    double min_confidence = votes[0].confidence;

    for (size_t i = 1; i < votes.size(); i++) {
        if (votes[i].vote_value != first_vote) {
            all_agree = false;
            break;
        }
        min_confidence = std::min(min_confidence, votes[i].confidence);
    }

    result.consensus_reached = all_agree;
    result.decision = all_agree ? first_vote : nlohmann::json::object();
    result.confidence = all_agree ? min_confidence : 0.0;
    result.reasoning = all_agree ? "Unanimous agreement" : "No unanimous agreement";
    result.votes = votes;

    return result;
}

ConsensusResult ConsensusEngine::calculate_majority(
    const std::vector<ConsensusVote>& votes,
    double threshold) {

    ConsensusResult result;

    if (votes.empty()) {
        result.consensus_reached = false;
        result.reasoning = "No votes cast";
        result.confidence = 0.0;
        return result;
    }

    // Count votes for each option
    std::map<std::string, int> vote_counts;
    std::map<std::string, double> avg_confidence;
    std::map<std::string, int> confidence_counts;

    for (const auto& vote : votes) {
        std::string vote_str = vote.vote_value.dump();
        vote_counts[vote_str]++;
        avg_confidence[vote_str] += vote.confidence;
        confidence_counts[vote_str]++;
    }

    // Find majority
    int total_votes = votes.size();
    int required_votes = std::ceil(total_votes * threshold);

    std::string winning_vote;
    int max_count = 0;
    double winning_confidence = 0.0;

    for (const auto& [vote_str, count] : vote_counts) {
        if (count > max_count) {
            max_count = count;
            winning_vote = vote_str;
            winning_confidence = avg_confidence[vote_str] / confidence_counts[vote_str];
        }
    }

    bool reached = max_count >= required_votes;

    result.consensus_reached = reached;
    result.decision = reached ? nlohmann::json::parse(winning_vote) : nlohmann::json::object();
    result.confidence = reached ? winning_confidence : 0.0;
    result.reasoning = reached ?
        ("Majority reached: " + std::to_string(max_count) + "/" + std::to_string(total_votes) + " votes") :
        ("Insufficient votes for consensus: " + std::to_string(max_count) + "/" + std::to_string(required_votes) + " required");
    result.votes = votes;

    return result;
}

ConsensusResult ConsensusEngine::calculate_weighted_voting(const std::vector<ConsensusVote>& votes) {
    ConsensusResult result;

    if (votes.empty()) {
        result.consensus_reached = false;
        result.reasoning = "No votes cast";
        result.confidence = 0.0;
        return result;
    }

    // Weight votes by confidence
    std::map<std::string, double> weighted_votes;
    double total_weight = 0.0;

    for (const auto& vote : votes) {
        std::string vote_str = vote.vote_value.dump();
        weighted_votes[vote_str] += vote.confidence;
        total_weight += vote.confidence;
    }

    // Find highest weighted option
    std::string winning_vote;
    double max_weight = 0.0;

    for (const auto& [vote_str, weight] : weighted_votes) {
        if (weight > max_weight) {
            max_weight = weight;
            winning_vote = vote_str;
        }
    }

    double winning_percentage = max_weight / total_weight;
    bool reached = winning_percentage > 0.5;  // Weighted majority

    result.consensus_reached = reached;
    result.decision = reached ? nlohmann::json::parse(winning_vote) : nlohmann::json::object();
    result.confidence = winning_percentage;
    result.reasoning = reached ?
        ("Weighted consensus reached: " + std::to_string(winning_percentage * 100) + "% confidence") :
        ("Insufficient weighted support: " + std::to_string(winning_percentage * 100) + "%");
    result.votes = votes;

    return result;
}

ConsensusResult ConsensusEngine::calculate_ranked_choice(const std::vector<ConsensusVote>& votes) {
    ConsensusResult result;

    if (votes.empty()) {
        result.consensus_reached = false;
        result.reasoning = "No votes cast";
        result.confidence = 0.0;
        return result;
    }

    // For ranked choice, we expect vote_value to be an array of preferences
    // e.g., ["option1", "option2", "option3"]
    std::map<std::string, int> first_choice_counts;
    std::map<std::string, double> avg_confidence;
    std::map<std::string, int> confidence_counts;

    for (const auto& vote : votes) {
        if (vote.vote_value.is_array() && !vote.vote_value.empty()) {
            std::string first_choice = vote.vote_value[0];
            first_choice_counts[first_choice]++;
            avg_confidence[first_choice] += vote.confidence;
            confidence_counts[first_choice]++;
        }
    }

    // Find majority of first choices
    int total_votes = votes.size();
    int required_votes = total_votes / 2 + 1;

    std::string winning_vote;
    int max_count = 0;
    double winning_confidence = 0.0;

    for (const auto& [vote_str, count] : first_choice_counts) {
        if (count > max_count) {
            max_count = count;
            winning_vote = vote_str;
            winning_confidence = avg_confidence[vote_str] / confidence_counts[vote_str];
        }
    }

    bool reached = max_count >= required_votes;

    result.consensus_reached = reached;
    result.decision = reached ? nlohmann::json(winning_vote) : nlohmann::json::object();
    result.confidence = reached ? winning_confidence : 0.0;
    result.reasoning = reached ?
        ("Ranked choice majority: " + std::to_string(max_count) + "/" + std::to_string(total_votes) + " first-choice votes") :
        ("No ranked choice majority: " + std::to_string(max_count) + "/" + std::to_string(required_votes) + " required");
    result.votes = votes;

    return result;
}

ConsensusResult ConsensusEngine::calculate_bayesian(const std::vector<ConsensusVote>& votes) {
    ConsensusResult result;

    if (votes.empty()) {
        result.consensus_reached = false;
        result.reasoning = "No votes cast";
        result.confidence = 0.0;
        return result;
    }

    // For Bayesian consensus, we expect vote_value to contain probabilities
    // This is a simplified implementation - in production, this would use proper Bayesian inference
    std::map<std::string, double> combined_probabilities;
    std::map<std::string, int> vote_counts;

    for (const auto& vote : votes) {
        if (vote.vote_value.is_object()) {
            // Assume vote_value is {"option1": 0.7, "option2": 0.3}
            for (auto& [option, prob] : vote.vote_value.items()) {
                if (prob.is_number()) {
                    combined_probabilities[option] += prob.get<double>() * vote.confidence;
                    vote_counts[option]++;
                }
            }
        }
    }

    // Normalize probabilities
    for (auto& [option, prob] : combined_probabilities) {
        prob /= vote_counts[option];
    }

    // Find option with highest probability
    std::string winning_vote;
    double max_prob = 0.0;

    for (const auto& [option, prob] : combined_probabilities) {
        if (prob > max_prob) {
            max_prob = prob;
            winning_vote = option;
        }
    }

    bool reached = max_prob > 0.5;  // Simple threshold

    result.consensus_reached = reached;
    result.decision = reached ? nlohmann::json(winning_vote) : nlohmann::json::object();
    result.confidence = max_prob;
    result.reasoning = reached ?
        ("Bayesian consensus reached: " + std::to_string(max_prob * 100) + "% probability") :
        ("Insufficient Bayesian support: " + std::to_string(max_prob * 100) + "%");
    result.votes = votes;

    return result;
}

ConsensusResult ConsensusEngine::calculate_result(const std::string& session_id) {
    // Get session details
    auto session_opt = get_session(session_id);
    if (!session_opt.has_value()) {
        ConsensusResult result;
        result.consensus_reached = false;
        result.reasoning = "Session not found";
        return result;
    }

    auto session = session_opt.value();
    auto votes = get_votes(session_id);

    // Calculate based on consensus type
    ConsensusResult result;

    switch (session.type) {
        case ConsensusType::UNANIMOUS:
            result = calculate_unanimous(votes);
            break;
        case ConsensusType::MAJORITY:
            result = calculate_majority(votes, 0.5);
            break;
        case ConsensusType::SUPERMAJORITY:
            result = calculate_majority(votes, session.threshold);
            break;
        case ConsensusType::WEIGHTED_VOTING:
            result = calculate_weighted_voting(votes);
            break;
        case ConsensusType::RANKED_CHOICE:
            result = calculate_ranked_choice(votes);
            break;
        case ConsensusType::BAYESIAN:
            result = calculate_bayesian(votes);
            break;
    }

    // Store result if consensus reached
    if (result.consensus_reached) {
        std::string result_json = result.decision.dump();
        std::string confidence_str = std::to_string(result.confidence);

        const char* params[3] = {
            result_json.c_str(),
            confidence_str.c_str(),
            session_id.c_str()
        };

        PGresult* update_result = PQexecParams(db_conn_,
            "UPDATE consensus_sessions "
            "SET result = $1::jsonb, result_confidence = $2, status = 'reached'::consensus_status "
            "WHERE session_id = $3",
            3, NULL, params, NULL, NULL, 0);
        PQclear(update_result);
    }

    return result;
}

bool ConsensusEngine::is_consensus_reached(const std::string& session_id) {
    auto result = calculate_result(session_id);
    return result.consensus_reached;
}

std::optional<ConsensusSession> ConsensusEngine::get_session(const std::string& session_id) {
    const char* params[] = {session_id.c_str()};

    PGresult* result = PQexecParams(db_conn_,
        "SELECT session_id, topic, consensus_type, status, required_votes, current_votes, "
        "       threshold, started_at, deadline, result, result_confidence "
        "FROM consensus_sessions "
        "WHERE session_id = $1",
        1, NULL, params, NULL, NULL, 0);

    if (PQresultStatus(result) != PGRES_TUPLES_OK || PQntuples(result) == 0) {
        PQclear(result);
        return std::nullopt;
    }

    ConsensusSession session;
    session.session_id = PQgetvalue(result, 0, 0);
    session.topic = PQgetvalue(result, 0, 1);
    session.type = string_to_consensus_type(PQgetvalue(result, 0, 2));
    session.status = string_to_consensus_status(PQgetvalue(result, 0, 3));
    session.required_votes = std::stoi(PQgetvalue(result, 0, 4));
    session.current_votes = std::stoi(PQgetvalue(result, 0, 5));
    session.threshold = std::stod(PQgetvalue(result, 0, 6));
    session.started_at = PQgetvalue(result, 0, 7);
    session.deadline = PQgetvalue(result, 0, 8);

    if (PQgetvalue(result, 0, 9) && std::string(PQgetvalue(result, 0, 9)) != "") {
        std::string result_json = PQgetvalue(result, 0, 9);
        try {
            session.result = nlohmann::json::parse(result_json);
        } catch (...) {
            session.result = nlohmann::json::object();
        }
    }

    if (PQgetvalue(result, 0, 10) && std::string(PQgetvalue(result, 0, 10)) != "") {
        session.result_confidence = std::stod(PQgetvalue(result, 0, 10));
    }

    PQclear(result);
    return session;
}

bool ConsensusEngine::close_session(const std::string& session_id) {
    const char* params[] = {session_id.c_str()};
    PGresult* result = PQexecParams(db_conn_,
        "UPDATE consensus_sessions "
        "SET status = 'closed'::consensus_status, closed_at = NOW() "
        "WHERE session_id = $1 AND status = 'open'::consensus_status",
        1, NULL, params, NULL, NULL, 0);

    bool success = PQresultStatus(result) == PGRES_COMMAND_OK;
    PQclear(result);
    return success;
}

void ConsensusEngine::process_expired_sessions() {
    // Close sessions that have passed their deadline
    PGresult* result = PQexec(db_conn_,
        "UPDATE consensus_sessions "
        "SET status = 'timeout'::consensus_status, closed_at = NOW() "
        "WHERE status = 'open'::consensus_status AND deadline < NOW()");

    PQclear(result);
}

} // namespace regulens