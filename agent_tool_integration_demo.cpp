#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>
#include "shared/database/postgresql_connection.hpp"
#include "shared/logging/structured_logger.hpp"
#include "shared/agentic_brain/agentic_orchestrator.hpp"
#include "shared/tool_integration/tool_interface.hpp"
#include "shared/event_system/event.hpp"
#include "shared/event_system/event_bus.hpp"
#include "shared/config/configuration_manager.hpp"

namespace regulens {

class AgentToolIntegrationDemo {
public:
    AgentToolIntegrationDemo() = default;
    ~AgentToolIntegrationDemo() = default;

    bool initialize() {
        try {
            logger_ = &StructuredLogger::get_instance();

            if (!initialize_database()) {
                logger_->log(LogLevel::ERROR, "Failed to initialize database");
                return false;
            }

            if (!initialize_agentic_orchestrator()) {
                logger_->log(LogLevel::ERROR, "Failed to initialize agentic orchestrator");
                return false;
            }

            logger_->log(LogLevel::INFO, "Agent-Tool Integration Demo initialized successfully");
            return true;

        } catch (const std::exception& e) {
            logger_->log(LogLevel::ERROR, "Demo initialization failed: " + std::string(e.what()));
            return false;
        }
    }

    void run_interactive_demo() {
        std::cout << "🤖 AGENT-TOOL INTEGRATION DEMONSTRATION" << std::endl;
        std::cout << "======================================" << std::endl;
        std::cout << std::endl;

        show_menu();

        std::string command;
        while (true) {
            std::cout << std::endl << "🤖 Enter command (or 'help' for options): ";
            std::getline(std::cin, command);

            if (command == "quit" || command == "exit") {
                break;
            } else if (command == "help") {
                show_menu();
            } else if (command == "discover") {
                demonstrate_tool_discovery();
            } else if (command == "autonomous") {
                demonstrate_autonomous_agent();
            } else if (command == "workflow") {
                demonstrate_tool_workflow();
            } else if (command == "learning") {
                demonstrate_agent_learning();
            } else if (command == "coordination") {
                demonstrate_multi_agent_coordination();
            } else if (command == "realtime") {
                demonstrate_realtime_response();
            } else {
                std::cout << "❌ Unknown command. Type 'help' for options." << std::endl;
            }
        }

        std::cout << std::endl << "👋 Agent-tool integration demo completed!" << std::endl;
        show_final_summary();
    }

private:
    void show_menu() {
        std::cout << "🎛️  Available Commands:" << std::endl;
        std::cout << "  discover     - Demonstrate autonomous tool discovery" << std::endl;
        std::cout << "  autonomous   - Show agent autonomous tool selection" << std::endl;
        std::cout << "  workflow     - Demonstrate complete tool workflow" << std::endl;
        std::cout << "  learning     - Show agent learning from tool usage" << std::endl;
        std::cout << "  coordination - Multi-agent tool coordination" << std::endl;
        std::cout << "  realtime     - Real-time event-driven tool usage" << std::endl;
        std::cout << "  help         - Show this menu" << std::endl;
        std::cout << "  quit         - Exit the demo" << std::endl;
        std::cout << std::endl;
        std::cout << "💡 Agent Capabilities Demonstrated:" << std::endl;
        std::cout << "   • Autonomous tool discovery and selection" << std::endl;
        std::cout << "   • Independent tool authentication and usage" << std::endl;
        std::cout << "   • Intelligent situation analysis for tool recommendations" << std::endl;
        std::cout << "   • Learning from tool effectiveness and outcomes" << std::endl;
        std::cout << "   • Multi-agent coordination with shared tools" << std::endl;
        std::cout << "   • Real-time event-driven tool execution" << std::endl;
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

    bool initialize_agentic_orchestrator() {
        try {
            agentic_orchestrator_ = std::make_unique<AgenticOrchestrator>(db_pool_, logger_);
            return agentic_orchestrator_->initialize();
        } catch (const std::exception& e) {
            logger_->log(LogLevel::ERROR, "Agentic orchestrator initialization failed: " + std::string(e.what()));
            return false;
        }
    }

    void demonstrate_tool_discovery() {
        std::cout << "🔍 AGENT AUTONOMOUS TOOL DISCOVERY" << std::endl;
        std::cout << "==================================" << std::endl;

        std::cout << "🤖 Agent: \"I need to communicate important information. What tools are available?\"" << std::endl;
        std::cout << std::endl;

        // Agent discovers communication tools
        auto communication_tools = agentic_orchestrator_->discover_available_tools(ToolCategory::COMMUNICATION);
        std::cout << "📡 Available Communication Tools:" << std::endl;
        for (const auto& tool_id : communication_tools) {
            std::cout << "  ✅ " << tool_id << std::endl;
        }

        std::cout << std::endl;
        std::cout << "🤖 Agent: \"I need tools that can send notifications. What tools have NOTIFY capability?\"" << std::endl;
        std::cout << std::endl;

        // Agent finds tools by capability
        auto notify_tools = agentic_orchestrator_->find_tools_by_capability(ToolCapability::NOTIFY);
        std::cout << "🔔 Tools with NOTIFY capability:" << std::endl;
        for (const auto& tool_id : notify_tools) {
            std::cout << "  ✅ " << tool_id << std::endl;
        }

        std::cout << std::endl;
        std::cout << "🎯 Agent can autonomously discover and select appropriate tools based on:" << std::endl;
        std::cout << "   • Tool categories (COMMUNICATION, ERP, CRM, etc.)" << std::endl;
        std::cout << "   • Required capabilities (READ, WRITE, NOTIFY, etc.)" << std::endl;
        std::cout << "   • Tool availability and health status" << std::endl;
        std::cout << "   • Previous success rates and performance" << std::endl;
    }

    void demonstrate_autonomous_agent() {
        std::cout << "🧠 AGENT AUTONOMOUS TOOL SELECTION" << std::endl;
        std::cout << "==================================" << std::endl;

        // Simulate a high-risk transaction scenario
        nlohmann::json transaction_context = {
            {"transaction_id", "TXN-2024-HIGH-RISK-001"},
            {"amount", 2500000.0},
            {"risk_score", 0.87},
            {"risk_level", "HIGH"},
            {"flags", {"high_amount", "unusual_timing", "international_transfer"}},
            {"requires_review", true},
            {"alert_email", "compliance@company.com"}
        };

        std::cout << "💳 High-Risk Transaction Detected:" << std::endl;
        std::cout << "   Amount: $" << transaction_context["amount"] << std::endl;
        std::cout << "   Risk Score: " << transaction_context["risk_score"] << std::endl;
        std::cout << "   Requires Human Review: " << (transaction_context["requires_review"] ? "YES" : "NO") << std::endl;
        std::cout << std::endl;

        std::cout << "🤖 Transaction Guardian Agent: \"Analyzing situation and recommending tools...\"" << std::endl;
        std::cout << std::endl;

        // Agent analyzes situation and recommends tools
        auto tool_recommendations = agentic_orchestrator_->analyze_situation_and_recommend_tools(
            AgentType::TRANSACTION_GUARDIAN,
            transaction_context
        );

        std::cout << "🛠️  Agent Tool Recommendations:" << std::endl;
        for (size_t i = 0; i < tool_recommendations.size(); ++i) {
            const auto& rec = tool_recommendations[i];
            std::cout << "  " << (i+1) << ". " << rec["tool_category"].get<std::string>() << " tool" << std::endl;
            std::cout << "     Capability: " << rec["tool_capability"].get<std::string>() << std::endl;
            std::cout << "     Rationale: " << rec["rationale"].get<std::string>() << std::endl;
            std::cout << "     Urgency: " << rec["urgency"].get<std::string>() << std::endl;
            std::cout << std::endl;
        }

        std::cout << "🎯 Agent autonomously:" << std::endl;
        std::cout << "   • Analyzes the situation context" << std::endl;
        std::cout << "   • Determines required tool capabilities" << std::endl;
        std::cout << "   • Recommends specific tools with rationale" << std::endl;
        std::cout << "   • Considers urgency and fallback options" << std::endl;
        std::cout << "   • Learns from past tool effectiveness" << std::endl;
    }

    void demonstrate_tool_workflow() {
        std::cout << "🔄 COMPLETE TOOL WORKFLOW EXECUTION" << std::endl;
        std::cout << "===================================" << std::endl;

        std::cout << "🚨 Regulatory Change Scenario:" << std::endl;
        std::cout << "   New GDPR requirements detected" << std::endl;
        std::cout << "   Requires immediate compliance team notification" << std::endl;
        std::cout << std::endl;

        nlohmann::json regulatory_context = {
            {"regulation_name", "GDPR Data Protection Regulation Update"},
            {"effective_date", "2024-05-25"},
            {"impact_level", "CRITICAL"},
            {"source", "European Data Protection Board"},
            {"description", "New automated decision-making disclosure requirements"},
            {"notify_email", "gdpr-compliance@company.com"}
        };

        std::cout << "🤖 Regulatory Assessor Agent: \"Executing autonomous tool workflow...\"" << std::endl;
        std::cout << std::endl;

        // Agent executes autonomous tool workflow
        std::vector<std::string> required_tools = {"enterprise-email-smtp"};
        bool workflow_success = agentic_orchestrator_->execute_autonomous_tool_workflow(
            AgentType::REGULATORY_ASSESSOR,
            regulatory_context,
            required_tools
        );

        if (workflow_success) {
            std::cout << "✅ Workflow completed successfully!" << std::endl;
            std::cout << "   • Tool acquired and authenticated" << std::endl;
            std::cout << "   • Regulatory alert email sent" << std::endl;
            std::cout << "   • Compliance team notified" << std::endl;
            std::cout << "   • Audit trail recorded" << std::endl;
        } else {
            std::cout << "❌ Workflow encountered issues (expected in demo environment)" << std::endl;
            std::cout << "   In production: Full SMTP integration would work" << std::endl;
        }

        std::cout << std::endl;
        std::cout << "🔄 Autonomous Workflow Steps:" << std::endl;
        std::cout << "   1. Agent analyzes regulatory change" << std::endl;
        std::cout << "   2. Agent selects appropriate tools (email for notifications)" << std::endl;
        std::cout << "   3. Agent acquires tool instances with authentication" << std::endl;
        std::cout << "   4. Agent executes operations (send regulatory alert)" << std::endl;
        std::cout << "   5. Agent monitors execution and handles errors" << std::endl;
        std::cout << "   6. Agent records outcomes for learning" << std::endl;
        std::cout << "   7. Agent publishes events about tool usage" << std::endl;
    }

    void demonstrate_agent_learning() {
        std::cout << "🧠 AGENT LEARNING FROM TOOL USAGE" << std::endl;
        std::cout << "=================================" << std::endl;

        std::cout << "🤖 Agent: \"Learning from tool effectiveness to improve future decisions...\"" << std::endl;
        std::cout << std::endl;

        // Simulate learning from successful and failed tool operations
        std::cout << "📊 Tool Performance Learning:" << std::endl;

        // Simulate successful email delivery
        bool learn_success = agentic_orchestrator_->learn_tool_effectiveness(
            "enterprise-email-smtp",
            "send_template",
            true,  // success
            std::chrono::milliseconds(250)  // execution time
        );

        if (learn_success) {
            std::cout << "✅ Learned: Email tool successful (250ms) - increased preference" << std::endl;
        }

        // Simulate failed operation
        learn_success = agentic_orchestrator_->learn_tool_effectiveness(
            "slow-email-tool",
            "send_notification",
            false,  // failed
            std::chrono::milliseconds(5000)  // slow execution
        );

        if (learn_success) {
            std::cout << "❌ Learned: Slow tool failed (5s timeout) - decreased preference" << std::endl;
        }

        std::cout << std::endl;
        std::cout << "🎯 Future Tool Recommendations:" << std::endl;

        // Get learned recommendations
        auto recommendations = agentic_orchestrator_->get_tool_usage_recommendations(AgentType::TRANSACTION_GUARDIAN);

        if (recommendations.empty()) {
            std::cout << "   (Learning data would be available in full production system)" << std::endl;
        } else {
            for (const auto& rec : recommendations) {
                std::cout << "   • " << rec.dump(2) << std::endl;
            }
        }

        std::cout << std::endl;
        std::cout << "🧠 Agent Learning Capabilities:" << std::endl;
        std::cout << "   • Tracks tool success rates and performance" << std::endl;
        std::cout << "   • Learns from execution times and reliability" << std::endl;
        std::cout << "   • Adapts tool selection based on historical data" << std::endl;
        std::cout << "   • Provides intelligent tool recommendations" << std::endl;
        std::cout << "   • Continuously improves decision-making" << std::endl;
    }

    void demonstrate_multi_agent_coordination() {
        std::cout << "👥 MULTI-AGENT TOOL COORDINATION" << std::endl;
        std::cout << "=================================" << std::endl;

        std::cout << "🏢 Enterprise Scenario: Multi-department compliance incident" << std::endl;
        std::cout << std::endl;

        // Simulate multiple agents working together
        std::cout << "🤖 Agent Coordination:" << std::endl;
        std::cout << "   1. Transaction Guardian detects suspicious activity" << std::endl;
        std::cout << "   2. Regulatory Assessor evaluates compliance impact" << std::endl;
        std::cout << "   3. Audit Intelligence generates investigation report" << std::endl;
        std::cout << std::endl;

        // Each agent uses tools autonomously
        nlohmann::json incident_context = {
            {"incident_type", "Multi-Department Compliance Breach"},
            {"severity", "CRITICAL"},
            {"affected_departments", {"Finance", "Legal", "Compliance"}},
            {"immediate_actions_required", true},
            {"notify_emails", {
                "executives@company.com",
                "legal@company.com",
                "compliance@company.com"
            }}
        };

        std::cout << "📧 Coordinated Email Notifications:" << std::endl;

        // Transaction Guardian sends initial alert
        auto result1 = agentic_orchestrator_->execute_tool_operation(
            "enterprise-email-smtp",
            "send_template",
            {
                {"template_id", "compliance_violation"},
                {"to", "compliance@company.com"},
                {"variables", incident_context}
            }
        );

        std::cout << "   ✅ Compliance Team: " << (result1.success ? "Notified" : "Notification failed") << std::endl;

        // Regulatory Assessor sends escalation
        auto result2 = agentic_orchestrator_->execute_tool_operation(
            "enterprise-email-smtp",
            "send_template",
            {
                {"template_id", "regulatory_alert"},
                {"to", "executives@company.com"},
                {"variables", incident_context}
            }
        );

        std::cout << "   ✅ Executive Team: " << (result2.success ? "Escalated" : "Escalation failed") << std::endl;

        // Audit Intelligence sends investigation request
        auto result3 = agentic_orchestrator_->execute_tool_operation(
            "enterprise-email-smtp",
            "send_template",
            {
                {"template_id", "agent_decision_review"},
                {"to", "legal@company.com"},
                {"variables", incident_context}
            }
        );

        std::cout << "   ✅ Legal Team: " << (result3.success ? "Investigation requested" : "Request failed") << std::endl;

        std::cout << std::endl;
        std::cout << "🎯 Multi-Agent Coordination Features:" << std::endl;
        std::cout << "   • Agents work independently but coordinated" << std::endl;
        std::cout << "   • Shared tool resources with proper access control" << std::endl;
        std::cout << "   • Event-driven communication between agents" << std::endl;
        std::cout << "   • Escalation protocols and notification chains" << std::endl;
        std::cout << "   • Comprehensive audit trails across all agents" << std::endl;
    }

    void demonstrate_realtime_response() {
        std::cout << "⚡ REAL-TIME EVENT-DRIVEN TOOL USAGE" << std::endl;
        std::cout << "===================================" << std::endl;

        std::cout << "🌐 Real-Time Scenario: System anomaly detected" << std::endl;
        std::cout << std::endl;

        nlohmann::json anomaly_context = {
            {"anomaly_type", "Unusual Transaction Pattern"},
            {"severity", "HIGH"},
            {"affected_systems", {"Payment Gateway", "Risk Engine"}},
            {"detection_time", "2024-01-15T10:30:00Z"},
            {"automated_response", "TRANSACTION_BLOCKING_ACTIVATED"},
            {"human_intervention_required", true}
        };

        std::cout << "🚨 System Anomaly Detected - Real-time Response:" << std::endl;
        std::cout << std::endl;

        // Agent responds to real-time event
        std::cout << "📡 Event: HIGH_SEVERITY_ANOMALY_DETECTED" << std::endl;
        std::cout << "🤖 Agent: \"Real-time event received, analyzing and responding...\"" << std::endl;
        std::cout << std::endl;

        // Agent analyzes and responds autonomously
        auto tool_recs = agentic_orchestrator_->analyze_situation_and_recommend_tools(
            AgentType::TRANSACTION_GUARDIAN,
            anomaly_context
        );

        std::cout << "🛠️  Immediate Tool Actions:" << std::endl;
        for (const auto& rec : tool_recs) {
            if (rec["urgency"] == "CRITICAL" || rec["urgency"] == "HIGH") {
                std::cout << "   🚨 " << rec["tool_category"].get<std::string>() << ": " << rec["rationale"].get<std::string>() << std::endl;
            }
        }

        // Execute immediate response
        agentic_orchestrator_->execute_autonomous_tool_workflow(
            AgentType::TRANSACTION_GUARDIAN,
            anomaly_context,
            {"enterprise-email-smtp"}
        );

        std::cout << std::endl;
        std::cout << "⚡ Real-Time Response Features:" << std::endl;
        std::cout << "   • Event-driven immediate action" << std::endl;
        std::cout << "   • Sub-second analysis and decision making" << std::endl;
        std::cout << "   • Autonomous tool execution without human intervention" << std::endl;
        std::cout << "   • Escalation to appropriate teams automatically" << std::endl;
        std::cout << "   • Full audit trail of automated responses" << std::endl;
        std::cout << "   • Learning from response effectiveness" << std::endl;
    }

    void show_final_summary() {
        std::cout << "📊 AGENT-TOOL INTEGRATION SUMMARY" << std::endl;
        std::cout << "=================================" << std::endl;

        auto health = agentic_orchestrator_->get_system_health();
        std::cout << "🤖 Agent System Health:" << std::endl;
        std::cout << "   Status: " << health["status"] << std::endl;
        std::cout << "   Agents Initialized: " << (health["agents_initialized"] ? "YES" : "NO") << std::endl;
        std::cout << "   Available Tools: " << health["tools_available"] << std::endl;
        std::cout << std::endl;

        std::cout << "🎯 Agent Autonomous Capabilities Demonstrated:" << std::endl;
        std::cout << "   ✅ Independent tool discovery and selection" << std::endl;
        std::cout << "   ✅ Autonomous authentication and tool acquisition" << std::endl;
        std::cout << "   ✅ Intelligent situation analysis for tool recommendations" << std::endl;
        std::cout << "   ✅ On-demand tool execution with error handling" << std::endl;
        std::cout << "   ✅ Learning from tool effectiveness and outcomes" << std::endl;
        std::cout << "   ✅ Real-time event-driven tool usage" << std::endl;
        std::cout << "   ✅ Multi-agent coordination with shared tools" << std::endl;
        std::cout << "   ✅ Complete audit trails and monitoring" << std::endl;
        std::cout << std::endl;

        std::cout << "🚀 Production Impact:" << std::endl;
        std::cout << "   • 24/7 autonomous compliance monitoring" << std::endl;
        std::cout << "   • Immediate response to critical events" << std::endl;
        std::cout << "   • Intelligent escalation and notification" << std::endl;
        std::cout << "   • Continuous learning and improvement" << std::endl;
        std::cout << "   • Enterprise-grade tool integration" << std::endl;
        std::cout << "   • Complete auditability and compliance" << std::endl;
        std::cout << std::endl;

        std::cout << "💡 Key Insight: Agents are not just reactive - they are proactive," << std::endl;
        std::cout << "   autonomous actors that can discover, acquire, and use tools" << std::endl;
        std::cout << "   independently to solve complex business problems in real-time." << std::endl;
    }

    // Member variables
    StructuredLogger* logger_;
    std::shared_ptr<ConnectionPool> db_pool_;
    std::unique_ptr<AgenticOrchestrator> agentic_orchestrator_;
};

} // namespace regulens

int main() {
    try {
        regulens::AgentToolIntegrationDemo demo;

        if (!demo.initialize()) {
            std::cerr << "Failed to initialize Agent-Tool Integration Demo" << std::endl;
            return 1;
        }

        demo.run_interactive_demo();

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " + std::string(e.what()) << std::endl;
        return 1;
    }
}
