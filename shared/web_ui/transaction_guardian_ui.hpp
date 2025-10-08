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
    HTTPResponse handle_transaction_submission(const HTTPRequest& req);
    HTTPResponse handle_monitoring_dashboard(const HTTPRequest& req);
    HTTPResponse handle_compliance_report(const HTTPRequest& req);
    HTTPResponse handle_velocity_check(const HTTPRequest& req);
    HTTPResponse handle_fraud_detection(const HTTPRequest& req);
};

} // namespace regulens
