/**
 * Advanced Learning Engine Implementation
 *
 * Feedback-based learning system with reinforcement signals, pattern learning,
 * and adaptive behavior modification for compliance AI agents.
 */

#include "learning_engine.hpp"
#include <algorithm>
#include <cmath>
#include <random>
#include <sstream>

namespace regulens {

// Helper function for state representation (used by multiple classes)
static std::string get_state_representation(const nlohmann::json& context) {
    // Create a simple state representation
    std::string state = "context:";

    if (context.contains("domain")) {
        state += context["domain"].get<std::string>() + ";";
    }

    if (context.contains("risk_level")) {
        state += "risk:" + context["risk_level"].get<std::string>() + ";";
    }

    if (context.contains("transaction_type")) {
        state += "type:" + context["transaction_type"].get<std::string>() + ";";
    }

    return state;
}

// LearnedPattern Implementation

std::string LearnedPattern::generate_pattern_id() {
    static std::atomic<size_t> counter{0};
    auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();

    std::stringstream ss;
    ss << "pattern_" << timestamp << "_" << counter++;
    return ss.str();
}

// FeedbackProcessor Implementation

FeedbackProcessor::FeedbackProcessor(std::shared_ptr<ConfigurationManager> config,
                                   StructuredLogger* logger)
    : config_(config), logger_(logger) {}

std::vector<LearningSignal> FeedbackProcessor::process_feedback(
    const std::string& agent_id,
    const nlohmann::json& original_decision,
    const nlohmann::json& human_feedback,
    LearningFeedbackType feedback_type,
    const nlohmann::json& context) {

    std::vector<LearningSignal> signals;

    try {
        double feedback_strength = 0.0;

        switch (feedback_type) {
            case LearningFeedbackType::CORRECTION: {
                feedback_strength = calculate_feedback_strength(original_decision, human_feedback);
                signals.emplace_back(LearningFeedbackType::CORRECTION, feedback_strength, 0.9);
                break;
            }
            case LearningFeedbackType::APPROVAL: {
                feedback_strength = 0.8; // Strong positive signal for approval
                signals.emplace_back(LearningFeedbackType::APPROVAL, feedback_strength, 0.95);
                break;
            }
            case LearningFeedbackType::ESCALATION: {
                feedback_strength = -0.6; // Negative signal for escalation
                signals.emplace_back(LearningFeedbackType::ESCALATION, feedback_strength, 0.85);
                break;
            }
            case LearningFeedbackType::PREFERENCE: {
                feedback_strength = 0.5; // Moderate positive signal
                signals.emplace_back(LearningFeedbackType::PREFERENCE, feedback_strength, 0.8);
                break;
            }
            default:
                signals.emplace_back(feedback_type, 0.0, 0.5);
                break;
        }

        // Add context-based signals
        if (context.contains("urgency") && context["urgency"] == "high") {
            signals.emplace_back(LearningFeedbackType::REWARD, 0.2, 0.7); // Reward for handling urgent cases
        }

        // Add metadata
        for (auto& signal : signals) {
            signal.metadata["agent_id"] = agent_id;
            signal.metadata["feedback_type"] = std::to_string(static_cast<int>(feedback_type));
            if (context.contains("domain")) {
                signal.metadata["domain"] = context["domain"];
            }
        }

        if (logger_) {
            logger_->info("Processed feedback for agent " + agent_id + ": " +
                         std::to_string(signals.size()) + " learning signals generated",
                         "FeedbackProcessor", "process_feedback");
        }

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Failed to process feedback: " + std::string(e.what()),
                          "FeedbackProcessor", "process_feedback");
        }
        // Return neutral signal on error
        signals.emplace_back(LearningFeedbackType::CORRECTION, 0.0, 0.5);
    }

    return signals;
}

LearningSignal FeedbackProcessor::process_outcome_feedback(
    const std::string& agent_id,
    const nlohmann::json& decision,
    bool positive_outcome,
    double outcome_confidence) {

    double strength = positive_outcome ? 0.7 : -0.7;
    strength *= outcome_confidence; // Weight by confidence

    LearningSignal signal(LearningFeedbackType::OUTCOME_BASED, strength, outcome_confidence);
    signal.metadata["agent_id"] = agent_id;
    signal.metadata["positive_outcome"] = positive_outcome ? "true" : "false";

    return signal;
}

LearningSignal FeedbackProcessor::aggregate_signals(const std::vector<LearningSignal>& signals) {
    if (signals.empty()) {
        return LearningSignal(LearningFeedbackType::REWARD, 0.0, 0.0);
    }

    // Weighted average based on confidence
    double total_weighted_strength = 0.0;
    double total_confidence = 0.0;

    for (const auto& signal : signals) {
        total_weighted_strength += signal.strength * signal.confidence;
        total_confidence += signal.confidence;
    }

    double aggregated_strength = total_confidence > 0.0 ?
        total_weighted_strength / total_confidence : 0.0;

    // Determine dominant feedback type
    std::unordered_map<LearningFeedbackType, double> type_weights;
    for (const auto& signal : signals) {
        type_weights[signal.type] += signal.confidence;
    }

    LearningFeedbackType dominant_type = LearningFeedbackType::REWARD;
    double max_weight = 0.0;
    for (const auto& [type, weight] : type_weights) {
        if (weight > max_weight) {
            max_weight = weight;
            dominant_type = type;
        }
    }

    LearningSignal aggregated(dominant_type, aggregated_strength,
                            total_confidence / signals.size());

    aggregated.metadata["aggregated_signals"] = std::to_string(signals.size());
    aggregated.metadata["dominant_type"] = std::to_string(static_cast<int>(dominant_type));

    return aggregated;
}

double FeedbackProcessor::calculate_feedback_strength(const nlohmann::json& original_decision,
                                                    const nlohmann::json& corrected_decision) {
    // Simple strength calculation based on decision differences
    if (original_decision == corrected_decision) {
        return 0.0; // No correction needed
    }

    // Check for opposite decisions (strong correction)
    if (original_decision.contains("decision") && corrected_decision.contains("decision")) {
        std::string orig = original_decision["decision"];
        std::string corr = corrected_decision["decision"];

        if ((orig == "approve" && corr == "deny") || (orig == "deny" && corr == "approve")) {
            return -0.9; // Very strong negative signal
        }
    }

    // Check for confidence differences
    if (original_decision.contains("confidence") && corrected_decision.contains("confidence")) {
        double orig_conf = original_decision["confidence"];
        double corr_conf = corrected_decision["confidence"];

        if (orig_conf > 0.8 && corr_conf < 0.5) {
            return -0.7; // Overconfident decision corrected
        }
    }

    return -0.5; // Moderate correction signal
}

// ReinforcementLearner Implementation

ReinforcementLearner::ReinforcementLearner(std::shared_ptr<ConfigurationManager> config,
                                         StructuredLogger* logger)
    : config_(config), logger_(logger),
      alpha_(config_->get_double("LEARNING_ALPHA").value_or(0.1)),
      gamma_(config_->get_double("LEARNING_GAMMA").value_or(0.9)),
      epsilon_(config_->get_double("LEARNING_EPSILON").value_or(0.1)) {}

nlohmann::json ReinforcementLearner::update_policy(AgentLearningProfile& agent_profile,
                                                const LearningSignal& signal,
                                                const nlohmann::json& context) {

    nlohmann::json policy_update;

    try {
        std::string state = get_state_representation(context);
        double reward = calculate_reward(signal);

        // Simple policy update based on feedback
        if (signal.type == LearningFeedbackType::CORRECTION && signal.strength < -0.5) {
            // Reduce exploration for frequently corrected contexts
            agent_profile.exploration_rate = std::max(0.01, agent_profile.exploration_rate * 0.9);
            policy_update["exploration_reduced"] = true;
        } else if (signal.type == LearningFeedbackType::APPROVAL && signal.strength > 0.5) {
            // Increase learning rate for successful contexts
            agent_profile.learning_rate = std::min(0.5, agent_profile.learning_rate * 1.1);
            policy_update["learning_increased"] = true;
        }

        // Update context performance
        agent_profile.context_performance[state] =
            (agent_profile.context_performance[state] * 0.9) + (reward * 0.1);

        policy_update["state"] = state;
        policy_update["reward"] = reward;
        policy_update["new_exploration_rate"] = agent_profile.exploration_rate;
        policy_update["new_learning_rate"] = agent_profile.learning_rate;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Failed to update policy: " + std::string(e.what()),
                          "ReinforcementLearner", "update_policy");
        }
        policy_update["error"] = std::string(e.what());
    }

    return policy_update;
}

std::pair<nlohmann::json, double> ReinforcementLearner::select_action(
    const AgentLearningProfile& agent_profile,
    const std::vector<nlohmann::json>& available_actions,
    const nlohmann::json& context) {

    // Epsilon-greedy action selection
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);

    if (dis(gen) < agent_profile.exploration_rate) {
        // Explore: random action
        std::uniform_int_distribution<> action_dis(0, available_actions.size() - 1);
        size_t random_index = action_dis(gen);
        return {available_actions[random_index], 0.5}; // Lower confidence for exploration
    } else {
        // Exploit: choose best action based on Q-values
        std::string current_state = get_state_representation(context);

        // Read Q-table (shared lock for reading)
        std::shared_lock<std::shared_mutex> lock(agent_profile.q_table_mutex);

        size_t best_index = 0;
        double best_q_value = std::numeric_limits<double>::lowest();
        bool found_q_value = false;

        for (size_t i = 0; i < available_actions.size(); ++i) {
            std::string action_str = get_action_representation(available_actions[i]);
            double q_value = 0.0;

            // Look up Q-value for this state-action pair
            auto state_it = agent_profile.q_table.find(current_state);
            if (state_it != agent_profile.q_table.end()) {
                auto action_it = state_it->second.find(action_str);
                if (action_it != state_it->second.end()) {
                    q_value = action_it->second;
                    found_q_value = true;
                }
            }

            // If no Q-value found, fall back to pattern-based scoring
            if (!found_q_value) {
                q_value = 0.5; // Default neutral score
                for (const auto& pattern : agent_profile.learned_patterns) {
                    if (pattern.learned_decision == available_actions[i]) {
                        q_value = pattern.success_rate;
                        break;
                    }
                }
            }

            if (q_value > best_q_value) {
                best_q_value = q_value;
                best_index = i;
            }
        }

        // Convert Q-value to confidence (normalize to 0.5-1.0 range)
        double confidence = 0.5 + (best_q_value / (1.0 + std::abs(best_q_value))) * 0.5;
        confidence = std::max(0.5, std::min(1.0, confidence));

        return {available_actions[best_index], confidence};
    }
}

void ReinforcementLearner::update_q_value(AgentLearningProfile& agent_profile,
                                        const std::string& state,
                                        const std::string& action,
                                        double reward,
                                        const std::string& next_state) {

    // Thread-safe Q-table update
    std::unique_lock<std::shared_mutex> lock(agent_profile.q_table_mutex);

    // Get current Q-value for state-action pair
    double current_q = 0.0;
    auto state_it = agent_profile.q_table.find(state);
    if (state_it != agent_profile.q_table.end()) {
        auto action_it = state_it->second.find(action);
        if (action_it != state_it->second.end()) {
            current_q = action_it->second;
        }
    }

    // Get max Q-value for next state (best action in next state)
    double max_next_q = 0.0;
    auto next_state_it = agent_profile.q_table.find(next_state);
    if (next_state_it != agent_profile.q_table.end() && !next_state_it->second.empty()) {
        for (const auto& [action, q_value] : next_state_it->second) {
            max_next_q = std::max(max_next_q, q_value);
        }
    }

    // Q-learning update: Q(s,a) = Q(s,a) + α[r + γ*max(Q(s',a')) - Q(s,a)]
    double new_q = current_q + alpha_ * (reward + gamma_ * max_next_q - current_q);

    // Store updated Q-value
    agent_profile.q_table[state][action] = new_q;

    // Update legacy context_performance for backward compatibility
    std::string state_action_key = state + "|" + action;
    agent_profile.context_performance[state_action_key] = new_q;
}

std::string ReinforcementLearner::get_state_representation(const nlohmann::json& context) {
    // Create a simple state representation
    std::string state = "context:";

    if (context.contains("domain")) {
        state += context["domain"].get<std::string>() + ";";
    }

    if (context.contains("risk_level")) {
        state += "risk:" + context["risk_level"].get<std::string>() + ";";
    }

    if (context.contains("transaction_type")) {
        state += "type:" + context["transaction_type"].get<std::string>() + ";";
    }

    return state;
}

std::string ReinforcementLearner::get_action_representation(const nlohmann::json& action) {
    if (action.contains("decision")) {
        return "decision:" + action["decision"].get<std::string>();
    }
    return action.dump();
}

double ReinforcementLearner::calculate_reward(const LearningSignal& signal) {
    // Convert learning signal to reward
    double reward = signal.strength * signal.confidence;

    // Scale and clamp reward
    reward = std::max(-1.0, std::min(1.0, reward));

    return reward;
}

// PatternLearner Implementation

PatternLearner::PatternLearner(std::shared_ptr<ConfigurationManager> config,
                             std::shared_ptr<OpenAIClient> openai_client,
                             std::shared_ptr<AnthropicClient> anthropic_client,
                             StructuredLogger* logger,
                             ErrorHandler* error_handler)
    : config_(config), openai_client_(openai_client), anthropic_client_(anthropic_client),
      logger_(logger), error_handler_(error_handler) {}

LearnedPattern PatternLearner::learn_pattern(AgentLearningProfile& agent_profile,
                                          const nlohmann::json& context,
                                          const nlohmann::json& successful_decision,
                                          const std::vector<LearningSignal>& feedback_signals) {

    LearnedPattern pattern("", agent_profile.agent_type,
                          get_state_representation(context), successful_decision);

    try {
        // Calculate initial success rate from feedback
        double total_signal_strength = 0.0;
        for (const auto& signal : feedback_signals) {
            total_signal_strength += signal.strength * signal.confidence;
        }
        pattern.success_rate = std::max(0.5, std::min(1.0,
            0.5 + (total_signal_strength / feedback_signals.size()) * 0.5));

        // Generate pattern description
        pattern.context_weights = extract_context_features(context);

        // Store recent signals
        pattern.recent_signals = feedback_signals;
        if (pattern.recent_signals.size() > 10) {
            pattern.recent_signals.erase(pattern.recent_signals.begin() + 10, pattern.recent_signals.end()); // Keep only recent 10
        }

        // Add to agent's learned patterns
        agent_profile.learned_patterns.push_back(pattern);

        if (logger_) {
            logger_->info("Learned new pattern for agent " + agent_profile.agent_id +
                         " with success rate: " + std::to_string(pattern.success_rate),
                         "PatternLearner", "learn_pattern");
        }

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Failed to learn pattern: " + std::string(e.what()),
                          "PatternLearner", "learn_pattern");
        }
    }

    return pattern;
}

std::vector<std::pair<LearnedPattern, double>> PatternLearner::apply_patterns(
    const AgentLearningProfile& agent_profile,
    const nlohmann::json& context) {

    std::vector<std::pair<LearnedPattern, double>> applicable_patterns;

    try {
        std::string current_state = get_state_representation(context);
        auto current_features = extract_context_features(context);

        for (const auto& pattern : agent_profile.learned_patterns) {
            double similarity = 0.0;

            // Check context similarity
            if (pattern.decision_context == current_state) {
                similarity = 1.0; // Exact match
            } else {
                // Calculate feature similarity
                size_t common_features = 0;
                for (const auto& [feature, weight] : current_features) {
                    if (pattern.context_weights.count(feature)) {
                        common_features++;
                        similarity += weight * pattern.context_weights.at(feature);
                    }
                }
                similarity = common_features > 0 ? similarity / common_features : 0.0;
            }

            // Apply success rate as confidence modifier
            double confidence = similarity * pattern.success_rate;

            if (confidence > 0.3) { // Minimum threshold
                applicable_patterns.emplace_back(pattern, confidence);
            }
        }

        // Sort by confidence (highest first)
        std::sort(applicable_patterns.begin(), applicable_patterns.end(),
                 [](const auto& a, const auto& b) { return a.second > b.second; });

        // Limit to top 5 patterns
        if (applicable_patterns.size() > 5) {
            applicable_patterns.erase(applicable_patterns.begin() + 5, applicable_patterns.end());
        }

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Failed to apply patterns: " + std::string(e.what()),
                          "PatternLearner", "apply_patterns");
        }
    }

    return applicable_patterns;
}

void PatternLearner::update_pattern_success(const std::string& pattern_id,
                                         bool success,
                                         double confidence) {

    // In a real implementation, this would update the pattern in the agent's profile
    // For now, just log the update
    if (logger_) {
        logger_->info("Updated pattern " + pattern_id + " success: " +
                     (success ? "true" : "false") + " with confidence: " +
                     std::to_string(confidence),
                     "PatternLearner", "update_pattern_success");
    }
}

void PatternLearner::consolidate_patterns(AgentLearningProfile& agent_profile) {
    // Simple consolidation - merge similar patterns
    std::vector<LearnedPattern> consolidated;

    for (const auto& pattern : agent_profile.learned_patterns) {
        bool merged = false;

        for (auto& existing : consolidated) {
            if (calculate_pattern_similarity(pattern, existing) > 0.8) {
                // Merge patterns
                existing.success_rate = (existing.success_rate + pattern.success_rate) / 2.0;
                existing.application_count += pattern.application_count;
                merged = true;
                break;
            }
        }

        if (!merged) {
            consolidated.push_back(pattern);
        }
    }

    agent_profile.learned_patterns = std::move(consolidated);

    if (logger_) {
        logger_->info("Consolidated patterns for agent " + agent_profile.agent_id +
                     ": " + std::to_string(agent_profile.learned_patterns.size()) +
                     " patterns remaining",
                     "PatternLearner", "consolidate_patterns");
    }
}

std::string PatternLearner::generate_pattern_description(const nlohmann::json& context,
                                                      const nlohmann::json& decision) {
    // Simple pattern description generation
    std::string description = "Pattern for ";

    if (context.contains("domain")) {
        description += context["domain"].get<std::string>() + " ";
    }

    if (decision.contains("decision")) {
        description += "decision: " + decision["decision"].get<std::string>();
    }

    return description;
}

double PatternLearner::calculate_pattern_similarity(const LearnedPattern& pattern1,
                                                 const LearnedPattern& pattern2) {
    // Simple similarity calculation based on context and decision
    double similarity = 0.0;

    if (pattern1.decision_context == pattern2.decision_context) {
        similarity += 0.5;
    }

    if (pattern1.learned_decision == pattern2.learned_decision) {
        similarity += 0.5;
    }

    return similarity;
}

std::unordered_map<std::string, double> PatternLearner::extract_context_features(const nlohmann::json& context) {
    std::unordered_map<std::string, double> features;

    if (context.contains("domain")) {
        features["domain:" + context["domain"].get<std::string>()] = 1.0;
    }

    if (context.contains("risk_level")) {
        features["risk:" + context["risk_level"].get<std::string>()] = 0.8;
    }

    if (context.contains("transaction_type")) {
        features["type:" + context["transaction_type"].get<std::string>()] = 0.7;
    }

    if (context.contains("amount")) {
        double amount = context["amount"];
        if (amount > 10000) features["high_amount"] = 0.9;
        else if (amount > 1000) features["medium_amount"] = 0.6;
        else features["low_amount"] = 0.4;
    }

    return features;
}

// LearningEngine Implementation

LearningEngine::LearningEngine(std::shared_ptr<ConfigurationManager> config,
                             std::shared_ptr<ConversationMemory> memory,
                             std::shared_ptr<OpenAIClient> openai_client,
                             std::shared_ptr<AnthropicClient> anthropic_client,
                             StructuredLogger* logger,
                             ErrorHandler* error_handler)
    : config_(config), memory_(memory), openai_client_(openai_client),
      anthropic_client_(anthropic_client), logger_(logger), error_handler_(error_handler) {}

LearningEngine::~LearningEngine() = default;

bool LearningEngine::initialize() {
    if (logger_) {
        logger_->info("Initializing LearningEngine", "LearningEngine", "initialize");
    }

    try {
        feedback_processor_ = std::make_unique<FeedbackProcessor>(config_, logger_);
        reinforcement_learner_ = std::make_unique<ReinforcementLearner>(config_, logger_);
        pattern_learner_ = std::make_unique<PatternLearner>(config_, openai_client_,
                                                          anthropic_client_, logger_, error_handler_);

        if (logger_) {
            logger_->info("LearningEngine initialized successfully", "LearningEngine", "initialize");
        }

        return true;

    } catch (const std::exception& e) {
        if (error_handler_) {
            error_handler_->report_error(ErrorInfo{
                ErrorCategory::CONFIGURATION,
                ErrorSeverity::HIGH,
                "LearningEngine",
                "initialize",
                "Failed to initialize learning engine: " + std::string(e.what()),
                "Learning engine initialization failure"
            });
        }

        if (logger_) {
            logger_->error("Failed to initialize LearningEngine: " + std::string(e.what()),
                          "LearningEngine", "initialize");
        }

        return false;
    }
}

bool LearningEngine::register_agent(const std::string& agent_id, const std::string& agent_type) {
    std::unique_lock<std::mutex> lock(profiles_mutex_);

    if (agent_profiles_.find(agent_id) != agent_profiles_.end()) {
        return false; // Already registered
    }

    agent_profiles_.emplace(std::piecewise_construct,
                          std::forward_as_tuple(agent_id),
                          std::forward_as_tuple(agent_id, agent_type));

    if (logger_) {
        logger_->info("Registered agent for learning: " + agent_id + " (" + agent_type + ")",
                     "LearningEngine", "register_agent");
    }

    return true;
}

nlohmann::json LearningEngine::process_feedback(const std::string& agent_id,
                                             const std::string& conversation_id,
                                             const nlohmann::json& feedback,
                                             LearningFeedbackType feedback_type) {

    nlohmann::json result = {{"success", false}};

    try {
        // Get agent profile
        auto& profile = get_or_create_profile(agent_id, "unknown");

        // Retrieve original memory entry
        auto memory_entry = memory_->retrieve_memory(conversation_id);
        if (!memory_entry) {
            result["error"] = "Memory entry not found";
            return result;
        }

        // Process feedback into learning signals
        auto learning_signals = feedback_processor_->process_feedback(
            agent_id, memory_entry->context.value("decision", nlohmann::json::object()),
            feedback, feedback_type, memory_entry->context);

        // Aggregate signals
        LearningSignal aggregated_signal = feedback_processor_->aggregate_signals(learning_signals);

        // Update memory with feedback
        memory_->update_with_feedback(conversation_id, feedback,
                                    feedback_type_to_string(feedback_type),
                                    aggregated_signal.strength);

        // Update agent profile
        update_performance_metrics(profile, feedback_type, aggregated_signal.strength);

        // Learn patterns if feedback is positive
        if (aggregated_signal.strength > 0.3) {
            auto pattern = pattern_learner_->learn_pattern(profile, memory_entry->context,
                                                         feedback, learning_signals);
            patterns_learned_++;
        }

        // Update reinforcement learning
        reinforcement_learner_->update_policy(profile, aggregated_signal, memory_entry->context);

        feedback_processed_++;

        result["success"] = true;
        result["signals_processed"] = learning_signals.size();
        result["aggregated_signal_strength"] = aggregated_signal.strength;

        if (logger_) {
            logger_->info("Processed feedback for agent " + agent_id +
                         ": " + std::to_string(learning_signals.size()) + " signals",
                         "LearningEngine", "process_feedback");
        }

    } catch (const std::exception& e) {
        if (error_handler_) {
            error_handler_->report_error(ErrorInfo{
                ErrorCategory::PROCESSING,
                ErrorSeverity::MEDIUM,
                "LearningEngine",
                "process_feedback",
                "Failed to process feedback: " + std::string(e.what()),
                "Feedback processing failure"
            });
        }

        result["error"] = std::string(e.what());

        if (logger_) {
            logger_->error("Failed to process feedback: " + std::string(e.what()),
                          "LearningEngine", "process_feedback");
        }
    }

    return result;
}

nlohmann::json LearningEngine::get_learning_recommendations(const std::string& agent_id,
                                                         const nlohmann::json& context) {

    nlohmann::json recommendations = {
        {"agent_id", agent_id},
        {"recommendations", nlohmann::json::array()},
        {"confidence", 0.0}
    };

    try {
        std::unique_lock<std::mutex> lock(profiles_mutex_);

        auto profile_it = agent_profiles_.find(agent_id);
        if (profile_it == agent_profiles_.end()) {
            recommendations["error"] = "Agent not registered for learning";
            return recommendations;
        }

        const auto& profile = profile_it->second;

        // Get applicable patterns
        auto applicable_patterns = pattern_learner_->apply_patterns(profile, context);

        // Generate recommendations
        for (const auto& [pattern, confidence] : applicable_patterns) {
            recommendations["recommendations"].push_back({
                {"pattern_id", pattern.pattern_id},
                {"decision", pattern.learned_decision},
                {"confidence", confidence},
                {"success_rate", pattern.success_rate},
                {"application_count", pattern.application_count}
            });
        }

        // Calculate overall confidence
        double total_confidence = 0.0;
        for (const auto& rec : recommendations["recommendations"]) {
            total_confidence += rec["confidence"].get<double>();
        }

        recommendations["confidence"] = recommendations["recommendations"].size() > 0 ?
            total_confidence / recommendations["recommendations"].size() : 0.0;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Failed to get learning recommendations: " + std::string(e.what()),
                          "LearningEngine", "get_learning_recommendations");
        }
        recommendations["error"] = std::string(e.what());
    }

    return recommendations;
}

nlohmann::json LearningEngine::adapt_agent_behavior(const std::string& agent_id) {
    nlohmann::json adaptation_result = {{"success", false}, {"agent_id", agent_id}};

    try {
        std::unique_lock<std::mutex> lock(profiles_mutex_);

        auto profile_it = agent_profiles_.find(agent_id);
        if (profile_it == agent_profiles_.end()) {
            adaptation_result["error"] = "Agent not registered for learning";
            return adaptation_result;
        }

        auto& profile = profile_it->second;

        // Consolidate learned patterns
        pattern_learner_->consolidate_patterns(profile);

        // Adjust learning parameters based on performance
        if (profile.overall_accuracy > 0.8) {
            // High performer - reduce exploration, increase learning rate
            profile.exploration_rate = std::max(0.01, profile.exploration_rate * 0.8);
            profile.learning_rate = std::min(0.3, profile.learning_rate * 1.2);
        } else if (profile.overall_accuracy < 0.6) {
            // Low performer - increase exploration, adjust learning rate
            profile.exploration_rate = std::min(0.3, profile.exploration_rate * 1.5);
            profile.learning_rate = std::max(0.05, profile.learning_rate * 0.8);
        }

        profile.last_adaptation = std::chrono::system_clock::now();
        adaptations_performed_++;

        adaptation_result["success"] = true;
        adaptation_result["new_exploration_rate"] = profile.exploration_rate;
        adaptation_result["new_learning_rate"] = profile.learning_rate;
        adaptation_result["patterns_consolidated"] = profile.learned_patterns.size();

        if (logger_) {
            logger_->info("Adapted behavior for agent " + agent_id +
                         ": exploration=" + std::to_string(profile.exploration_rate) +
                         ", learning=" + std::to_string(profile.learning_rate),
                         "LearningEngine", "adapt_agent_behavior");
        }

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Failed to adapt agent behavior: " + std::string(e.what()),
                          "LearningEngine", "adapt_agent_behavior");
        }
        adaptation_result["error"] = std::string(e.what());
    }

    return adaptation_result;
}

nlohmann::json LearningEngine::get_learning_statistics(const std::optional<std::string>& agent_id) {
    nlohmann::json stats = {
        {"feedback_processed", feedback_processed_.load()},
        {"patterns_learned", patterns_learned_.load()},
        {"adaptations_performed", adaptations_performed_.load()}
    };

    if (agent_id) {
        std::unique_lock<std::mutex> lock(profiles_mutex_);
        auto profile_it = agent_profiles_.find(*agent_id);
        if (profile_it != agent_profiles_.end()) {
            const auto& profile = profile_it->second;
            stats["agent_stats"] = {
                {"overall_accuracy", profile.overall_accuracy},
                {"human_override_rate", profile.human_override_rate},
                {"escalation_rate", profile.escalation_rate},
                {"total_decisions", profile.total_decisions},
                {"corrected_decisions", profile.corrected_decisions},
                {"learned_patterns", profile.learned_patterns.size()},
                {"learning_enabled", profile.learning_enabled},
                {"exploration_rate", profile.exploration_rate},
                {"learning_rate", profile.learning_rate}
            };
        }
    } else {
        // System-wide statistics
        std::unique_lock<std::mutex> lock(profiles_mutex_);
        stats["total_agents"] = agent_profiles_.size();

        double avg_accuracy = 0.0;
        double total_decisions = 0;
        size_t total_patterns = 0;

        for (const auto& [id, profile] : agent_profiles_) {
            avg_accuracy += profile.overall_accuracy;
            total_decisions += profile.total_decisions;
            total_patterns += profile.learned_patterns.size();
        }

        if (!agent_profiles_.empty()) {
            stats["average_accuracy"] = avg_accuracy / agent_profiles_.size();
            stats["total_decisions"] = total_decisions;
            stats["total_patterns"] = total_patterns;
        }
    }

    return stats;
}

nlohmann::json LearningEngine::export_learning_data(const std::optional<std::string>& agent_id) {
    nlohmann::json export_data = nlohmann::json::array();

    try {
        std::unique_lock<std::mutex> lock(profiles_mutex_);

        if (agent_id) {
            auto profile_it = agent_profiles_.find(*agent_id);
            if (profile_it != agent_profiles_.end()) {
                export_data.push_back({
                    {"agent_id", *agent_id},
                    {"profile", export_agent_profile(profile_it->second)}
                });
            }
        } else {
            for (const auto& [id, profile] : agent_profiles_) {
                export_data.push_back({
                    {"agent_id", id},
                    {"profile", export_agent_profile(profile)}
                });
            }
        }

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Failed to export learning data: " + std::string(e.what()),
                          "LearningEngine", "export_learning_data");
        }
    }

    return export_data;
}

bool LearningEngine::reset_agent_learning(const std::string& agent_id) {
    try {
        std::unique_lock<std::mutex> lock(profiles_mutex_);

        auto profile_it = agent_profiles_.find(agent_id);
        if (profile_it == agent_profiles_.end()) {
            return false;
        }

        // Reset profile to initial state by erasing and reinserting
        std::string agent_type = profile_it->second.agent_type;
        agent_profiles_.erase(profile_it);
        agent_profiles_.emplace(std::piecewise_construct,
                              std::forward_as_tuple(agent_id),
                              std::forward_as_tuple(agent_id, agent_type));

        if (logger_) {
            logger_->info("Reset learning for agent: " + agent_id,
                         "LearningEngine", "reset_agent_learning");
        }

        return true;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Failed to reset agent learning: " + std::string(e.what()),
                          "LearningEngine", "reset_agent_learning");
        }
        return false;
    }
}

AgentLearningProfile& LearningEngine::get_or_create_profile(const std::string& agent_id,
                                                          const std::string& agent_type) {
    auto it = agent_profiles_.find(agent_id);
    if (it == agent_profiles_.end()) {
        auto [new_it, inserted] = agent_profiles_.emplace(std::piecewise_construct,
                                                         std::forward_as_tuple(agent_id),
                                                         std::forward_as_tuple(agent_id, agent_type));
        return new_it->second;
    }
    return it->second;
}

void LearningEngine::update_performance_metrics(AgentLearningProfile& profile,
                                             LearningFeedbackType feedback_type,
                                             double signal_strength) {

    profile.total_decisions++;

    switch (feedback_type) {
        case LearningFeedbackType::CORRECTION:
            profile.corrected_decisions++;
            break;
        case LearningFeedbackType::ESCALATION:
            profile.escalation_rate = (profile.escalation_rate * 0.95) + 0.05; // Increment escalation rate
            break;
        case LearningFeedbackType::APPROVAL:
            // Positive feedback - slight accuracy boost
            break;
        default:
            break;
    }

    // Update overall accuracy
    if (profile.total_decisions > 0) {
        profile.overall_accuracy = 1.0 - (profile.corrected_decisions / static_cast<double>(profile.total_decisions));
    }

    // Update human override rate
    profile.human_override_rate = profile.corrected_decisions / static_cast<double>(profile.total_decisions);

    // Store recent feedback
    profile.recent_feedback.push_back(LearningSignal(feedback_type, signal_strength));
    if (profile.recent_feedback.size() > 100) { // Keep last 100 feedback items
        profile.recent_feedback.pop_front();
    }
}

nlohmann::json LearningEngine::export_agent_profile(const AgentLearningProfile& profile) {
    nlohmann::json profile_json = {
        {"agent_id", profile.agent_id},
        {"agent_type", profile.agent_type},
        {"learning_rate", profile.learning_rate},
        {"exploration_rate", profile.exploration_rate},
        {"feedback_weight", profile.feedback_weight},
        {"overall_accuracy", profile.overall_accuracy},
        {"human_override_rate", profile.human_override_rate},
        {"escalation_rate", profile.escalation_rate},
        {"total_decisions", profile.total_decisions},
        {"corrected_decisions", profile.corrected_decisions},
        {"learning_enabled", profile.learning_enabled}
    };

    // Export learned patterns with full details
    profile_json["learned_patterns"] = nlohmann::json::array();
    for (const auto& pattern : profile.learned_patterns) {
        nlohmann::json pattern_json = {
            {"pattern_id", pattern.pattern_id},
            {"agent_type", pattern.agent_type},
            {"decision_context", pattern.decision_context},
            {"learned_decision", pattern.learned_decision},
            {"success_rate", pattern.success_rate},
            {"application_count", pattern.application_count},
            {"average_confidence", pattern.average_confidence},
            {"first_learned", std::chrono::duration_cast<std::chrono::milliseconds>(
                pattern.first_learned.time_since_epoch()).count()},
            {"last_updated", std::chrono::duration_cast<std::chrono::milliseconds>(
                pattern.last_updated.time_since_epoch()).count()},
            {"context_weights", pattern.context_weights}
        };

        // Export recent signals
        pattern_json["recent_signals"] = nlohmann::json::array();
        for (const auto& signal : pattern.recent_signals) {
            pattern_json["recent_signals"].push_back({
                {"type", static_cast<int>(signal.type)},
                {"strength", signal.strength},
                {"confidence", signal.confidence},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                    signal.timestamp.time_since_epoch()).count()}
            });
        }

        profile_json["learned_patterns"].push_back(pattern_json);
    }

    // Export Q-table
    {
        std::shared_lock<std::shared_mutex> lock(profile.q_table_mutex);
        profile_json["q_table"] = nlohmann::json::object();
        for (const auto& [state, actions] : profile.q_table) {
            profile_json["q_table"][state] = nlohmann::json::object();
            for (const auto& [action, q_value] : actions) {
                profile_json["q_table"][state][action] = q_value;
            }
        }
    }

    // Export recent feedback
    profile_json["recent_feedback"] = nlohmann::json::array();
    for (const auto& signal : profile.recent_feedback) {
        profile_json["recent_feedback"].push_back({
            {"type", static_cast<int>(signal.type)},
            {"strength", signal.strength},
            {"confidence", signal.confidence},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                signal.timestamp.time_since_epoch()).count()}
        });
    }

    return profile_json;
}

// Utility function
std::string feedback_type_to_string(LearningFeedbackType type) {
    switch (type) {
        case LearningFeedbackType::CORRECTION: return "correction";
        case LearningFeedbackType::APPROVAL: return "approval";
        case LearningFeedbackType::ESCALATION: return "escalation";
        case LearningFeedbackType::REWARD: return "reward";
        case LearningFeedbackType::PENALTY: return "penalty";
        case LearningFeedbackType::PREFERENCE: return "preference";
        case LearningFeedbackType::OUTCOME_BASED: return "outcome_based";
        default: return "unknown";
    }
}

// Factory function

std::shared_ptr<LearningEngine> create_learning_engine(
    std::shared_ptr<ConfigurationManager> config,
    std::shared_ptr<ConversationMemory> memory,
    std::shared_ptr<OpenAIClient> openai_client,
    std::shared_ptr<AnthropicClient> anthropic_client,
    StructuredLogger* logger,
    ErrorHandler* error_handler) {

    auto engine = std::make_shared<LearningEngine>(
        config, memory, openai_client, anthropic_client, logger, error_handler);

    if (!engine->initialize()) {
        return nullptr;
    }

    return engine;
}

} // namespace regulens
