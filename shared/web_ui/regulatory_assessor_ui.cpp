/**
 * Regulatory Assessor UI Implementation
 *
 * Production-grade web interface for testing regulatory assessor agent
 * as required by Rule 6. Provides comprehensive testing capabilities
 * for regulatory change assessment, impact analysis, and compliance monitoring.
 */

#include "regulatory_assessor_ui.hpp"
#include <sstream>
#include <iomanip>
#include <chrono>
#include "../utils/timer.hpp"

namespace regulens {

RegulatoryAssessorUI::RegulatoryAssessorUI(int port)
    : port_(port), config_manager_(nullptr), logger_(nullptr),
      metrics_collector_(nullptr), running_(false) {
}

RegulatoryAssessorUI::~RegulatoryAssessorUI() {
    stop();
}

bool RegulatoryAssessorUI::initialize(ConfigurationManager* config,
                                   StructuredLogger* logger,
                                   MetricsCollector* metrics,
                                   std::shared_ptr<RegulatoryAssessorAgent> regulatory_agent) {
    config_manager_ = config;
    logger_ = logger;
    metrics_collector_ = metrics;
    regulatory_agent_ = regulatory_agent;

    if (logger_) {
        logger_->log(LogLevel::INFO, "Initializing Regulatory Assessor UI on port " + std::to_string(port_));
    }

    try {
        // Initialize web server
        server_ = std::make_unique<WebUIServer>(port_);
        handlers_ = std::make_unique<WebUIHandlers>();

        // Setup regulatory assessor specific handlers
        if (!setup_regulatory_handlers()) {
            if (logger_) {
                logger_->log(LogLevel::ERROR, "Failed to setup regulatory assessor handlers");
            }
            return false;
        }

        if (logger_) {
            logger_->log(LogLevel::INFO, "Regulatory Assessor UI initialized successfully");
        }
        return true;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->log(LogLevel::ERROR, "Failed to initialize Regulatory Assessor UI: " + std::string(e.what()));
        }
        return false;
    }
}

bool RegulatoryAssessorUI::start() {
    if (running_) {
        if (logger_) {
            logger_->log(LogLevel::WARN, "Regulatory Assessor UI is already running");
        }
        return true;
    }

    try {
        if (!server_ || !server_->start()) {
            if (logger_) {
                logger_->log(LogLevel::ERROR, "Failed to start web server");
            }
            return false;
        }

        running_ = true;

        if (logger_) {
            logger_->log(LogLevel::INFO, "Regulatory Assessor UI started successfully on port " + std::to_string(port_));
        }
        return true;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->log(LogLevel::ERROR, "Failed to start Regulatory Assessor UI: " + std::string(e.what()));
        }
        return false;
    }
}

void RegulatoryAssessorUI::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    if (server_) {
        server_->stop();
    }

    if (logger_) {
        logger_->log(LogLevel::INFO, "Regulatory Assessor UI stopped");
    }
}

bool RegulatoryAssessorUI::is_running() const {
    return running_;
}

bool RegulatoryAssessorUI::setup_regulatory_handlers() {
    try {
        // Dashboard handler
        handlers_->add_handler("/regulatory/dashboard", "GET",
            [this](const std::string& method, const std::string& path, const std::string& body) {
                return generate_dashboard_html();
            });

        // Assessment handler
        handlers_->add_handler("/regulatory/assess", "POST",
            std::bind(&RegulatoryAssessorUI::handle_assess_regulation, std::placeholders::_1,
                     std::placeholders::_2, std::placeholders::_3, this));

        // Impact analysis handler
        handlers_->add_handler("/regulatory/impact", "POST",
            std::bind(&RegulatoryAssessorUI::handle_impact_analysis, std::placeholders::_1,
                     std::placeholders::_2, std::placeholders::_3, this));

        // Monitoring handler
        handlers_->add_handler("/regulatory/monitor", "GET",
            std::bind(&RegulatoryAssessorUI::handle_monitor_changes, std::placeholders::_1,
                     std::placeholders::_2, std::placeholders::_3, this));

        // Report handler
        handlers_->add_handler("/regulatory/report", "GET",
            std::bind(&RegulatoryAssessorUI::handle_assessment_report, std::placeholders::_1,
                     std::placeholders::_2, std::placeholders::_3, this));

        // Form handlers
        handlers_->add_handler("/regulatory/forms/assessment", "GET",
            [this](const std::string& method, const std::string& path, const std::string& body) {
                return generate_assessment_form_html();
            });

        handlers_->add_handler("/regulatory/forms/impact", "GET",
            [this](const std::string& method, const std::string& path, const std::string& body) {
                return generate_impact_form_html();
            });

        handlers_->add_handler("/regulatory/forms/monitor", "GET",
            [this](const std::string& method, const std::string& path, const std::string& body) {
                return generate_monitoring_html();
            });

        // Register handlers with server
        server_->set_request_handler([this](const std::string& method, const std::string& path, const std::string& body) {
            return handlers_->handle_request(method, path, body);
        });

        return true;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->log(LogLevel::ERROR, "Failed to setup regulatory handlers: " + std::string(e.what()));
        }
        return false;
    }
}

std::string RegulatoryAssessorUI::handle_assess_regulation(const std::string& method,
                                                         const std::string& path,
                                                         const std::string& body,
                                                         RegulatoryAssessorUI* ui) {
    if (!ui->regulatory_agent_) {
        return WebUIHandlers::generate_error_response("Regulatory agent not available", 500);
    }

    try {
        // Parse JSON request
        nlohmann::json request = nlohmann::json::parse(body);

        if (!request.contains("regulation_text")) {
            return WebUIHandlers::generate_error_response("Missing regulation_text field", 400);
        }

        std::string regulation_text = request["regulation_text"];

        // Perform assessment
        Timer timer;
        auto assessment_result = ui->regulatory_agent_->assess_regulation(regulation_text);
        double processing_time = timer.elapsed_ms();

        // Record metrics
        if (ui->metrics_collector_) {
            ui->metrics_collector_->record_metric("regulatory.assessment.duration", processing_time);
            ui->metrics_collector_->record_metric("regulatory.assessment.count", 1.0);
        }

        // Generate response
        nlohmann::json response = {
            {"success", true},
            {"assessment", assessment_result},
            {"processing_time_ms", processing_time},
            {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
        };

        return WebUIHandlers::generate_json_response(response);

    } catch (const std::exception& e) {
        if (ui->logger_) {
            ui->logger_->log(LogLevel::ERROR, "Regulatory assessment failed: " + std::string(e.what()));
        }
        return WebUIHandlers::generate_error_response("Assessment failed: " + std::string(e.what()), 500);
    }
}

std::string RegulatoryAssessorUI::handle_impact_analysis(const std::string& method,
                                                       const std::string& path,
                                                       const std::string& body,
                                                       RegulatoryAssessorUI* ui) {
    if (!ui->regulatory_agent_) {
        return WebUIHandlers::generate_error_response("Regulatory agent not available", 500);
    }

    try {
        // Parse JSON request
        nlohmann::json request = nlohmann::json::parse(body);

        if (!request.contains("change_description")) {
            return WebUIHandlers::generate_error_response("Missing change_description field", 400);
        }

        std::string change_description = request["change_description"];

        // Perform impact analysis
        Timer timer;
        auto impact_result = ui->regulatory_agent_->analyze_impact(change_description);
        double processing_time = timer.elapsed_ms();

        // Record metrics
        if (ui->metrics_collector_) {
            ui->metrics_collector_->record_metric("regulatory.impact.duration", processing_time);
            ui->metrics_collector_->record_metric("regulatory.impact.count", 1.0);
        }

        // Generate response
        nlohmann::json response = {
            {"success", true},
            {"impact_analysis", impact_result},
            {"processing_time_ms", processing_time},
            {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
        };

        return WebUIHandlers::generate_json_response(response);

    } catch (const std::exception& e) {
        if (ui->logger_) {
            ui->logger_->log(LogLevel::ERROR, "Impact analysis failed: " + std::string(e.what()));
        }
        return WebUIHandlers::generate_error_response("Impact analysis failed: " + std::string(e.what()), 500);
    }
}

std::string RegulatoryAssessorUI::handle_monitor_changes(const std::string& method,
                                                       const std::string& path,
                                                       const std::string& body,
                                                       RegulatoryAssessorUI* ui) {
    if (!ui->regulatory_agent_) {
        return WebUIHandlers::generate_error_response("Regulatory agent not available", 500);
    }

    try {
        // Get monitoring status
        auto monitoring_status = ui->regulatory_agent_->get_monitoring_status();

        // Generate response
        nlohmann::json response = {
            {"success", true},
            {"monitoring_status", monitoring_status},
            {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
        };

        return WebUIHandlers::generate_json_response(response);

    } catch (const std::exception& e) {
        if (ui->logger_) {
            ui->logger_->log(LogLevel::ERROR, "Monitoring status retrieval failed: " + std::string(e.what()));
        }
        return WebUIHandlers::generate_error_response("Monitoring status retrieval failed: " + std::string(e.what()), 500);
    }
}

std::string RegulatoryAssessorUI::handle_assessment_report(const std::string& method,
                                                         const std::string& path,
                                                         const std::string& body,
                                                         RegulatoryAssessorUI* ui) {
    if (!ui->regulatory_agent_) {
        return WebUIHandlers::generate_error_response("Regulatory agent not available", 500);
    }

    try {
        // Generate assessment report
        auto report = ui->regulatory_agent_->generate_assessment_report();

        // Generate response
        nlohmann::json response = {
            {"success", true},
            {"report", report},
            {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
        };

        return WebUIHandlers::generate_json_response(response);

    } catch (const std::exception& e) {
        if (ui->logger_) {
            ui->logger_->log(LogLevel::ERROR, "Report generation failed: " + std::string(e.what()));
        }
        return WebUIHandlers::generate_error_response("Report generation failed: " + std::string(e.what()), 500);
    }
}

std::string RegulatoryAssessorUI::generate_dashboard_html() const {
    std::stringstream html;
    html << R"(
<!DOCTYPE html>
<html>
<head>
    <title>Regulatory Assessor Agent - Dashboard</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }
        .container { max-width: 1200px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        .header { text-align: center; margin-bottom: 30px; }
        .nav { margin-bottom: 20px; }
        .nav a { margin: 0 10px; padding: 10px 20px; background: #007bff; color: white; text-decoration: none; border-radius: 4px; }
        .nav a:hover { background: #0056b3; }
        .status { padding: 10px; margin: 10px 0; border-radius: 4px; }
        .status.running { background: #d4edda; color: #155724; }
        .status.stopped { background: #f8d7da; color: #721c24; }
        .metrics { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 20px; margin: 20px 0; }
        .metric { background: #f8f9fa; padding: 15px; border-radius: 4px; text-align: center; }
        .metric h3 { margin: 0 0 10px 0; color: #333; }
        .metric .value { font-size: 24px; font-weight: bold; color: #007bff; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🔍 Regulatory Assessor Agent Dashboard</h1>
            <p>Real-time regulatory change assessment and impact analysis</p>
        </div>

        <div class="nav">
            <a href="/regulatory/forms/assessment">Assess Regulation</a>
            <a href="/regulatory/forms/impact">Impact Analysis</a>
            <a href="/regulatory/forms/monitor">Monitoring</a>
            <a href="/regulatory/report">Reports</a>
        </div>

        <div class="status running">
            <strong>Status:</strong> Agent Active - Monitoring regulatory changes
        </div>

        <div class="metrics">
            <div class="metric">
                <h3>Assessments Today</h3>
                <div class="value">0</div>
            </div>
            <div class="metric">
                <h3>High Impact Changes</h3>
                <div class="value">0</div>
            </div>
            <div class="metric">
                <h3>Processing Time</h3>
                <div class="value">0ms</div>
            </div>
            <div class="metric">
                <h3>Success Rate</h3>
                <div class="value">100%</div>
            </div>
        </div>
    </div>
</body>
</html>
    )";

    return html.str();
}

std::string RegulatoryAssessorUI::generate_assessment_form_html() const {
    std::stringstream html;
    html << R"(
<!DOCTYPE html>
<html>
<head>
    <title>Regulatory Assessment - Regulatory Assessor Agent</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }
        .container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        .form-group { margin-bottom: 20px; }
        label { display: block; margin-bottom: 5px; font-weight: bold; }
        textarea { width: 100%; min-height: 200px; padding: 10px; border: 1px solid #ddd; border-radius: 4px; font-family: monospace; }
        button { background: #28a745; color: white; padding: 12px 24px; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; }
        button:hover { background: #218838; }
        .back-link { margin-top: 20px; }
        .back-link a { color: #007bff; text-decoration: none; }
        .result { margin-top: 20px; padding: 15px; background: #f8f9fa; border-radius: 4px; display: none; }
    </style>
</head>
<body>
    <div class="container">
        <h1>📋 Regulatory Assessment</h1>
        <p>Analyze regulatory text for compliance impact and requirements</p>

        <form id="assessmentForm">
            <div class="form-group">
                <label for="regulation_text">Regulatory Text:</label>
                <textarea id="regulation_text" name="regulation_text" placeholder="Paste regulatory text here..." required></textarea>
            </div>

            <button type="submit">Assess Regulation</button>
        </form>

        <div id="result" class="result">
            <h3>Assessment Results:</h3>
            <pre id="resultContent"></pre>
        </div>

        <div class="back-link">
            <a href="/regulatory/dashboard">← Back to Dashboard</a>
        </div>
    </div>

    <script>
        document.getElementById('assessmentForm').addEventListener('submit', async function(e) {
            e.preventDefault();

            const regulationText = document.getElementById('regulation_text').value;
            const resultDiv = document.getElementById('result');
            const resultContent = document.getElementById('resultContent');

            try {
                const response = await fetch('/regulatory/assess', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ regulation_text: regulationText })
                });

                const data = await response.json();

                if (data.success) {
                    resultContent.textContent = JSON.stringify(data.assessment, null, 2);
                    resultDiv.style.display = 'block';
                } else {
                    resultContent.textContent = 'Error: ' + (data.error || 'Unknown error');
                    resultDiv.style.display = 'block';
                }
            } catch (error) {
                resultContent.textContent = 'Error: ' + error.message;
                resultDiv.style.display = 'block';
            }
        });
    </script>
</body>
</html>
    )";

    return html.str();
}

std::string RegulatoryAssessorUI::generate_impact_form_html() const {
    std::stringstream html;
    html << R"(
<!DOCTYPE html>
<html>
<head>
    <title>Impact Analysis - Regulatory Assessor Agent</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }
        .container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        .form-group { margin-bottom: 20px; }
        label { display: block; margin-bottom: 5px; font-weight: bold; }
        textarea { width: 100%; min-height: 150px; padding: 10px; border: 1px solid #ddd; border-radius: 4px; }
        button { background: #dc3545; color: white; padding: 12px 24px; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; }
        button:hover { background: #c82333; }
        .back-link { margin-top: 20px; }
        .back-link a { color: #007bff; text-decoration: none; }
        .result { margin-top: 20px; padding: 15px; background: #f8f9fa; border-radius: 4px; display: none; }
    </style>
</head>
<body>
    <div class="container">
        <h1>⚡ Impact Analysis</h1>
        <p>Analyze the business impact of regulatory changes</p>

        <form id="impactForm">
            <div class="form-group">
                <label for="change_description">Regulatory Change Description:</label>
                <textarea id="change_description" name="change_description" placeholder="Describe the regulatory change..." required></textarea>
            </div>

            <button type="submit">Analyze Impact</button>
        </form>

        <div id="result" class="result">
            <h3>Impact Analysis Results:</h3>
            <pre id="resultContent"></pre>
        </div>

        <div class="back-link">
            <a href="/regulatory/dashboard">← Back to Dashboard</a>
        </div>
    </div>

    <script>
        document.getElementById('impactForm').addEventListener('submit', async function(e) {
            e.preventDefault();

            const changeDescription = document.getElementById('change_description').value;
            const resultDiv = document.getElementById('result');
            const resultContent = document.getElementById('resultContent');

            try {
                const response = await fetch('/regulatory/impact', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ change_description: changeDescription })
                });

                const data = await response.json();

                if (data.success) {
                    resultContent.textContent = JSON.stringify(data.impact_analysis, null, 2);
                    resultDiv.style.display = 'block';
                } else {
                    resultContent.textContent = 'Error: ' + (data.error || 'Unknown error');
                    resultDiv.style.display = 'block';
                }
            } catch (error) {
                resultContent.textContent = 'Error: ' + error.message;
                resultDiv.style.display = 'block';
            }
        });
    </script>
</body>
</html>
    )";

    return html.str();
}

std::string RegulatoryAssessorUI::generate_monitoring_html() const {
    std::stringstream html;
    html << R"(
<!DOCTYPE html>
<html>
<head>
    <title>Regulatory Monitoring - Regulatory Assessor Agent</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }
        .container { max-width: 1000px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        .status { padding: 15px; margin: 20px 0; border-radius: 4px; }
        .status.active { background: #d4edda; color: #155724; }
        .refresh-btn { background: #17a2b8; color: white; padding: 10px 20px; border: none; border-radius: 4px; cursor: pointer; }
        .refresh-btn:hover { background: #138496; }
        .back-link { margin-top: 20px; }
        .back-link a { color: #007bff; text-decoration: none; }
        .monitoring-data { margin-top: 20px; }
        pre { background: #f8f9fa; padding: 15px; border-radius: 4px; overflow-x: auto; }
    </style>
</head>
<body>
    <div class="container">
        <h1>📊 Regulatory Change Monitoring</h1>
        <p>Real-time monitoring of regulatory sources and change detection</p>

        <div class="status active">
            <strong>Monitoring Status:</strong> Active - Scanning SEC, FCA, ECB sources
        </div>

        <button class="refresh-btn" onclick="refreshStatus()">Refresh Status</button>

        <div class="monitoring-data">
            <h3>Current Monitoring Status:</h3>
            <pre id="statusContent">Loading...</pre>
        </div>

        <div class="back-link">
            <a href="/regulatory/dashboard">← Back to Dashboard</a>
        </div>
    </div>

    <script>
        async function refreshStatus() {
            const statusContent = document.getElementById('statusContent');

            try {
                const response = await fetch('/regulatory/monitor');
                const data = await response.json();

                if (data.success) {
                    statusContent.textContent = JSON.stringify(data.monitoring_status, null, 2);
                } else {
                    statusContent.textContent = 'Error: ' + (data.error || 'Unknown error');
                }
            } catch (error) {
                statusContent.textContent = 'Error: ' + error.message;
            }
        }

        // Load initial status
        refreshStatus();

        // Refresh every 30 seconds
        setInterval(refreshStatus, 30000);
    </script>
</body>
</html>
    )";

    return html.str();
}

} // namespace regulens
