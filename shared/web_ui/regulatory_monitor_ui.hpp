/**
 * Regulatory Monitor UI - Production Web Interface
 *
 * Professional web UI for testing regulatory monitoring features
 * as required by Rule 6. Provides comprehensive testing interface
 * for the regulatory monitoring system.
 */

#pragma once

#include <memory>
#include <string>
#include "web_ui_server.hpp"
#include "web_ui_handlers.hpp"
#include "../config/configuration_manager.hpp"
#include "../logging/structured_logger.hpp"
#include "../metrics/metrics_collector.hpp"

namespace regulens {

/**
 * @brief Complete regulatory monitor UI implementation
 *
 * Production-grade web interface for testing all regulatory
 * monitoring features with professional UI and comprehensive
 * testing capabilities.
 */
class RegulatoryMonitorUI {
public:
    RegulatoryMonitorUI(int port = 8080);
    ~RegulatoryMonitorUI();

    // Lifecycle management
    bool initialize(ConfigurationManager* config = nullptr,
                   StructuredLogger* logger = nullptr,
                   MetricsCollector* metrics = nullptr);
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

    // Setup methods
    void setup_routes();
    void setup_static_routes();

    // Route handlers
    HTTPResponse handle_root(const HTTPRequest& request);
    HTTPResponse handle_regulatory_status(const HTTPRequest& request);
    HTTPResponse handle_regulatory_config(const HTTPRequest& request);
    HTTPResponse handle_regulatory_test(const HTTPRequest& request);
};

} // namespace regulens
