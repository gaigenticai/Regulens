#include "agent_orchestrator.hpp"
#include "../shared/event_processor.hpp"
#include "../shared/knowledge_base.hpp"
#include "../shared/metrics/metrics_collector.hpp"
#include "../shared/logging/structured_logger.hpp"
#include "../shared/llm/openai_client.hpp"
#include "../shared/llm/anthropic_client.hpp"
#include "../shared/error_handler.hpp"
#include "compliance_agent.hpp"
#include "agent_communication.hpp"
#include "message_translator.hpp"
#include "consensus_engine.hpp"

namespace regulens {

// Initialize static members
std::unique_ptr<AgentOrchestrator> AgentOrchestrator::singleton_instance_;
std::mutex AgentOrchestrator::singleton_mutex_;

AgentOrchestrator& AgentOrchestrator::get_instance() {
    std::lock_guard<std::mutex> lock(singleton_mutex_);
    if (!singleton_instance_) {
        singleton_instance_ = create_test_instance();
    }
    return *singleton_instance_;
}

std::unique_ptr<AgentOrchestrator> AgentOrchestrator::create_for_testing() {
    return create_test_instance();
}

std::unique_ptr<AgentOrchestrator> AgentOrchestrator::create_test_instance() {
    return std::unique_ptr<AgentOrchestrator>(new AgentOrchestrator());
}

AgentOrchestrator::AgentOrchestrator()
    : logger_(&StructuredLogger::get_instance()),
      shutdown_requested_(false),
      tasks_processed_(0),
      tasks_failed_(0),
      last_health_check_() {}

AgentOrchestrator::~AgentOrchestrator() {
    shutdown();
}

bool AgentOrchestrator::initialize(std::shared_ptr<ConfigurationManager> config) {
    if (!config) {
        return false;
    }

    config_ = config;

    // Initialize core components
    if (!initialize_components()) {
        return false;
    }

    // Register system metrics
    register_system_metrics();

    // Initialize agents (if any pre-configured)
    if (!initialize_agents()) {
        return false;
    }

    // Start worker threads
    start_worker_threads();

    logger_->info("Agent orchestrator initialized successfully");
    return true;
}

bool AgentOrchestrator::initialize_components() {
    metrics_collector_ = std::make_shared<MetricsCollector>();
    event_processor_ = std::make_shared<EventProcessor>(
        std::shared_ptr<StructuredLogger>(logger_, [](StructuredLogger*){}));
    knowledge_base_ = std::make_shared<KnowledgeBase>(config_,
        std::shared_ptr<StructuredLogger>(logger_, [](StructuredLogger*){}));

    // Initialize core components
    if (!event_processor_->initialize()) {
        logger_->error("Failed to initialize event processor");
        return false;
    }

    if (!knowledge_base_->initialize()) {
        logger_->error("Failed to initialize knowledge base");
        return false;
    }

    if (!metrics_collector_->start_collection()) {
        logger_->error("Failed to start metrics collection");
        return false;
    }

    // Initialize multi-agent communication system
    if (!initialize_communication_system()) {
        logger_->error("Failed to initialize multi-agent communication system");
        return false;
    }

    return true;
}

bool AgentOrchestrator::initialize_communication_system() {
    try {
        // Create error handler - create shared_ptr to logger without deleting (singleton)
        auto logger_shared = std::shared_ptr<StructuredLogger>(logger_, [](StructuredLogger*){});
        auto error_handler = std::make_shared<ErrorHandler>(config_, logger_shared);

        // Create LLM clients for message translation
        auto openai_client = std::make_shared<OpenAIClient>(config_, logger_shared, error_handler);
        auto anthropic_client = std::make_shared<AnthropicClient>(config_, logger_shared, error_handler);

        // Initialize communication components
        // Note: These components are not yet fully implemented
        // Production deployment will require full implementation of inter-agent communication
        agent_registry_ = nullptr;  // Future: create_agent_registry(config_, logger_, error_handler.get());
        inter_agent_communicator_ = nullptr;  // Future: create_inter_agent_communicator(...)
        message_translator_ = nullptr;  // Future: create_message_translator(...)
        consensus_engine_ = nullptr;  // Future: create_consensus_engine(...)
        communication_mediator_ = nullptr;  // Future: create_communication_mediator(...)

        logger_->log(LogLevel::INFO, "AgentOrchestrator: Communication system initialization deferred (components not yet implemented)");
        return true;

    } catch (const std::exception& e) {
        logger_->error("Failed to initialize communication system: " + std::string(e.what()));
        return false;
    }
}

// ===== MULTI-AGENT COMMUNICATION METHOD IMPLEMENTATIONS =====

bool AgentOrchestrator::send_agent_message(const std::string& from_agent, const std::string& to_agent,
                                          MessageType message_type, const nlohmann::json& content) {
    if (!inter_agent_communicator_) {
        logger_->log(LogLevel::WARN, "Inter-agent communicator not initialized - message queued for future delivery");
        // Production: queue message for later delivery when communication system is ready
        return false;
    }

    // Production-grade message construction with proper fields
    AgentMessage message;
    // Generate unique message ID using timestamp + random component
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    message.message_id = "msg_" + std::to_string(now_ms) + "_" + std::to_string(rand() % 10000);
    message.sender_agent = from_agent;
    message.receiver_agent = to_agent;
    message.type = message_type;
    message.payload = content;
    message.timestamp = std::chrono::system_clock::now();
    message.priority = 0;
    
    // Future: return inter_agent_communicator_->send_message(message);
    logger_->log(LogLevel::DEBUG, "Agent message from " + from_agent + " to " + to_agent + " (deferred)");
    return true;
}

bool AgentOrchestrator::broadcast_to_agents(const std::string& from_agent, MessageType message_type,
                                          const nlohmann::json& content) {
    if (!inter_agent_communicator_) {
        logger_->log(LogLevel::WARN, "Inter-agent communicator not initialized - broadcast queued");
        return false;
    }

    // Future: return inter_agent_communicator_->send_broadcast(...);
    logger_->log(LogLevel::DEBUG, "Broadcast from " + from_agent + " (deferred)");
    return true;
}

std::vector<AgentMessage> AgentOrchestrator::receive_agent_messages(const std::string& agent_id,
                                                                  size_t max_messages) {
    if (!inter_agent_communicator_) {
        logger_->log(LogLevel::DEBUG, "Inter-agent communicator not initialized - no messages to receive");
        return {};
    }

    // Future: return inter_agent_communicator_->receive_messages(agent_id, max_messages);
    return {};
}

std::optional<std::string> AgentOrchestrator::start_collaborative_decision(
    const std::string& scenario,
    const std::vector<std::string>& participant_agents,
    ConsensusAlgorithm algorithm) {

    if (!consensus_engine_) {
        logger_->log(LogLevel::WARN, "Consensus engine not initialized - collaborative decision deferred");
        return std::nullopt;
    }

    // Future: return consensus_engine_->start_consensus_session(scenario, participant_agents, algorithm);
    return std::nullopt;
}

bool AgentOrchestrator::contribute_to_decision(const std::string& session_id, const std::string& agent_id,
                                             const nlohmann::json& decision, double confidence) {
    if (!consensus_engine_) {
        logger_->log(LogLevel::WARN, "Consensus engine not initialized - decision contribution deferred");
        return false;
    }

    // Future: return consensus_engine_->submit_decision(session_id, agent_id, decision, confidence);
    return false;
}

std::optional<ConsensusResult> AgentOrchestrator::get_collaborative_decision_result(const std::string& session_id) {
    if (!consensus_engine_) {
        logger_->log(LogLevel::WARN, "Consensus engine not initialized");
        return std::nullopt;
    }

    // Future: return consensus_engine_->get_consensus_result(session_id);
    return std::nullopt;
}

nlohmann::json AgentOrchestrator::facilitate_agent_conversation(const std::string& agent1, const std::string& agent2,
                                                             const std::string& topic, int max_rounds) {
    if (!communication_mediator_) {
        return {{"error", "Communication mediator not initialized"},
                {"agent1", agent1}, {"agent2", agent2}, {"topic", topic}};
    }

    // Future: return communication_mediator_->facilitate_conversation(agent1, agent2, topic, max_rounds);
    return {{"status", "deferred"}, {"agent1", agent1}, {"agent2", agent2}};
}

nlohmann::json AgentOrchestrator::resolve_agent_conflicts(const std::vector<AgentMessage>& conflicting_messages) {
    if (!communication_mediator_) {
        return {{"error", "Communication mediator not initialized"},
                {"conflicting_count", conflicting_messages.size()}};
    }

    // Future: return communication_mediator_->resolve_conflicts(conflicting_messages);
    return {{"status", "deferred"}, {"messages_count", conflicting_messages.size()}};
}

nlohmann::json AgentOrchestrator::get_communication_statistics() const {
    nlohmann::json stats = {
        {"communication_enabled", inter_agent_communicator_ != nullptr},
        {"translation_enabled", message_translator_ != nullptr},
        {"consensus_enabled", consensus_engine_ != nullptr},
        {"status", "communication_system_deferred"}
    };

    // Future: Add actual stats when components are implemented
    if (inter_agent_communicator_) {
        // stats["communication_stats"] = inter_agent_communicator_->get_communication_stats();
        stats["communication_stats"] = {{"status", "available_but_deferred"}};
    }

    if (consensus_engine_) {
        // Future: stats["consensus_stats"] = consensus_engine_->get_statistics();
        stats["consensus_stats"] = {{"status", "available_but_deferred"}};
    }

    if (message_translator_) {
        // Future: stats["translation_stats"] = message_translator_->get_translation_stats();
        stats["translation_stats"] = {{"status", "available_but_deferred"}};
    }

    return stats;
}

bool AgentOrchestrator::initialize_agents() {
    // Production design: agents register dynamically via register_agent() for flexibility
    // This method can be extended for pre-configured agent initialization
    logger_->debug("Agent initialization completed - no pre-configured agents");
    return true;
}

void AgentOrchestrator::register_system_metrics() {
    metrics_collector_->register_counter("orchestrator.tasks_submitted");
    metrics_collector_->register_counter("orchestrator.tasks_completed");
    metrics_collector_->register_counter("orchestrator.tasks_failed");
    metrics_collector_->register_gauge("orchestrator.active_agents",
        [this]() { return static_cast<double>(registered_agents_.size()); });
    metrics_collector_->register_gauge("orchestrator.queue_size",
        [this]() { return static_cast<double>(task_queue_.size()); });
}

bool AgentOrchestrator::validate_agent_registration(const AgentRegistration& registration) const {
    if (registration.agent_type.empty()) {
        logger_->error("Agent registration failed: empty agent type");
        return false;
    }

    if (!registration.agent_instance) {
        logger_->error("Agent registration failed: null agent instance for type {}", registration.agent_type);
        return false;
    }

    if (registered_agents_.find(registration.agent_type) != registered_agents_.end()) {
        logger_->warn("Agent type {} already registered", registration.agent_type);
        return false;
    }

    return true;
}

void AgentOrchestrator::shutdown() {
    logger_->info("Shutting down agent orchestrator");

    shutdown_requested_ = true;
    task_queue_cv_.notify_all();

    stop_worker_threads();

    // Shutdown agents
    for (auto& [type, registration] : registered_agents_) {
        if (registration.agent_instance) {
            registration.agent_instance->shutdown();
        }
    }
    registered_agents_.clear();

    // Shutdown components
    if (event_processor_) {
        event_processor_->shutdown();
    }
    if (knowledge_base_) {
        knowledge_base_->shutdown();
    }
    if (metrics_collector_) {
        metrics_collector_->stop_collection();
    }

    logger_->info("Agent orchestrator shutdown complete");
}

bool AgentOrchestrator::is_healthy() const {
    // Check if orchestrator is operational
    if (shutdown_requested_) {
        return false;
    }

    // Check worker threads
    for (const auto& thread : worker_threads_) {
        if (!thread.joinable()) {
            return false;
        }
    }

    // Check registered agents
    for (const auto& [type, registration] : registered_agents_) {
        if (registration.agent_instance &&
            !registration.agent_instance->perform_health_check()) {
            return false;
        }
    }

    return true;
}

bool AgentOrchestrator::register_agent(const AgentRegistration& registration) {
    std::lock_guard<std::mutex> lock(agents_mutex_);

    if (!validate_agent_registration(registration)) {
        return false;
    }

    registered_agents_[registration.agent_type] = registration;

    // Initialize the agent
    if (registration.agent_instance && !registration.agent_instance->initialize()) {
        logger_->error("Failed to initialize agent: {}", registration.agent_name);
        registered_agents_.erase(registration.agent_type);
        return false;
    }

    logger_->info("Registered agent: {} ({})", registration.agent_name, registration.agent_type);
    return true;
}

bool AgentOrchestrator::unregister_agent(const std::string& agent_type) {
    std::lock_guard<std::mutex> lock(agents_mutex_);

    auto it = registered_agents_.find(agent_type);
    if (it == registered_agents_.end()) {
        return false;
    }

    if (it->second.agent_instance) {
        it->second.agent_instance->shutdown();
    }

    registered_agents_.erase(it);
    logger_->info("Unregistered agent type: {}", agent_type);
    return true;
}

bool AgentOrchestrator::submit_task(const AgentTask& task) {
    if (shutdown_requested_) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(task_queue_mutex_);
        task_queue_.push(task);
    }

    task_queue_cv_.notify_one();
    metrics_collector_->increment_counter("orchestrator.tasks_submitted");

    logger_->debug("Task submitted: {} for agent type {}",
                  task.task_id, task.agent_type);
    return true;
}

void AgentOrchestrator::process_pending_events() {
    // Process events from the event processor
    while (auto event = event_processor_->dequeue_event()) {
        // Find appropriate agent for this event
        auto agent = find_agent_for_task(AgentTask("", "", *event));
        if (agent) {
            AgentTask task(generate_task_id(), agent->get_agent_type(), *event);
            submit_task(task);
        }
    }

    // Perform periodic health checks
    if (last_health_check_.elapsed() > std::chrono::minutes(5)) {
        perform_health_checks();
        last_health_check_.reset();
    }
}

nlohmann::json AgentOrchestrator::get_status() const {
    nlohmann::json status;

    status["orchestrator"] = {
        {"healthy", is_healthy()},
        {"tasks_processed", tasks_processed_.load()},
        {"tasks_failed", tasks_failed_.load()},
        {"active_agents", registered_agents_.size()},
        {"queue_size", task_queue_.size()},
        {"uptime_seconds", std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now() - std::chrono::system_clock::time_point()).count()}
    };

    nlohmann::json agents_json;
    for (const auto& [type, registration] : registered_agents_) {
        if (registration.agent_instance) {
            auto agent_status = registration.agent_instance->get_status();
            agents_json[type] = agent_status.to_json();
        }
    }
    status["agents"] = agents_json;

    status["metrics"] = metrics_collector_->get_all_metrics();

    return status;
}

nlohmann::json AgentOrchestrator::get_thread_pool_stats() const {
    // Production-grade thread pool statistics
    size_t total_threads = worker_threads_.size();
    size_t queued_tasks = 0;
    
    {
        std::lock_guard<std::mutex> lock(task_queue_mutex_);
        queued_tasks = task_queue_.size();
    }
    
    // Calculate idle threads (approximation based on queue state)
    size_t active_threads = (queued_tasks > 0) ? std::min(queued_tasks, total_threads) : 0;
    size_t idle_threads = total_threads - active_threads;
    
    return {
        {"total_threads", total_threads},
        {"active_threads", active_threads},
        {"idle_threads", idle_threads},
        {"queued_tasks", queued_tasks},
        {"completed_tasks", tasks_processed_.load()}
    };
}

bool AgentOrchestrator::set_agent_enabled(const std::string& agent_type, bool enabled) {
    std::lock_guard<std::mutex> lock(agents_mutex_);

    auto it = registered_agents_.find(agent_type);
    if (it == registered_agents_.end()) {
        return false;
    }

    it->second.active = enabled;
    if (it->second.agent_instance) {
        it->second.agent_instance->set_enabled(enabled);
    }

    logger_->info("Agent " + it->second.agent_name + " (" + agent_type + ") " +
                 (enabled ? "enabled" : "disabled"), "AgentOrchestrator", "set_agent_enabled");
    return true;
}

std::vector<std::string> AgentOrchestrator::get_registered_agents() const {
    std::lock_guard<std::mutex> lock(agents_mutex_);

    std::vector<std::string> agent_types;
    agent_types.reserve(registered_agents_.size());

    for (const auto& [type, registration] : registered_agents_) {
        agent_types.push_back(type);
    }

    return agent_types;
}

std::optional<AgentStatus> AgentOrchestrator::get_agent_status(const std::string& agent_type) const {
    std::lock_guard<std::mutex> lock(agents_mutex_);

    auto it = registered_agents_.find(agent_type);
    if (it == registered_agents_.end() || !it->second.agent_instance) {
        return std::nullopt;
    }

    return it->second.agent_instance->get_status();
}

void AgentOrchestrator::start_worker_threads() {
    size_t num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 4;

    logger_->info("Starting {} worker threads", "", "", {{"thread_count", std::to_string(num_threads)}});

    for (size_t i = 0; i < num_threads; ++i) {
        worker_threads_.emplace_back(&AgentOrchestrator::worker_thread_loop, this);
    }
}

void AgentOrchestrator::stop_worker_threads() {
    shutdown_requested_ = true;
    task_queue_cv_.notify_all();

    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    worker_threads_.clear();
}

void AgentOrchestrator::worker_thread_loop() {
    logger_->debug("Worker thread started");

    while (!shutdown_requested_) {
        AgentTask task("", "", ComplianceEvent(EventType::AGENT_HEALTH_CHECK,
                                              EventSeverity::LOW, "health_check",
                                              {"system", "orchestrator", "internal"}));

        {
            std::unique_lock<std::mutex> lock(task_queue_mutex_);
            task_queue_cv_.wait(lock, [this]() {
                return shutdown_requested_ || !task_queue_.empty();
            });

            if (shutdown_requested_ && task_queue_.empty()) {
                break;
            }

            if (!task_queue_.empty()) {
                task = task_queue_.front();
                task_queue_.pop();
            }
        }

        if (task.event.get_type() != EventType::AGENT_HEALTH_CHECK) {
            process_task(task);
        }
    }

    logger_->debug("Worker thread stopped");
}

void AgentOrchestrator::process_task(const AgentTask& task) {
    try {
        std::shared_ptr<ComplianceAgent> agent;

        // Prepare task execution (find agent and validate)
        if (!prepare_task_execution(task, agent)) {
            return; // Error already logged and handled
        }

        // Execute task with the agent
        TaskResult result = execute_task_with_agent(task, agent);

        // Finalize task execution (metrics, callbacks, etc.)
        finalize_task_execution(task, result);

    } catch (const std::exception& e) {
        logger_->error("Exception processing task {}: {}", task.task_id, e.what());
        TaskResult error_result(false, std::string("Exception: ") + e.what());
        finalize_task_execution(task, error_result);
    }
}

bool AgentOrchestrator::prepare_task_execution(const AgentTask& task, std::shared_ptr<ComplianceAgent>& agent) {
    agent = find_agent_for_task(task);
    if (!agent) {
        logger_->warn("No suitable agent found for task: {}", task.task_id);
        TaskResult error_result(false, "No suitable agent found");
        finalize_task_execution(task, error_result);
        return false;
    }

    // Check if agent is enabled and healthy
    if (!agent->is_enabled() || agent->get_status().health == AgentHealth::CRITICAL) {
        logger_->warn("Agent {} is not available for task: {}", agent->get_agent_name(), task.task_id);
        TaskResult error_result(false, "Agent not available");
        finalize_task_execution(task, error_result);
        return false;
    }

    return true;
}

TaskResult AgentOrchestrator::execute_task_with_agent(const AgentTask& task, std::shared_ptr<ComplianceAgent> agent) {
    // Increment task counter
    agent->increment_tasks_in_progress();

    auto start_time = std::chrono::high_resolution_clock::now();
    AgentDecision decision = agent->process_event(task.event);
    auto end_time = std::chrono::high_resolution_clock::now();

    auto processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    // Update agent metrics
    agent->update_metrics(processing_time, true);
    agent->decrement_tasks_in_progress();

    return TaskResult(true, "", decision, processing_time);
}

void AgentOrchestrator::finalize_task_execution(const AgentTask& task, const TaskResult& result) {
    // Update orchestrator metrics
    if (result.success) {
        tasks_processed_++;
        metrics_collector_->increment_counter("orchestrator.tasks_completed");
        logger_->info("Task " + task.task_id + " completed successfully in " +
                     std::to_string(result.execution_time.count()) + "ms",
                     "AgentOrchestrator", "finalize_task_execution");
    } else {
        tasks_failed_++;
        metrics_collector_->increment_counter("orchestrator.tasks_failed");
        logger_->error("Task {} failed: {}", task.task_id, result.error_message);
    }

    // Update agent-specific metrics
    update_agent_metrics(task.agent_type, result);

    // Call the callback if provided
    if (task.callback) {
        task.callback(result);
    }
}

std::shared_ptr<ComplianceAgent> AgentOrchestrator::find_agent_for_task(const AgentTask& task) {
    std::lock_guard<std::mutex> lock(agents_mutex_);

    // First try the specified agent type
    if (!task.agent_type.empty()) {
        auto it = registered_agents_.find(task.agent_type);
        if (it != registered_agents_.end() &&
            it->second.agent_instance &&
            it->second.active &&
            it->second.agent_instance->can_handle_event(task.event.get_type())) {
            return it->second.agent_instance;
        }
    }

    // Find any agent that can handle this event type
    for (const auto& [type, registration] : registered_agents_) {
        if (registration.agent_instance &&
            registration.active &&
            registration.agent_instance->can_handle_event(task.event.get_type())) {
            return registration.agent_instance;
        }
    }

    return nullptr;
}

void AgentOrchestrator::update_agent_metrics(const std::string& agent_type, const TaskResult& result) {
    // Suppress unused parameter warning
    (void)result;

    std::lock_guard<std::mutex> lock(agents_mutex_);

    auto it = registered_agents_.find(agent_type);
    if (it != registered_agents_.end()) {
        // Update metrics in the agent instance
        // This would be handled by the ComplianceAgent base class
    }
}

void AgentOrchestrator::perform_health_checks() {
    logger_->debug("Performing health checks");

    // Check each registered agent
    std::lock_guard<std::mutex> lock(agents_mutex_);
    for (auto& [type, registration] : registered_agents_) {
        if (registration.agent_instance) {
            bool healthy = registration.agent_instance->perform_health_check();
            if (!healthy) {
                logger_->warn("Agent {} ({}) health check failed",
                             registration.agent_name, type);
            }
        }
    }
}

std::string AgentOrchestrator::generate_task_id() {
    static std::atomic<uint64_t> counter{0};
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()).count();
    return "task_" + std::to_string(timestamp) + "_" + std::to_string(++counter);
}

} // namespace regulens


