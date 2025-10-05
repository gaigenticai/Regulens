/**
 * Regulens Agentic AI - Simple Working UI Demo
 *
 * A working implementation that demonstrates the 5-tab interface
 * with proper tab switching functionality.
 */

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <atomic>
#include <unordered_map>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

namespace regulens {

class SimpleHTTPServer {
public:
    SimpleHTTPServer() : running_(false), server_socket_(-1) {}

    ~SimpleHTTPServer() {
        stop();
    }

    bool start(int port = 8080) {
        server_port_ = port;

        server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket_ < 0) {
            std::cerr << "Failed to create server socket" << std::endl;
            return false;
        }

        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(server_port_);

        int opt = 1;
        if (setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            std::cerr << "Failed to set socket options" << std::endl;
            return false;
        }

        if (bind(server_socket_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << "Failed to bind server socket to port " << server_port_ << std::endl;
            return false;
        }

        if (listen(server_socket_, 10) < 0) {
            std::cerr << "Failed to listen on server socket" << std::endl;
            return false;
        }

        running_ = true;
        server_thread_ = std::thread(&SimpleHTTPServer::server_loop, this);

        std::cout << "HTTP Server started on port " << server_port_ << std::endl;
        return true;
    }

    void stop() {
        running_ = false;
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
        if (server_socket_ >= 0) {
            close(server_socket_);
        }
    }

private:
    void server_loop() {
        while (running_) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_socket = accept(server_socket_, (struct sockaddr*)&client_addr, &client_len);

            if (client_socket < 0) {
                if (running_) {
                    std::cerr << "Failed to accept client connection" << std::endl;
                }
                continue;
            }

            handle_client(client_socket);
            close(client_socket);
        }
    }

    void handle_client(int client_socket) {
        char buffer[4096];
        ssize_t bytes_read = read(client_socket, buffer, sizeof(buffer) - 1);
        if (bytes_read <= 0) return;

        buffer[bytes_read] = '\0';
        std::string request(buffer);

        std::string response = generate_html();

        std::string http_response = "HTTP/1.1 200 OK\r\n"
                                  "Content-Type: text/html\r\n"
                                  "Content-Length: " + std::to_string(response.length()) + "\r\n"
                                  "Connection: close\r\n"
                                  "\r\n" + response;

        write(client_socket, http_response.c_str(), http_response.length());
    }

    std::string generate_html() {
        std::stringstream html;

        html << R"html(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Regulens - Agentic AI Compliance</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 0;
            padding: 0;
            background: #f5f5f5;
        }
        .header {
            background: #2563eb;
            color: white;
            padding: 1rem;
            text-align: center;
        }
        .nav {
            background: #1f2937;
            padding: 0;
        }
        .nav-tabs {
            display: flex;
            list-style: none;
            margin: 0;
            padding: 0;
        }
        .nav-tab {
            flex: 1;
            text-align: center;
            padding: 1rem;
            cursor: pointer;
            background: #374151;
            color: #9ca3af;
            border: none;
            transition: background 0.3s;
        }
        .nav-tab:hover {
            background: #4b5563;
        }
        .nav-tab.active {
            background: #fbbf24;
            color: #1f2937;
            font-weight: bold;
        }
        .tab-content {
            display: none;
            padding: 2rem;
            min-height: 400px;
        }
        .tab-content.active {
            display: block;
        }
        .metric-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
            gap: 1rem;
            margin-bottom: 2rem;
        }
        .metric-card {
            background: white;
            border-radius: 8px;
            padding: 1.5rem;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
            text-align: center;
        }
        .metric-value {
            font-size: 2rem;
            font-weight: bold;
            color: #2563eb;
            margin-bottom: 0.5rem;
        }
        .metric-title {
            font-size: 1.1rem;
            color: #374151;
            margin-bottom: 0.5rem;
        }
        .metric-desc {
            color: #6b7280;
            font-size: 0.9rem;
        }
        .agent-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
            gap: 1rem;
        }
        .agent-card {
            background: white;
            border-radius: 8px;
            padding: 1.5rem;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }
        .agent-header {
            display: flex;
            align-items: center;
            gap: 1rem;
            margin-bottom: 1rem;
        }
        .agent-avatar {
            width: 50px;
            height: 50px;
            border-radius: 50%;
            background: #2563eb;
            display: flex;
            align-items: center;
            justify-content: center;
            color: white;
            font-size: 1.5rem;
        }
        .agent-info h3 {
            margin: 0;
            color: #1f2937;
        }
        .agent-status {
            background: #10b981;
            color: white;
            padding: 0.25rem 0.75rem;
            border-radius: 20px;
            font-size: 0.8rem;
            display: inline-block;
        }
        .agent-metrics {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 1rem;
            margin-bottom: 1rem;
        }
        .metric-item {
            text-align: center;
            padding: 1rem;
            background: #f9fafb;
            border-radius: 6px;
        }
        .metric-number {
            font-size: 1.5rem;
            font-weight: bold;
            color: #2563eb;
        }
        .metric-label {
            color: #6b7280;
            font-size: 0.8rem;
        }
    </style>
</head>
<body>
    <div class="header">
        <h1>🤖 Regulens - Agentic AI Compliance Platform</h1>
        <p>Enterprise compliance intelligence through autonomous AI agents</p>
    </div>

    <nav class="nav">
        <ul class="nav-tabs">
            <li class="nav-tab active" onclick="switchTab('dashboard')">Dashboard</li>
            <li class="nav-tab" onclick="switchTab('agents')">AI Agents</li>
            <li class="nav-tab" onclick="switchTab('compliance')">Compliance</li>
            <li class="nav-tab" onclick="switchTab('analytics')">Analytics</li>
            <li class="nav-tab" onclick="switchTab('settings')">Settings</li>
        </ul>
    </nav>

    <!-- Dashboard Tab -->
    <div id="dashboard" class="tab-content active">
        <h2>Dashboard - Agentic AI Compliance Overview</h2>

        <div class="metric-grid">
            <div class="metric-card">
                <div class="metric-value">47</div>
                <div class="metric-title">Regulatory Changes Detected</div>
                <div class="metric-desc">Active monitoring across SEC & FCA sources</div>
            </div>

            <div class="metric-card">
                <div class="metric-value">23</div>
                <div class="metric-title">AI Decisions Made</div>
                <div class="metric-desc">Autonomous compliance decisions</div>
            </div>

            <div class="metric-card">
                <div class="metric-value">1.2s</div>
                <div class="metric-title">Response Time</div>
                <div class="metric-desc">Average detection to action time</div>
            </div>

            <div class="metric-card">
                <div class="metric-value">$2.3M</div>
                <div class="metric-title">Compliance Savings</div>
                <div class="metric-desc">Potential fines prevented</div>
            </div>
        </div>

        <div class="metric-card" style="margin-top: 2rem;">
            <h3 style="text-align: left; margin-bottom: 1rem;">Agentic AI Value Proposition</h3>
            <ul style="text-align: left; line-height: 1.6;">
                <li><strong>24/7 Autonomous Monitoring:</strong> AI agents work around the clock, detecting regulatory changes the moment they're published</li>
                <li><strong>Intelligent Risk Assessment:</strong> AI analyzes regulatory impact using contextual understanding and business intelligence</li>
                <li><strong>Automated Actions:</strong> Critical changes trigger automatic workflows, notifications, and remediation processes</li>
                <li><strong>Continuous Learning:</strong> AI agents improve accuracy over time, adapting to your organization's patterns</li>
            </ul>
        </div>
    </div>

    <!-- AI Agents Tab -->
    <div id="agents" class="tab-content">
        <h2>AI Agents - Your Autonomous Compliance Team</h2>

        <div class="agent-grid">
            <div class="agent-card">
                <div class="agent-header">
                    <div class="agent-avatar">🔍</div>
                    <div class="agent-info">
                        <h3>Regulatory Sentinel</h3>
                        <span class="agent-status">Active Monitoring</span>
                    </div>
                </div>
                <div class="agent-metrics">
                    <div class="metric-item">
                        <div class="metric-number">47</div>
                        <div class="metric-label">Changes Detected</div>
                    </div>
                    <div class="metric-item">
                        <div class="metric-number">2</div>
                        <div class="metric-label">Sources Monitored</div>
                    </div>
                </div>
                <p>Continuously scans SEC EDGAR and FCA websites for regulatory updates and rule changes.</p>
            </div>

            <div class="agent-card">
                <div class="agent-header">
                    <div class="agent-avatar">🧠</div>
                    <div class="agent-info">
                        <h3>Compliance Analyst</h3>
                        <span class="agent-status">Deep Analysis</span>
                    </div>
                </div>
                <div class="agent-metrics">
                    <div class="metric-item">
                        <div class="metric-number">23</div>
                        <div class="metric-label">Decisions Made</div>
                    </div>
                    <div class="metric-item">
                        <div class="metric-number">94%</div>
                        <div class="metric-label">Accuracy Rate</div>
                    </div>
                </div>
                <p>Analyzes regulatory impact, prioritizes changes, and recommends specific mitigation strategies.</p>
            </div>

            <div class="agent-card">
                <div class="agent-header">
                    <div class="agent-avatar">⚠️</div>
                    <div class="agent-info">
                        <h3>Risk Assessor</h3>
                        <span class="agent-status">Evaluating</span>
                    </div>
                </div>
                <div class="agent-metrics">
                    <div class="metric-item">
                        <div class="metric-number">12</div>
                        <div class="metric-label">Active Assessments</div>
                    </div>
                    <div class="metric-item">
                        <div class="metric-number">3</div>
                        <div class="metric-label">Critical Risks</div>
                    </div>
                </div>
                <p>Performs multi-factor risk analysis including regulatory impact, implementation complexity, and business disruption.</p>
            </div>

            <div class="agent-card">
                <div class="agent-header">
                    <div class="agent-avatar">🎯</div>
                    <div class="agent-info">
                        <h3>Action Orchestrator</h3>
                        <span class="agent-status">Executing</span>
                    </div>
                </div>
                <div class="agent-metrics">
                    <div class="metric-item">
                        <div class="metric-number">156</div>
                        <div class="metric-label">Actions Completed</div>
                    </div>
                    <div class="metric-item">
                        <div class="metric-number">98%</div>
                        <div class="metric-label">Success Rate</div>
                    </div>
                </div>
                <p>Coordinates automated compliance responses, stakeholder notifications, and workflow execution.</p>
            </div>
        </div>
    </div>

    <!-- Compliance Tab -->
    <div id="compliance" class="tab-content">
        <h2>Compliance Intelligence Hub</h2>

        <div class="metric-grid">
            <div class="metric-card">
                <div class="metric-value">98.5%</div>
                <div class="metric-title">Compliance Score</div>
                <div class="metric-desc">Overall compliance rating</div>
            </div>

            <div class="metric-card">
                <div class="metric-value">12</div>
                <div class="metric-title">Active Risk Items</div>
                <div class="metric-desc">Requiring attention</div>
            </div>

            <div class="metric-card">
                <div class="metric-value">3</div>
                <div class="metric-title">Critical Issues</div>
                <div class="metric-desc">Immediate action required</div>
            </div>

            <div class="metric-card">
                <div class="metric-value">45</div>
                <div class="metric-title">Days to Deadline</div>
                <div class="metric-desc">Next compliance deadline</div>
            </div>
        </div>
    </div>

    <!-- Analytics Tab -->
    <div id="analytics" class="tab-content">
        <h2>Predictive Analytics Dashboard</h2>

        <div class="metric-grid">
            <div class="metric-card">
                <div class="metric-value">+23%</div>
                <div class="metric-title">Regulatory Trends</div>
                <div class="metric-desc">Increase in regulatory activity</div>
            </div>

            <div class="metric-card">
                <div class="metric-value">94.7%</div>
                <div class="metric-title">AI Accuracy</div>
                <div class="metric-desc">Decision accuracy rate</div>
            </div>

            <div class="metric-card">
                <div class="metric-value">1.8x</div>
                <div class="metric-title">Response Velocity</div>
                <div class="metric-desc">Faster than industry average</div>
            </div>

            <div class="metric-card">
                <div class="metric-value">$2.3M</div>
                <div class="metric-title">Cost Savings</div>
                <div class="metric-desc">Fines prevented this quarter</div>
            </div>
        </div>
    </div>

    <!-- Settings Tab -->
    <div id="settings" class="tab-content">
        <h2>AI Agent Configuration</h2>

        <div class="metric-grid">
            <div class="metric-card">
                <div class="metric-value">4</div>
                <div class="metric-title">Active AI Agents</div>
                <div class="metric-desc">Configured and running</div>
            </div>

            <div class="metric-card">
                <div class="metric-value">2</div>
                <div class="metric-title">Data Sources</div>
                <div class="metric-desc">Regulatory feeds monitored</div>
            </div>

            <div class="metric-card">
                <div class="metric-value">5</div>
                <div class="metric-title">Notifications</div>
                <div class="metric-desc">Stakeholder groups configured</div>
            </div>

            <div class="metric-card">
                <div class="metric-value">90</div>
                <div class="metric-title">Data Retention</div>
                <div class="metric-desc">Days of compliance history</div>
            </div>
        </div>
    </div>

    <script>
        function switchTab(tabName) {
            // Hide all tab contents
            const tabContents = document.querySelectorAll('.tab-content');
            tabContents.forEach(content => {
                content.classList.remove('active');
            });

            // Remove active class from all tabs
            const tabs = document.querySelectorAll('.nav-tab');
            tabs.forEach(tab => {
                tab.classList.remove('active');
            });

            // Show selected tab content
            const selectedTab = document.getElementById(tabName);
            if (selectedTab) {
                selectedTab.classList.add('active');
            }

            // Add active class to clicked tab
            const clickedTab = Array.from(tabs).find(tab =>
                tab.textContent.trim().toLowerCase().includes(tabName.toLowerCase())
            );
            if (clickedTab) {
                clickedTab.classList.add('active');
            }
        }

        // Initialize
        document.addEventListener('DOMContentLoaded', function() {
            switchTab('dashboard');
        });
    </script>
</body>
</html>)html";

        return html.str();
    }

    std::atomic<bool> running_;
    int server_socket_;
    int server_port_;
    std::thread server_thread_;
};

class SimpleUIDemo {
public:
    SimpleUIDemo() : server_running_(false) {}

    void run_demo() {
        std::cout << "🤖 Regulens Agentic AI Compliance Platform - Simple UI Demo" << std::endl;
        std::cout << "===========================================================" << std::endl;
        std::cout << "This demo shows a working 5-tab interface with:" << std::endl;
        std::cout << "• Dashboard - Compliance metrics and AI value proposition" << std::endl;
        std::cout << "• AI Agents - Four specialized autonomous agents" << std::endl;
        std::cout << "• Compliance - Risk management and compliance scoring" << std::endl;
        std::cout << "• Analytics - Predictive insights and performance metrics" << std::endl;
        std::cout << "• Settings - AI agent configuration and system preferences" << std::endl;

        start_server();

        const char* display_host = std::getenv("WEB_SERVER_DISPLAY_HOST");
        const char* port_env = std::getenv("WEB_SERVER_PORT");
        std::string host = display_host ? display_host : "localhost";
        std::string port = port_env ? port_env : "8080";
        std::cout << "🌐 Open your browser and navigate to: http://" << host << ":" << port << std::endl;
        std::cout << "📊 Click through all 5 tabs to see the complete interface!" << std::endl;
        std::cout << "🎬 Press Ctrl+C to stop the demo" << std::endl;

        // Keep the main thread alive
        while (server_running_) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

private:
    void start_server() {
        if (http_server_.start(8080)) {
            server_running_ = true;
            std::cout << "✅ HTTP Server started successfully" << std::endl;
        } else {
            std::cerr << "❌ Failed to start HTTP server" << std::endl;
        }
    }

    SimpleHTTPServer http_server_;
    std::atomic<bool> server_running_;
};

} // namespace regulens

int main() {
    try {
        regulens::SimpleUIDemo demo;
        demo.run_demo();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "💥 Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
