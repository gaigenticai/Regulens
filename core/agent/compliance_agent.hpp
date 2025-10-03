#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <chrono>

#include "../shared/models/compliance_event.hpp"
#include "../shared/models/agent_decision.hpp"
#include "../shared/models/agent_state.hpp"
#include "../shared/logging/structured_logger.hpp"
#include "../shared/config/configuration_manager.hpp"

namespace regulens {

/**
 * @brief Abstract base class for all compliance agents
 *
 * Defines the interface that all compliance agents must implement.
 * Provides common functionality for agent lifecycle management,
 * configuration, and monitoring.
 */
class ComplianceAgent {
public:
    ComplianceAgent(std::string agent_type, std::string agent_name,
                   std::shared_ptr<ConfigurationManager> config,
                   std::shared_ptr<StructuredLogger> logger)
        : agent_type_(std::move(agent_type)), agent_name_(std::move(agent_name)),
          config_(config), logger_(logger), state_(AgentState::INITIALIZING),
          health_(AgentHealth::HEALTHY), enabled_(true),
          startup_time_(std::chrono::system_clock::now()) {

        metrics_.startup_time = startup_time_;
        logger_->info("Initializing agent: {} ({})", agent_name_, agent_type_);
    }

    virtual ~ComplianceAgent() = default;

    // Prevent copying
    ComplianceAgent(const ComplianceAgent&) = delete;
    ComplianceAgent& operator=(const ComplianceAgent&) = delete;

    /**
     * @brief Initialize the agent
     * @return true if initialization successful
     */
    virtual bool initialize() = 0;

    /**
     * @brief Shutdown the agent and cleanup resources
     */
    virtual void shutdown() = 0;

    /**
     * @brief Process a compliance event
     * @param event Event to process
     * @return Agent decision
     */
    virtual AgentDecision process_event(const ComplianceEvent& event) = 0;

    /**
     * @brief Learn from feedback on previous decisions
     * @param feedback Feedback data
     */
    virtual void learn_from_feedback(const AgentDecision& decision,
                                   const std::string& feedback) = 0;

    /**
     * @brief Get agent capabilities
     * @return Agent capabilities
     */
    virtual AgentCapabilities get_capabilities() const = 0;

    /**
     * @brief Check if agent can handle a specific event type
     * @param event_type Type of event
     * @return true if agent can handle this event type
     */
    virtual bool can_handle_event(EventType event_type) const = 0;

    /**
     * @brief Get agent status
     * @return Current agent status
     */
    AgentStatus get_status() const {
        AgentStatus status(agent_type_, agent_name_);
        status.state = state_.load();
        status.health = health_.load();
        status.capabilities = get_capabilities();
        status.metrics = metrics_;
        status.last_error = last_error_;
        status.last_health_check = last_health_check_;
        status.enabled = enabled_.load();
        return status;
    }

    /**
     * @brief Enable or disable the agent
     * @param enabled Enable state
     */
    void set_enabled(bool enabled) {
        enabled_ = enabled;
        std::string status = enabled ? "enabled" : "disabled";
        logger_->info("Agent " + agent_name_ + " (" + agent_type_ + ") " + status);
    }

    /**
     * @brief Check if agent is enabled
     * @return true if agent is enabled
     */
    bool is_enabled() const { return enabled_.load(); }

    /**
     * @brief Get agent type
     * @return Agent type identifier
     */
    const std::string& get_agent_type() const { return agent_type_; }

    /**
     * @brief Get agent name
     * @return Human-readable agent name
     */
    const std::string& get_agent_name() const { return agent_name_; }

    /**
     * @brief Perform health check
     * @return true if agent is healthy
     */
    virtual bool perform_health_check() {
        last_health_check_ = std::chrono::system_clock::now();

        // Basic health checks
        bool config_valid = validate_configuration();
        bool resources_available = check_resources();

        if (!config_valid || !resources_available) {
            health_ = AgentHealth::UNHEALTHY;
            return false;
        }

        // Check if agent is processing too many tasks
        if (metrics_.tasks_in_progress.load() > static_cast<unsigned long>(get_capabilities().max_concurrent_tasks)) {
            health_ = AgentHealth::DEGRADED;
            logger_->warn("Agent {} has too many concurrent tasks", agent_name_);
        } else {
            health_ = AgentHealth::HEALTHY;
        }

        return health_.load() == AgentHealth::HEALTHY;
    }

    /**
     * @brief Get agent configuration parameter
     * @param key Parameter key
     * @return Parameter value or empty string if not found
     */
    std::string get_config_parameter(const std::string& key) const {
        if (!config_) return "";

        // Try to get as string first
        if (auto value = config_->get_string(key)) {
            return *value;
        }

        // Try other types and convert to string
        if (auto value = config_->get_int(key)) {
            return std::to_string(*value);
        }

        if (auto value = config_->get_bool(key)) {
            return *value ? "true" : "false";
        }

        if (auto value = config_->get_double(key)) {
            return std::to_string(*value);
        }

        return "";
    }

protected:
    /**
     * @brief Update agent state
     * @param new_state New agent state
     */
    void update_state(AgentState new_state) {
        AgentState old_state = state_.load();
        state_ = new_state;

        if (old_state != new_state) {
            logger_->info("Agent " + agent_name_ + " state changed: " +
                         agent_state_to_string(old_state) + " -> " +
                         agent_state_to_string(new_state));
        }
    }

    /**
     * @brief Set last error message
     * @param error Error message
     */
    void set_last_error(const std::string& error) {
        last_error_ = error;
        health_ = AgentHealth::UNHEALTHY;
        logger_->error("Agent {} error: {}", agent_name_, error);
    }

public:
    /**
     * @brief Increment tasks in progress counter
     */
    void increment_tasks_in_progress() {
        metrics_.tasks_in_progress++;
    }

    /**
     * @brief Decrement tasks in progress counter
     */
    void decrement_tasks_in_progress() {
        if (metrics_.tasks_in_progress > 0) {
            metrics_.tasks_in_progress--;
        }
    }

    /**
     * @brief Update performance metrics
     * @param processing_time_ms Time taken to process task
     * @param success Whether task was successful
     */
    void update_metrics(std::chrono::milliseconds processing_time_ms, bool success) {
        metrics_.tasks_processed++;
        metrics_.last_task_time = std::chrono::system_clock::now();

        if (!success) {
            metrics_.tasks_failed++;
        }

        // Update average processing time
        double current_avg = metrics_.average_processing_time_ms.load();
        size_t total_tasks = metrics_.tasks_processed.load();
        double new_avg = (current_avg * (total_tasks - 1) + processing_time_ms.count()) / total_tasks;
        metrics_.average_processing_time_ms = new_avg;

        // Update success rate
        size_t successful_tasks = total_tasks - metrics_.tasks_failed.load();
        metrics_.success_rate = static_cast<double>(successful_tasks) / total_tasks;
    }

protected:

    /**
     * @brief Validate agent configuration
     * @return true if configuration is valid
     */
    virtual bool validate_configuration() const {
        // Basic validation - override in derived classes for specific checks
        return config_ != nullptr;
    }

    /**
     * @brief Check if required resources are available
     * @return true if resources are available
     */
    virtual bool check_resources() const {
        // Basic resource check - override in derived classes
        return true;
    }

    // Member variables
    std::string agent_type_;
    std::string agent_name_;
    std::shared_ptr<ConfigurationManager> config_;
    std::shared_ptr<StructuredLogger> logger_;

    std::atomic<AgentState> state_;
    std::atomic<AgentHealth> health_;
    std::atomic<bool> enabled_;

    AgentMetrics metrics_;
    std::string last_error_;
    std::chrono::system_clock::time_point last_health_check_;
    std::chrono::system_clock::time_point startup_time_;
};

/**
 * @brief Factory function type for creating agents
 */
using AgentFactory = std::function<std::shared_ptr<ComplianceAgent>(
    std::string agent_name,
    std::shared_ptr<ConfigurationManager> config,
    std::shared_ptr<StructuredLogger> logger
)>;

/**
 * @brief Agent registry for managing agent factories
 */
class AgentRegistry {
public:
    static AgentRegistry& get_instance();

    /**
     * @brief Register an agent factory
     * @param agent_type Agent type identifier
     * @param factory Factory function
     * @return true if registration successful
     */
    bool register_agent_factory(const std::string& agent_type, AgentFactory factory);

    /**
     * @brief Create an agent instance
     * @param agent_type Agent type
     * @param agent_name Agent name
     * @param config Configuration manager
     * @param logger Logger instance
     * @return Agent instance or nullptr if creation failed
     */
    std::shared_ptr<ComplianceAgent> create_agent(
        const std::string& agent_type,
        const std::string& agent_name,
        std::shared_ptr<ConfigurationManager> config,
        std::shared_ptr<StructuredLogger> logger
    ) const;

    /**
     * @brief Get registered agent types
     * @return List of registered agent types
     */
    std::vector<std::string> get_registered_types() const;

private:
    AgentRegistry() = default;
    static std::unordered_map<std::string, AgentFactory> factories_;
};

} // namespace regulens


