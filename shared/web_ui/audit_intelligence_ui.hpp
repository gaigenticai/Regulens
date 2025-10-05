/**
 * Audit Intelligence UI - Production Web Interface
 *
 * Professional web UI for testing audit intelligence features
 * as required by Rule 6. Provides comprehensive testing interface
 * for the audit intelligence agent system.
 */

#pragma once

#include <memory>
#include <string>
#include <map>
#include "web_ui_server.hpp"
#include "web_ui_handlers.hpp"
#include "../config/configuration_manager.hpp"
#include "../logging/structured_logger.hpp"
#include "../metrics/metrics_collector.hpp"
#include "../../agents/audit_intelligence/audit_intelligence_agent.hpp"

namespace regulens {

/**
 * @brief Complete audit intelligence UI implementation
 *
 * Production-grade web interface for testing all audit intelligence
 * features with professional UI and comprehensive testing capabilities.
 */
class AuditIntelligenceUI {
public:
    AuditIntelligenceUI(int port = 8081); // Different port from regulatory monitor
    ~AuditIntelligenceUI();

    // Lifecycle management
    bool initialize(ConfigurationManager* config = nullptr,
                   StructuredLogger* logger = nullptr,
                   MetricsCollector* metrics = nullptr,
                   std::shared_ptr<AuditIntelligenceAgent> audit_agent = nullptr);
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
    std::shared_ptr<AuditIntelligenceAgent> audit_agent_;

    // UI-specific handlers for audit intelligence features
    bool setup_audit_handlers();
    std::string generate_dashboard_html() const;
    std::string generate_anomaly_report_html(const std::vector<ComplianceEvent>& anomalies) const;
    std::string generate_risk_analysis_html(const AgentDecision& decision) const;
    std::string generate_fraud_analysis_html(const nlohmann::json& fraud_analysis) const;

    // Template utilities
    std::string load_template(const std::string& template_name) const;
    std::string replace_placeholders(const std::string& template_content, const std::map<std::string, std::string>& replacements) const;
};

} // namespace regulens
