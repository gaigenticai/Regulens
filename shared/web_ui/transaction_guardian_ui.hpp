/**
 * Transaction Guardian UI - Production Web Interface
 *
 * Professional web UI for testing transaction guardian features
 * as required by Rule 6. Provides comprehensive testing interface
 * for the transaction monitoring and compliance system.
 */

#pragma once

#include <memory>
#include <string>
#include "web_ui_server.hpp"
#include "web_ui_handlers.hpp"
#include "../config/configuration_manager.hpp"
#include "../logging/structured_logger.hpp"
#include "../metrics/metrics_collector.hpp"
#include "../../agents/transaction_guardian/transaction_guardian_agent.hpp"

namespace regulens {

/**
 * @brief Complete transaction guardian UI implementation
 *
 * Production-grade web interface for testing all transaction guardian
 * features with professional UI and comprehensive testing capabilities.
 */
class TransactionGuardianUI {
public:
    TransactionGuardianUI(int port = 8082); // Different port from other UIs
    ~TransactionGuardianUI();

    // Lifecycle management
    bool initialize(ConfigurationManager* config = nullptr,
                   StructuredLogger* logger = nullptr,
                   MetricsCollector* metrics = nullptr,
                   std::shared_ptr<TransactionGuardianAgent> transaction_agent = nullptr);
    bool start();
    void stop();
    bool is_running() const;

    // Server access for advanced operations
    WebUIServer* get_server() { return server_.get(); }

private:
    int port_;
    std::unique_ptr<WebUIServer> server_;
    std::unique_ptr<WebUIHandlers> handlers_;
    ConfigurationManager* config_manager_;
    StructuredLogger* logger_;
    MetricsCollector* metrics_collector_;
    std::shared_ptr<TransactionGuardianAgent> transaction_agent_;

    // UI setup and handlers
    void setup_routes();
    void setup_transaction_handlers();
    std::string generate_main_page();
    std::string generate_transaction_form();
    std::string generate_monitoring_dashboard();
    std::string generate_compliance_report_page();

    // Handler methods
    void handle_transaction_submission(const httplib::Request& req, httplib::Response& res);
    void handle_monitoring_dashboard(const httplib::Request& req, httplib::Response& res);
    void handle_compliance_report(const httplib::Request& req, httplib::Response& res);
    void handle_velocity_check(const httplib::Request& req, httplib::Response& res);
    void handle_fraud_detection(const httplib::Request& req, httplib::Response& res);
};

} // namespace regulens
