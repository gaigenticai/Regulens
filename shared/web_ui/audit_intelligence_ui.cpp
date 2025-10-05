/**
 * Audit Intelligence UI Implementation
 *
 * Production-grade web interface for testing audit intelligence agent
 * as required by Rule 6. Provides comprehensive testing capabilities
 * for anomaly detection, risk assessment, and fraud analysis.
 */

#include "audit_intelligence_ui.hpp"
#include <sstream>
#include <iomanip>
#include <chrono>
#include <fstream>
#include <filesystem>
#include "../utils/timer.hpp"

namespace regulens {

AuditIntelligenceUI::AuditIntelligenceUI(int port)
    : port_(port), config_manager_(nullptr), logger_(nullptr),
      metrics_collector_(nullptr) {
}

AuditIntelligenceUI::~AuditIntelligenceUI() {
    stop();
}

bool AuditIntelligenceUI::initialize(ConfigurationManager* config,
                                   StructuredLogger* logger,
                                   MetricsCollector* metrics,
                                   std::shared_ptr<AuditIntelligenceAgent> audit_agent) {
    config_manager_ = config;
    logger_ = logger;
    metrics_collector_ = metrics;
    audit_agent_ = audit_agent;

    if (logger_) {
        logger_->log(LogLevel::INFO, "Initializing Audit Intelligence UI on port " + std::to_string(port_));
    }

    try {
        // Initialize web server
        server_ = std::make_unique<WebUIServer>(port_);
        handlers_ = std::make_unique<WebUIHandlers>();

        // Setup audit intelligence specific handlers
        if (!setup_audit_handlers()) {
            if (logger_) {
                logger_->log(LogLevel::ERROR, "Failed to setup audit intelligence handlers");
            }
            return false;
        }

        if (logger_) {
            logger_->log(LogLevel::INFO, "Audit Intelligence UI initialized successfully");
        }
        return true;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->log(LogLevel::ERROR, "Failed to initialize Audit Intelligence UI: " + std::string(e.what()));
        }
        return false;
    }
}

bool AuditIntelligenceUI::start() {
    if (!server_) {
        if (logger_) {
            logger_->log(LogLevel::ERROR, "Server not initialized");
        }
        return false;
    }

    if (logger_) {
        logger_->log(LogLevel::INFO, "Starting Audit Intelligence UI server");
    }

    try {
        return server_->start();
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->log(LogLevel::ERROR, "Failed to start Audit Intelligence UI: " + std::string(e.what()));
        }
        return false;
    }
}

void AuditIntelligenceUI::stop() {
    if (server_) {
        if (logger_) {
            logger_->log(LogLevel::INFO, "Stopping Audit Intelligence UI server");
        }
        server_->stop();
    }
}

bool AuditIntelligenceUI::is_running() const {
    return server_ && server_->is_running();
}

bool AuditIntelligenceUI::setup_audit_handlers() {
    if (!server_ || !handlers_) return false;

    // Main dashboard
    server_->add_handler("/audit", [this](const httplib::Request& req, httplib::Response& res) {
        res.set_content(generate_dashboard_html(), "text/html");
    });

    // Analyze audit trails
    server_->add_handler("/audit/analyze", [this](const httplib::Request& req, httplib::Response& res) {
        if (!audit_agent_) {
            res.status = 500;
            res.set_content("Audit agent not available", "text/plain");
            return;
        }

        int hours = 24; // Default to 24 hours
        if (req.has_param("hours")) {
            try {
                hours = std::stoi(req.get_param_value("hours"));
            } catch (...) {
                hours = 24;
            }
        }

        auto anomalies = audit_agent_->analyze_audit_trails(hours);
        res.set_content(generate_anomaly_report_html(anomalies), "text/html");
    });

    // Test compliance monitoring
    server_->add_handler("/audit/compliance", [this](const httplib::Request& req, httplib::Response& res) {
        if (!audit_agent_) {
            res.status = 500;
            res.set_content("Audit agent not available", "text/plain");
            return;
        }

        // Create a test compliance event
        ComplianceEvent test_event(
            "TEST_COMPLIANCE_EVENT",
            "MEDIUM",
            "Test compliance event for UI demonstration",
            "audit_intelligence_ui",
            "ui_testing",
            nlohmann::json{{"test_data", "sample_compliance_check"}},
            std::chrono::system_clock::now()
        );

        auto decision = audit_agent_->perform_compliance_monitoring(test_event);
        res.set_content(generate_risk_analysis_html(decision), "text/html");
    });

    // Test fraud detection
    server_->add_handler("/audit/fraud", [this](const httplib::Request& req, httplib::Response& res) {
        if (!audit_agent_) {
            res.status = 500;
            res.set_content("Audit agent not available", "text/plain");
            return;
        }

        // Create test transaction data
        nlohmann::json test_transaction = {
            {"amount", 5000.0},
            {"currency", "USD"},
            {"location", "Unknown Location"},
            {"usual_location", "New York"},
            {"transaction_type", "wire_transfer"},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };

        auto fraud_analysis = audit_agent_->detect_fraud_patterns(test_transaction);
        res.set_content(generate_fraud_analysis_html(fraud_analysis), "text/html");
    });

    // Generate audit report
    server_->add_handler("/audit/report", [this](const httplib::Request& req, httplib::Response& res) {
        if (!audit_agent_) {
            res.status = 500;
            res.set_content("Audit agent not available", "text/plain");
            return;
        }

        auto now = std::chrono::system_clock::now();
        auto week_ago = now - std::chrono::hours(24 * 7);

        auto report = audit_agent_->generate_audit_report(week_ago, now);

        std::string template_content = load_template("audit_report.html");
        if (template_content.empty()) {
            res.set_content("<html><body><h1>Error: Template not found</h1></body></html>", "text/html");
            return;
        }

        std::map<std::string, std::string> replacements;
        replacements["generated_at"] = std::to_string(report["generated_at"].get<long long>());
        replacements["report_content"] = report.dump(2);

        std::string html = replace_placeholders(template_content, replacements);
        res.set_content(html, "text/html");
    });

    return true;
}

std::string AuditIntelligenceUI::generate_dashboard_html() const {
    std::string template_content = load_template("dashboard.html");
    if (template_content.empty()) {
        return "<html><body><h1>Error: Template not found</h1></body></html>";
    }

    std::map<std::string, std::string> replacements;
    replacements["port"] = std::to_string(port_);

    if (audit_agent_) {
        replacements["agent_status_class"] = "good";
        replacements["agent_status_text"] = "✅ Agent Connected";
        replacements["agent_status"] = "Connected and Ready";
    } else {
        replacements["agent_status_class"] = "error";
        replacements["agent_status_text"] = "❌ Agent Not Connected";
        replacements["agent_status"] = "Not Connected";
    }

    return replace_placeholders(template_content, replacements);
}

std::string AuditIntelligenceUI::generate_anomaly_report_html(const std::vector<ComplianceEvent>& anomalies) const {
    std::string template_content = load_template("anomaly_report.html");
    if (template_content.empty()) {
        return "<html><body><h1>Error: Template not found</h1></body></html>";
    }

    std::map<std::string, std::string> replacements;
    replacements["anomaly_count"] = std::to_string(anomalies.size());

    std::stringstream anomalies_stream;
    if (anomalies.empty()) {
        anomalies_stream << R"(
        <div class="anomaly low">
            <h3>✅ No Anomalies Detected</h3>
            <p>All audit trails appear normal. No anomalies requiring attention at this time.</p>
        </div>)";
    } else {
        for (const auto& anomaly : anomalies) {
            std::string severity_class = "low";
            if (anomaly.severity == "HIGH") severity_class = "high";
            else if (anomaly.severity == "MEDIUM") severity_class = "medium";

            anomalies_stream << R"(
        <div class="anomaly )" << severity_class << R"(">
            <h3>)" << anomaly.event_type << R"( - )" << anomaly.severity << R"(</h3>
            <p><strong>Description:</strong> )" << anomaly.description << R"(</p>
            <p><strong>Source:</strong> )" << anomaly.source_type << R"( - )" << anomaly.source_id << R"(</p>
            <p><strong>Time:</strong> )" << std::chrono::duration_cast<std::chrono::seconds>(
                anomaly.occurred_at.time_since_epoch()).count() << R"(</p>
        </div>)";
        }
    }

    replacements["anomalies_list"] = anomalies_stream.str();

    return replace_placeholders(template_content, replacements);
}

std::string AuditIntelligenceUI::generate_risk_analysis_html(const AgentDecision& decision) const {
    std::string template_content = load_template("risk_analysis.html");
    if (template_content.empty()) {
        return "<html><body><h1>Error: Template not found</h1></body></html>";
    }

    std::map<std::string, std::string> replacements;
    replacements["decision_type"] = decision_type_to_string(decision.get_type());
    replacements["confidence_class"] = confidence_to_string(decision.get_confidence());
    replacements["confidence_text"] = confidence_to_string(decision.get_confidence());
    replacements["agent_id"] = decision.get_agent_id();
    replacements["event_id"] = decision.get_event_id();

    std::stringstream actions_stream;
    for (const auto& action : decision.get_actions()) {
        actions_stream << "<li><strong>" << action.action_type << ":</strong> " << action.description << "</li>";
    }
    replacements["actions_list"] = actions_stream.str();

    return replace_placeholders(template_content, replacements);
}

std::string AuditIntelligenceUI::generate_fraud_analysis_html(const nlohmann::json& fraud_analysis) const {
    std::string template_content = load_template("fraud_analysis.html");
    if (template_content.empty()) {
        return "<html><body><h1>Error: Template not found</h1></body></html>";
    }

    std::map<std::string, std::string> replacements;

    if (fraud_analysis.contains("risk_score")) {
        double risk_score = fraud_analysis["risk_score"];
        std::string risk_class = "low-risk";
        std::string risk_level = "Low Risk";

        if (risk_score > 0.7) {
            risk_class = "high-risk";
            risk_level = "High Risk";
        } else if (risk_score > 0.4) {
            risk_class = "medium-risk";
            risk_level = "Medium Risk";
        }

        replacements["risk_score_class"] = risk_class;
        replacements["risk_level"] = risk_level;
        replacements["risk_score_value"] = "(" + std::to_string(risk_score) + ")";
    } else {
        replacements["risk_score_class"] = "low-risk";
        replacements["risk_level"] = "No Risk Score Available";
        replacements["risk_score_value"] = "";
    }

    std::stringstream recommendations_stream;
    if (fraud_analysis.contains("recommendations")) {
        for (const auto& rec : fraud_analysis["recommendations"]) {
            recommendations_stream << "<li>" << rec.get<std::string>() << "</li>";
        }
    }
    replacements["recommendations_list"] = recommendations_stream.str();

    return replace_placeholders(template_content, replacements);
}

std::string AuditIntelligenceUI::load_template(const std::string& template_name) const {
    std::filesystem::path template_path = std::filesystem::path(__FILE__).parent_path() / "templates" / template_name;
    std::ifstream file(template_path);
    if (!file.is_open()) {
        if (logger_) {
            logger_->log(LogLevel::ERROR, "Failed to load template: " + template_path.string());
        }
        return "";
    }
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return content;
}

std::string AuditIntelligenceUI::replace_placeholders(const std::string& template_content, const std::map<std::string, std::string>& replacements) const {
    std::string result = template_content;
    for (const auto& [placeholder, value] : replacements) {
        std::string search = "{{" + placeholder + "}}";
        size_t pos = 0;
        while ((pos = result.find(search, pos)) != std::string::npos) {
            result.replace(pos, search.length(), value);
            pos += value.length();
        }
    }
    return result;
}

} // namespace regulens
