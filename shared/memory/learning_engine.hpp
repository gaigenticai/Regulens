/**
 * Advanced Learning Engine for Compliance Agents
 *
 * Feedback-based learning system with reinforcement signals, decision optimization,
 * and adaptive behavior modification for compliance AI agents.
 *
 * Features:
 * - Human feedback integration and processing
 * - Reinforcement learning from decision outcomes
 * - Agent behavior adaptation based on feedback
 * - Performance tracking and improvement metrics
 * - Learning from both positive and negative examples
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <optional>
#include <deque>
#include <mutex>
#include <shared_mutex>
#include <nlohmann/json.hpp>

#include "../config/configuration_manager.hpp"
#include "../logging/structured_logger.hpp"
#include "../error_handler.hpp"
#include "../llm/openai_client.hpp"
#include "../llm/anthropic_client.hpp"
#include "conversation_memory.hpp"

namespace regulens {

/**
 * @brief Feedback types for learning signals
 */
enum class FeedbackType {
    CORRECTION,        // Human correction of agent decision
    APPROVAL,          // Human approval of agent decision
    ESCALATION,        // Decision escalated to human oversight
    REWARD,           // Positive reinforcement signal
    PENALTY,          // Negative reinforcement signal
    PREFERENCE,       // Human preference indication
    OUTCOME_BASED     // Learning from actual outcomes
};

/**
 * @brief Learning signal strength and confidence
 */
struct LearningSignal {
    FeedbackType type;
    double strength;              // -1.0 to 1.0 (negative to positive)
    double confidence;            // 0.0 to 1.0
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> metadata;

    LearningSignal(FeedbackType t, double s, double c = 1.0)
        : type(t), strength(s), confidence(c), timestamp(std::chrono::system_clock::now()) {}
};

/**
 * @brief Decision pattern learned from feedback
 */
struct LearnedPattern {
    std::string pattern_id;
    std::string agent_type;
    std::string decision_context;      // Context where this pattern applies
    nlohmann::json learned_decision;   // The learned decision/action
    double success_rate;               // Historical success rate
    int application_count;             // How many times applied
    double average_confidence;         // Average confidence when applied
    std::chrono::system_clock::time_point last_updated;
    std::chrono::system_clock::time_point first_learned;

    // Learning statistics
    std::vector<LearningSignal> recent_signals;
    std::unordered_map<std::string, double> context_weights;

    LearnedPattern(std::string /*aid*/, std::string atype, std::string context, nlohmann::json decision)
        : pattern_id(generate_pattern_id()), agent_type(std::move(atype)),
          decision_context(std::move(context)), learned_decision(std::move(decision)),
          success_rate(0.5), application_count(0), average_confidence(0.5),
          last_updated(std::chrono::system_clock::now()),
          first_learned(std::chrono::system_clock::now()) {}

    static std::string generate_pattern_id();
};

/**
 * @brief Agent learning profile and adaptation state
 */
struct AgentLearningProfile {
    std::string agent_id;
    std::string agent_type;

    // Learning parameters
    double learning_rate;              // How quickly agent adapts (0.0-1.0)
    double exploration_rate;           // Exploration vs exploitation balance (0.0-1.0)
    double feedback_weight;            // Weight given to human feedback vs outcomes

    // Performance metrics
    double overall_accuracy;
    double human_override_rate;
    double escalation_rate;
    int total_decisions;
    int corrected_decisions;

    // Learning history
    std::vector<LearnedPattern> learned_patterns;
    std::deque<LearningSignal> recent_feedback;  // Rolling window of feedback

    // Q-learning table: state -> action -> Q-value
    std::unordered_map<std::string, std::unordered_map<std::string, double>> q_table;
    mutable std::shared_mutex q_table_mutex;  // For thread-safe Q-table access

    // Legacy context performance (for backward compatibility)
    std::unordered_map<std::string, double> context_performance;

    // Adaptation state
    bool learning_enabled;
    std::chrono::system_clock::time_point last_adaptation;
    std::unordered_map<std::string, std::string> adaptation_metadata;

    AgentLearningProfile(std::string aid, std::string atype)
        : agent_id(std::move(aid)), agent_type(std::move(atype)),
          learning_rate(0.1), exploration_rate(0.1), feedback_weight(0.7),
          overall_accuracy(0.5), human_override_rate(0.0), escalation_rate(0.0),
          total_decisions(0), corrected_decisions(0), learning_enabled(true) {}
};

/**
 * @brief Feedback processing pipeline
 */
class FeedbackProcessor {
public:
    FeedbackProcessor(std::shared_ptr<ConfigurationManager> config,
                     StructuredLogger* logger);

    /**
     * @brief Process human feedback into learning signals
     * @param agent_id Agent that received feedback
     * @param original_decision Original agent decision
     * @param human_feedback Human feedback/correction
     * @param feedback_type Type of feedback
     * @param context Decision context
     * @return Processed learning signals
     */
    std::vector<LearningSignal> process_feedback(
        const std::string& agent_id,
        const nlohmann::json& original_decision,
        const nlohmann::json& human_feedback,
        FeedbackType feedback_type,
        const nlohmann::json& context = nullptr);

    /**
     * @brief Process outcome-based learning signals
     * @param agent_id Agent that made the decision
     * @param decision Decision made
     * @param outcome Actual outcome (success/failure)
     * @param outcome_confidence Confidence in outcome assessment
     * @return Learning signal from outcome
     */
    LearningSignal process_outcome_feedback(
        const std::string& agent_id,
        const nlohmann::json& decision,
        bool positive_outcome,
        double outcome_confidence = 1.0);

    /**
     * @brief Aggregate multiple learning signals
     * @param signals Individual learning signals
     * @return Aggregated signal with confidence weighting
     */
    LearningSignal aggregate_signals(const std::vector<LearningSignal>& signals);

private:
    std::shared_ptr<ConfigurationManager> config_;
    StructuredLogger* logger_;

    /**
     * @brief Calculate feedback strength based on correction severity
     * @param original_decision Original decision
     * @param corrected_decision Human correction
     * @return Feedback strength (-1.0 to 1.0)
     */
    double calculate_feedback_strength(const nlohmann::json& original_decision,
                                     const nlohmann::json& corrected_decision);

    /**
     * @brief Extract context features for learning
     * @param context Decision context
     * @return Feature vector for learning
     */
    std::vector<std::string> extract_context_features(const nlohmann::json& context);
};

/**
 * @brief Reinforcement learning system for agents
 */
class ReinforcementLearner {
public:
    ReinforcementLearner(std::shared_ptr<ConfigurationManager> config,
                        StructuredLogger* logger);

    /**
     * @brief Update agent policy based on learning signal
     * @param agent_profile Agent learning profile
     * @param signal Learning signal
     * @param context Decision context
     * @return Updated policy parameters
     */
    nlohmann::json update_policy(AgentLearningProfile& agent_profile,
                               const LearningSignal& signal,
                               const nlohmann::json& context);

    /**
     * @brief Select action using epsilon-greedy policy
     * @param agent_profile Agent profile
     * @param available_actions Possible actions
     * @param context Current context
     * @return Selected action with confidence
     */
    std::pair<nlohmann::json, double> select_action(
        const AgentLearningProfile& agent_profile,
        const std::vector<nlohmann::json>& available_actions,
        const nlohmann::json& context);

    /**
     * @brief Update Q-values based on reward
     * @param agent_profile Agent profile
     * @param state Current state
     * @param action Taken action
     * @param reward Received reward
     * @param next_state Next state
     */
    void update_q_value(AgentLearningProfile& agent_profile,
                       const std::string& state,
                       const std::string& action,
                       double reward,
                       const std::string& next_state);

private:
    std::shared_ptr<ConfigurationManager> config_;
    StructuredLogger* logger_;

    // Q-learning parameters
    double alpha_;  // Learning rate
    double gamma_;  // Discount factor
    double epsilon_; // Exploration rate

    /**
     * @brief Get state representation for Q-learning
     * @param context Decision context
     * @return State string
     */
    std::string get_state_representation(const nlohmann::json& context);

    /**
     * @brief Get action representation
     * @param action Decision/action
     * @return Action string
     */
    std::string get_action_representation(const nlohmann::json& action);

    /**
     * @brief Calculate reward from learning signal
     * @param signal Learning signal
     * @return Reward value
     */
    double calculate_reward(const LearningSignal& signal);
};

/**
 * @brief Pattern learning and adaptation system
 */
class PatternLearner {
public:
    PatternLearner(std::shared_ptr<ConfigurationManager> config,
                  std::shared_ptr<OpenAIClient> openai_client,
                  std::shared_ptr<AnthropicClient> anthropic_client,
                  StructuredLogger* logger,
                  ErrorHandler* error_handler);

    /**
     * @brief Learn new pattern from feedback
     * @param agent_profile Agent learning profile
     * @param context Decision context
     * @param successful_decision Decision that worked well
     * @param feedback_signals Supporting feedback
     * @return Learned pattern
     */
    LearnedPattern learn_pattern(AgentLearningProfile& agent_profile,
                               const nlohmann::json& context,
                               const nlohmann::json& successful_decision,
                               const std::vector<LearningSignal>& feedback_signals);

    /**
     * @brief Apply learned pattern to new context
     * @param agent_profile Agent profile
     * @param context Current context
     * @return Applicable learned patterns with confidence scores
     */
    std::vector<std::pair<LearnedPattern, double>> apply_patterns(
        const AgentLearningProfile& agent_profile,
        const nlohmann::json& context);

    /**
     * @brief Update pattern success rates
     * @param pattern_id Pattern to update
     * @param success Whether application was successful
     * @param confidence Confidence in outcome
     */
    void update_pattern_success(const std::string& pattern_id,
                              bool success,
                              double confidence);

    /**
     * @brief Consolidate similar patterns
     * @param agent_profile Agent profile to consolidate patterns for
     */
    void consolidate_patterns(AgentLearningProfile& agent_profile);

private:
    std::shared_ptr<ConfigurationManager> config_;
    std::shared_ptr<OpenAIClient> openai_client_;
    std::shared_ptr<AnthropicClient> anthropic_client_;
    StructuredLogger* logger_;
    ErrorHandler* error_handler_;

    /**
     * @brief Generate pattern description using LLM
     * @param context Decision context
     * @param decision Successful decision
     * @return Natural language pattern description
     */
    std::string generate_pattern_description(const nlohmann::json& context,
                                           const nlohmann::json& decision);

    /**
     * @brief Calculate pattern similarity
     * @param pattern1 First pattern
     * @param pattern2 Second pattern
     * @return Similarity score (0.0-1.0)
     */
    double calculate_pattern_similarity(const LearnedPattern& pattern1,
                                     const LearnedPattern& pattern2);
};

/**
 * @brief Main learning engine coordinating all learning systems
 */
class LearningEngine {
public:
    LearningEngine(std::shared_ptr<ConfigurationManager> config,
                  std::shared_ptr<ConversationMemory> memory,
                  std::shared_ptr<OpenAIClient> openai_client,
                  std::shared_ptr<AnthropicClient> anthropic_client,
                  StructuredLogger* logger,
                  ErrorHandler* error_handler);

    ~LearningEngine();

    /**
     * @brief Initialize the learning engine
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * @brief Register agent for learning
     * @param agent_id Agent identifier
     * @param agent_type Agent type
     * @return true if registration successful
     */
    bool register_agent(const std::string& agent_id, const std::string& agent_type);

    /**
     * @brief Process feedback for agent learning
     * @param agent_id Agent identifier
     * @param conversation_id Conversation/memory identifier
     * @param feedback Human feedback
     * @param feedback_type Type of feedback
     * @return Learning outcome
     */
    nlohmann::json process_feedback(const std::string& agent_id,
                                  const std::string& conversation_id,
                                  const nlohmann::json& feedback,
                                  FeedbackType feedback_type);

    /**
     * @brief Get learning recommendations for agent
     * @param agent_id Agent identifier
     * @param context Current decision context
     * @return Learning-based recommendations
     */
    nlohmann::json get_learning_recommendations(const std::string& agent_id,
                                              const nlohmann::json& context);

    /**
     * @brief Adapt agent behavior based on accumulated learning
     * @param agent_id Agent to adapt
     * @return Adaptation results
     */
    nlohmann::json adapt_agent_behavior(const std::string& agent_id);

    /**
     * @brief Get learning statistics for agent or system
     * @param agent_id Optional specific agent, empty for system-wide stats
     * @return Learning statistics
     */
    nlohmann::json get_learning_statistics(const std::optional<std::string>& agent_id = std::nullopt);

    /**
     * @brief Export learned patterns and agent profiles
     * @param agent_id Optional specific agent, empty for all
     * @return Exported learning data
     */
    nlohmann::json export_learning_data(const std::optional<std::string>& agent_id = std::nullopt);

    /**
     * @brief Reset agent learning (for testing/debugging)
     * @param agent_id Agent to reset
     * @return true if reset successful
     */
    bool reset_agent_learning(const std::string& agent_id);

private:
    std::shared_ptr<ConfigurationManager> config_;
    std::shared_ptr<ConversationMemory> memory_;
    std::shared_ptr<OpenAIClient> openai_client_;
    std::shared_ptr<AnthropicClient> anthropic_client_;
    StructuredLogger* logger_;
    ErrorHandler* error_handler_;

    // Learning subsystems
    std::unique_ptr<FeedbackProcessor> feedback_processor_;
    std::unique_ptr<ReinforcementLearner> reinforcement_learner_;
    std::unique_ptr<PatternLearner> pattern_learner_;

    // Agent learning profiles
    std::unordered_map<std::string, AgentLearningProfile> agent_profiles_;
    mutable std::mutex profiles_mutex_;

    // Learning statistics
    std::atomic<size_t> feedback_processed_{0};
    std::atomic<size_t> patterns_learned_{0};
    std::atomic<size_t> adaptations_performed_{0};

    /**
     * @brief Get or create agent learning profile
     * @param agent_id Agent identifier
     * @param agent_type Agent type
     * @return Agent learning profile
     */
    AgentLearningProfile& get_or_create_profile(const std::string& agent_id,
                                              const std::string& agent_type);

    /**
     * @brief Update agent performance metrics
     * @param profile Agent profile to update
     * @param feedback_type Type of feedback received
     * @param signal_strength Feedback signal strength
     */
    void update_performance_metrics(AgentLearningProfile& profile,
                                  FeedbackType feedback_type,
                                  double signal_strength);

    /**
     * @brief Persist learning data to database
     * @param agent_id Agent identifier
     * @param data Learning data to persist
     */
    void persist_learning_data(const std::string& agent_id, const nlohmann::json& data);

    /**
     * @brief Load learning data from database
     * @param agent_id Agent identifier
     * @return Loaded learning data
     */
    nlohmann::json load_learning_data(const std::string& agent_id);

    /**
     * @brief Perform periodic learning maintenance
     */
    void perform_learning_maintenance();
};

/**
 * @brief Create learning engine instance
 * @param config Configuration manager
 * @param memory Conversation memory system
 * @param openai_client OpenAI client
 * @param anthropic_client Anthropic client
 * @param logger Structured logger
 * @param error_handler Error handler
 * @return Shared pointer to learning engine
 */
std::shared_ptr<LearningEngine> create_learning_engine(
    std::shared_ptr<ConfigurationManager> config,
    std::shared_ptr<ConversationMemory> memory,
    std::shared_ptr<OpenAIClient> openai_client,
    std::shared_ptr<AnthropicClient> anthropic_client,
    StructuredLogger* logger,
    ErrorHandler* error_handler);

} // namespace regulens
