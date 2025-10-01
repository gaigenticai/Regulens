#include <iostream>
#include <memory>
#include <csignal>
#include <thread>
#include <chrono>

#include "config/configuration_manager.hpp"
#include "core/agent/agent_orchestrator.hpp"
#include "shared/logging/structured_logger.hpp"
#include "shared/metrics/metrics_collector.hpp"

namespace regulens {

// Global shutdown flag for graceful termination
std::atomic<bool> g_shutdown_requested{false};

// Signal handler for graceful shutdown
void signal_handler(int signal) {
    StructuredLogger::get_instance().info("Received shutdown signal: {}", signal);
    g_shutdown_requested = true;
}

// Main application class - production-grade initialization and lifecycle management
class RegulensApplication {
public:
    RegulensApplication(int argc, char* argv[])
        : config_manager_(std::make_unique<ConfigurationManager>(argc, argv)),
          logger_(StructuredLogger::get_instance()),
          metrics_collector_(std::make_unique<MetricsCollector>()),
          agent_orchestrator_(std::make_unique<AgentOrchestrator>()) {

        logger_.info("Initializing Regulens Agentic AI Compliance System v{}", REGULENS_VERSION);
        initialize_components();
    }

    ~RegulensApplication() {
        logger_.info("Shutting down Regulens system gracefully");
        shutdown_components();
    }

    // Main application loop with proper error handling
    int run() {
        try {
            logger_.info("Starting agent orchestration engine");

            // Health check before starting
            if (!perform_health_checks()) {
                logger_.error("Health checks failed, aborting startup");
                return EXIT_FAILURE;
            }

            // Start metrics collection
            metrics_collector_->start_collection();

            // Main processing loop
            while (!g_shutdown_requested) {
                agent_orchestrator_->process_pending_events();

                // Periodic health monitoring
                if (health_check_timer_.elapsed() > std::chrono::minutes(5)) {
                    perform_health_checks();
                    health_check_timer_.reset();
                }

                // Prevent busy waiting
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            logger_.info("Shutdown requested, terminating gracefully");
            return EXIT_SUCCESS;

        } catch (const std::exception& e) {
            logger_.error("Critical error in main application loop: {}", e.what());
            return EXIT_FAILURE;
        } catch (...) {
            logger_.error("Unknown critical error in main application loop");
            return EXIT_FAILURE;
        }
    }

private:
    void initialize_components() {
        logger_.info("Initializing system components");

        // Load and validate configuration
        if (!config_manager_->load_configuration()) {
            throw std::runtime_error("Failed to load configuration");
        }

        // Initialize agent orchestrator with configuration manager
        agent_orchestrator_->initialize(config_manager_);

        // Register metrics
        register_system_metrics();

        logger_.info("All components initialized successfully");
    }

    void shutdown_components() {
        logger_.info("Shutting down system components");

        // Stop metrics collection
        metrics_collector_->stop_collection();

        // Shutdown agents gracefully
        agent_orchestrator_->shutdown();

        // Final log flush
        logger_.flush();

        logger_.info("All components shut down successfully");
    }

    bool perform_health_checks() {
        bool all_healthy = true;

        // Check configuration validity
        if (!config_manager_->validate_configuration()) {
            logger_.error("Configuration validation failed");
            all_healthy = false;
        }

        // Check agent orchestrator health
        if (!agent_orchestrator_->is_healthy()) {
            logger_.error("Agent orchestrator health check failed");
            all_healthy = false;
        }

        // Check data ingestion connectivity
        if (!check_data_sources_connectivity()) {
            logger_.error("Data source connectivity check failed");
            all_healthy = false;
        }

        // Check regulatory monitor status
        if (!check_regulatory_monitor_status()) {
            logger_.error("Regulatory monitor status check failed");
            all_healthy = false;
        }

        if (all_healthy) {
            logger_.info("All health checks passed");
        }

        return all_healthy;
    }

    void register_system_metrics() {
        metrics_collector_->register_gauge("system.uptime_seconds",
            []() { return std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count(); });

        metrics_collector_->register_counter("system.health_checks_total");
        metrics_collector_->register_histogram("agent.processing_time_ms");
    }

    bool check_data_sources_connectivity() {
        // Implementation will check database, API, and stream connections
        // This is a placeholder for the actual implementation
        return true; // Will be replaced with real connectivity checks
    }

    bool check_regulatory_monitor_status() {
        // Implementation will check regulatory data sources
        // This is a placeholder for the actual implementation
        return true; // Will be replaced with real status checks
    }

    std::unique_ptr<ConfigurationManager> config_manager_;
    StructuredLogger& logger_;
    std::unique_ptr<MetricsCollector> metrics_collector_;
    std::unique_ptr<AgentOrchestrator> agent_orchestrator_;
    Timer health_check_timer_;
};

} // namespace regulens

int main(int argc, char* argv[]) {
    // Set up signal handlers for graceful shutdown
    std::signal(SIGINT, regulens::signal_handler);
    std::signal(SIGTERM, regulens::signal_handler);

    try {
        // Initialize and run the application
        regulens::RegulensApplication app(argc, argv);
        return app.run();

    } catch (const std::exception& e) {
        std::cerr << "Critical error during startup: " << e.what() << std::endl;
        return EXIT_FAILURE;
    } catch (...) {
        std::cerr << "Unknown critical error during startup" << std::endl;
        return EXIT_FAILURE;
    }
}
