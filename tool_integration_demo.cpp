#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>
#include "shared/database/postgresql_connection.hpp"
#include "shared/logging/structured_logger.hpp"
#include "shared/tool_integration/tool_interface.hpp"
#include "shared/tool_integration/tools/email_tool.hpp"
#include "shared/config/configuration_manager.hpp"

namespace regulens {

class ToolIntegrationDemo {
public:
    ToolIntegrationDemo() = default;
    ~ToolIntegrationDemo() = default;

    bool initialize() {
        try {
            logger_ = &StructuredLogger::get_instance();

            if (!initialize_database()) {
                logger_->log(LogLevel::ERROR, "Failed to initialize database");
                return false;
            }

            if (!initialize_tool_registry()) {
                logger_->log(LogLevel::ERROR, "Failed to initialize tool registry");
                return false;
            }

            if (!register_sample_tools()) {
                logger_->log(LogLevel::ERROR, "Failed to register sample tools");
                return false;
            }

            logger_->log(LogLevel::INFO, "Tool Integration Demo initialized successfully");
            return true;

        } catch (const std::exception& e) {
            logger_->log(LogLevel::ERROR, "Demo initialization failed: " + std::string(e.what()));
            return false;
        }
    }

    void run_interactive_demo() {
        std::cout << "🔧 TOOL INTEGRATION LAYER DEMONSTRATION" << std::endl;
        std::cout << "=======================================" << std::endl;
        std::cout << std::endl;

        show_menu();

        std::string command;
        while (true) {
            std::cout << std::endl << "🔧 Enter command (or 'help' for options): ";
            std::getline(std::cin, command);

            if (command == "quit" || command == "exit") {
                break;
            } else if (command == "help") {
                show_menu();
            } else if (command == "list") {
                list_available_tools();
            } else if (command == "email") {
                demonstrate_email_tool();
            } else if (command == "template") {
                demonstrate_email_templates();
            } else if (command == "health") {
                show_tool_health();
            } else if (command == "metrics") {
                show_tool_metrics();
            } else if (command == "stress") {
                run_tool_stress_test();
            } else if (command == "catalog") {
                show_tool_catalog();
            } else {
                std::cout << "❌ Unknown command. Type 'help' for options." << std::endl;
            }
        }

        std::cout << std::endl << "👋 Tool integration demo completed!" << std::endl;
        show_final_statistics();
    }

private:
    void show_menu() {
        std::cout << "🎛️  Available Commands:" << std::endl;
        std::cout << "  list      - List all available tools" << std::endl;
        std::cout << "  email     - Demonstrate email tool functionality" << std::endl;
        std::cout << "  template  - Demonstrate email template system" << std::endl;
        std::cout << "  health    - Show tool health status" << std::endl;
        std::cout << "  metrics   - Show tool performance metrics" << std::endl;
        std::cout << "  stress    - Run stress test on tools" << std::endl;
        std::cout << "  catalog   - Show complete tool catalog" << std::endl;
        std::cout << "  help      - Show this menu" << std::endl;
        std::cout << "  quit      - Exit the demo" << std::endl;
        std::cout << std::endl;
        std::cout << "💡 Tool Integration Features Demonstrated:" << std::endl;
        std::cout << "   • Standardized tool interfaces" << std::endl;
        std::cout << "   • Authentication and security" << std::endl;
        std::cout << "   • Rate limiting and throttling" << std::endl;
        std::cout << "   • Health monitoring and metrics" << std::endl;
        std::cout << "   • Template-based operations" << std::endl;
        std::cout << "   • Error handling and retry logic" << std::endl;
        std::cout << "   • Real-time status monitoring" << std::endl;
    }

    bool initialize_database() {
        try {
            // Get database configuration from centralized configuration manager
            auto& config_manager = ConfigurationManager::get_instance();
            DatabaseConfig config = config_manager.get_database_config();
            config.ssl_mode = false; // Local development

            db_pool_ = std::make_shared<ConnectionPool>(config);
            return true;
        } catch (const std::exception& e) {
            logger_->log(LogLevel::ERROR, "Database initialization failed: " + std::string(e.what()));
            return false;
        }
    }

    bool initialize_tool_registry() {
        try {
            tool_registry_ = std::make_unique<ToolRegistry>(db_pool_, logger_);
            return tool_registry_->initialize();
        } catch (const std::exception& e) {
            logger_->log(LogLevel::ERROR, "Tool registry initialization failed: " + std::string(e.what()));
            return false;
        }
    }

    bool register_sample_tools() {
        try {
            // Register Email Tool
            ToolConfig email_config;
            email_config.tool_id = "enterprise-email-smtp";
            email_config.tool_name = "Enterprise Email SMTP";
            email_config.description = "SMTP-based email delivery for compliance notifications and alerts";
            email_config.category = ToolCategory::COMMUNICATION;
            email_config.capabilities = {
                ToolCapability::NOTIFY,
                ToolCapability::WRITE
            };
            email_config.auth_type = AuthType::BASIC;
            email_config.connection_config = {
                {"smtp_server", "smtp.gmail.com"},  // Demo - replace with actual SMTP
                {"smtp_port", 587},
                {"use_tls", true},
                {"use_ssl", false}
            };
            email_config.auth_config = {
                {"username", "noreply@regulens.com"},  // Demo credentials
                {"password", "demo-password"}
            };
            email_config.metadata = {
                {"from_address", "noreply@regulens.com"},
                {"from_name", "Regulens AI Compliance System"}
            };

            auto email_tool = ToolFactory::create_tool(email_config, logger_);
            if (!tool_registry_->register_tool(std::move(email_tool))) {
                logger_->log(LogLevel::WARN, "Failed to register email tool");
            }

            // Register CRM Tool (integration to be implemented)
            ToolConfig crm_config;
            crm_config.tool_id = "salesforce-crm";
            crm_config.tool_name = "Salesforce CRM Integration";
            crm_config.description = "Customer relationship management integration";
            crm_config.category = ToolCategory::CRM;
            crm_config.capabilities = {
                ToolCapability::READ,
                ToolCapability::WRITE,
                ToolCapability::SEARCH
            };
            crm_config.auth_type = AuthType::OAUTH2;

            // Note: Real CRM tool implementation would be added later
            // For demo, we show the registry capabilities

            return true;

        } catch (const std::exception& e) {
            logger_->log(LogLevel::ERROR, "Sample tool registration failed: " + std::string(e.what()));
            return false;
        }
    }

    void list_available_tools() {
        std::cout << "📋 AVAILABLE TOOLS" << std::endl;
        std::cout << "==================" << std::endl;

        auto tools = tool_registry_->get_available_tools();

        if (tools.empty()) {
            std::cout << "No tools available." << std::endl;
            return;
        }

        for (const auto& tool_id : tools) {
            auto details = tool_registry_->get_tool_details(tool_id);
            if (details.contains("error")) {
                std::cout << "❌ " << tool_id << " - Error: " << details["error"] << std::endl;
            } else {
                std::string status = details["health"]["status"];
                std::string category = details["category"];
                std::cout << "✅ " << tool_id << " (" << category << ") - " << status << std::endl;
            }
        }

        std::cout << std::endl << "Total tools: " << tools.size() << std::endl;
    }

    void demonstrate_email_tool() {
        std::cout << "📧 EMAIL TOOL DEMONSTRATION" << std::endl;
        std::cout << "===========================" << std::endl;

        Tool* email_tool = tool_registry_->get_tool("enterprise-email-smtp");
        if (!email_tool) {
            std::cout << "❌ Email tool not available" << std::endl;
            return;
        }

        std::cout << "✅ Email tool found and ready" << std::endl;

        // Demonstrate email validation
        std::cout << std::endl << "🔍 Email Validation:" << std::endl;
        std::vector<std::string> test_emails = {
            "user@company.com",
            "invalid-email",
            "test@regulens.ai",
            "user.name+tag@domain.co.uk"
        };

        for (const auto& email : test_emails) {
            auto result = email_tool->execute_operation("validate_email", {{"email", email}});
            std::string status = result.success ? "✅ Valid" : "❌ Invalid";
            std::cout << "  " << email << " - " << status << std::endl;
        }

        // Demonstrate email sending (mock - won't actually send)
        std::cout << std::endl << "📤 Email Sending Demo:" << std::endl;
        std::cout << "⚠️  Note: This is a demonstration. Actual email sending requires valid SMTP configuration." << std::endl;

        nlohmann::json email_params = {
            {"to", "compliance@company.com"},
            {"subject", "Test Email from Regulens AI"},
            {"body_html", "<h1>Test Email</h1><p>This is a test email from the Regulens AI system.</p>"},
            {"body_text", "Test Email\nThis is a test email from the Regulens AI system."}
        };

        auto send_result = email_tool->execute_operation("send_email", email_params);

        if (send_result.success) {
            std::cout << "✅ Email would be sent successfully" << std::endl;
            if (send_result.data.contains("message_id")) {
                std::cout << "   Message ID: " << send_result.data["message_id"] << std::endl;
            }
        } else {
            std::cout << "❌ Email sending failed: " << send_result.error_message << std::endl;
        }

        std::cout << "   Execution time: " << send_result.execution_time.count() << "ms" << std::endl;
    }

    void demonstrate_email_templates() {
        std::cout << "📝 EMAIL TEMPLATE SYSTEM DEMONSTRATION" << std::endl;
        std::cout << "======================================" << std::endl;

        Tool* email_tool = tool_registry_->get_tool("enterprise-email-smtp");
        if (!email_tool) {
            std::cout << "❌ Email tool not available" << std::endl;
            return;
        }

        // Cast to EmailTool to access template methods
        EmailTool* email_tool_ptr = dynamic_cast<EmailTool*>(email_tool);
        if (!email_tool_ptr) {
            std::cout << "❌ Cannot access email template methods" << std::endl;
            return;
        }

        std::cout << "📋 Available Email Templates:" << std::endl;
        auto templates = email_tool_ptr->get_available_templates();

        for (const auto& template_id : templates) {
            EmailTemplate* tmpl = email_tool_ptr->get_template(template_id);
            if (tmpl) {
                std::cout << "  • " << tmpl->template_id << ": " << tmpl->name << std::endl;
                std::cout << "    Required variables: ";
                for (size_t i = 0; i < tmpl->required_variables.size(); ++i) {
                    std::cout << tmpl->required_variables[i];
                    if (i < tmpl->required_variables.size() - 1) std::cout << ", ";
                }
                std::cout << std::endl;
            }
        }

        std::cout << std::endl << "📤 Template Email Demo:" << std::endl;

        // Demonstrate regulatory alert template
        std::unordered_map<std::string, std::string> alert_variables = {
            {"regulation_name", "GDPR Data Protection Regulation"},
            {"effective_date", "2024-05-25"},
            {"impact_level", "HIGH"},
            {"source", "European Data Protection Board"},
            {"description", "New requirements for automated decision-making systems"},
            {"action_required", "Update AI decision processes and implement human oversight mechanisms"}
        };

        auto template_result = email_tool->execute_operation("send_template", {
            {"template_id", "regulatory_alert"},
            {"to", "compliance@company.com"},
            {"variables", alert_variables}
        });

        if (template_result.success) {
            std::cout << "✅ Regulatory alert template email would be sent" << std::endl;
        } else {
            std::cout << "❌ Template email failed: " << template_result.error_message << std::endl;
        }
    }

    void show_tool_health() {
        std::cout << "🏥 TOOL HEALTH STATUS" << std::endl;
        std::cout << "====================" << std::endl;

        auto health = tool_registry_->get_system_health();

        std::cout << "📊 System Overview:" << std::endl;
        std::cout << "  Total Tools: " << health["total_tools"] << std::endl;
        std::cout << "  Enabled Tools: " << health["enabled_tools"] << std::endl;
        std::cout << "  Healthy Tools: " << health["healthy_tools"] << std::endl;
        std::cout << "  Degraded Tools: " << health["degraded_tools"] << std::endl;
        std::cout << "  Unhealthy Tools: " << health["unhealthy_tools"] << std::endl;
        std::cout << "  Offline Tools: " << health["offline_tools"] << std::endl;

        std::cout << std::endl << "🔍 Individual Tool Status:" << std::endl;
        const auto& tools = health["tools"];
        for (const auto& tool : tools) {
            std::string status = tool["status"];
            std::string status_icon = (status == "HEALTHY") ? "✅" :
                                     (status == "DEGRADED") ? "⚠️" :
                                     (status == "UNHEALTHY") ? "❌" : "🔌";
            std::cout << "  " << status_icon << " " << tool["tool_id"].get<std::string>()
                      << " - " << status << std::endl;
        }
    }

    void show_tool_metrics() {
        std::cout << "📈 TOOL PERFORMANCE METRICS" << std::endl;
        std::cout << "============================" << std::endl;

        auto tools = tool_registry_->get_available_tools();

        for (const auto& tool_id : tools) {
            Tool* tool = tool_registry_->get_tool(tool_id);
            if (!tool) continue;

            const auto& metrics = tool->get_metrics();
            const auto& health = tool->get_health_details();

            std::cout << "🔧 " << tool_id << ":" << std::endl;
            std::cout << "   Operations Total: " << metrics.operations_total.load() << std::endl;
            std::cout << "   Operations Successful: " << metrics.operations_successful.load() << std::endl;
            std::cout << "   Operations Failed: " << metrics.operations_failed.load() << std::endl;
            std::cout << "   Operations Retried: " << metrics.operations_retried.load() << std::endl;
            std::cout << "   Rate Limit Hits: " << metrics.rate_limit_hits.load() << std::endl;
            std::cout << "   Timeouts: " << metrics.timeouts.load() << std::endl;
            std::cout << "   Avg Response Time: " << metrics.avg_response_time.count() << "ms" << std::endl;
            std::cout << "   Health Status: " << tool_health_status_to_string(metrics.health_status) << std::endl;
            std::cout << std::endl;
        }
    }

    void run_tool_stress_test() {
        std::cout << "⚡ TOOL STRESS TEST" << std::endl;
        std::cout << "==================" << std::endl;

        Tool* email_tool = tool_registry_->get_tool("enterprise-email-smtp");
        if (!email_tool) {
            std::cout << "❌ Email tool not available for stress test" << std::endl;
            return;
        }

        const int NUM_OPERATIONS = 50;
        std::cout << "📤 Running " << NUM_OPERATIONS << " email validation operations..." << std::endl;

        auto start_time = std::chrono::steady_clock::now();

        for (int i = 1; i <= NUM_OPERATIONS; ++i) {
            std::string test_email = "test" + std::to_string(i) + "@company.com";
            auto result = email_tool->execute_operation("validate_email", {{"email", test_email}});

            if (i % 10 == 0) {
                std::cout << "   Completed " << i << "/" << NUM_OPERATIONS << " operations" << std::endl;
            }
        }

        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        std::cout << "✅ Stress test completed in " << duration.count() << "ms" << std::endl;
        std::cout << "   Average operations per second: " << (NUM_OPERATIONS * 1000.0 / duration.count()) << std::endl;

        show_tool_metrics();
    }

    void show_tool_catalog() {
        std::cout << "📚 TOOL CATALOG" << std::endl;
        std::cout << "===============" << std::endl;

        auto catalog = tool_registry_->get_tool_catalog();

        if (catalog.empty()) {
            std::cout << "No tools in catalog." << std::endl;
            return;
        }

        for (const auto& tool : catalog) {
            std::cout << "🔧 " << tool["tool_name"].get<std::string>() << std::endl;
            std::cout << "   ID: " << tool["tool_id"].get<std::string>() << std::endl;
            std::cout << "   Category: " << tool["category"].get<std::string>() << std::endl;

            if (tool.contains("description") && !tool["description"].is_null()) {
                std::cout << "   Description: " << tool["description"].get<std::string>() << std::endl;
            }

            std::cout << "   Capabilities: ";
            const auto& caps = tool["capabilities"];
            for (size_t i = 0; i < caps.size(); ++i) {
                std::cout << caps[i].get<std::string>();
                if (i < caps.size() - 1) std::cout << ", ";
            }
            std::cout << std::endl;

            std::cout << "   Auth Type: " << tool["auth_type"].get<std::string>() << std::endl;
            std::cout << "   Rate Limit: " << tool["rate_limit_per_minute"].get<int>() << " ops/min" << std::endl;
            std::cout << "   Status: " << (tool["enabled"].get<bool>() ? "Enabled" : "Disabled") << std::endl;
            std::cout << std::endl;
        }
    }

    void show_final_statistics() {
        std::cout << "📊 FINAL DEMO STATISTICS" << std::endl;
        std::cout << "========================" << std::endl;

        auto health = tool_registry_->get_system_health();
        std::cout << "System Health:" << std::endl;
        std::cout << "  Tools: " << health["total_tools"] << " total, "
                  << health["healthy_tools"] << " healthy" << std::endl;

        show_tool_metrics();

        std::cout << std::endl << "🎯 Tool Integration Layer Capabilities Demonstrated:" << std::endl;
        std::cout << "   • Tool Registry and Discovery" << std::endl;
        std::cout << "   • Standardized Tool Interfaces" << std::endl;
        std::cout << "   • Authentication and Configuration" << std::endl;
        std::cout << "   • Health Monitoring and Metrics" << std::endl;
        std::cout << "   • Rate Limiting and Performance Control" << std::endl;
        std::cout << "   • Template-based Operations" << std::endl;
        std::cout << "   • Error Handling and Retry Logic" << std::endl;
        std::cout << "   • Real-time Status Tracking" << std::endl;
    }

    // Member variables
    StructuredLogger* logger_;
    std::shared_ptr<ConnectionPool> db_pool_;
    std::unique_ptr<ToolRegistry> tool_registry_;
};

} // namespace regulens

int main() {
    try {
        regulens::ToolIntegrationDemo demo;

        if (!demo.initialize()) {
            std::cerr << "Failed to initialize Tool Integration Demo" << std::endl;
            return 1;
        }

        demo.run_interactive_demo();

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
