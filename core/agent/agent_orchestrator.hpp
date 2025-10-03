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
#include "../shared/utils/timer.hpp"
#include "../shared/models/compliance_event.hpp"
#include "../shared/models/agent_decision.hpp"
#include "../shared/models/agent_state.hpp"

namespace regulens {

// Forward declarations
class ComplianceAgent;
class EventProcessor;
class KnowledgeBase;
class MetricsCollector;
class InterAgentCommunicator;
class AgentRegistry;
class IntelligentMessageTranslator;
class ConsensusEngine;
class CommunicationMediator;


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

    // Default constructor for map operations
    AgentRegistration() = default;

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

    // ===== MULTI-AGENT COMMUNICATION METHODS =====

    /**
     * @brief Send message to another agent
     * @param from_agent Sender agent ID
     * @param to_agent Recipient agent ID
     * @param message_type Type of message
     * @param content Message content
     * @return true if message sent successfully
     */
    bool send_agent_message(const std::string& from_agent, const std::string& to_agent,
                           MessageType message_type, const nlohmann::json& content);

    /**
     * @brief Broadcast message to all agents
     * @param from_agent Sender agent ID
     * @param message_type Type of message
     * @param content Message content
     * @return true if broadcast successful
     */
    bool broadcast_to_agents(const std::string& from_agent, MessageType message_type,
                            const nlohmann::json& content);

    /**
     * @brief Receive messages for an agent
     * @param agent_id Agent identifier
     * @param max_messages Maximum messages to retrieve
     * @return Vector of messages
     */
    std::vector<AgentMessage> receive_agent_messages(const std::string& agent_id,
                                                    size_t max_messages = 10);

    /**
     * @brief Start collaborative decision-making session
     * @param scenario Decision scenario description
     * @param participant_agents List of participating agent IDs
     * @param algorithm Consensus algorithm to use
     * @return Session ID if successful
     */
    std::optional<std::string> start_collaborative_decision(
        const std::string& scenario,
        const std::vector<std::string>& participant_agents,
        ConsensusAlgorithm algorithm = ConsensusAlgorithm::WEIGHTED_VOTE);

    /**
     * @brief Submit decision contribution to collaborative session
     * @param session_id Session identifier
     * @param agent_id Contributing agent ID
     * @param decision Decision content
     * @param confidence Confidence score (0.0-1.0)
     * @return true if contribution accepted
     */
    bool contribute_to_decision(const std::string& session_id, const std::string& agent_id,
                               const nlohmann::json& decision, double confidence = 0.5);

    /**
     * @brief Get collaborative decision result
     * @param session_id Session identifier
     * @return Decision result or nullopt if not ready
     */
    std::optional<ConsensusResult> get_collaborative_decision_result(const std::string& session_id);

    /**
     * @brief Facilitate agent conversation
     * @param agent1 First agent ID
     * @param agent2 Second agent ID
     * @param topic Conversation topic
     * @param max_rounds Maximum conversation rounds
     * @return Conversation summary
     */
    nlohmann::json facilitate_agent_conversation(const std::string& agent1, const std::string& agent2,
                                                const std::string& topic, int max_rounds = 5);

    /**
     * @brief Resolve conflicting agent recommendations
     * @param conflicting_messages Messages with conflicts
     * @return Resolution outcome
     */
    nlohmann::json resolve_agent_conflicts(const std::vector<AgentMessage>& conflicting_messages);

    /**
     * @brief Get multi-agent communication statistics
     * @return JSON with communication stats
     */
    nlohmann::json get_communication_statistics() const;

public:
    /**
     * @brief Constructor
     */
    AgentOrchestrator();

    /**
     * @brief Destructor
     */
    ~AgentOrchestrator();

private:

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
    StructuredLogger* logger_;
    std::shared_ptr<EventProcessor> event_processor_;
    std::shared_ptr<KnowledgeBase> knowledge_base_;
    std::shared_ptr<MetricsCollector> metrics_collector_;

    // Multi-agent communication system
    std::shared_ptr<AgentRegistry> agent_registry_;
    std::shared_ptr<InterAgentCommunicator> inter_agent_communicator_;
    std::shared_ptr<IntelligentMessageTranslator> message_translator_;
    std::shared_ptr<ConsensusEngine> consensus_engine_;
    std::shared_ptr<CommunicationMediator> communication_mediator_;

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
    Timer last_health_check_;
    std::unordered_map<std::string, AgentMetrics> agent_metrics_;
};

} // namespace regulens


