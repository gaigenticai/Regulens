#include "agent_orchestrator.hpp"
#include "shared/event_processor.hpp"
#include "shared/knowledge_base.hpp"
#include "shared/metrics/metrics_collector.hpp"
#include "shared/logging/structured_logger.hpp"
#include "core/agent/compliance_agent.hpp"

namespace regulens {

AgentOrchestrator& AgentOrchestrator::get_instance() {
    static AgentOrchestrator instance;
    return instance;
}

AgentOrchestrator::AgentOrchestrator()
    : shutdown_requested_(false),
      tasks_processed_(0),
      tasks_failed_(0),
      last_health_check_(std::chrono::system_clock::now()) {}

AgentOrchestrator::~AgentOrchestrator() {
    shutdown();
}

bool AgentOrchestrator::initialize(std::shared_ptr<ConfigurationManager> config) {
    if (!config) {
        return false;
    }

    config_ = config;
    logger_ = std::make_shared<StructuredLogger>(StructuredLogger::get_instance());
    metrics_collector_ = std::make_shared<MetricsCollector>();
    event_processor_ = std::make_shared<EventProcessor>(logger_);
    knowledge_base_ = std::make_shared<KnowledgeBase>(config_, logger_);

    // Initialize components
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

    // Register system metrics
    metrics_collector_->register_counter("orchestrator.tasks_submitted");
    metrics_collector_->register_counter("orchestrator.tasks_completed");
    metrics_collector_->register_counter("orchestrator.tasks_failed");
    metrics_collector_->register_gauge("orchestrator.active_agents",
        [this]() { return static_cast<double>(registered_agents_.size()); });
    metrics_collector_->register_gauge("orchestrator.queue_size",
        [this]() { return static_cast<double>(task_queue_.size()); });

    // Start worker threads
    start_worker_threads();

    logger_->info("Agent orchestrator initialized successfully");
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

    if (registered_agents_.find(registration.agent_type) != registered_agents_.end()) {
        logger_->warn("Agent type {} already registered", registration.agent_type);
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

bool AgentOrchestrator::set_agent_enabled(const std::string& agent_type, bool enabled) {
    std::lock_guard<std::mutex> lock(agents_mutex_);

    auto it = registered_agents_.find(agent_type);
    if (it == registered_agents_.end()) {
        return false;
    }

    it->second.enabled = enabled;
    if (it->second.agent_instance) {
        it->second.agent_instance->set_enabled(enabled);
    }

    logger_->info("Agent {} ({}) {}", it->second.agent_name, agent_type,
                 enabled ? "enabled" : "disabled");
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

    logger_->info("Starting {} worker threads", num_threads);

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
                                              EventSeverity::LOW, "health_check"));

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
        auto agent = find_agent_for_task(task);
        if (!agent) {
            logger_->warn("No suitable agent found for task: {}", task.task_id);
            handle_task_result(task, TaskResult(false, "No suitable agent found"));
            return;
        }

        // Check if agent is enabled and healthy
        if (!agent->is_enabled() || agent->get_status().health == AgentHealth::CRITICAL) {
            logger_->warn("Agent {} is not available for task: {}", agent->get_agent_name(), task.task_id);
            handle_task_result(task, TaskResult(false, "Agent not available"));
            return;
        }

        // Process the task
        agent->increment_tasks_in_progress();

        auto start_time = std::chrono::high_resolution_clock::now();
        AgentDecision decision = agent->process_event(task.event);
        auto end_time = std::chrono::high_resolution_clock::now();

        auto processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);

        agent->update_metrics(processing_time, true);
        agent->decrement_tasks_in_progress();

        TaskResult result(true, "", decision, processing_time);
        handle_task_result(task, result);

    } catch (const std::exception& e) {
        logger_->error("Exception processing task {}: {}", task.task_id, e.what());
        handle_task_result(task, TaskResult(false, std::string("Exception: ") + e.what()));
    }
}

std::shared_ptr<ComplianceAgent> AgentOrchestrator::find_agent_for_task(const AgentTask& task) {
    std::lock_guard<std::mutex> lock(agents_mutex_);

    // First try the specified agent type
    if (!task.agent_type.empty()) {
        auto it = registered_agents_.find(task.agent_type);
        if (it != registered_agents_.end() &&
            it->second.agent_instance &&
            it->second.enabled &&
            it->second.agent_instance->can_handle_event(task.event.get_type())) {
            return it->second.agent_instance;
        }
    }

    // Find any agent that can handle this event type
    for (const auto& [type, registration] : registered_agents_) {
        if (registration.agent_instance &&
            registration.enabled &&
            registration.agent_instance->can_handle_event(task.event.get_type())) {
            return registration.agent_instance;
        }
    }

    return nullptr;
}

void AgentOrchestrator::handle_task_result(const AgentTask& task, const TaskResult& result) {
    if (result.success) {
        tasks_processed_++;
        metrics_collector_->increment_counter("orchestrator.tasks_completed");
        logger_->info("Task {} completed successfully in {}ms",
                     task.task_id, result.execution_time.count());
    } else {
        tasks_failed_++;
        metrics_collector_->increment_counter("orchestrator.tasks_failed");
        logger_->error("Task {} failed: {}", task.task_id, result.error_message);
    }

    update_agent_metrics(task.agent_type, result);

    // Call the callback if provided
    if (task.callback) {
        task.callback(result);
    }
}

void AgentOrchestrator::update_agent_metrics(const std::string& agent_type, const TaskResult& result) {
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


