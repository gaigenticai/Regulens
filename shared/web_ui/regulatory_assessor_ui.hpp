/**
 * Regulatory Assessor UI Implementation
 *
 * Production-grade web interface for testing regulatory assessor agent
 * as required by Rule 6. Provides comprehensive testing capabilities
 * for regulatory change assessment, impact analysis, and compliance monitoring.
 */

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "../config/configuration_manager.hpp"
#include "../logging/structured_logger.hpp"
#include "../metrics/metrics_collector.hpp"
#include "web_ui_server.hpp"
#include "web_ui_handlers.hpp"
#include "../../agents/regulatory_assessor/regulatory_assessor_agent.hpp"

namespace regulens {

/**
 * @brief Web UI for Regulatory Assessor Agent testing and monitoring
 */
class RegulatoryAssessorUI {
public:
    RegulatoryAssessorUI(int port);
    ~RegulatoryAssessorUI();

    /**
     * @brief Initialize the regulatory assessor UI
     */
    bool initialize(ConfigurationManager* config,
                   StructuredLogger* logger,
                   MetricsCollector* metrics,
                   std::shared_ptr<RegulatoryAssessorAgent> regulatory_agent);

    /**
     * @brief Start the web server
     */
    bool start();

    /**
     * @brief Stop the web server
     */
    void stop();

    /**
     * @brief Check if the server is running
     */
    bool is_running() const;

private:
    /**
     * @brief Setup regulatory assessor specific HTTP handlers
     */
    bool setup_regulatory_handlers();

    /**
     * @brief Handler for regulatory assessment requests
     */
    static std::string handle_assess_regulation(const std::string& method,
                                               const std::string& path,
                                               const std::string& body,
                                               RegulatoryAssessorUI* ui);

    /**
     * @brief Handler for regulatory impact analysis
     */
    static std::string handle_impact_analysis(const std::string& method,
                                             const std::string& path,
                                             const std::string& body,
                                             RegulatoryAssessorUI* ui);

    /**
     * @brief Handler for regulatory change monitoring
     */
    static std::string handle_monitor_changes(const std::string& method,
                                             const std::string& path,
                                             const std::string& body,
                                             RegulatoryAssessorUI* ui);

    /**
     * @brief Handler for regulatory assessment reports
     */
    static std::string handle_assessment_report(const std::string& method,
                                               const std::string& path,
                                               const std::string& body,
                                               RegulatoryAssessorUI* ui);

    /**
     * @brief Generate HTML dashboard for regulatory assessor
     */
    std::string generate_dashboard_html() const;

    /**
     * @brief Generate assessment form HTML
     */
    std::string generate_assessment_form_html() const;

    /**
     * @brief Generate impact analysis form HTML
     */
    std::string generate_impact_form_html() const;

    /**
     * @brief Generate monitoring dashboard HTML
     */
    std::string generate_monitoring_html() const;

    int port_;
    ConfigurationManager* config_manager_;
    StructuredLogger* logger_;
    MetricsCollector* metrics_collector_;
    std::shared_ptr<RegulatoryAssessorAgent> regulatory_agent_;
    std::unique_ptr<WebUIServer> server_;
    std::unique_ptr<WebUIHandlers> handlers_;
    std::atomic<bool> running_;
};

} // namespace regulens
