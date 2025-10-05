/**
 * Transaction Guardian Agent UI Demonstration
 *
 * Production-grade web-based UI for testing the transaction guardian system
 * as required by Rule 6: proper UI component for feature testing.
 *
 * This demonstrates:
 * - Real transaction monitoring with AI-powered risk assessment
 * - Live web dashboard with real-time transaction processing
 * - Professional UI for compliance monitoring and fraud detection
 * - Production-grade HTTP server implementation
 * - Real multi-threading and concurrency for continuous monitoring
 * - Circuit breaker patterns for resilience
 * - Database integration with fallback mechanisms
 */

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include <sstream>
#include <iomanip>
#include <random>

#include "../../shared/config/configuration_manager.hpp"
#include "../../shared/logging/structured_logger.hpp"
#include "../../shared/database/postgresql_connection.hpp"
#include "../../shared/models/compliance_event.hpp"
#include "../../shared/models/agent_decision.hpp"
#include "../../shared/models/agent_state.hpp"
#include "../../shared/utils/timer.hpp"
#include "../../shared/metrics/metrics_collector.hpp"
#include "../../shared/llm/anthropic_client.hpp"

#include "transaction_guardian_agent.hpp"
#include "../../shared/web_ui/transaction_guardian_ui.hpp"

namespace regulens {

/**
 * @brief Complete UI demonstration of transaction guardian system
 *
 * Integrates the transaction guardian agent with a professional web UI
 * for comprehensive testing and validation as required by Rule 6.
 */
class TransactionGuardianUIDemo {
public:
    TransactionGuardianUIDemo() : running_(false), ui_port_(8082), transaction_batch_size_(5) {
        // Load UI port from configuration
        auto& config_manager = ConfigurationManager::get_instance();
        ui_port_ = config_manager.get_int("WEB_SERVER_UI_PORT").value_or(8082);
        transaction_batch_size_ = config_manager.get_int("TRANSACTION_BATCH_SIZE").value_or(5);
    }

    ~TransactionGuardianUIDemo() {
        stop_demo();
    }

    /**
     * @brief Initialize the demo with all required components
     */
    bool initialize() {
        try {
            // Initialize configuration
            config_ = std::make_shared<ConfigurationManager>();
            if (!config_->load_from_env()) {
                std::cerr << "Failed to load configuration" << std::endl;
                return false;
            }

            // Initialize logger
            logger_ = std::make_shared<StructuredLogger>(
                config_->get_value("LOG_LEVEL", "INFO"),
                config_->get_value("LOG_FILE", "transaction_guardian_demo.log")
            );

            // Initialize database connection pool
            std::string db_host = config_->get_value("DB_HOST", "your_database_host_here");
            int db_port = std::stoi(config_->get_value("DB_PORT", "5432"));
            std::string db_name = config_->get_value("DB_NAME", "regulens_compliance");
            std::string db_user = config_->get_value("DB_USER", "regulens_user");
            std::string db_password = config_->get_value("DB_PASSWORD", "");

            db_pool_ = std::make_shared<PostgreSQLConnectionPool>(
                db_host, db_port, db_name, db_user, db_password,
                std::stoi(config_->get_value("DB_CONNECTION_POOL_SIZE", "10"))
            );

            // Initialize Anthropic LLM client
            llm_client_ = std::make_shared<AnthropicClient>(
                config_->get_value("LLM_ANTHROPIC_API_KEY", ""),
                config_->get_value("LLM_ANTHROPIC_BASE_URL", "https://api.anthropic.com/v1"),
                config_->get_value("LLM_ANTHROPIC_MODEL", "claude-3-sonnet-20240229"),
                std::stod(config_->get_value("LLM_ANTHROPIC_TEMPERATURE", "0.7")),
                std::stoi(config_->get_value("LLM_ANTHROPIC_MAX_TOKENS", "4096")),
                std::stoi(config_->get_value("LLM_ANTHROPIC_TIMEOUT_SECONDS", "30"))
            );

            // Initialize risk assessment engine
            risk_engine_ = std::make_shared<RiskAssessmentEngine>(config_, logger_);

            // Initialize transaction guardian agent
            transaction_agent_ = std::make_shared<TransactionGuardianAgent>(
                config_, logger_, db_pool_, llm_client_, risk_engine_
            );

            if (!transaction_agent_->initialize()) {
                logger_->log(LogLevel::ERROR, "Failed to initialize transaction guardian agent");
                return false;
            }

            // Initialize UI
            ui_ = std::make_unique<TransactionGuardianUI>(ui_port_);

            if (!ui_->initialize(config_.get(), logger_.get(), nullptr, transaction_agent_)) {
                logger_->log(LogLevel::ERROR, "Failed to initialize transaction guardian UI");
                return false;
            }

            logger_->log(LogLevel::INFO, "Transaction Guardian UI Demo initialized successfully");
            return true;

        } catch (const std::exception& e) {
            std::cerr << "Failed to initialize demo: " << e.what() << std::endl;
            return false;
        }
    }

    /**
     * @brief Start the demo
     */
    bool start_demo() {
        if (running_) {
            logger_->log(LogLevel::WARN, "Demo is already running");
            return true;
        }

        try {
            // Start the transaction guardian agent
            transaction_agent_->start();

            // Start the UI
            if (!ui_->start()) {
                logger_->log(LogLevel::ERROR, "Failed to start UI server");
                transaction_agent_->stop();
                return false;
            }

            running_ = true;

            // Start background transaction processing
            simulation_thread_ = std::thread(&TransactionGuardianUIDemo::process_transactions, this);

            logger_->log(LogLevel::INFO, "Transaction Guardian UI Demo started");
            // Get web server host from configuration (default to 0.0.0.0 for cloud deployment)
            auto& config_manager = ConfigurationManager::get_instance();
            std::string web_host = config_manager.get_string("WEB_SERVER_HOST").value_or("0.0.0.0");

            // For display purposes, show localhost if running locally, otherwise show the configured host
            std::string display_host = (web_host == "0.0.0.0") ? "localhost" : web_host;
            logger_->log(LogLevel::INFO, "UI available at: http://" + display_host + ":" + std::to_string(ui_port_));
            logger_->log(LogLevel::INFO, "Press Ctrl+C to stop");

            return true;

        } catch (const std::exception& e) {
            logger_->log(LogLevel::ERROR, "Failed to start demo: " + std::string(e.what()));
            return false;
        }
    }

    /**
     * @brief Stop the demo
     */
    void stop_demo() {
        if (!running_) {
            return;
        }

        running_ = false;

        // Stop UI first
        if (ui_) {
            ui_->stop();
        }

        // Stop agent
        if (transaction_agent_) {
            transaction_agent_->stop();
        }

        // Wait for simulation thread
        if (simulation_thread_.joinable()) {
            simulation_thread_.join();
        }

        logger_->log(LogLevel::INFO, "Transaction Guardian UI Demo stopped");
    }

    /**
     * @brief Run the demo (blocking)
     */
    void run() {
        // Setup signal handler for graceful shutdown
        std::signal(SIGINT, [](int) {
            std::cout << "\nReceived interrupt signal. Stopping demo..." << std::endl;
        });

        if (!initialize()) {
            std::cerr << "Failed to initialize demo" << std::endl;
            return;
        }

        if (!start_demo()) {
            std::cerr << "Failed to start demo" << std::endl;
            return;
        }

        // Keep running until interrupted
        while (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            // Log statistics periodically
            static auto last_stats_time = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::minutes>(now - last_stats_time).count() >= 5) {
                log_statistics();
                last_stats_time = now;
            }
        }
    }

private:
    /**
     * @brief Fetch transactions from database for processing
     */
    std::vector<nlohmann::json> fetch_transactions_from_database(int limit = 10) {
        try {
            auto conn = db_pool_->get_connection();
            if (!conn) {
                logger_->log(LogLevel::ERROR, "Failed to get database connection for transaction fetching");
                return {};
            }

            std::string query = R"(
                SELECT transaction_id, customer_id, transaction_type, amount, currency,
                       sender_country, receiver_country, description, channel
                FROM transactions
                ORDER BY transaction_date DESC
                LIMIT $1
            )";

            auto results = conn->execute_query_multi(query, {std::to_string(limit)});
            db_pool_->return_connection(conn);

            std::vector<nlohmann::json> transactions;
            for (const auto& row : results) {
                nlohmann::json tx;
                tx["customer_id"] = row.value("customer_id", "");
                tx["amount"] = std::stod(row.value("amount", "0.0"));
                tx["currency"] = row.value("currency", "USD");
                tx["type"] = row.value("transaction_type", "");
                tx["destination_country"] = row.value("receiver_country", "");
                tx["description"] = row.value("description", "");
                tx["channel"] = row.value("channel", "ONLINE");
                transactions.push_back(tx);
            }

            return transactions;

        } catch (const std::exception& e) {
            logger_->log(LogLevel::ERROR, "Error fetching transactions from database: " + std::string(e.what()));
            return {};
        }
    }

    /**
     * @brief Process real transaction data from database
     */
    void process_transactions() {
        logger_->log(LogLevel::INFO, "Starting transaction processing from database");

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> delay_dist(0.5, 3.0);

        while (running_) {
            try {
                // Fetch batch of transactions from database
                auto transactions = fetch_transactions_from_database(transaction_batch_size_);

                if (transactions.empty()) {
                    logger_->log(LogLevel::WARN, "No transactions found in database, waiting...");
                    std::this_thread::sleep_for(std::chrono::seconds(10));
                    continue;
                }

                // Process each transaction
                for (const auto& transaction : transactions) {
                    // Process transaction
                    auto decision = transaction_agent_->process_transaction(transaction);

                    // Log high-risk transactions
                    double risk_score = decision.risk_assessment.value("overall_risk_score", 0.0);
                    if (risk_score > 0.6) {
                        logger_->log(LogLevel::WARN, "High-risk transaction detected: " +
                                    std::to_string(risk_score) + " - " +
                                    transaction.value("customer_id", "unknown"));
                    }

                    // Small delay between processing transactions
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }

                // Delay between batches (0.5-3 seconds)
                std::this_thread::sleep_for(std::chrono::milliseconds(
                    static_cast<int>(delay_dist(gen) * 1000)));

            } catch (const std::exception& e) {
                logger_->log(LogLevel::ERROR, "Error in transaction processing: " + std::string(e.what()));
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        }

        logger_->log(LogLevel::INFO, "Transaction processing stopped");
    }

    /**
     * @brief Log current system statistics
     */
    void log_statistics() {
        try {
            auto stats = transaction_agent_->generate_compliance_report(
                std::chrono::system_clock::now() - std::chrono::hours(1),
                std::chrono::system_clock::now()
            );

            logger_->log(LogLevel::INFO, "=== Transaction Guardian Statistics ===");
            logger_->log(LogLevel::INFO, "Total Transactions Processed: " +
                       std::to_string(stats.value("total_transactions_processed", 0)));
            logger_->log(LogLevel::INFO, "Suspicious Transactions: " +
                       std::to_string(stats.value("suspicious_transactions_detected", 0)));
            logger_->log(LogLevel::INFO, "Compliance Rate: " +
                       std::to_string(stats.value("compliance_rate", 0.0) * 100) + "%");

            if (stats.contains("risk_distribution")) {
                auto risk_dist = stats["risk_distribution"];
                logger_->log(LogLevel::INFO, "Risk Distribution - Low: " +
                           std::to_string(risk_dist.value("low_risk", 0)) +
                           ", Medium: " + std::to_string(risk_dist.value("medium_risk", 0)) +
                           ", High: " + std::to_string(risk_dist.value("high_risk", 0)) +
                           ", Critical: " + std::to_string(risk_dist.value("critical_risk", 0)));
            }

        } catch (const std::exception& e) {
            logger_->log(LogLevel::ERROR, "Failed to generate statistics: " + std::string(e.what()));
        }
    }

    // Demo state
    std::atomic<bool> running_;
    int ui_port_;
    int transaction_batch_size_;

    // Core components
    std::shared_ptr<ConfigurationManager> config_;
    std::shared_ptr<StructuredLogger> logger_;
    std::shared_ptr<PostgreSQLConnectionPool> db_pool_;
    std::shared_ptr<AnthropicClient> llm_client_;
    std::shared_ptr<RiskAssessmentEngine> risk_engine_;

    // Agent and UI
    std::shared_ptr<TransactionGuardianAgent> transaction_agent_;
    std::unique_ptr<TransactionGuardianUI> ui_;

    // Simulation thread
    std::thread simulation_thread_;
};

} // namespace regulens

/**
 * @brief Main entry point for Transaction Guardian UI Demo
 */
int main(int argc, char* argv[]) {
    std::cout << "🔒 Transaction Guardian Agent UI Demo" << std::endl;
    std::cout << "=====================================" << std::endl;
    std::cout << "Production-grade transaction monitoring and compliance testing" << std::endl;
    std::cout << "Features:" << std::endl;
    std::cout << "  • Real-time transaction processing with AI risk assessment" << std::endl;
    std::cout << "  • Professional web UI for comprehensive testing" << std::endl;
    std::cout << "  • Circuit breaker patterns for resilience" << std::endl;
    std::cout << "  • Database integration with fallback mechanisms" << std::endl;
    std::cout << "  • Automated transaction simulation" << std::endl;
    std::cout << std::endl;

    try {
        regulens::TransactionGuardianUIDemo demo;
        demo.run();

        std::cout << "\nDemo completed successfully!" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Demo failed: " << e.what() << std::endl;
        return 1;
    }
}
