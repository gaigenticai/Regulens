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
    if (!server_) return false;

    // Main dashboard
    server_->add_route("GET", "/audit", [this](const HTTPRequest& req) {
        std::string html = generate_dashboard_html();
        return HTTPResponse{200, "text/html", html};
    });

    // Analyze audit trails
    server_->add_route("GET", "/audit/analyze", [this](const HTTPRequest& req) {
        if (!audit_agent_) {
            HTTPResponse response;
            response.status_code = 500;
            response.content_type = "application/json";
            response.body = nlohmann::json{{"error", "Audit agent not available"}}.dump();
            return response;
        }

        int hours = 24; // Default to 24 hours
        auto it = req.query_params.find("hours");
        if (it != req.query_params.end()) {
            try {
                hours = std::stoi(it->second);
            } catch (...) {
                hours = 24;
            }
        }

        auto anomalies = audit_agent_->analyze_audit_trails(hours);
        std::string html = generate_anomaly_report_html(anomalies);
        return HTTPResponse{200, "text/html", html};
    });

    // Test compliance monitoring
    server_->add_route("GET", "/audit/compliance", [this](const HTTPRequest& req) {
        if (!audit_agent_) {
            HTTPResponse response;
            response.status_code = 500;
            response.content_type = "application/json";
            response.body = nlohmann::json{{"error", "Audit agent not available"}}.dump();
            return response;
        }

        // Create a production-grade test compliance event with proper structure
        EventSource source{
            "audit_intelligence_ui",
            "ui_testing_instance_001",
            "local_system"
        };

        EventMetadata metadata;
        metadata["test_category"] = std::string("ui_demonstration");
        metadata["sample_id"] = std::string("compliance_check_001");

        ComplianceEvent test_event(
            EventType::AUDIT_LOG_ENTRY,
            EventSeverity::MEDIUM,
            "Test compliance event for UI demonstration - validates monitoring capabilities",
            source,
            metadata
        );

        auto decision = audit_agent_->perform_compliance_monitoring(test_event);
        std::string html = generate_risk_analysis_html(decision);
        return HTTPResponse{200, "text/html", html};
    });

    // Test fraud detection
    server_->add_route("GET", "/audit/fraud", [this](const HTTPRequest& req) {
        if (!audit_agent_) {
            HTTPResponse response;
            response.status_code = 500;
            response.content_type = "application/json";
            response.body = nlohmann::json{{"error", "Audit agent not available"}}.dump();
            return response;
        }

        // Create production-grade test transaction data
        nlohmann::json test_transaction = {
            {"amount", 5000.0},
            {"currency", "USD"},
            {"location", "Unknown Location"},
            {"usual_location", "New York"},
            {"transaction_type", "wire_transfer"},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()},
            {"account_id", "test_account_12345"},
            {"velocity_score", 0.75},
            {"risk_factors", nlohmann::json::array({"location_mismatch", "high_amount"})}
        };

        auto fraud_analysis = audit_agent_->detect_fraud_patterns(test_transaction);
        std::string html = generate_fraud_analysis_html(fraud_analysis);
        return HTTPResponse{200, "text/html", html};
    });

    // Generate audit report
    server_->add_route("GET", "/audit/report", [this](const HTTPRequest& req) {
        if (!audit_agent_) {
            HTTPResponse response;
            response.status_code = 500;
            response.content_type = "application/json";
            response.body = nlohmann::json{{"error", "Audit agent not available"}}.dump();
            return response;
        }

        auto now = std::chrono::system_clock::now();
        auto week_ago = now - std::chrono::hours(24 * 7);

        auto report = audit_agent_->generate_audit_report(week_ago, now);

        std::string template_content = load_template("audit_report.html");
        if (template_content.empty()) {
            std::string html = "<html><body><h1>Audit Report</h1><pre>" + report.dump(2) + "</pre></body></html>";
            return HTTPResponse{200, "text/html", html};
        }

        std::map<std::string, std::string> replacements;
        replacements["generated_at"] = std::to_string(report["generated_at"].get<long long>());
        replacements["report_content"] = report.dump(2);

        std::string html = replace_placeholders(template_content, replacements);
        return HTTPResponse{200, "text/html", html};
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
            EventSeverity sev = anomaly.get_severity();
            if (sev == EventSeverity::HIGH) severity_class = "high";
            else if (sev == EventSeverity::MEDIUM) severity_class = "medium";
            else if (sev == EventSeverity::CRITICAL) severity_class = "critical";

            anomalies_stream << R"(
        <div class="anomaly )" << severity_class << R"(">
            <h3>)" << event_type_to_string(anomaly.get_type()) << R"( - )" 
                    << event_severity_to_string(anomaly.get_severity()) << R"(</h3>
            <p><strong>Description:</strong> )" << anomaly.get_description() << R"(</p>
            <p><strong>Source:</strong> )" << anomaly.get_source().source_type << R"( - )" 
                    << anomaly.get_source().source_id << R"(</p>
            <p><strong>Time:</strong> )" << std::chrono::duration_cast<std::chrono::seconds>(
                anomaly.get_timestamp().time_since_epoch()).count() << R"(</p>
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
