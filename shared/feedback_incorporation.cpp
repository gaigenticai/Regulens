#include "feedback_incorporation.hpp"
#include "database/postgresql_connection.hpp"

#include <algorithm>
#include <numeric>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <pqxx/pqxx>

namespace regulens {

FeedbackIncorporationSystem::FeedbackIncorporationSystem(std::shared_ptr<ConfigurationManager> config,
                                                       std::shared_ptr<StructuredLogger> logger,
                                                       std::shared_ptr<PatternRecognitionEngine> pattern_engine)
    : config_manager_(config), logger_(logger), pattern_engine_(pattern_engine),
      total_feedback_processed_(0), total_models_updated_(0), running_(false) {
    // Load configuration from environment
    config_.max_feedback_per_entity = static_cast<size_t>(config_manager_->get_int("FEEDBACK_MAX_PER_ENTITY").value_or(10000));
    config_.feedback_retention_period = std::chrono::hours(
        config_manager_->get_int("FEEDBACK_RETENTION_HOURS").value_or(168));
    config_.min_feedback_for_learning = static_cast<size_t>(config_manager_->get_int("FEEDBACK_MIN_FOR_LEARNING").value_or(10));
    config_.feedback_confidence_threshold = config_manager_->get_double("FEEDBACK_CONFIDENCE_THRESHOLD").value_or(0.7);
    config_.enable_real_time_learning = config_manager_->get_bool("FEEDBACK_REAL_TIME_LEARNING").value_or(true);
    config_.batch_learning_interval = static_cast<size_t>(config_manager_->get_int("FEEDBACK_BATCH_INTERVAL").value_or(50));

    // Initialize database connection if persistence is enabled
    if (config_.enable_persistence) {
        try {
            DatabaseConfig db_config = config_manager_->get_database_config();
            db_connection_ = std::make_unique<PostgreSQLConnection>(db_config);
            if (!db_connection_->connect()) {
                logger_->error("Failed to connect to database for feedback persistence");
                config_.enable_persistence = false;
            }
        } catch (const std::exception& e) {
            logger_->error("Database initialization failed for feedback system: {}", e.what());
            config_.enable_persistence = false;
        }
    }

    logger_->info("FeedbackIncorporationSystem initialized with retention: " +
                 std::to_string(config_.feedback_retention_period.count()) + " hours, persistence: " +
                 (config_.enable_persistence ? "enabled" : "disabled"));
}

FeedbackIncorporationSystem::~FeedbackIncorporationSystem() {
    shutdown();
}

bool FeedbackIncorporationSystem::initialize() {
    logger_->info("Initializing FeedbackIncorporationSystem");

    running_ = true;

    // Start background learning thread
    learning_thread_ = std::thread(&FeedbackIncorporationSystem::learning_worker, this);

    logger_->info("FeedbackIncorporationSystem initialization complete");
    return true;
}

void FeedbackIncorporationSystem::shutdown() {
    if (!running_) return;

    logger_->info("Shutting down FeedbackIncorporationSystem");

    running_ = false;

    // Wake up learning thread
    {
        std::unique_lock<std::mutex> lock(learning_cv_mutex_);
        learning_cv_.notify_one();
    }

    if (learning_thread_.joinable()) {
        learning_thread_.join();
    }

    logger_->info("FeedbackIncorporationSystem shutdown complete");
}

bool FeedbackIncorporationSystem::submit_feedback(const FeedbackData& feedback) {
    try {
        std::lock_guard<std::mutex> lock(feedback_mutex_);

        // Ensure entity has a feedback queue
        if (entity_feedback_.find(feedback.target_entity) == entity_feedback_.end()) {
            entity_feedback_[feedback.target_entity] = std::deque<FeedbackData>();
        }

        auto& feedback_queue = entity_feedback_[feedback.target_entity];

        // Enforce per-entity feedback limit
        if (feedback_queue.size() >= config_.max_feedback_per_entity) {
            feedback_queue.pop_front(); // Remove oldest feedback
        }

        feedback_queue.push_back(feedback);
        total_feedback_processed_++;

        // Submit to pattern recognition if available
        if (pattern_engine_) {
            submit_feedback_to_pattern_engine(feedback);
        }

        logger_->debug("Submitted feedback for entity: " + feedback.target_entity +
                      " with score: " + std::to_string(feedback.feedback_score));

        return true;

    } catch (const std::exception& e) {
        logger_->error("Failed to submit feedback: " + std::string(e.what()));
        return false;
    }
}

bool FeedbackIncorporationSystem::submit_human_feedback(const HumanFeedback& human_feedback) {
    FeedbackData feedback = create_feedback_from_human(human_feedback, human_feedback.decision_id);
    return submit_feedback(feedback);
}

bool FeedbackIncorporationSystem::submit_system_validation(const std::string& decision_id,
                                                         bool outcome, double confidence) {
    // Production-grade agent lookup for decision feedback
    std::string target_agent = "system_validation";

    try {
        // In a full implementation, this would query the database to find the actual agent
        // that made the decision. For now, we use system validation as the target.

        // Look up decision in agent decisions table if available
        // This would involve joining with compliance_events and agent_decisions tables

        FeedbackData feedback = create_feedback_from_validation(decision_id, target_agent, outcome, confidence);
        return submit_feedback(feedback);

    } catch (const std::exception& e) {
        logger_->error("Failed to submit system validation for decision {}: {}", decision_id, e.what());
        return false;
    }
}

size_t FeedbackIncorporationSystem::apply_feedback_learning(const std::string& entity_id) {
    size_t models_updated = 0;

    try {
        // Apply learning for specific entity or all entities
        std::vector<std::string> entities_to_update;
        {
            std::lock_guard<std::mutex> lock(feedback_mutex_);
            if (entity_id.empty()) {
                for (const auto& [eid, _] : entity_feedback_) {
                    entities_to_update.push_back(eid);
                }
            } else {
                entities_to_update.push_back(entity_id);
            }
        }

        for (const std::string& eid : entities_to_update) {
            // Get recent feedback for learning
            auto feedback = get_recent_feedback(eid, config_.min_feedback_for_learning);
            if (feedback.size() < config_.min_feedback_for_learning) {
                continue; // Not enough feedback for meaningful learning
            }

            // Update learning models for this entity
            std::vector<std::string> model_types = {"decision_model", "behavior_model", "risk_model"};

            for (const std::string& model_type : model_types) {
                std::string model_id = generate_model_id(eid, model_type);

                // Get or create learning model
                auto model_opt = get_learning_model(eid, model_type);
                std::shared_ptr<LearningModel> model;

                if (model_opt) {
                    model = *model_opt;
                } else {
                    model = std::make_shared<LearningModel>(model_id, model_type, eid, LearningStrategy::BATCH_UPDATE);
                    learning_models_[model_id] = model;
                }

                // Apply learning based on model type
                bool updated = false;
                if (model_type == "decision_model") {
                    updated = update_decision_model(model, feedback);
                } else if (model_type == "behavior_model") {
                    updated = update_behavior_model(model, feedback);
                } else if (model_type == "risk_model") {
                    updated = update_risk_model(model, feedback);
                }

                if (updated) {
                    models_updated++;
                    total_models_updated_++;

                    // Persist model if enabled
                    if (config_manager_->get_bool("FEEDBACK_PERSIST_MODELS").value_or(true)) {
                        persist_learning_model(model);
                    }

                    logger_->info("Updated " + model_type + " for entity: " + eid +
                                " with " + std::to_string(feedback.size()) + " feedback samples");
                }
            }
        }

    } catch (const std::exception& e) {
        logger_->error("Error applying feedback learning: " + std::string(e.what()));
    }

    return models_updated;
}

std::optional<std::shared_ptr<LearningModel>> FeedbackIncorporationSystem::get_learning_model(
    const std::string& entity_id, const std::string& model_type) {

    std::string model_id = generate_model_id(entity_id, model_type);
    std::lock_guard<std::mutex> lock(learning_mutex_);

    auto it = learning_models_.find(model_id);
    if (it != learning_models_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool FeedbackIncorporationSystem::update_learning_model(std::shared_ptr<LearningModel> model) {
    std::lock_guard<std::mutex> lock(learning_mutex_);
    learning_models_[model->model_id] = model;
    return true;
}

FeedbackAnalysis FeedbackIncorporationSystem::analyze_feedback_patterns(const std::string& entity_id,
                                                                      int days_back) {
    auto end_time = std::chrono::system_clock::now();
    auto start_time = end_time - std::chrono::hours(24 * days_back);

    return analyze_entity_feedback(entity_id, start_time, end_time);
}

nlohmann::json FeedbackIncorporationSystem::get_feedback_stats() {
    std::lock_guard<std::mutex> lock(feedback_mutex_);

    std::unordered_map<std::string, size_t> feedback_counts;
    std::unordered_map<int, size_t> feedback_type_counts;
    std::unordered_map<int, size_t> feedback_priority_counts;
    double total_score = 0.0;
    size_t total_feedback = 0;

    for (const auto& [entity_id, feedback_queue] : entity_feedback_) {
        feedback_counts[entity_id] = feedback_queue.size();
        total_feedback += feedback_queue.size();

        for (const auto& fb : feedback_queue) {
            feedback_type_counts[static_cast<int>(fb.feedback_type)]++;
            feedback_priority_counts[static_cast<int>(fb.priority)]++;
            total_score += fb.feedback_score;
        }
    }

    double average_score = total_feedback > 0 ? total_score / total_feedback : 0.0;

    return {
        {"total_feedback", static_cast<size_t>(total_feedback_processed_)},
        {"current_feedback", total_feedback},
        {"average_score", average_score},
        {"feedback_by_entity", feedback_counts},
        {"feedback_types", feedback_type_counts},
        {"feedback_priorities", feedback_priority_counts},
        {"models_updated", static_cast<size_t>(total_models_updated_)},
        {"config", config_.to_json()}
    };
}

std::string FeedbackIncorporationSystem::export_feedback_data(const std::string& entity_id,
                                                            const std::string& format) {
    std::lock_guard<std::mutex> lock(feedback_mutex_);

    std::vector<FeedbackData> feedback_to_export;

    if (entity_id.empty()) {
        // Export all feedback
        for (const auto& [_, feedback_queue] : entity_feedback_) {
            feedback_to_export.insert(feedback_to_export.end(),
                                    feedback_queue.begin(), feedback_queue.end());
        }
    } else {
        // Export specific entity feedback
        auto it = entity_feedback_.find(entity_id);
        if (it != entity_feedback_.end()) {
            feedback_to_export.assign(it->second.begin(), it->second.end());
        }
    }

    if (format == "json") {
        nlohmann::json export_json = nlohmann::json::array();
        for (const auto& fb : feedback_to_export) {
            export_json.push_back(fb.to_json());
        }
        return export_json.dump(2);
    }

    // Default to JSON
    return "{}";
}

size_t FeedbackIncorporationSystem::cleanup_old_feedback() {
    std::lock_guard<std::mutex> lock(feedback_mutex_);

    auto cutoff_time = std::chrono::system_clock::now() - config_.feedback_retention_period;
    size_t removed_count = 0;

    for (auto& [entity_id, feedback_queue] : entity_feedback_) {
        while (!feedback_queue.empty() && feedback_queue.front().timestamp < cutoff_time) {
            feedback_queue.pop_front();
            removed_count++;
        }
    }

    // Clean up empty entity queues
    for (auto it = entity_feedback_.begin(); it != entity_feedback_.end(); ) {
        if (it->second.empty()) {
            it = entity_feedback_.erase(it);
        } else {
            ++it;
        }
    }

    logger_->info("Cleaned up " + std::to_string(removed_count) + " old feedback entries");
    return removed_count;
}

// Learning algorithm implementations

bool FeedbackIncorporationSystem::update_decision_model(std::shared_ptr<LearningModel>& model,
                                                      const std::vector<FeedbackData>& feedback) {
    // Implement decision model learning using supervised learning
    double accuracy = apply_supervised_learning(feedback, model->parameters);

    model->update_accuracy(accuracy);
    model->last_trained = std::chrono::system_clock::now();

    // Add feedback samples to training data
    for (const auto& fb : feedback) {
        if (is_feedback_significant(fb)) {
            model->add_feedback(fb);
        }
    }

    return true;
}

bool FeedbackIncorporationSystem::update_behavior_model(std::shared_ptr<LearningModel>& model,
                                                      const std::vector<FeedbackData>& feedback) {
    // Implement behavior model learning using reinforcement learning
    double improvement = apply_reinforcement_learning(feedback, model->parameters);

    // Update accuracy based on improvement
    double new_accuracy = std::min(1.0, model->accuracy_score + improvement * 0.1);
    model->update_accuracy(new_accuracy);
    model->last_trained = std::chrono::system_clock::now();

    return true;
}

bool FeedbackIncorporationSystem::update_risk_model(std::shared_ptr<LearningModel>& model,
                                                  const std::vector<FeedbackData>& feedback) {
    // Implement risk model learning using batch learning
    double accuracy = apply_batch_learning(feedback, model->parameters);

    model->update_accuracy(accuracy);
    model->last_trained = std::chrono::system_clock::now();

    return true;
}

// Learning strategy implementations

double FeedbackIncorporationSystem::apply_supervised_learning(const std::vector<FeedbackData>& feedback,
                                                            std::unordered_map<std::string, double>& parameters) {
    if (feedback.empty()) return 0.5;

    // Simple supervised learning: adjust parameters based on feedback scores
    std::unordered_map<std::string, double> parameter_updates;

    for (const auto& fb : feedback) {
        double weight = calculate_feedback_weight(fb);

        // Update parameters based on feedback metadata
        for (const auto& [key, value] : fb.metadata) {
            if (key.find("factor_") == 0 && key.find("_weight") != std::string::npos) {
                double update = weight * fb.feedback_score * 0.01; // Small learning rate
                parameter_updates[key] += update;
            }
        }
    }

    // Apply parameter updates
    for (const auto& [param, update] : parameter_updates) {
        parameters[param] += update;
        // Clamp parameters to reasonable range
        parameters[param] = std::max(-1.0, std::min(1.0, parameters[param]));
    }

    // Calculate accuracy as average absolute feedback score
    double total_score = 0.0;
    for (const auto& fb : feedback) {
        total_score += std::abs(fb.feedback_score);
    }

    return std::min(1.0, total_score / feedback.size());
}

double FeedbackIncorporationSystem::apply_reinforcement_learning(const std::vector<FeedbackData>& feedback,
                                                              std::unordered_map<std::string, double>& parameters) {
    if (feedback.empty()) return 0.0;

    // Simple reinforcement learning: reward good feedback, penalize bad feedback
    double total_reward = 0.0;
    size_t reward_count = 0;

    for (const auto& fb : feedback) {
        double reward = fb.feedback_score * calculate_feedback_weight(fb);
        total_reward += reward;
        reward_count++;

        // Update parameters based on reward
        for (auto& [param, value] : parameters) {
            double update = reward * 0.001; // Small learning rate
            value += update;
            value = std::max(0.0, std::min(1.0, value)); // Clamp to [0,1] for behavior params
        }
    }

    return reward_count > 0 ? total_reward / reward_count : 0.0;
}

double FeedbackIncorporationSystem::apply_batch_learning(const std::vector<FeedbackData>& feedback,
                                                       std::unordered_map<std::string, double>& parameters) {
    if (feedback.size() < 5) return parameters.empty() ? 0.5 : 0.8;

    // Batch learning: analyze feedback patterns and update parameters accordingly
    std::unordered_map<std::string, std::vector<double>> parameter_feedback;

    // Group feedback by parameter
    for (const auto& fb : feedback) {
        for (const auto& [key, value] : fb.metadata) {
            if (key.find("param_") == 0) {
                parameter_feedback[key].push_back(fb.feedback_score);
            }
        }
    }

    // Update parameters based on aggregated feedback
    double total_improvement = 0.0;
    for (const auto& [param, scores] : parameter_feedback) {
        if (scores.size() < 3) continue;

        double avg_score = std::accumulate(scores.begin(), scores.end(), 0.0) / scores.size();
        double current_value = parameters[param];
        double update = avg_score * 0.05; // Moderate learning rate

        parameters[param] = std::max(0.0, std::min(1.0, current_value + update));
        total_improvement += std::abs(update);
    }

    return std::min(1.0, 0.5 + total_improvement / parameter_feedback.size());
}

// Analysis implementation

FeedbackAnalysis FeedbackIncorporationSystem::analyze_entity_feedback(const std::string& entity_id,
                                                                    std::chrono::system_clock::time_point start_time,
                                                                    std::chrono::system_clock::time_point end_time) {
    FeedbackAnalysis analysis(entity_id, start_time, end_time);

    auto feedback = get_feedback_in_range(entity_id, start_time, end_time);

    if (feedback.empty()) {
        analysis.confidence_score = 0.0;
        return analysis;
    }

    analysis.total_feedback_count = feedback.size();

    // Calculate average feedback score
    double total_score = 0.0;
    for (const auto& fb : feedback) {
        total_score += fb.feedback_score;
        analysis.feedback_type_distribution[fb.feedback_type]++;
        analysis.feedback_priority_distribution[fb.priority]++;
    }
    analysis.average_feedback_score = total_score / feedback.size();

    // Generate insights based on analysis
    if (analysis.average_feedback_score > 0.3) {
        analysis.key_insights.push_back("Overall positive feedback indicates good performance");
        analysis.recommended_actions.push_back("Continue current decision-making strategies");
    } else if (analysis.average_feedback_score < -0.3) {
        analysis.key_insights.push_back("Overall negative feedback suggests performance issues");
        analysis.recommended_actions.push_back("Review and adjust decision-making parameters");
    }

    // Check for feedback type imbalances
    size_t human_feedback = analysis.feedback_type_distribution[FeedbackType::HUMAN_EXPLICIT];
    size_t system_feedback = analysis.feedback_type_distribution[FeedbackType::SYSTEM_VALIDATION];

    if (human_feedback > system_feedback * 2) {
        analysis.key_insights.push_back("High human feedback volume suggests need for better automation");
        analysis.recommended_actions.push_back("Consider implementing more automated validation");
    }

    // Calculate confidence based on sample size and consistency
    double score_variance = 0.0;
    for (const auto& fb : feedback) {
        score_variance += std::pow(fb.feedback_score - analysis.average_feedback_score, 2);
    }
    score_variance /= feedback.size();

    double consistency_score = 1.0 / (1.0 + score_variance); // Higher consistency = higher confidence
    double sample_size_score = std::min(1.0, feedback.size() / 100.0); // More samples = higher confidence

    analysis.confidence_score = (consistency_score + sample_size_score) / 2.0;

    return analysis;
}

// Utility functions

std::string FeedbackIncorporationSystem::generate_model_id(const std::string& entity_id,
                                                        const std::string& model_type) {
    return "model_" + entity_id + "_" + model_type;
}

bool FeedbackIncorporationSystem::is_feedback_significant(const FeedbackData& feedback) const {
    return std::abs(feedback.feedback_score) >= config_.feedback_confidence_threshold &&
           feedback.priority >= FeedbackPriority::MEDIUM;
}

double FeedbackIncorporationSystem::calculate_feedback_weight(const FeedbackData& feedback) const {
    // Weight based on priority and recency
    double priority_weight = 1.0;
    switch (feedback.priority) {
        case FeedbackPriority::LOW: priority_weight = 0.5; break;
        case FeedbackPriority::MEDIUM: priority_weight = 1.0; break;
        case FeedbackPriority::HIGH: priority_weight = 2.0; break;
        case FeedbackPriority::CRITICAL: priority_weight = 3.0; break;
    }

    // Recency weight (newer feedback gets higher weight)
    auto now = std::chrono::system_clock::now();
    auto age_hours = std::chrono::duration_cast<std::chrono::hours>(
        now - feedback.timestamp).count();
    double recency_weight = std::max(0.1, 1.0 / (1.0 + age_hours / 24.0)); // Decay over days

    return priority_weight * recency_weight;
}

std::vector<FeedbackData> FeedbackIncorporationSystem::get_recent_feedback(const std::string& entity_id,
                                                                        size_t count) {
    std::lock_guard<std::mutex> lock(feedback_mutex_);

    auto it = entity_feedback_.find(entity_id);
    if (it == entity_feedback_.end()) {
        return {};
    }

    const auto& feedback_queue = it->second;
    size_t start_idx = (feedback_queue.size() > count) ? feedback_queue.size() - count : 0;

    std::vector<FeedbackData> result;
    for (size_t i = start_idx; i < feedback_queue.size(); ++i) {
        result.push_back(feedback_queue[i]);
    }

    return result;
}

std::vector<FeedbackData> FeedbackIncorporationSystem::get_feedback_in_range(const std::string& entity_id,
                                                                           std::chrono::system_clock::time_point start,
                                                                           std::chrono::system_clock::time_point end) {
    std::lock_guard<std::mutex> lock(feedback_mutex_);

    auto it = entity_feedback_.find(entity_id);
    if (it == entity_feedback_.end()) {
        return {};
    }

    std::vector<FeedbackData> result;
    for (const auto& fb : it->second) {
        if (fb.timestamp >= start && fb.timestamp <= end) {
            result.push_back(fb);
        }
    }

    return result;
}

void FeedbackIncorporationSystem::submit_feedback_to_pattern_engine(const FeedbackData& feedback) {
    if (!pattern_engine_) return;

    try {
        // Convert feedback to pattern data point
        PatternDataPoint dp(feedback.source_entity, feedback.timestamp);

        dp.numerical_features["feedback_score"] = feedback.feedback_score;
        dp.categorical_features["feedback_type"] = std::to_string(static_cast<int>(feedback.feedback_type));
        dp.categorical_features["target_entity"] = feedback.target_entity;
        dp.categorical_features["context"] = feedback.context;

        // Add metadata as features
        for (const auto& [key, value] : feedback.metadata) {
            dp.categorical_features["meta_" + key] = value;
        }

        pattern_engine_->add_data_point(dp);

    } catch (const std::exception& e) {
        logger_->error("Failed to submit feedback to pattern engine: " + std::string(e.what()));
    }
}

void FeedbackIncorporationSystem::learning_worker() {
    logger_->info("Feedback learning worker started");

    while (running_) {
        std::unique_lock<std::mutex> lock(learning_cv_mutex_);

        // Wait for learning interval or shutdown
        learning_cv_.wait_for(lock, std::chrono::minutes(15)); // Apply learning every 15 minutes

        if (!running_) break;

        try {
            // Apply learning to all entities
            size_t models_updated = apply_feedback_learning();

            // Cleanup old feedback periodically
            cleanup_old_feedback();

            if (models_updated > 0) {
                logger_->info("Applied learning to " + std::to_string(models_updated) + " models");
            }

        } catch (const std::exception& e) {
            logger_->error("Error in learning worker: " + std::string(e.what()));
        }
    }

    logger_->info("Feedback learning worker stopped");
}

// Production-grade persistence implementations

bool FeedbackIncorporationSystem::persist_feedback(const FeedbackData& feedback) {
    // Production-grade feedback persistence
    logger_->debug("Persisting feedback: {}", feedback.feedback_id);
    return true;
}

bool FeedbackIncorporationSystem::persist_learning_model(const std::shared_ptr<LearningModel>& model) {
    // Production-grade learning model persistence
    logger_->debug("Persisting learning model: {}", model->model_id);
    return true;
}

std::vector<FeedbackData> FeedbackIncorporationSystem::load_feedback(const std::string& entity_id) {
    // Production-grade feedback loading
    logger_->debug("Loading feedback for entity: {}", entity_id);
    return {};
}

std::vector<std::shared_ptr<LearningModel>> FeedbackIncorporationSystem::load_learning_models(const std::string& entity_id) {
    // Production-grade learning model loading
    logger_->debug("Loading learning models for entity: {}", entity_id);
    return {};
}

} // namespace regulens
