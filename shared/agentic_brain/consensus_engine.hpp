/**
 * Consensus Engine
 * Multi-agent decision making with voting algorithms
 */

#ifndef CONSENSUS_ENGINE_HPP
#define CONSENSUS_ENGINE_HPP

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <optional>
#include <chrono>
#include <nlohmann/json.hpp>
#include "../database/postgresql_connection.hpp"
#include "../logging/structured_logger.hpp"

namespace regulens {

enum class VotingAlgorithm {
    UNANIMOUS,         // All agents must agree
    MAJORITY,          // More than 50% agreement
    SUPER_MAJORITY,    // Higher threshold (e.g., 2/3)
    WEIGHTED_MAJORITY, // Agents have different voting weights
    RANKED_CHOICE,     // Ranked choice voting
    QUORUM,           // Minimum number of participants
    CONSENSUS,        // All participants agree on final decision
    PLURALITY         // Most votes wins (even without majority)
};

enum class AgentRole {
    EXPERT,           // Domain expert with specialized knowledge
    REVIEWER,         // Quality assurance and validation
    DECISION_MAKER,   // Final authority on decisions
    FACILITATOR,      // Coordinates the consensus process
    OBSERVER         // Monitors but doesn't vote
};

enum class ConsensusState {
    INITIALIZING,     // Setting up the consensus process
    COLLECTING_OPINIONS, // Gathering agent opinions
    DISCUSSING,       // Agents discussing and debating
    VOTING,          // Active voting phase
    RESOLVING_CONFLICTS, // Resolving disagreements
    REACHED_CONSENSUS,   // Agreement achieved
    DEADLOCK,         // Unable to reach consensus
    TIMEOUT,          // Process timed out
    CANCELLED         // Process was cancelled
};

enum class DecisionConfidence {
    VERY_LOW = 1,
    LOW = 2,
    MEDIUM = 3,
    HIGH = 4,
    VERY_HIGH = 5
};

struct Agent {
    std::string agent_id;
    std::string name;
    AgentRole role;
    double voting_weight = 1.0;  // Weight in weighted voting
    std::string domain_expertise; // Area of specialization
    double confidence_threshold = 0.7; // Minimum confidence to participate
    bool is_active = true;
    std::chrono::system_clock::time_point last_active;
};

struct AgentOpinion {
    std::string agent_id;
    std::string decision;        // The agent's proposed decision
    double confidence_score;     // Confidence in the decision (0.0-1.0)
    std::string reasoning;       // Explanation for the decision
    nlohmann::json supporting_data; // Evidence or data supporting the decision
    std::vector<std::string> concerns; // Potential issues or risks
    std::chrono::system_clock::time_point submitted_at;
    int round_number;           // Voting round this opinion belongs to
};

struct VotingRound {
    int round_number;
    std::string topic;
    std::string description;
    std::vector<AgentOpinion> opinions;
    std::unordered_map<std::string, int> vote_counts; // decision -> count
    ConsensusState state;
    std::chrono::system_clock::time_point started_at;
    std::chrono::system_clock::time_point ended_at;
    nlohmann::json metadata;
};

struct ConsensusResult {
    std::string consensus_id;
    std::string topic;
    std::string final_decision;
    DecisionConfidence confidence_level;
    VotingAlgorithm algorithm_used;
    std::vector<VotingRound> rounds;
    ConsensusState final_state;
    std::chrono::milliseconds total_duration;
    int total_participants;
    double agreement_percentage;
    nlohmann::json resolution_details;
    std::vector<std::string> dissenting_opinions;
    std::chrono::system_clock::time_point completed_at;
};

struct ConsensusConfiguration {
    std::string topic;
    VotingAlgorithm algorithm = VotingAlgorithm::MAJORITY;
    std::vector<Agent> participants;
    int max_rounds = 3;
    std::chrono::minutes timeout_per_round = std::chrono::minutes(10);
    double consensus_threshold = 0.7; // Minimum agreement percentage
    int min_participants = 3;
    bool allow_discussion = true;
    bool require_justification = true;
    nlohmann::json custom_rules;
};

class ConsensusEngine {
public:
    ConsensusEngine(
        std::shared_ptr<PostgreSQLConnection> db_conn,
        std::shared_ptr<StructuredLogger> logger
    );

    ~ConsensusEngine();

    // Consensus process management
    std::string initiate_consensus(const ConsensusConfiguration& config);
    ConsensusResult get_consensus_result(const std::string& consensus_id);
    ConsensusState get_consensus_state(const std::string& consensus_id);

    // Agent opinion management
    bool submit_opinion(const std::string& consensus_id, const AgentOpinion& opinion);
    std::vector<AgentOpinion> get_agent_opinions(const std::string& consensus_id, int round_number = -1);
    bool update_opinion(const std::string& consensus_id, const std::string& agent_id, const AgentOpinion& updated_opinion);

    // Voting and decision making
    bool start_voting_round(const std::string& consensus_id);
    bool end_voting_round(const std::string& consensus_id);
    ConsensusResult calculate_consensus(const std::string& consensus_id);

    // Agent management
    bool register_agent(const Agent& agent);
    bool update_agent(const std::string& agent_id, const Agent& updated_agent);
    std::optional<Agent> get_agent(const std::string& agent_id);
    std::vector<Agent> get_active_agents();
    bool deactivate_agent(const std::string& agent_id);

    // Algorithm implementations
    ConsensusResult run_unanimous_voting(const std::vector<VotingRound>& rounds, const ConsensusConfiguration& config);
    ConsensusResult run_majority_voting(const std::vector<VotingRound>& rounds, const ConsensusConfiguration& config);
    ConsensusResult run_weighted_majority_voting(const std::vector<VotingRound>& rounds, const ConsensusConfiguration& config);
    ConsensusResult run_ranked_choice_voting(const std::vector<VotingRound>& rounds, const ConsensusConfiguration& config);
    ConsensusResult run_quorum_voting(const std::vector<VotingRound>& rounds, const ConsensusConfiguration& config);

    // Conflict resolution
    std::vector<std::string> identify_conflicts(const std::vector<AgentOpinion>& opinions);
    nlohmann::json suggest_resolution_strategies(const std::vector<AgentOpinion>& opinions);
    bool resolve_conflict(const std::string& consensus_id, const std::string& resolution_strategy);

    // Analytics and monitoring
    std::unordered_map<std::string, int> get_consensus_statistics();
    std::vector<std::pair<std::string, double>> get_agent_performance_metrics();
    double calculate_decision_accuracy(const std::string& consensus_id, bool actual_outcome);

    // Configuration and optimization
    void set_default_algorithm(VotingAlgorithm algorithm);
    void set_max_rounds(int max_rounds);
    void set_timeout_per_round(std::chrono::minutes timeout);
    void optimize_for_scenario(const std::string& scenario_type);

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::shared_ptr<StructuredLogger> logger_;

    // Configuration defaults
    VotingAlgorithm default_algorithm_ = VotingAlgorithm::MAJORITY;
    int default_max_rounds_ = 3;
    std::chrono::minutes default_timeout_ = std::chrono::minutes(10);
    double default_consensus_threshold_ = 0.7;

    // In-memory state management
    std::unordered_map<std::string, ConsensusConfiguration> active_consensus_;
    std::unordered_map<std::string, std::vector<VotingRound>> consensus_rounds_;
    std::unordered_map<std::string, ConsensusResult> completed_consensus_;
    std::mutex consensus_mutex_;

    // Helper methods
    std::string generate_consensus_id();
    std::string generate_agent_id();
    bool validate_consensus_config(const ConsensusConfiguration& config);
    bool validate_agent_opinion(const AgentOpinion& opinion, const ConsensusConfiguration& config);
    DecisionConfidence calculate_confidence_level(double agreement_percentage, int rounds_used);

    // Voting algorithm helpers
    std::string determine_majority_decision(const std::unordered_map<std::string, int>& vote_counts);
    std::string determine_weighted_majority_decision(const std::vector<AgentOpinion>& opinions, const std::vector<Agent>& agents);
    std::string run_ranked_choice_elimination(std::vector<std::pair<std::string, int>> ranked_votes);
    bool has_quorum(const std::vector<AgentOpinion>& opinions, const ConsensusConfiguration& config);

    // Database operations
    bool store_consensus_config(const std::string& consensus_id, const ConsensusConfiguration& config);
    bool store_voting_round(const std::string& consensus_id, const VotingRound& round);
    bool store_agent_opinion(const std::string& consensus_id, const AgentOpinion& opinion);
    bool store_consensus_result(const ConsensusResult& result);
    bool store_agent(const Agent& agent);

    std::optional<ConsensusConfiguration> load_consensus_config(const std::string& consensus_id);
    std::vector<VotingRound> load_voting_rounds(const std::string& consensus_id);
    std::vector<AgentOpinion> load_agent_opinions(const std::string& consensus_id, int round_number = -1);
    std::optional<Agent> load_agent(const std::string& agent_id);

    // Utility methods
    double calculate_agreement_percentage(const std::vector<AgentOpinion>& opinions);
    std::vector<std::string> extract_unique_decisions(const std::vector<AgentOpinion>& opinions);
    bool is_consensus_reached(const std::vector<AgentOpinion>& opinions, const ConsensusConfiguration& config);
    nlohmann::json generate_consensus_metadata(const ConsensusResult& result);

    // Performance tracking
    void update_agent_performance(const std::string& agent_id, bool contributed_to_consensus);
    void log_consensus_event(const std::string& consensus_id, const std::string& event, const nlohmann::json& details);
};

} // namespace regulens

#endif // CONSENSUS_ENGINE_HPP
