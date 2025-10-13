#ifndef CONSENSUS_ENGINE_H
#define CONSENSUS_ENGINE_H

#include <string>
#include <vector>
#include <optional>
#include <map>
#include <nlohmann/json.hpp>
#include <libpq-fe.h>

namespace regulens {

enum class ConsensusType {
    UNANIMOUS,
    MAJORITY,
    SUPERMAJORITY,
    WEIGHTED_VOTING,
    RANKED_CHOICE,
    BAYESIAN
};

enum class ConsensusStatus {
    OPEN,
    CLOSED,
    REACHED,
    FAILED,
    TIMEOUT
};

struct ConsensusSession {
    std::string session_id;
    std::string topic;
    ConsensusType type;
    ConsensusStatus status;
    int required_votes;
    int current_votes;
    double threshold;
    std::string started_at;
    std::string deadline;
    nlohmann::json result;
    double result_confidence;
};

struct ConsensusVote {
    std::string vote_id;
    std::string session_id;
    std::string agent_id;
    nlohmann::json vote_value;
    double confidence;
    std::string reasoning;
    std::string cast_at;
};

struct ConsensusResult {
    bool consensus_reached;
    nlohmann::json decision;
    double confidence;
    std::string reasoning;
    std::vector<ConsensusVote> votes;
};

class ConsensusEngine {
public:
    ConsensusEngine(PGconn* db_conn);
    ~ConsensusEngine();

    // Start new consensus session
    std::optional<std::string> start_session(
        const std::string& topic,
        const std::vector<std::string>& participant_agent_ids,
        ConsensusType type,
        const nlohmann::json& parameters = nlohmann::json::object()
    );

    // Submit vote
    bool contribute_vote(
        const std::string& session_id,
        const std::string& agent_id,
        const nlohmann::json& vote_value,
        double confidence,
        const std::string& reasoning = ""
    );

    // Calculate consensus result
    ConsensusResult calculate_result(const std::string& session_id);

    // Check if consensus reached
    bool is_consensus_reached(const std::string& session_id);

    // Get session details
    std::optional<ConsensusSession> get_session(const std::string& session_id);

    // Get all votes for session
    std::vector<ConsensusVote> get_votes(const std::string& session_id);

    // Close session
    bool close_session(const std::string& session_id);

    // Handle timeouts
    void process_expired_sessions();

private:
    PGconn* db_conn_;

    // Consensus algorithm implementations
    ConsensusResult calculate_unanimous(const std::vector<ConsensusVote>& votes);
    ConsensusResult calculate_majority(const std::vector<ConsensusVote>& votes, double threshold);
    ConsensusResult calculate_weighted_voting(const std::vector<ConsensusVote>& votes);
    ConsensusResult calculate_ranked_choice(const std::vector<ConsensusVote>& votes);
    ConsensusResult calculate_bayesian(const std::vector<ConsensusVote>& votes);

    std::string consensus_type_to_string(ConsensusType type);
    ConsensusType string_to_consensus_type(const std::string& str);
    std::string consensus_status_to_string(ConsensusStatus status);
    ConsensusStatus string_to_consensus_status(const std::string& str);
};

} // namespace regulens

#endif // CONSENSUS_ENGINE_H
