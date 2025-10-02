#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <queue>
#include <functional>
#include <chrono>
#include <optional>

#include "../shared/config/configuration_manager.hpp"
#include "../shared/logging/structured_logger.hpp"
#include "../shared/models/compliance_event.hpp"
#include "../shared/models/agent_decision.hpp"
#include "../shared/models/agent_state.hpp"
#include "../shared/utils/timer.hpp"

namespace regulens {

// Forward declarations
class ComplianceAgent;
class EventProcessor;
class KnowledgeBase;
class MetricsCollector;


/**
 * @brief Task execution result
 */
struct TaskResult {
    bool success;
    std::string error_message;
    std::optional<AgentDecision> decision;
    std::chrono::milliseconds execution_time;

    TaskResult(bool s = true, std::string msg = "", std::optional<AgentDecision> d = std::nullopt, std::chrono::milliseconds t = std::chrono::milliseconds(0))
        : success(s), error_message(std::move(msg)), decision(std::move(d)), execution_time(t) {}
};

/**
 * @brief Agent task definition
 */
struct AgentTask {
    std::string task_id;
    std::string agent_type;
    ComplianceEvent event;
    Priority priority;
    std::chrono::system_clock::time_point deadline;
    std::function<void(const TaskResult&)> callback;

    AgentTask(std::string id, std::string type, ComplianceEvent evt, Priority p = Priority::NORMAL,
              std::chrono::system_clock::time_point dl = std::chrono::system_clock::time_point::max(),
              std::function<void(const TaskResult&)> cb = nullptr)
        : task_id(std::move(id)), agent_type(std::move(type)), event(std::move(evt)),
          priority(p), deadline(dl), callback(std::move(cb)) {}
};

/**
 * @brief Agent registration information
 */
struct AgentRegistration {
    std::string agent_type;
    std::string agent_name;
    std::shared_ptr<ComplianceAgent> agent_instance;
    AgentCapabilities capabilities;
    bool active;

    AgentRegistration(std::string type, std::string name, std::shared_ptr<ComplianceAgent> instance,
                     AgentCapabilities caps, bool act = true)
        : agent_type(std::move(type)), agent_name(std::move(name)), agent_instance(std::move(instance)),
          capabilities(std::move(caps)), active(act) {}
};

/**
 * @brief Core agent orchestration system
 *
 * Coordinates multiple compliance agents, manages task scheduling,
 * handles event routing, and provides centralized monitoring and control.
 * This is the "brain" of the agentic AI compliance system.
 */
class AgentOrchestrator {
private:
    // Singleton instance - private to enforce factory pattern
    static std::unique_ptr<AgentOrchestrator> singleton_instance_;
    static std::mutex singleton_mutex_;

public:
    /**
     * @brief Get singleton instance
     */
    static AgentOrchestrator& get_instance();

    /**
     * @brief Initialize the orchestrator with configuration
     * @param config Configuration manager instance
     * @return true if initialization successful
     */
    bool initialize(std::shared_ptr<ConfigurationManager> config);

    /**
     * @brief Shutdown all agents and cleanup resources
     */
    void shutdown();

    /**
     * @brief Check if orchestrator is healthy
     * @return true if all components are operational
     */
    bool is_healthy() const;

    /**
     * @brief Register a new compliance agent
     * @param registration Agent registration information
     * @return true if registration successful
     */
    bool register_agent(const AgentRegistration& registration);

    /**
     * @brief Unregister an agent
     * @param agent_type Type of agent to unregister
     * @return true if unregistration successful
     */
    bool unregister_agent(const std::string& agent_type);

    /**
     * @brief Submit a task for agent processing
     * @param task Task to be processed
     * @return true if task submitted successfully
     */
    bool submit_task(const AgentTask& task);

    /**
     * @brief Process pending events (called by main loop)
     */
    void process_pending_events();

    /**
     * @brief Get orchestrator status and metrics
     * @return JSON status information
     */
    nlohmann::json get_status() const;

    /**
     * @brief Enable/disable an agent type
     * @param agent_type Type of agent
     * @param enabled Enable or disable
     * @return true if operation successful
     */
    bool set_agent_enabled(const std::string& agent_type, bool enabled);

    /**
     * @brief Get registered agent types
     * @return List of registered agent types
     */
    std::vector<std::string> get_registered_agents() const;

    /**
     * @brief Get agent status information
     * @param agent_type Type of agent
     * @return Agent status information
     */
    std::optional<AgentStatus> get_agent_status(const std::string& agent_type) const;

private:
    // Private constructor and destructor for proper singleton pattern
    // External instantiation only through factory methods
    AgentOrchestrator();
    ~AgentOrchestrator();

    // Factory method for testing - allows creating test instances
    // Declared private to maintain proper encapsulation
    static std::unique_ptr<AgentOrchestrator> create_test_instance();

public:
    // Public factory method that delegates to private implementation
    static std::unique_ptr<AgentOrchestrator> create_for_testing();

    // Prevent copying
    AgentOrchestrator(const AgentOrchestrator&) = delete;
    AgentOrchestrator& operator=(const AgentOrchestrator&) = delete;

    // Helper methods for improved modularity
    bool initialize_components();
    bool initialize_agents();
    void register_system_metrics();
    bool validate_agent_registration(const AgentRegistration& registration) const;
    bool prepare_task_execution(const AgentTask& task, std::shared_ptr<ComplianceAgent>& agent);
    TaskResult execute_task_with_agent(const AgentTask& task, std::shared_ptr<ComplianceAgent> agent);
    void finalize_task_execution(const AgentTask& task, const TaskResult& result);

    // Core coordination methods
    void start_worker_threads();
    void stop_worker_threads();
    void worker_thread_loop();
    void process_task(const AgentTask& task);
    std::shared_ptr<ComplianceAgent> find_agent_for_task(const AgentTask& task);
    void update_agent_metrics(const std::string& agent_type, const TaskResult& result);

    // Health monitoring
    void perform_health_checks();
    bool check_agent_health(const std::string& agent_type) const;

    // Utility methods
    std::string generate_task_id();

    // Configuration and dependencies
    std::shared_ptr<ConfigurationManager> config_;
    std::shared_ptr<StructuredLogger> logger_;
    std::shared_ptr<EventProcessor> event_processor_;
    std::shared_ptr<KnowledgeBase> knowledge_base_;
    std::shared_ptr<MetricsCollector> metrics_collector_;

    // Agent registry
    mutable std::mutex agents_mutex_;
    std::unordered_map<std::string, AgentRegistration> registered_agents_;

    // Task queue management
    mutable std::mutex task_queue_mutex_;
    std::condition_variable task_queue_cv_;
    std::queue<AgentTask> task_queue_;
    std::atomic<bool> shutdown_requested_;
    std::vector<std::thread> worker_threads_;

    // Performance monitoring
    std::atomic<size_t> tasks_processed_;
    std::atomic<size_t> tasks_failed_;
    std::chrono::system_clock::time_point last_health_check_;
    std::unordered_map<std::string, AgentMetrics> agent_metrics_;
};

} // namespace regulens


