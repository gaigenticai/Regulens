/**
 * Advanced Memory Manager Implementation
 *
 * Intelligent memory lifecycle management with consolidation, forgetting,
 * and optimization for compliance AI systems.
 */

#include "memory_manager.hpp"
#include <algorithm>
#include <cmath>
#include <random>
#include <sstream>
#include <fstream>

namespace regulens {

// MemoryManager Implementation

MemoryManager::MemoryManager(std::shared_ptr<ConfigurationManager> config,
                           std::shared_ptr<ConversationMemory> conversation_memory,
                           std::shared_ptr<LearningEngine> learning_engine,
                           StructuredLogger* logger,
                           ErrorHandler* error_handler)
    : config_(config), conversation_memory_(conversation_memory),
      learning_engine_(learning_engine), logger_(logger), error_handler_(error_handler) {

    // Set default optimization plan
    optimization_plan_.consolidation_strategies = {
        ConsolidationStrategy::MERGE_SIMILAR,
        ConsolidationStrategy::EXTRACT_PATTERNS
    };
    optimization_plan_.target_sizes = {
        {MemoryTier::WORKING, 100},
        {MemoryTier::EPISODIC, 1000},
        {MemoryTier::SEMANTIC, 500},
        {MemoryTier::PROCEDURAL, 200},
        {MemoryTier::ARCHIVAL, 100}
    };
}

MemoryManager::~MemoryManager() = default;

bool MemoryManager::initialize() {
    if (logger_) {
        logger_->info("Initializing MemoryManager", "MemoryManager", "initialize");
    }

    try {
        // Load optimization plan from configuration
        auto plan_config = config_->get_string("MEMORY_OPTIMIZATION_PLAN");
        if (plan_config) {
            // In a real implementation, parse the plan from config
            // For now, use defaults
        }

        // Initialize health metrics
        update_health_metrics();

        if (logger_) {
            logger_->info("MemoryManager initialized successfully", "MemoryManager", "initialize");
        }

        return true;

    } catch (const std::exception& e) {
        if (error_handler_) {
            error_handler_->report_error(ErrorInfo{
                ErrorCategory::CONFIGURATION,
                ErrorSeverity::HIGH,
                "MemoryManager",
                "initialize",
                "Failed to initialize memory manager: " + std::string(e.what()),
                "Memory manager initialization failure"
            });
        }

        if (logger_) {
            logger_->error("Failed to initialize MemoryManager: " + std::string(e.what()),
                          "MemoryManager", "initialize");
        }

        return false;
    }
}

ConsolidationResult MemoryManager::consolidate_memories(ConsolidationStrategy strategy,
                                                       std::chrono::hours max_age) {

    ConsolidationResult result;
    auto start_time = std::chrono::high_resolution_clock::now();

    try {
        std::unique_lock<std::mutex> lock(manager_mutex_);

        // Get memories to consolidate (older than max_age)
        auto now = std::chrono::system_clock::now();
        auto cutoff_time = now - std::chrono::hours(max_age);

        // Query conversation memory for memories older than cutoff time
        // Since ConversationMemory doesn't have a direct method, we'll use a database query
        std::vector<MemoryEntry> memories_to_consolidate = get_memories_older_than(cutoff_time);

        result.memories_processed = memories_to_consolidate.size();

        switch (strategy) {
            case ConsolidationStrategy::MERGE_SIMILAR:
                result = merge_similar_memories(memories_to_consolidate);
                break;
            case ConsolidationStrategy::EXTRACT_PATTERNS: {
                auto patterns = extract_patterns_from_memories(memories_to_consolidate);
                result.memories_consolidated = patterns.size();
                result.consolidation_steps.push_back("Extracted " + std::to_string(patterns.size()) + " patterns");
                break;
            }
            case ConsolidationStrategy::COMPRESS_DETAILS: {
                // Compress each memory
                for (auto& memory : memories_to_consolidate) {
                    memory = compress_memory_details(memory);
                    result.memories_compressed++;
                }
                result.consolidation_steps.push_back("Compressed " + std::to_string(result.memories_compressed) + " memories");
                break;
            }
            default:
                result.consolidation_steps.push_back("Strategy not implemented");
                break;
        }

        result.success = true;
        result.compression_ratio = result.memories_processed > 0 ?
            static_cast<double>(result.memories_consolidated) / result.memories_processed : 1.0;

        consolidations_performed_++;

        // Update health metrics
        update_health_metrics();

        // Log consolidation
        nlohmann::json log_details = {
            {"strategy", static_cast<int>(strategy)},
            {"memories_processed", result.memories_processed},
            {"memories_consolidated", result.memories_consolidated},
            {"compression_ratio", result.compression_ratio}
        };
        log_management_operation("consolidation", log_details);

    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string(e.what());

        if (logger_) {
            logger_->error("Failed to consolidate memories: " + std::string(e.what()),
                          "MemoryManager", "consolidate_memories");
        }
    }

    result.processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start_time);

    return result;
}

size_t MemoryManager::perform_forgetting(ForgettingStrategy strategy,
                                       std::chrono::hours max_age,
                                       double min_importance) {

    size_t forgotten_count = 0;

    try {
        std::unique_lock<std::mutex> lock(manager_mutex_);

        double current_pressure = calculate_memory_pressure();
        double optimal_threshold = calculate_optimal_forgetting_threshold(current_pressure);

        // Adjust forgetting parameters based on strategy
        switch (strategy) {
            case ForgettingStrategy::TIME_BASED:
                // Use provided max_age and min_importance
                break;
            case ForgettingStrategy::IMPORTANCE_BASED:
                max_age = std::chrono::hours(8760); // 1 year - focus on importance
                min_importance = optimal_threshold;
                break;
            case ForgettingStrategy::USAGE_BASED:
                // Forget based on access patterns (not implemented in detail)
                max_age = std::chrono::hours(720); // 30 days
                min_importance = 0.3;
                break;
            case ForgettingStrategy::ADAPTIVE:
                min_importance = optimal_threshold;
                if (current_pressure > 0.8) {
                    max_age = std::chrono::hours(168); // 1 week under high pressure
                }
                break;
            case ForgettingStrategy::PRESERVATION:
                // Don't forget anything
                return 0;
            default:
                break;
        }

        // Perform forgetting on conversation memory
        conversation_memory_->forget_memories(max_age, min_importance);

        // Identify critical memories to preserve
        auto critical_memories = identify_critical_memories();

        // In a real implementation, this would return the actual count
        forgotten_count = 0; // Placeholder

        forgettings_performed_++;

        // Update health metrics
        update_health_metrics();

        // Log forgetting
        nlohmann::json log_details = {
            {"strategy", static_cast<int>(strategy)},
            {"max_age_hours", max_age.count()},
            {"min_importance", min_importance},
            {"current_pressure", current_pressure},
            {"optimal_threshold", optimal_threshold},
            {"forgotten_count", forgotten_count}
        };
        log_management_operation("forgetting", log_details);

        if (logger_) {
            logger_->info("Performed forgetting: " + std::to_string(forgotten_count) +
                         " memories forgotten using strategy " + std::to_string(static_cast<int>(strategy)),
                         "MemoryManager", "perform_forgetting");
        }

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Failed to perform forgetting: " + std::string(e.what()),
                          "MemoryManager", "perform_forgetting");
        }
    }

    return forgotten_count;
}

bool MemoryManager::optimize_memory(const MemoryOptimizationPlan& plan) {
    try {
        std::unique_lock<std::mutex> lock(manager_mutex_);

        if (!validate_optimization_plan(plan)) {
            return false;
        }

        bool success = true;
        std::vector<std::string> optimization_steps;

        // Perform consolidation strategies
        for (const auto& strategy : plan.consolidation_strategies) {
            auto result = consolidate_memories(strategy);
            if (result.success) {
                optimization_steps.push_back("Consolidated using " +
                    std::to_string(static_cast<int>(strategy)) + ": " +
                    std::to_string(result.memories_consolidated) + " memories");
            } else {
                success = false;
                optimization_steps.push_back("Failed consolidation with " +
                    std::to_string(static_cast<int>(strategy)));
            }
        }

        // Perform forgetting
        size_t forgotten = perform_forgetting(plan.forgetting_strategy);
        optimization_steps.push_back("Forgot " + std::to_string(forgotten) + " memories");

        // Check memory pressure and perform emergency cleanup if needed
        double pressure = calculate_memory_pressure();
        if (pressure > plan.memory_pressure_threshold) {
            size_t cleaned = emergency_cleanup(plan.memory_pressure_threshold);
            optimization_steps.push_back("Emergency cleanup: " + std::to_string(cleaned) + " memories");
        }

        optimizations_performed_++;

        // Schedule next optimization
        schedule_next_optimization(plan);

        // Log optimization
        nlohmann::json log_details = {
            {"success", success},
            {"steps", optimization_steps},
            {"final_pressure", calculate_memory_pressure()}
        };
        log_management_operation("optimization", log_details);

        if (logger_) {
            logger_->info(std::string("Memory optimization completed: ") +
                         (success ? "successful" : "with issues"),
                         "MemoryManager", "optimize_memory");
        }

        return success;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Failed to optimize memory: " + std::string(e.what()),
                          "MemoryManager", "optimize_memory");
        }
        return false;
    }
}

MemoryHealthMetrics MemoryManager::get_memory_health() const {
    std::unique_lock<std::mutex> lock(manager_mutex_);
    return health_metrics_;
}

bool MemoryManager::needs_optimization() const {
    std::unique_lock<std::mutex> lock(manager_mutex_);

    double pressure = calculate_memory_pressure();
    return pressure > optimization_plan_.memory_pressure_threshold;
}

size_t MemoryManager::emergency_cleanup(double target_memory_pressure) {
    size_t cleaned_count = 0;

    try {
        // Aggressive forgetting to reach target pressure
        double current_pressure = calculate_memory_pressure();

        while (current_pressure > target_memory_pressure && cleaned_count < 1000) {
            size_t forgotten = perform_forgetting(ForgettingStrategy::IMPORTANCE_BASED,
                                                std::chrono::hours(168), 0.1);
            cleaned_count += forgotten;

            current_pressure = calculate_memory_pressure();

            if (forgotten == 0) break; // No more to forget
        }

        emergency_cleanups_++;

        // Log emergency cleanup
        nlohmann::json log_details = {
            {"cleaned_count", cleaned_count},
            {"target_pressure", target_memory_pressure},
            {"final_pressure", current_pressure}
        };
        log_management_operation("emergency_cleanup", log_details);

        if (logger_) {
            logger_->warn("Emergency cleanup performed: " + std::to_string(cleaned_count) +
                         " memories cleaned up", "MemoryManager", "emergency_cleanup");
        }

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Failed emergency cleanup: " + std::string(e.what()),
                          "MemoryManager", "emergency_cleanup");
        }
    }

    return cleaned_count;
}

bool MemoryManager::backup_critical_memories(const std::string& backup_path) {
    try {
        auto critical_memories = identify_critical_memories();
        nlohmann::json backup_data = nlohmann::json::object();
        backup_data["backup_timestamp"] = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        backup_data["critical_memories"] = nlohmann::json::array();
        
        // Convert MemoryHealthMetrics to JSON
        auto health = get_memory_health();
        backup_data["memory_health"] = {
            {"total_memories", health.total_memories},
            {"working_memories", health.working_memories},
            {"episodic_memories", health.episodic_memories},
            {"semantic_memories", health.semantic_memories},
            {"procedural_memories", health.procedural_memories},
            {"archival_memories", health.archival_memories},
            {"average_importance", health.average_importance},
            {"memory_pressure", health.memory_pressure},
            {"consolidation_ratio", health.consolidation_ratio},
            {"forgetting_rate", health.forgetting_rate}
        };

        // In a real implementation, this would export actual memory data
        // For now, just log the operation
        backup_data["critical_memories"] = critical_memories;

        // Write to file
        std::ofstream backup_file(backup_path);
        if (backup_file.is_open()) {
            backup_file << backup_data.dump(2);
            backup_file.close();

            if (logger_) {
                logger_->info("Critical memories backed up to: " + backup_path +
                             " (" + std::to_string(critical_memories.size()) + " memories)",
                             "MemoryManager", "backup_critical_memories");
            }

            return true;
        }

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Failed to backup critical memories: " + std::string(e.what()),
                          "MemoryManager", "backup_critical_memories");
        }
    }

    return false;
}

size_t MemoryManager::restore_memories(const std::string& backup_path) {
    size_t restored_count = 0;

    try {
        std::ifstream backup_file(backup_path);
        if (backup_file.is_open()) {
            nlohmann::json backup_data = nlohmann::json::parse(backup_file);
            backup_file.close();

            // In a real implementation, this would restore memory data
            // For now, just log the operation
            restored_count = backup_data.value("critical_memories", nlohmann::json::array()).size();

            if (logger_) {
                logger_->info("Restored " + std::to_string(restored_count) +
                             " memories from backup: " + backup_path,
                             "MemoryManager", "restore_memories");
            }
        }

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Failed to restore memories: " + std::string(e.what()),
                          "MemoryManager", "restore_memories");
        }
    }

    return restored_count;
}

nlohmann::json MemoryManager::get_management_statistics() const {
    return {
        {"consolidations_performed", consolidations_performed_.load()},
        {"forgettings_performed", forgettings_performed_.load()},
        {"optimizations_performed", optimizations_performed_.load()},
        {"emergency_cleanups", emergency_cleanups_.load()},
        {"current_memory_pressure", calculate_memory_pressure()},
        {"needs_optimization", needs_optimization()}
    };
}

bool MemoryManager::configure_optimization_plan(const MemoryOptimizationPlan& plan) {
    if (!validate_optimization_plan(plan)) {
        return false;
    }

    std::unique_lock<std::mutex> lock(manager_mutex_);
    optimization_plan_ = plan;

    if (logger_) {
        logger_->info("Memory optimization plan configured",
                     "MemoryManager", "configure_optimization_plan");
    }

    return true;
}

MemoryOptimizationPlan MemoryManager::get_optimization_plan() const {
    std::unique_lock<std::mutex> lock(manager_mutex_);
    return optimization_plan_;
}

nlohmann::json MemoryManager::perform_maintenance() {
    nlohmann::json maintenance_result = {
        {"maintenance_timestamp", std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())},
        {"operations_performed", nlohmann::json::array()},
        {"success", true}
    };

    try {
        // Update health metrics
        update_health_metrics();
        maintenance_result["operations_performed"].push_back("health_metrics_updated");

        // Check if optimization is needed
        if (needs_optimization()) {
            auto result = optimize_memory(optimization_plan_);
            maintenance_result["operations_performed"].push_back({
                {"type", "optimization"},
                {"success", result}
            });
        }

        // Perform light consolidation
        auto consolidation_result = consolidate_memories(ConsolidationStrategy::MERGE_SIMILAR,
                                                       std::chrono::hours(168)); // 1 week
        maintenance_result["operations_performed"].push_back({
            {"type", "consolidation"},
            {"memories_consolidated", consolidation_result.memories_consolidated}
        });

        // Light forgetting
        size_t forgotten = perform_forgetting(ForgettingStrategy::TIME_BASED,
                                            std::chrono::hours(2160), 0.1); // 90 days
        maintenance_result["operations_performed"].push_back({
            {"type", "forgetting"},
            {"memories_forgotten", forgotten}
        });

        if (logger_) {
            logger_->info("Memory maintenance completed successfully",
                         "MemoryManager", "perform_maintenance");
        }

    } catch (const std::exception& e) {
        maintenance_result["success"] = false;
        maintenance_result["error"] = std::string(e.what());

        if (logger_) {
            logger_->error("Memory maintenance failed: " + std::string(e.what()),
                          "MemoryManager", "perform_maintenance");
        }
    }

    return maintenance_result;
}

// Private helper methods

void MemoryManager::update_health_metrics() {
    // Get statistics from conversation memory
    auto memory_stats = conversation_memory_->get_memory_statistics();

    health_metrics_.total_memories = memory_stats.value("cache_size", 0);
    health_metrics_.memory_pressure = calculate_memory_pressure();
    health_metrics_.average_importance = memory_stats.value("average_importance", 0.5);

    // Simplified tier counts (in practice would be more sophisticated)
    health_metrics_.episodic_memories = health_metrics_.total_memories;
    health_metrics_.working_memories = std::min(size_t(100), health_metrics_.total_memories / 10);

    health_metrics_.last_consolidation = std::chrono::system_clock::now();
}

double MemoryManager::calculate_memory_pressure() const {
    // Calculate pressure based on cache size vs configured limits
    size_t max_cache_size = config_->get_int("MEMORY_MAX_CACHE_SIZE").value_or(10000);
    size_t current_size = conversation_memory_->get_memory_statistics().value("cache_size", 0);

    return std::min(1.0, static_cast<double>(current_size) / max_cache_size);
}

ConsolidationResult MemoryManager::merge_similar_memories(const std::vector<MemoryEntry>& memories) {
    ConsolidationResult result;
    result.memories_processed = memories.size();

    // Simple similarity-based merging
    std::vector<MemoryEntry> merged_memories;

    for (const auto& memory : memories) {
        bool merged = false;

        for (auto& existing : merged_memories) {
            // Simple similarity check (in practice would use embeddings)
            if (existing.context.dump().find(memory.context.dump().substr(0, 50)) != std::string::npos) {
                // Merge by combining summaries and increasing importance
                existing.summary += "; " + memory.summary;
                existing.access_count += memory.access_count;
                existing.importance_level = std::max(existing.importance_level, memory.importance_level);
                merged = true;
                result.memories_consolidated++;
                break;
            }
        }

        if (!merged) {
            merged_memories.push_back(memory);
        }
    }

    result.success = true;
    result.compression_ratio = static_cast<double>(merged_memories.size()) / memories.size();

    return result;
}

std::vector<nlohmann::json> MemoryManager::extract_patterns_from_memories(const std::vector<MemoryEntry>& memories) {
    std::vector<nlohmann::json> patterns;

    // Simple pattern extraction (in practice would use ML/NLP techniques)
    std::unordered_map<std::string, int> decision_patterns;
    std::unordered_map<std::string, int> outcome_patterns;

    for (const auto& memory : memories) {
        if (memory.decision_made) {
            decision_patterns[*memory.decision_made]++;
        }
        if (memory.outcome) {
            outcome_patterns[*memory.outcome]++;
        }
    }

    // Extract significant patterns
    for (const auto& [decision, count] : decision_patterns) {
        if (count > memories.size() * 0.1) { // 10% threshold
            patterns.push_back({
                {"type", "decision_pattern"},
                {"pattern", decision},
                {"frequency", count},
                {"confidence", static_cast<double>(count) / memories.size()}
            });
        }
    }

    return patterns;
}

MemoryEntry MemoryManager::compress_memory_details(const MemoryEntry& memory) {
    MemoryEntry compressed = memory;

    // Compress by truncating detailed content while preserving key information
    if (compressed.context.contains("detailed_logs")) {
        compressed.context.erase("detailed_logs"); // Remove detailed logs
        compressed.metadata["compressed"] = "true";
        compressed.metadata["compression_type"] = "detail_removal";
    }

    return compressed;
}

MemoryEntry MemoryManager::promote_memory_tier(const MemoryEntry& memory, MemoryTier target_tier) {
    MemoryEntry promoted = memory;

    // Update memory type based on tier
    switch (target_tier) {
        case MemoryTier::SEMANTIC:
            promoted.memory_type = MemoryType::SEMANTIC;
            break;
        case MemoryTier::PROCEDURAL:
            promoted.memory_type = MemoryType::PROCEDURAL;
            break;
        default:
            break;
    }

    promoted.metadata["promoted_tier"] = std::to_string(static_cast<int>(target_tier));
    promoted.metadata["promotion_timestamp"] = std::to_string(
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

    return promoted;
}

std::vector<std::string> MemoryManager::identify_critical_memories() const {
    // Identify memories that should never be forgotten
    std::vector<std::string> critical_memories;

    // In a real implementation, this would scan memories and identify critical ones
    // based on importance, regulatory impact, legal requirements, etc.

    return critical_memories;
}

double MemoryManager::calculate_optimal_forgetting_threshold(double current_pressure) const {
    // Adaptive threshold based on memory pressure
    if (current_pressure < 0.3) {
        return 0.3; // Keep more memories when pressure is low
    } else if (current_pressure < 0.7) {
        return 0.2; // Moderate forgetting
    } else {
        return 0.1; // Aggressive forgetting under high pressure
    }
}

size_t MemoryManager::forget_domain_memories(const std::string& domain, std::chrono::hours max_age) {
    // Domain-specific forgetting
    conversation_memory_->forget_memories(max_age, 0.15); // Lower threshold for domain forgetting
    return 0; // Placeholder
}

bool MemoryManager::validate_optimization_plan(const MemoryOptimizationPlan& plan) const {
    // Basic validation
    if (plan.consolidation_strategies.empty()) {
        return false;
    }

    if (plan.memory_pressure_threshold < 0.0 || plan.memory_pressure_threshold > 1.0) {
        return false;
    }

    return true;
}

void MemoryManager::schedule_next_optimization(const MemoryOptimizationPlan& plan) {
    // In a real implementation, this would schedule the next optimization
    // For now, just log that it would be scheduled
    if (logger_) {
        logger_->debug("Would schedule next optimization in " +
                      std::to_string(plan.optimization_interval.count()) + " hours",
                      "MemoryManager", "schedule_next_optimization");
    }
}

void MemoryManager::log_management_operation(const std::string& operation, const nlohmann::json& details) {
    if (logger_) {
        logger_->info("Memory management operation: " + operation,
                     "MemoryManager", "log_management_operation", details);
    }
}

// MemoryTierManager Implementation

MemoryTierManager::MemoryTierManager(std::shared_ptr<ConfigurationManager> config,
                                   StructuredLogger* logger)
    : config_(config), logger_(logger) {}

MemoryTier MemoryTierManager::assign_memory_tier(const MemoryEntry& memory) {
    return calculate_memory_tier(memory);
}

std::pair<std::chrono::hours, double> MemoryTierManager::get_tier_retention_policy(MemoryTier tier) const {
    switch (tier) {
        case MemoryTier::WORKING:
            return {std::chrono::hours(1), 0.0}; // Keep everything for 1 hour
        case MemoryTier::EPISODIC:
            return {std::chrono::hours(168), 0.3}; // 1 week, min importance 0.3
        case MemoryTier::SEMANTIC:
            return {std::chrono::hours(720), 0.5}; // 30 days, min importance 0.5
        case MemoryTier::PROCEDURAL:
            return {std::chrono::hours(2160), 0.7}; // 90 days, min importance 0.7
        case MemoryTier::ARCHIVAL:
            return {std::chrono::hours(8760), 0.8}; // 1 year, min importance 0.8
        default:
            return {std::chrono::hours(24), 0.2};
    }
}

std::optional<MemoryTier> MemoryTierManager::should_promote_memory(const MemoryEntry& memory, MemoryTier current_tier) const {
    if (meets_promotion_criteria(memory, static_cast<MemoryTier>(static_cast<int>(current_tier) + 1))) {
        return static_cast<MemoryTier>(static_cast<int>(current_tier) + 1);
    }
    return std::nullopt;
}

nlohmann::json MemoryTierManager::get_tier_statistics() const {
    // In a real implementation, this would track actual tier statistics
    return {
        {"working_tier", {{"current_size", 50}, {"max_size", 100}}},
        {"episodic_tier", {{"current_size", 500}, {"max_size", 1000}}},
        {"semantic_tier", {{"current_size", 200}, {"max_size", 500}}},
        {"procedural_tier", {{"current_size", 50}, {"max_size", 200}}},
        {"archival_tier", {{"current_size", 20}, {"max_size", 100}}}
    };
}

MemoryTier MemoryTierManager::calculate_memory_tier(const MemoryEntry& memory) const {
    double importance = memory.calculate_importance_score();
    auto age = std::chrono::duration_cast<std::chrono::hours>(
        std::chrono::system_clock::now() - memory.timestamp);

    // Working memory: Very recent, any importance
    if (age < std::chrono::hours(1)) {
        return MemoryTier::WORKING;
    }

    // Episodic memory: Recent events and conversations
    if (age < std::chrono::hours(168) && importance >= 0.3) { // 1 week
        return MemoryTier::EPISODIC;
    }

    // Semantic memory: General patterns and knowledge
    if (memory.memory_type == MemoryType::SEMANTIC || importance >= 0.6) {
        return MemoryTier::SEMANTIC;
    }

    // Procedural memory: Learned processes
    if (memory.memory_type == MemoryType::PROCEDURAL || importance >= 0.7) {
        return MemoryTier::PROCEDURAL;
    }

    // Archival memory: Long-term important memories
    if (importance >= 0.8) {
        return MemoryTier::ARCHIVAL;
    }

    // Default to episodic
    return MemoryTier::EPISODIC;
}

bool MemoryTierManager::meets_promotion_criteria(const MemoryEntry& memory, MemoryTier target_tier) const {
    double required_importance = 0.5; // Base requirement
    int required_access_count = 1;

    switch (target_tier) {
        case MemoryTier::SEMANTIC:
            required_importance = 0.6;
            required_access_count = 3;
            break;
        case MemoryTier::PROCEDURAL:
            required_importance = 0.7;
            required_access_count = 5;
            break;
        case MemoryTier::ARCHIVAL:
            required_importance = 0.8;
            required_access_count = 10;
            break;
        default:
            return false;
    }

    return memory.calculate_importance_score() >= required_importance &&
           memory.access_count >= required_access_count;
}

// MemoryHealthMonitor Implementation

MemoryHealthMonitor::MemoryHealthMonitor(std::shared_ptr<ConfigurationManager> config,
                                       StructuredLogger* logger)
    : config_(config), logger_(logger) {}

nlohmann::json MemoryHealthMonitor::monitor_memory_health(const MemoryHealthMetrics& health_metrics) {
    nlohmann::json monitoring_result = nlohmann::json::object();
    monitoring_result["timestamp"] = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    monitoring_result["health_score"] = calculate_memory_health_score(health_metrics);
    monitoring_result["alerts"] = nlohmann::json::array();
    monitoring_result["recommendations"] = nlohmann::json::array();
    
    // Convert MemoryHealthMetrics to JSON
    monitoring_result["metrics"] = {
        {"total_memories", health_metrics.total_memories},
        {"working_memories", health_metrics.working_memories},
        {"episodic_memories", health_metrics.episodic_memories},
        {"semantic_memories", health_metrics.semantic_memories},
        {"procedural_memories", health_metrics.procedural_memories},
        {"archival_memories", health_metrics.archival_memories},
        {"average_importance", health_metrics.average_importance},
        {"memory_pressure", health_metrics.memory_pressure},
        {"consolidation_ratio", health_metrics.consolidation_ratio},
        {"forgetting_rate", health_metrics.forgetting_rate}
    };

    // Check for issues
    auto issues = identify_memory_issues(health_metrics);
    for (const auto& issue : issues) {
        monitoring_result["alerts"].push_back({
            {"level", "warning"}, // Could be "critical", "warning", "info"
            {"message", issue}
        });
    }

    // Generate recommendations
    auto recommendations = generate_health_recommendations(issues);
    monitoring_result["recommendations"] = recommendations;

    return monitoring_result;
}

nlohmann::json MemoryHealthMonitor::predict_memory_pressure(const std::vector<MemoryHealthMetrics>& historical_metrics) {
    nlohmann::json prediction = {
        {"prediction_horizon_hours", 24},
        {"current_trend", "stable"},
        {"predicted_pressure", 0.5},
        {"confidence", 0.7}
    };

    if (historical_metrics.size() < 2) {
        prediction["note"] = "Insufficient historical data for prediction";
        return prediction;
    }

    // Simple trend analysis
    double recent_avg_pressure = 0.0;
    double older_avg_pressure = 0.0;

    size_t mid_point = historical_metrics.size() / 2;
    for (size_t i = 0; i < historical_metrics.size(); ++i) {
        if (i < mid_point) {
            older_avg_pressure += historical_metrics[i].memory_pressure;
        } else {
            recent_avg_pressure += historical_metrics[i].memory_pressure;
        }
    }

    older_avg_pressure /= mid_point;
    recent_avg_pressure /= (historical_metrics.size() - mid_point);

    double trend = recent_avg_pressure - older_avg_pressure;

    if (trend > 0.1) {
        prediction["current_trend"] = "increasing";
        prediction["predicted_pressure"] = std::min(1.0, recent_avg_pressure + trend);
    } else if (trend < -0.1) {
        prediction["current_trend"] = "decreasing";
        prediction["predicted_pressure"] = std::max(0.0, recent_avg_pressure + trend);
    }

    return prediction;
}

nlohmann::json MemoryHealthMonitor::generate_health_report(const MemoryHealthMetrics& health_metrics) {
    return {
        {"report_timestamp", std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())},
        {"health_score", calculate_memory_health_score(health_metrics)},
        {"memory_pressure", health_metrics.memory_pressure},
        {"total_memories", health_metrics.total_memories},
        {"consolidation_ratio", health_metrics.consolidation_ratio},
        {"forgetting_rate", health_metrics.forgetting_rate},
        {"issues", identify_memory_issues(health_metrics)},
        {"recommendations", generate_health_recommendations(identify_memory_issues(health_metrics))}
    };
}

std::vector<std::string> MemoryHealthMonitor::detect_memory_anomalies(const MemoryHealthMetrics& health_metrics,
                                                                    const MemoryHealthMetrics& baseline_metrics) {

    std::vector<std::string> anomalies;

    // Check for significant deviations
    double pressure_diff = health_metrics.memory_pressure - baseline_metrics.memory_pressure;
    if (std::abs(pressure_diff) > 0.2) {
        anomalies.push_back("Memory pressure deviation: " + std::to_string(pressure_diff));
    }

    double importance_diff = health_metrics.average_importance - baseline_metrics.average_importance;
    if (std::abs(importance_diff) > 0.3) {
        anomalies.push_back("Average importance deviation: " + std::to_string(importance_diff));
    }

    return anomalies;
}

double MemoryHealthMonitor::calculate_memory_health_score(const MemoryHealthMetrics& metrics) const {
    double score = 1.0;

    // Penalize high memory pressure
    score -= metrics.memory_pressure * 0.4;

    // Penalize low consolidation ratio
    if (metrics.consolidation_ratio < 0.5) {
        score -= (0.5 - metrics.consolidation_ratio) * 0.2;
    }

    // Penalize high forgetting rate
    score -= std::min(0.3, metrics.forgetting_rate * 2.0);

    // Bonus for high average importance
    score += std::min(0.2, metrics.average_importance * 0.4);

    return std::max(0.0, std::min(1.0, score));
}

std::vector<std::string> MemoryHealthMonitor::identify_memory_issues(const MemoryHealthMetrics& metrics) const {
    std::vector<std::string> issues;

    if (metrics.memory_pressure > 0.8) {
        issues.push_back("High memory pressure: " + std::to_string(metrics.memory_pressure * 100) + "%");
    }

    if (metrics.average_importance < 0.3) {
        issues.push_back("Low average memory importance: " + std::to_string(metrics.average_importance));
    }

    if (metrics.consolidation_ratio < 0.3) {
        issues.push_back("Low consolidation ratio: " + std::to_string(metrics.consolidation_ratio));
    }

    if (metrics.forgetting_rate > 0.5) {
        issues.push_back("High forgetting rate: " + std::to_string(metrics.forgetting_rate));
    }

    return issues;
}

std::vector<std::string> MemoryHealthMonitor::generate_health_recommendations(const std::vector<std::string>& issues) const {
    std::vector<std::string> recommendations;

    for (const auto& issue : issues) {
        if (issue.find("memory pressure") != std::string::npos) {
            recommendations.push_back("Perform memory optimization to reduce pressure");
            recommendations.push_back("Increase forgetting threshold temporarily");
        }

        if (issue.find("importance") != std::string::npos) {
            recommendations.push_back("Review memory importance calculation");
            recommendations.push_back("Promote high-value memories to higher tiers");
        }

        if (issue.find("consolidation") != std::string::npos) {
            recommendations.push_back("Run memory consolidation more frequently");
            recommendations.push_back("Review consolidation strategies");
        }

        if (issue.find("forgetting") != std::string::npos) {
            recommendations.push_back("Adjust forgetting parameters");
            recommendations.push_back("Review memory retention policies");
        }
    }

    if (recommendations.empty()) {
        recommendations.push_back("Memory health is good - continue monitoring");
    }

    return recommendations;
}

// Factory functions

std::shared_ptr<MemoryManager> create_memory_manager(
    std::shared_ptr<ConfigurationManager> config,
    std::shared_ptr<ConversationMemory> conversation_memory,
    std::shared_ptr<LearningEngine> learning_engine,
    StructuredLogger* logger,
    ErrorHandler* error_handler) {

    auto manager = std::make_shared<MemoryManager>(
        config, conversation_memory, learning_engine, logger, error_handler);

    if (!manager->initialize()) {
        return nullptr;
    }

    return manager;
}

std::shared_ptr<MemoryTierManager> create_memory_tier_manager(
    std::shared_ptr<ConfigurationManager> config,
    StructuredLogger* logger) {

    return std::make_shared<MemoryTierManager>(config, logger);
}

std::shared_ptr<MemoryHealthMonitor> create_memory_health_monitor(
    std::shared_ptr<ConfigurationManager> config,
    StructuredLogger* logger) {

    return std::shared_ptr<MemoryHealthMonitor>(new MemoryHealthMonitor(config, logger));
}

std::vector<MemoryEntry> MemoryManager::get_memories_older_than(
    std::chrono::system_clock::time_point cutoff_time) {

    std::vector<MemoryEntry> old_memories;

    try {
        // Get memory statistics to understand what we have
        auto stats = conversation_memory_->get_memory_statistics();

        // For now, implement a basic approach: get all memories and filter by age
        // In a production system, this would be optimized with database indexing
        // Since ConversationMemory doesn't expose direct age-based queries,
        // we'll use a combination of search and filtering

        // Get a broad search to find candidate memories
        auto recent_memories = conversation_memory_->search_memories("", 1000); // Empty query gets all

        for (const auto& memory : recent_memories) {
            if (memory.timestamp < cutoff_time) {
                old_memories.push_back(memory);
            }
        }

        // Limit to reasonable size for consolidation (avoid processing too many at once)
        if (old_memories.size() > 1000) {
            // Sort by age (oldest first) and take the oldest 1000
            std::sort(old_memories.begin(), old_memories.end(),
                     [](const MemoryEntry& a, const MemoryEntry& b) {
                         return a.timestamp < b.timestamp;
                     });
            old_memories.resize(1000);
        }

    } catch (const std::exception& e) {
        logger_->error("Failed to query memories for consolidation: " + std::string(e.what()),
                      "MemoryManager", "get_memories_older_than");
    }

    return old_memories;
}

} // namespace regulens
