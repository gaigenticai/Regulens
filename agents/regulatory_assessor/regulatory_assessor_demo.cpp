/**
 * Regulatory Assessor Agent UI Demonstration
 *
 * Production-grade web-based UI for testing the regulatory assessor system
 * as required by Rule 6: proper UI component for feature testing.
 *
 * This demonstrates:
 * - Real regulatory impact assessment with AI-powered analysis
 * - Live web dashboard with real-time regulatory change monitoring
 * - Professional UI for compliance assessment and adaptation planning
 * - Production-grade HTTP server implementation
 * - Real multi-threading and concurrency for continuous monitoring
 */

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include <sstream>
#include <iomanip>
#include <unordered_map>
#include <optional>

#include "../../shared/config/configuration_manager.hpp"
#include "../../shared/logging/structured_logger.hpp"
#include "../../shared/database/postgresql_connection.hpp"
#include "../../shared/models/compliance_event.hpp"
#include "../../shared/models/agent_decision.hpp"
#include "../../shared/models/agent_state.hpp"
#include "../../shared/utils/timer.hpp"
#include "../../shared/metrics/metrics_collector.hpp"
#include "../../shared/llm/anthropic_client.hpp"
#include "../../shared/knowledge_base.hpp"
#include "../../shared/web_ui/web_ui_server.hpp"
#include "../../shared/regulatory_knowledge_base.hpp"

#include "../../regulatory_monitor/regulatory_monitor.hpp"
#include "../../regulatory_monitor/regulatory_source.hpp"

#include "regulatory_assessor_agent.hpp"

namespace regulens {

/**
 * @brief Complete UI demonstration of regulatory assessor system
 *
 * Integrates the regulatory assessor agent with a professional web UI
 * for comprehensive testing and validation as required by Rule 6.
 */
class RegulatoryAssessorUIDemo {
public:
    RegulatoryAssessorUIDemo() : running_(false), ui_port_(8082) {
        // Load UI port from configuration
        auto& config_manager = ConfigurationManager::get_instance();
        ui_port_ = config_manager.get_int("WEB_SERVER_UI_PORT").value_or(8082);
    }

    ~RegulatoryAssessorUIDemo() {
        stop_demo();
    }

    /**
     * @brief Initialize the regulatory assessor demo with all components
     */
    bool initialize() {
        try {
            logger_->log(LogLevel::INFO, "Initializing Regulatory Assessor UI Demo");

            // Initialize configuration
            if (!config_->initialize()) {
                logger_->log(LogLevel::ERROR, "Failed to initialize configuration manager");
                return false;
            }

            // Initialize database connection pool
            db_pool_ = std::make_shared<PostgreSQLConnectionPool>(config_, logger_);
            if (!db_pool_->initialize()) {
                logger_->log(LogLevel::ERROR, "Failed to initialize database connection pool");
                return false;
            }

            // Initialize LLM client
            llm_client_ = std::make_shared<AnthropicClient>(config_, logger_, nullptr);
            if (!llm_client_->initialize()) {
                logger_->log(LogLevel::ERROR, "Failed to initialize LLM client");
                return false;
            }

            // Initialize knowledge base
            knowledge_base_ = std::make_shared<VectorKnowledgeBase>(config_, logger_, db_pool_);
            if (!knowledge_base_->initialize()) {
                logger_->log(LogLevel::WARN, "Failed to initialize knowledge base - continuing without it");
            }

            // Initialize regulatory assessor agent
            assessor_agent_ = std::make_shared<RegulatoryAssessorAgent>(
                config_, logger_, db_pool_, llm_client_, knowledge_base_
            );

            if (!assessor_agent_->initialize()) {
                logger_->log(LogLevel::ERROR, "Failed to initialize regulatory assessor agent");
                return false;
            }

            // Initialize regulatory knowledge base
            regulatory_kb_ = std::make_shared<RegulatoryKnowledgeBase>(config_, logger_, db_pool_);
            if (!regulatory_kb_->initialize()) {
                logger_->log(LogLevel::WARN, "Failed to initialize regulatory knowledge base - continuing without it");
            }

            // Initialize regulatory monitor
            regulatory_monitor_ = std::make_shared<RegulatoryMonitor>(config_, logger_, regulatory_kb_);
            if (!regulatory_monitor_->initialize()) {
                logger_->log(LogLevel::ERROR, "Failed to initialize regulatory monitor");
                return false;
            }

            // Add real regulatory sources
            if (!add_regulatory_sources()) {
                logger_->log(LogLevel::WARN, "Failed to add some regulatory sources - continuing with available sources");
            }

            // Initialize web UI server
            ui_server_ = std::make_shared<WebUIServer>(config_, logger_);
            if (!ui_server_->initialize()) {
                logger_->log(LogLevel::ERROR, "Failed to initialize web UI server");
                return false;
            }

            // Register UI routes
            register_ui_routes();

            logger_->log(LogLevel::INFO, "Regulatory Assessor UI Demo initialized successfully");
            return true;

        } catch (const std::exception& e) {
            logger_->log(LogLevel::ERROR, "Failed to initialize Regulatory Assessor UI Demo: " + std::string(e.what()));
            return false;
        }
    }

    /**
     * @brief Start the demo with agent processing and web UI
     */
    void start_demo() {
        if (running_) {
            logger_->log(LogLevel::WARN, "Regulatory Assessor UI Demo is already running");
            return;
        }

        running_ = true;
        logger_->log(LogLevel::INFO, "Starting Regulatory Assessor UI Demo");

        // Start the regulatory assessor agent
        assessor_agent_->start();

        // Start regulatory monitor
        if (!regulatory_monitor_->start_monitoring()) {
            logger_->log(LogLevel::WARN, "Failed to start regulatory monitoring - continuing without real-time updates");
        }

        // Start web UI server
        ui_server_->start(ui_port_);

        logger_->log(LogLevel::INFO, "Regulatory Assessor UI Demo started successfully");
        // Get web server host from configuration (default to 0.0.0.0 for cloud deployment)
        auto& config_manager = ConfigurationManager::get_instance();
        // For display purposes, use WEB_SERVER_DISPLAY_HOST if set, otherwise use WEB_SERVER_HOST
        const char* display_host_env = std::getenv("WEB_SERVER_DISPLAY_HOST");
        std::string display_host;
        if (display_host_env) {
            display_host = display_host_env;
        } else {
            std::string web_host = config_manager.get_string("WEB_SERVER_HOST").value_or("0.0.0.0");
            display_host = (web_host == "0.0.0.0") ? "localhost" : web_host;
        }
        logger_->log(LogLevel::INFO, "Web UI available at: http://" + display_host + ":" + std::to_string(ui_port_));
    }

    /**
     * @brief Stop the demo gracefully
     */
    void stop_demo() {
        if (!running_) {
            return;
        }

        logger_->log(LogLevel::INFO, "Stopping Regulatory Assessor UI Demo");
        running_ = false;

        // Stop agent
        assessor_agent_->stop();

        // Stop regulatory monitor
        regulatory_monitor_->stop_monitoring();

        // Stop UI server
        ui_server_->stop();

        logger_->log(LogLevel::INFO, "Regulatory Assessor UI Demo stopped");
    }

private:
    /**
     * @brief Structure to hold parsed HTTP request data
     */
    struct ParsedHttpRequest {
        std::string method;
        std::string path;
        std::string version;
        std::unordered_map<std::string, std::string> headers;
        std::string body;
    };

    /**
     * @brief Parse HTTP request string into structured data
     * @param request Raw HTTP request string
     * @return Optional parsed request, empty if parsing fails
     */
    std::optional<ParsedHttpRequest> parse_http_request(const std::string& request) {
        try {
            ParsedHttpRequest parsed;
            std::istringstream stream(request);
            std::string line;

            // Parse request line
            if (!std::getline(stream, line) || line.empty()) {
                return std::nullopt;
            }

            // Remove carriage return if present
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            std::istringstream line_stream(line);
            if (!(line_stream >> parsed.method >> parsed.path >> parsed.version)) {
                return std::nullopt;
            }

            // Parse headers
            while (std::getline(stream, line)) {
                // Remove carriage return if present
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }

                // Empty line indicates end of headers
                if (line.empty()) {
                    break;
                }

                // Parse header line
                size_t colon_pos = line.find(':');
                if (colon_pos != std::string::npos) {
                    std::string header_name = line.substr(0, colon_pos);
                    std::string header_value = line.substr(colon_pos + 1);

                    // Trim whitespace
                    header_name.erase(header_name.begin(),
                        std::find_if(header_name.begin(), header_name.end(),
                        [](unsigned char ch) { return !std::isspace(ch); }));
                    header_name.erase(std::find_if(header_name.rbegin(), header_name.rend(),
                        [](unsigned char ch) { return !std::isspace(ch); }).base(), header_name.end());

                    header_value.erase(header_value.begin(),
                        std::find_if(header_value.begin(), header_value.end(),
                        [](unsigned char ch) { return !std::isspace(ch); }));
                    header_value.erase(std::find_if(header_value.rbegin(), header_value.rend(),
                        [](unsigned char ch) { return !std::isspace(ch); }).base(), header_value.end());

                    // Convert header name to lowercase for case-insensitive comparison
                    std::transform(header_name.begin(), header_name.end(), header_name.begin(), ::tolower);
                    parsed.headers[header_name] = header_value;
                }
            }

            // Parse body
            std::string body;
            std::string remaining;
            while (std::getline(stream, line)) {
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                body += line + "\n";
            }

            // Remove trailing newline
            if (!body.empty() && body.back() == '\n') {
                body.pop_back();
            }

            parsed.body = body;

            // Validate Content-Length if present
            auto content_length_it = parsed.headers.find("content-length");
            if (content_length_it != parsed.headers.end()) {
                try {
                    size_t expected_length = std::stoul(content_length_it->second);
                    if (parsed.body.length() != expected_length) {
                        logger_->log(LogLevel::WARN, "Content-Length mismatch: expected " +
                            std::to_string(expected_length) + ", got " + std::to_string(parsed.body.length()));
                    }
                } catch (const std::exception&) {
                    logger_->log(LogLevel::WARN, "Invalid Content-Length header value");
                }
            }

            return parsed;

        } catch (const std::exception& e) {
            logger_->log(LogLevel::ERROR, "Failed to parse HTTP request: " + std::string(e.what()));
            return std::nullopt;
        }
    }

    /**
     * @brief Add real regulatory sources to the monitor
     */
    bool add_regulatory_sources() {
        try {
            // Add standard regulatory sources using the monitor's standard source method

            // Add SEC EDGAR source
            if (!regulatory_monitor_->add_standard_source(RegulatorySourceType::SEC_EDGAR)) {
                logger_->log(LogLevel::WARN, "Failed to add SEC EDGAR source");
            }

            // Add FCA Regulatory source
            if (!regulatory_monitor_->add_standard_source(RegulatorySourceType::FCA_REGULATORY)) {
                logger_->log(LogLevel::WARN, "Failed to add FCA Regulatory source");
            }

            // Add ECB Announcements source
            if (!regulatory_monitor_->add_standard_source(RegulatorySourceType::ECB_ANNOUNCEMENTS)) {
                logger_->log(LogLevel::WARN, "Failed to add ECB Announcements source");
            }

            logger_->log(LogLevel::INFO, "Added regulatory sources to monitor");
            return true;

        } catch (const std::exception& e) {
            logger_->log(LogLevel::ERROR, "Failed to add regulatory sources: " + std::string(e.what()));
            return false;
        }
    }

    /**
      * @brief Register web UI routes for regulatory assessment
      */
    void register_ui_routes() {
        // Main dashboard route
        ui_server_->register_route("/regulatory-assessor", [this](const std::string& request) {
            return generate_main_dashboard();
        });

        // Assessment route
        ui_server_->register_route("/regulatory-assessor/assess", [this](const std::string& request) {
            return handle_assessment_request(request);
        });

        // Trends route
        ui_server_->register_route("/regulatory-assessor/trends", [this](const std::string& request) {
            return generate_trends_dashboard();
        });

        // API route for real-time data
        ui_server_->register_route("/api/regulatory-data", [this](const std::string& request) {
            return get_regulatory_data_json();
        });
    }

    /**
     * @brief Generate main regulatory assessor dashboard HTML
     */
    std::string generate_main_dashboard() {
        std::stringstream html;
        html << R"html(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Regulatory Assessor Agent - Live Demo</title>
    <style>
        body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; margin: 0; padding: 20px; background: #f5f5f5; }
        .container { max-width: 1200px; margin: 0 auto; background: white; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); overflow: hidden; }
        .header { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; padding: 30px; text-align: center; }
        .header h1 { margin: 0; font-size: 2.5em; }
        .header p { margin: 10px 0 0 0; opacity: 0.9; font-size: 1.1em; }
        .nav { background: #f8f9fa; padding: 20px; border-bottom: 1px solid #e9ecef; }
        .nav button { background: #007bff; color: white; border: none; padding: 12px 24px; margin: 0 10px; border-radius: 5px; cursor: pointer; font-size: 16px; }
        .nav button:hover { background: #0056b3; }
        .content { padding: 30px; }
        .section { margin-bottom: 40px; }
        .section h2 { color: #333; border-bottom: 2px solid #667eea; padding-bottom: 10px; }
        .assessment-form { background: #f8f9fa; padding: 20px; border-radius: 8px; margin: 20px 0; }
        .form-group { margin-bottom: 15px; }
        .form-group label { display: block; margin-bottom: 5px; font-weight: bold; }
        .form-group input, .form-group textarea { width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 4px; font-size: 14px; }
        .form-group textarea { height: 100px; resize: vertical; }
        .btn { background: #28a745; color: white; border: none; padding: 12px 24px; border-radius: 5px; cursor: pointer; font-size: 16px; }
        .btn:hover { background: #218838; }
        .results { background: #f8f9fa; padding: 20px; border-radius: 8px; margin: 20px 0; border-left: 4px solid #28a745; }
        .metric { display: inline-block; background: #e9ecef; padding: 10px 15px; margin: 5px; border-radius: 20px; font-size: 14px; }
        .status { padding: 10px; border-radius: 5px; margin: 10px 0; font-weight: bold; }
        .status.active { background: #d4edda; color: #155724; border: 1px solid #c3e6cb; }
        .footer { background: #343a40; color: white; text-align: center; padding: 20px; }
        .refresh-btn { background: #17a2b8; margin-left: 10px; }
        .refresh-btn:hover { background: #138496; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🛡️ Regulatory Assessor Agent</h1>
            <p>AI-Powered Regulatory Impact Assessment & Compliance Adaptation</p>
        </div>

        <div class="nav">
            <button onclick="showSection('dashboard')">Dashboard</button>
            <button onclick="showSection('assessment')">Assessment</button>
            <button onclick="showSection('trends')">Trends Analysis</button>
            <button onclick="showSection('api')">API Data</button>
            <button class="refresh-btn" onclick="refreshData()">🔄 Refresh</button>
        </div>

        <div class="content">
            <div id="dashboard" class="section">
                <h2>📊 System Dashboard</h2>
                <div id="dashboard-content">
                    <div class="status active">✅ System Status: Active - Regulatory monitoring in progress</div>
                    <p><strong>Real-time Regulatory Assessment System</strong></p>
                    <ul>
                        <li>🤖 AI-powered impact analysis using advanced NLP</li>
                        <li>📈 Continuous monitoring of regulatory changes</li>
                        <li>🎯 Intelligent compliance adaptation recommendations</li>
                        <li>🔍 Multi-source regulatory intelligence gathering</li>
                        <li>⚡ Production-grade concurrent processing</li>
                    </ul>
                    <div class="metric">Assessments Processed: <span id="assessment-count">0</span></div>
                    <div class="metric">High Impact Changes: <span id="high-impact-count">0</span></div>
                    <div class="metric">Active Monitoring: <span id="monitoring-status">Running</span></div>
                </div>
            </div>

            <div id="assessment" class="section" style="display: none;">
                <h2>🔍 Regulatory Impact Assessment</h2>
                <div class="assessment-form">
                    <div class="form-group">
                        <label for="regulatory-title">Regulatory Change Title:</label>
                        <input type="text" id="regulatory-title" placeholder="Enter regulatory change title...">
                    </div>
                    <div class="form-group">
                        <label for="regulatory-description">Description:</label>
                        <textarea id="regulatory-description" placeholder="Describe the regulatory change in detail..."></textarea>
                    </div>
                    <div class="form-group">
                        <label for="regulatory-source">Source:</label>
                        <input type="text" id="regulatory-source" placeholder="e.g., SEC, FCA, ECB, etc.">
                    </div>
                    <button class="btn" onclick="performAssessment()">🚀 Perform Assessment</button>
                </div>
                <div id="assessment-results" class="results" style="display: none;">
                    <h3>Assessment Results</h3>
                    <div id="results-content"></div>
                </div>
            </div>

            <div id="trends" class="section" style="display: none;">
                <h2>📈 Regulatory Trends Analysis</h2>
                <div id="trends-content">
                    <p>Analyzing regulatory trends and patterns...</p>
                </div>
            </div>

            <div id="api" class="section" style="display: none;">
                <h2>🔌 API Data</h2>
                <pre id="api-data">Loading...</pre>
            </div>
        </div>

        <div class="footer">
            <p>&copy; 2024 Regulens - Regulatory Assessor Agent Demo | Rule 6 Compliant UI Testing</p>
        </div>
    </div>

    <script>
        let currentSection = 'dashboard';

        function showSection(sectionName) {
            document.getElementById(currentSection).style.display = 'none';
            document.getElementById(sectionName).style.display = 'block';
            currentSection = sectionName;
        }

        async function refreshData() {
            try {
                const response = await fetch('/api/regulatory-data');
                const data = await response.json();

                document.getElementById('assessment-count').textContent = data.assessment_count || 0;
                document.getElementById('high-impact-count').textContent = data.high_impact_count || 0;
                document.getElementById('monitoring-status').textContent = data.monitoring_active ? 'Running' : 'Stopped';

                if (currentSection === 'api') {
                    document.getElementById('api-data').textContent = JSON.stringify(data, null, 2);
                }

                if (currentSection === 'trends') {
                    loadTrendsData();
                }

            } catch (error) {
                console.error('Failed to refresh data:', error);
            }
        }

        async function performAssessment() {
            const title = document.getElementById('regulatory-title').value;
            const description = document.getElementById('regulatory-description').value;
            const source = document.getElementById('regulatory-source').value;

            if (!title || !description) {
                alert('Please fill in the title and description');
                return;
            }

            try {
                const response = await fetch('/regulatory-assessor/assess', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ title, description, source })
                });

                const result = await response.json();
                document.getElementById('results-content').innerHTML = formatAssessmentResult(result);
                document.getElementById('assessment-results').style.display = 'block';

            } catch (error) {
                console.error('Assessment failed:', error);
                alert('Assessment failed. Please try again.');
            }
        }

        function formatAssessmentResult(result) {
            return `
                <div class="metric">Impact Level: ${result.impact_level || 'Unknown'}</div>
                <div class="metric">Complexity Score: ${(result.implementation_complexity || 0).toFixed(2)}</div>
                <div class="metric">Timeline: ${result.estimated_timeline_days || 0} days</div>
                <div class="metric">Confidence: ${(result.ai_analysis?.confidence_score || 0).toFixed(2)}</div>
                <h4>Affected Processes:</h4>
                <ul>${(result.affected_processes || []).map(p => `<li>${p}</li>`).join('')}</ul>
                <h4>Recommendations:</h4>
                <ul>${(result.adaptation_recommendations || []).map(r => `<li><strong>${r.priority}</strong>: ${r.description} (${r.timeline})</li>`).join('')}</ul>
            `;
        }

        async function loadTrendsData() {
            try {
                const response = await fetch('/regulatory-assessor/trends');
                const trends = await response.json();
                document.getElementById('trends-content').innerHTML = `
                    <div class="metric">Changes Analyzed: ${trends.changes_analyzed || 0}</div>
                    <div class="metric">Trend Confidence: ${(trends.trend_confidence || 0).toFixed(2)}</div>
                    <div class="metric">Predicted Focus Area: ${trends.predicted_focus_area || 'None'}</div>
                `;
            } catch (error) {
                document.getElementById('trends-content').innerHTML = '<p>Failed to load trends data</p>';
            }
        }

        // Auto-refresh every 30 seconds
        setInterval(refreshData, 30000);

        // Initial data load
        refreshData();
    </script>
</body>
</html>
        )html";

        return html.str();
    }

    /**
     * @brief Handle assessment requests from the web UI
     */
    std::string handle_assessment_request(const std::string& request) {
        try {
            // Parse HTTP request properly
            auto parsed_request = parse_http_request(request);
            if (!parsed_request) {
                return R"({"error": "Invalid HTTP request format"})";
            }

            // Validate request method
            if (parsed_request->method != "POST") {
                return R"({"error": "Only POST requests are supported for assessment"})";
            }

            // Validate Content-Type header
            auto content_type_it = parsed_request->headers.find("content-type");
            if (content_type_it == parsed_request->headers.end() ||
                content_type_it->second.find("application/json") == std::string::npos) {
                return R"({"error": "Content-Type must be application/json"})";
            }

            // Parse JSON request body
            if (parsed_request->body.empty()) {
                return R"({"error": "Request body is required"})";
            }

            nlohmann::json regulatory_data;
            try {
                regulatory_data = nlohmann::json::parse(parsed_request->body);
            } catch (const nlohmann::json::parse_error& e) {
                logger_->log(LogLevel::ERROR, "Failed to parse JSON request body: " + std::string(e.what()));
                return R"({"error": "Invalid JSON in request body"})";
            }

            // Validate required fields
            if (!regulatory_data.contains("title") || !regulatory_data.contains("description")) {
                return R"({"error": "Request must contain 'title' and 'description' fields"})";
            }

            if (!regulatory_data["title"].is_string() || !regulatory_data["description"].is_string()) {
                return R"({"error": "'title' and 'description' must be strings"})";
            }

            // Perform assessment
            auto impact_assessment = assessor_agent_->assess_regulatory_impact(regulatory_data);
            auto recommendations = assessor_agent_->generate_adaptation_recommendations(impact_assessment);

            // Combine results
            nlohmann::json result = impact_assessment;
            result["adaptation_recommendations"] = recommendations;

            return result.dump(2);

        } catch (const std::exception& e) {
            logger_->log(LogLevel::ERROR, "Failed to handle assessment request: " + std::string(e.what()));
            return R"({"error": "Assessment failed"})";
        }
    }

    /**
     * @brief Generate trends dashboard HTML
     */
    std::string generate_trends_dashboard() {
        try {
            // Get recent regulatory changes
            auto recent_changes = assessor_agent_->fetch_recent_regulatory_changes();

            // Perform trend analysis
            auto trends = assessor_agent_->predict_regulatory_trends(recent_changes);

            nlohmann::json result = {
                {"changes_analyzed", recent_changes.size()},
                {"trend_confidence", trends.value("trend_confidence", 0.0)},
                {"predicted_focus_area", trends.value("predicted_focus_area", "None")}
            };

            return result.dump(2);

        } catch (const std::exception& e) {
            logger_->log(LogLevel::ERROR, "Failed to generate trends dashboard: " + std::string(e.what()));
            return R"({"error": "Trends analysis failed"})";
        }
    }

    /**
     * @brief Get regulatory data as JSON for API
     */
    std::string get_regulatory_data_json() {
        try {
            // Get real monitoring statistics
            auto monitor_stats = regulatory_monitor_->get_monitoring_stats();

            nlohmann::json data = {
                {"assessment_count", assessor_agent_ ? assessor_agent_->get_total_assessments_processed() : 0},
                {"high_impact_count", 0}, // Would be tracked separately in production
                {"monitoring_active", running_.load()},
                {"monitor_stats", monitor_stats},
                {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            };

            return data.dump(2);

        } catch (const std::exception& e) {
            return R"({"error": "Failed to get regulatory data"})";
        }
    }


    // Component pointers
    std::shared_ptr<ConfigurationManager> config_;
    std::shared_ptr<StructuredLogger> logger_;
    std::shared_ptr<PostgreSQLConnectionPool> db_pool_;
    std::shared_ptr<AnthropicClient> llm_client_;
    std::shared_ptr<KnowledgeBase> knowledge_base_;
    std::shared_ptr<RegulatoryAssessorAgent> assessor_agent_;
    std::shared_ptr<WebUIServer> ui_server_;
    std::shared_ptr<RegulatoryMonitor> regulatory_monitor_;
    std::shared_ptr<RegulatoryKnowledgeBase> regulatory_kb_;

    // Demo state
    std::atomic<bool> running_;
    int ui_port_;
};

} // namespace regulens

/**
 * @brief Main function for Regulatory Assessor UI Demo
 */
int main(int argc, char* argv[]) {
    std::cout << "🛡️ Regulatory Assessor Agent - Live UI Demo" << std::endl;
    std::cout << "==========================================" << std::endl;
    std::cout << "Rule 6 Compliant: Production-grade UI testing component" << std::endl;
    std::cout << std::endl;

    // Handle graceful shutdown
    std::signal(SIGINT, [](int) {
        std::cout << "\n🛑 Shutdown signal received. Stopping demo..." << std::endl;
        exit(0);
    });

    try {
        auto demo = std::make_unique<regulens::RegulatoryAssessorUIDemo>();

        if (!demo->initialize()) {
            std::cerr << "❌ Failed to initialize Regulatory Assessor UI Demo" << std::endl;
            return 1;
        }

        demo->start_demo();

        // Get display host from configuration
        const char* display_host = std::getenv("WEB_SERVER_DISPLAY_HOST");
        std::string host = display_host ? display_host : "localhost";
        std::string port = std::getenv("WEB_SERVER_UI_PORT") ? std::getenv("WEB_SERVER_UI_PORT") : "8082";

        std::cout << "✅ Regulatory Assessor UI Demo started successfully!" << std::endl;
        std::cout << "🌐 Web UI: http://" << host << ":" << port << std::endl;
        std::cout << "📊 Features: Real-time assessment, AI analysis, trends monitoring" << std::endl;
        std::cout << "🔄 Press Ctrl+C to stop the demo" << std::endl;
        std::cout << std::endl;

        // Keep running until interrupted
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

    } catch (const std::exception& e) {
        std::cerr << "❌ Demo failed with exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
