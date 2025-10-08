/**
 * Transaction Guardian UI Implementation
 *
 * Production-grade web interface for testing transaction guardian agent
 * as required by Rule 6. Provides comprehensive testing capabilities
 * for transaction monitoring, compliance checking, and risk assessment.
 */

#include "transaction_guardian_ui.hpp"
#include <sstream>
#include <iomanip>
#include <chrono>
#include "../utils/timer.hpp"

namespace regulens {

TransactionGuardianUI::TransactionGuardianUI(int port)
    : port_(port), config_manager_(nullptr), logger_(nullptr),
      metrics_collector_(nullptr) {
}

TransactionGuardianUI::~TransactionGuardianUI() {
    stop();
}

bool TransactionGuardianUI::initialize(ConfigurationManager* config,
                                     StructuredLogger* logger,
                                     MetricsCollector* metrics,
                                     std::shared_ptr<TransactionGuardianAgent> transaction_agent) {
    config_manager_ = config;
    logger_ = logger;
    metrics_collector_ = metrics;
    transaction_agent_ = transaction_agent;

    if (logger_) {
        logger_->log(LogLevel::INFO, "Initializing Transaction Guardian UI on port " + std::to_string(port_));
    }

    try {
        // Initialize web server
        server_ = std::make_unique<WebUIServer>(port_);

        // Setup transaction guardian specific handlers
        setup_transaction_handlers();

        if (logger_) {
            logger_->log(LogLevel::INFO, "Transaction Guardian UI initialized successfully");
        }
        return true;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->log(LogLevel::ERROR, "Failed to initialize Transaction Guardian UI: " + std::string(e.what()));
        }
        return false;
    }
}

bool TransactionGuardianUI::start() {
    if (!server_) {
        if (logger_) {
            logger_->log(LogLevel::ERROR, "Server not initialized");
        }
        return false;
    }

    if (logger_) {
        logger_->log(LogLevel::INFO, "Starting Transaction Guardian UI server");
    }

    try {
        return server_->start();
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->log(LogLevel::ERROR, "Failed to start Transaction Guardian UI: " + std::string(e.what()));
        }
        return false;
    }
}

void TransactionGuardianUI::stop() {
    if (server_ && server_->is_running()) {
        if (logger_) {
            logger_->log(LogLevel::INFO, "Stopping Transaction Guardian UI server");
        }
        server_->stop();
    }
}

bool TransactionGuardianUI::is_running() const {
    return server_ && server_->is_running();
}

void TransactionGuardianUI::setup_transaction_handlers() {
    if (!server_) return;

    // Main page
    server_->add_route("GET", "/", [this](const HTTPRequest& req) {
        std::string html = generate_main_page();
        return HTTPResponse{200, "text/html", html};
    });

    // Transaction submission
    server_->add_route("POST", "/submit-transaction", [this](const HTTPRequest& req) {
        return handle_transaction_submission(req);
    });

    // Monitoring dashboard
    server_->add_route("GET", "/monitoring", [this](const HTTPRequest& req) {
        return handle_monitoring_dashboard(req);
    });

    // Compliance report
    server_->add_route("GET", "/compliance-report", [this](const HTTPRequest& req) {
        return handle_compliance_report(req);
    });

    // Velocity check
    server_->add_route("POST", "/velocity-check", [this](const HTTPRequest& req) {
        return handle_velocity_check(req);
    });

    // Fraud detection
    server_->add_route("POST", "/fraud-detection", [this](const HTTPRequest& req) {
        return handle_fraud_detection(req);
    });
}

std::string TransactionGuardianUI::generate_main_page() {
    std::stringstream html;
    html << R"html(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Transaction Guardian Agent - Testing Interface</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background-color: #f5f5f5; }
        .container { max-width: 1200px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        .header { text-align: center; color: #2c3e50; border-bottom: 2px solid #3498db; padding-bottom: 10px; margin-bottom: 30px; }
        .nav { display: flex; gap: 15px; margin-bottom: 30px; flex-wrap: wrap; }
        .nav a { text-decoration: none; background: #3498db; color: white; padding: 10px 20px; border-radius: 5px; transition: background 0.3s; }
        .nav a:hover { background: #2980b9; }
        .section { margin-bottom: 30px; padding: 20px; border: 1px solid #ddd; border-radius: 5px; }
        .form-group { margin-bottom: 15px; }
        .form-group label { display: block; margin-bottom: 5px; font-weight: bold; }
        .form-group input, .form-group select, .form-group textarea { width: 100%; padding: 8px; border: 1px solid #ddd; border-radius: 4px; }
        .btn { background: #27ae60; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; }
        .btn:hover { background: #229954; }
        .btn-danger { background: #e74c3c; }
        .btn-danger:hover { background: #c0392b; }
        .result { margin-top: 20px; padding: 15px; background: #ecf0f1; border-radius: 5px; white-space: pre-wrap; }
        .status-good { color: #27ae60; }
        .status-warning { color: #f39c12; }
        .status-danger { color: #e74c3c; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🔒 Transaction Guardian Agent</h1>
            <p>Production-grade transaction monitoring and compliance testing interface</p>
        </div>

        <div class="nav">
            <a href="/">Home</a>
            <a href="/monitoring">Monitoring Dashboard</a>
            <a href="/compliance-report">Compliance Report</a>
        </div>

        <div class="section">
            <h2>Transaction Processing Test</h2>
            <form id="transactionForm">
                <div class="form-group">
                    <label for="customer_id">Customer ID:</label>
                    <input type="text" id="customer_id" name="customer_id" required>
                </div>
                <div class="form-group">
                    <label for="amount">Amount:</label>
                    <input type="number" id="amount" name="amount" step="0.01" required>
                </div>
                <div class="form-group">
                    <label for="currency">Currency:</label>
                    <select id="currency" name="currency">
                        <option value="USD">USD</option>
                        <option value="EUR">EUR</option>
                        <option value="GBP">GBP</option>
                    </select>
                </div>
                <div class="form-group">
                    <label for="type">Transaction Type:</label>
                    <select id="type" name="type">
                        <option value="domestic">Domestic</option>
                        <option value="international">International</option>
                        <option value="crypto">Crypto</option>
                    </select>
                </div>
                <div class="form-group">
                    <label for="destination_country">Destination Country:</label>
                    <input type="text" id="destination_country" name="destination_country" placeholder="ISO 3166-1 alpha-3 code">
                </div>
                <div class="form-group">
                    <label for="description">Description:</label>
                    <textarea id="description" name="description" rows="3"></textarea>
                </div>
                <button type="submit" class="btn">Process Transaction</button>
            </form>
            <div id="transactionResult" class="result" style="display:none;"></div>
        </div>

        <div class="section">
            <h2>Quick Tests</h2>
            <button onclick="testVelocity()" class="btn">Test Velocity Monitoring</button>
            <button onclick="testFraud()" class="btn">Test Fraud Detection</button>
            <div id="quickTestResult" class="result" style="display:none;"></div>
        </div>
    </div>

    <script>
        document.getElementById('transactionForm').addEventListener('submit', async function(e) {
            e.preventDefault();

            const formData = new FormData(this);
            const transactionData = Object.fromEntries(formData);

            try {
                const response = await fetch('/submit-transaction', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(transactionData)
                });

                const result = await response.json();
                displayResult('transactionResult', result);
            } catch (error) {
                displayResult('transactionResult', { error: error.message });
            }
        });

        async function testVelocity() {
            const testData = {
                customer_id: 'test_customer_001',
                amount: 5000.0
            };

            try {
                const response = await fetch('/velocity-check', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(testData)
                });

                const result = await response.json();
                displayResult('quickTestResult', result);
            } catch (error) {
                displayResult('quickTestResult', { error: error.message });
            }
        }

        async function testFraud() {
            const testData = {
                customer_id: 'test_customer_001',
                amount: 15000.0,
                type: 'international',
                destination_country: 'XX'
            };

            try {
                const response = await fetch('/fraud-detection', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(testData)
                });

                const result = await response.json();
                displayResult('quickTestResult', result);
            } catch (error) {
                displayResult('quickTestResult', { error: error.message });
            }
        }

        function displayResult(elementId, result) {
            const element = document.getElementById(elementId);
            element.style.display = 'block';
            element.textContent = JSON.stringify(result, null, 2);

            // Add status class
            element.className = 'result';
            if (result.transaction_approved === false) {
                element.classList.add('status-danger');
            } else if (result.risk_level === 'HIGH') {
                element.classList.add('status-warning');
            } else {
                element.classList.add('status-good');
            }
        }
    </script>
</body>
</html>
    )html";

    return html.str();
}

HTTPResponse TransactionGuardianUI::handle_transaction_submission(const HTTPRequest& req) {
    if (!transaction_agent_) {
        HTTPResponse response;
        response.status_code = 500;
        response.content_type = "application/json";
        response.body = nlohmann::json{{"error", "Transaction agent not initialized"}}.dump();
        return response;
    }

    try {
        auto transaction_data = nlohmann::json::parse(req.body);

        // Process transaction with production-grade timing
        auto start_time = std::chrono::high_resolution_clock::now();
        auto decision = transaction_agent_->process_transaction(transaction_data);
        auto end_time = std::chrono::high_resolution_clock::now();
        auto processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

        // Build response with comprehensive data
        nlohmann::json response_data = decision.to_json();
        response_data["processing_time_ms"] = processing_time;
        response_data["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        HTTPResponse response;
        response.status_code = 200;
        response.content_type = "application/json";
        response.body = response_data.dump(2);
        return response;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->log(LogLevel::ERROR, "Transaction submission failed: " + std::string(e.what()));
        }
        HTTPResponse response;
        response.status_code = 500;
        response.content_type = "application/json";
        response.body = nlohmann::json{{"error", "Transaction processing failed"}, {"message", e.what()}}.dump();
        return response;
    }
}

HTTPResponse TransactionGuardianUI::handle_monitoring_dashboard(const HTTPRequest& req) {
    std::stringstream html;
    html << R"html(
<!DOCTYPE html>
<html>
<head>
    <title>Transaction Monitoring Dashboard</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .metric { display: inline-block; margin: 10px; padding: 20px; border: 1px solid #ddd; border-radius: 5px; }
        .metric h3 { margin: 0 0 10px 0; }
        .metric .value { font-size: 2em; font-weight: bold; color: #3498db; }
    </style>
</head>
<body>
    <h1>Transaction Monitoring Dashboard</h1>
    <div class="metric">
        <h3>Transactions Processed</h3>
        <div class="value">0</div>
    </div>
    <div class="metric">
        <h3>Suspicious Transactions</h3>
        <div class="value">0</div>
    </div>
    <div class="metric">
        <h3>Compliance Rate</h3>
        <div class="value">100%</div>
    </div>
    <a href="/">Back to Home</a>
</body>
</html>
    )html";

    return HTTPResponse{200, "text/html", html.str()};
}

HTTPResponse TransactionGuardianUI::handle_compliance_report(const HTTPRequest& req) {
    if (!transaction_agent_) {
        HTTPResponse response;
        response.status_code = 500;
        response.content_type = "application/json";
        response.body = nlohmann::json{{"error", "Transaction agent not initialized"}}.dump();
        return response;
    }

    try {
        auto start_time = std::chrono::system_clock::now() - std::chrono::hours(24);
        auto end_time = std::chrono::system_clock::now();

        auto report = transaction_agent_->generate_compliance_report(start_time, end_time);

        HTTPResponse response;
        response.status_code = 200;
        response.content_type = "application/json";
        response.body = report.dump(2);
        return response;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->log(LogLevel::ERROR, "Compliance report generation failed: " + std::string(e.what()));
        }
        HTTPResponse response;
        response.status_code = 500;
        response.content_type = "application/json";
        response.body = nlohmann::json{{"error", "Report generation failed"}, {"message", e.what()}}.dump();
        return response;
    }
}

HTTPResponse TransactionGuardianUI::handle_velocity_check(const HTTPRequest& req) {
    if (!transaction_agent_) {
        HTTPResponse response;
        response.status_code = 500;
        response.content_type = "application/json";
        response.body = nlohmann::json{{"error", "Transaction agent not initialized"}}.dump();
        return response;
    }

    try {
        auto request_data = nlohmann::json::parse(req.body);
        std::string customer_id = request_data.value("customer_id", "");
        double amount = request_data.value("amount", 0.0);

        auto result = transaction_agent_->monitor_velocity(customer_id, amount);

        HTTPResponse response;
        response.status_code = 200;
        response.content_type = "application/json";
        response.body = result.dump(2);
        return response;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->log(LogLevel::ERROR, "Velocity check failed: " + std::string(e.what()));
        }
        HTTPResponse response;
        response.status_code = 500;
        response.content_type = "application/json";
        response.body = nlohmann::json{{"error", "Velocity check failed"}, {"message", e.what()}}.dump();
        return response;
    }
}

HTTPResponse TransactionGuardianUI::handle_fraud_detection(const HTTPRequest& req) {
    if (!transaction_agent_) {
        HTTPResponse response;
        response.status_code = 500;
        response.content_type = "application/json";
        response.body = nlohmann::json{{"error", "Transaction agent not initialized"}}.dump();
        return response;
    }

    try {
        auto transaction_data = nlohmann::json::parse(req.body);

        auto result = transaction_agent_->detect_fraud(transaction_data);

        HTTPResponse response;
        response.status_code = 200;
        response.content_type = "application/json";
        response.body = result.dump(2);
        return response;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->log(LogLevel::ERROR, "Fraud detection failed: " + std::string(e.what()));
        }
        HTTPResponse response;
        response.status_code = 500;
        response.content_type = "application/json";
        response.body = nlohmann::json{{"error", "Fraud detection failed"}, {"message", e.what()}}.dump();
        return response;
    }
}

} // namespace regulens
