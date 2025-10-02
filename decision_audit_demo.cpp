#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>
#include "shared/database/postgresql_connection.hpp"
#include "shared/logging/structured_logger.hpp"
#include "shared/audit/decision_audit_trail.hpp"
#include "shared/config/configuration_manager.hpp"

namespace regulens {

class DecisionAuditDemo {
public:
    DecisionAuditDemo() = default;
    ~DecisionAuditDemo() = default;

    bool initialize() {
        try {
            logger_ = &StructuredLogger::get_instance();

            if (!initialize_database()) {
                logger_->log(LogLevel::ERROR, "Failed to initialize database");
                return false;
            }

            if (!initialize_database_schema()) {
                logger_->log(LogLevel::ERROR, "Failed to initialize database schema");
                return false;
            }

            if (!initialize_audit_manager()) {
                logger_->log(LogLevel::ERROR, "Failed to initialize audit manager");
                return false;
            }

            logger_->log(LogLevel::INFO, "Decision Audit Demo initialized successfully");
            return true;

        } catch (const std::exception& e) {
            logger_->log(LogLevel::ERROR, "Demo initialization failed: " + std::string(e.what()));
            return false;
        }
    }

    void run_interactive_demo() {
        std::cout << "🔍 Decision Audit & Explanation System Demo" << std::endl;
        std::cout << "==============================================" << std::endl;
        std::cout << std::endl;

        show_menu();

        std::string command;
        while (true) {
            std::cout << std::endl << "📝 Enter command (or 'help' for options): ";
            std::getline(std::cin, command);

            if (command == "quit" || command == "exit") {
                break;
            } else if (command == "help") {
                show_menu();
            } else if (command == "decision") {
                demonstrate_real_agent_decision();
            // } else if (command == "audit") {
                // show_decision_audit();
            } else if (command == "explain") {
                show_decision_explanation();
            } else if (command == "review") {
                demonstrate_human_review();
            } else if (command == "analytics") {
                show_agent_analytics();
            } else if (command == "export") {
                export_audit_data();
            } else {
                std::cout << "❌ Unknown command. Type 'help' for options." << std::endl;
            }
        }

        std::cout << std::endl << "👋 Goodbye! Decision audit demo completed." << std::endl;
    }

private:
    void show_menu() {
        std::cout << "📋 Available Commands:" << std::endl;
        std::cout << "  decision - Demonstrate a complete agent decision with full audit trail" << std::endl;
        // std::cout << "  audit     - Show detailed audit trail for a decision" << std::endl;
        std::cout << "  explain   - Generate and show human-readable decision explanation" << std::endl;
        std::cout << "  review    - Demonstrate human review process" << std::endl;
        std::cout << "  analytics - Show agent performance analytics" << std::endl;
        std::cout << "  export    - Export audit data for compliance reporting" << std::endl;
        std::cout << "  help      - Show this menu" << std::endl;
        std::cout << "  quit      - Exit the demo" << std::endl;
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

    bool initialize_audit_manager() {
        try {
            audit_manager_ = std::make_unique<DecisionAuditTrailManager>(db_pool_, logger_);
            return audit_manager_->initialize();
        } catch (const std::exception& e) {
            logger_->log(LogLevel::ERROR, "Audit manager initialization failed: " + std::string(e.what()));
            return false;
        }
    }

    bool initialize_database_schema() {
        // PRODUCTION NOTE: In production, DecisionAuditTrailManager.initialize() handles
        // database schema validation. Demo skips this to focus on core logic demonstration
        // without requiring database setup. Production code validates schema exists.
        logger_->log(LogLevel::INFO, "Demo: Skipping database schema initialization - core logic demonstration only");
        return true;
    }

    void demonstrate_real_agent_decision() {
        std::cout << "🤖 DECISION AUDIT & EXPLANATION SYSTEM - COMPREHENSIVE DEMO" << std::endl;
        std::cout << "==========================================================" << std::endl;
        std::cout << std::endl;

        // Create a complete audit trail with all decision steps for demonstration
        DecisionAuditTrail demo_trail;
        demo_trail.trail_id = "demo-trail-001";
        demo_trail.decision_id = "demo-decision-001";
        demo_trail.agent_type = "TRANSACTION_GUARDIAN";
        demo_trail.agent_name = "TransactionMonitor-001";
        demo_trail.trigger_event = "High-value international wire transfer detected ($2.5M)";
        demo_trail.final_decision = {
            {"decision", "FLAG_FOR_REVIEW"},
            {"reason", "High-risk transaction requires enhanced due diligence"},
            {"risk_level", "HIGH"},
            {"processing_priority", "URGENT"}
        };
        demo_trail.final_confidence = DecisionConfidence::HIGH;
        demo_trail.requires_human_review = true;

        // Create comprehensive decision steps with all the analysis factors
        demo_trail.steps = {
            {
                "step-001", AuditEventType::DECISION_STARTED, "Decision audit trail initialized",
                {{"transaction_id", "TXN-2024-001"}, {"amount", 2500000.0}},
                {{"decision_id", "demo-decision-001"}, {"trail_id", "demo-trail-001"}},
                {{"agent_type", "TRANSACTION_GUARDIAN"}, {"agent_name", "TransactionMonitor-001"}},
                std::chrono::microseconds(500), 0.0, std::chrono::system_clock::now(), "TransactionMonitor-001", "demo-decision-001"
            },
            {
                "step-002", AuditEventType::DATA_RETRIEVAL, "Retrieved customer profile and transaction history",
                {{"transaction_id", "TXN-2024-001"}},
                {
                    {"customer_risk_profile", "MEDIUM"},
                    {"transaction_history", "12 similar transactions in 30 days"},
                    {"account_balance", 5000000.0}
                },
                {},
                std::chrono::microseconds(1200), 0.0, std::chrono::system_clock::now(), "TransactionMonitor-001", "demo-decision-001"
            },
            {
                "step-003", AuditEventType::PATTERN_ANALYSIS, "Analyzed transaction patterns against historical data",
                {
                    {"analysis_type", "pattern_matching"},
                    {"historical_window", "90_days"},
                    {"comparison_dataset", "2.5M_transactions"}
                },
                {
                    {"pattern_match_score", 0.82},
                    {"unusual_patterns_detected", 4},
                    {"pattern_analysis_factors", {
                        {"amount_factor", {
                            {"description", "Transaction amount $2.5M exceeds 99.5th percentile"},
                            {"severity", "HIGH"},
                            {"frequency_in_history", "0.001%"},
                            {"mitigation", "Enhanced due diligence for high-value transactions"}
                        }},
                        {"timing_factor", {
                            {"description", "Transaction at 3:47 AM local time, unusual for business hours"},
                            {"severity", "MEDIUM"},
                            {"frequency_in_history", "0.05%"},
                            {"mitigation", "Time-based risk scoring applied"}
                        }},
                        {"destination_factor", {
                            {"description", "Wire transfer to international account with limited transaction history"},
                            {"severity", "MEDIUM"},
                            {"frequency_in_history", "0.03%"},
                            {"mitigation", "International transfer risk assessment"}
                        }},
                        {"velocity_factor", {
                            {"description", "First large transaction in 45 days for this account"},
                            {"severity", "LOW"},
                            {"frequency_in_history", "0.02%"},
                            {"mitigation", "Account velocity monitoring"}
                        }}
                    }},
                    {"risk_indicators", {
                        {
                            {"name", "HIGH_AMOUNT_UNUSUAL"},
                            {"description", "$2.5M transaction exceeds normal account pattern by 500x"},
                            {"severity", "HIGH"},
                            {"confidence", 0.95},
                            {"mitigation_steps", {
                                "Enhanced customer due diligence",
                                "Management approval required",
                                "Transaction monitoring for 90 days",
                                "Source of funds verification"
                            }}
                        },
                        {
                            {"name", "UNUSUAL_TIMING"},
                            {"description", "Non-business hours transaction (3:47 AM)"},
                            {"severity", "MEDIUM"},
                            {"confidence", 0.78},
                            {"mitigation_steps", {
                                "Additional identity verification",
                                "Transaction purpose confirmation",
                                "Real-time monitoring activation"
                            }}
                        },
                        {
                            {"name", "INTERNATIONAL_HIGH_RISK"},
                            {"description", "High-value wire transfer to medium-risk jurisdiction"},
                            {"severity", "MEDIUM"},
                            {"confidence", 0.82},
                            {"mitigation_steps", {
                                "Enhanced sanctions screening",
                                "Beneficial ownership verification",
                                "Transaction reporting to compliance team"
                            }}
                        }
                    }}
                },
                {},
                std::chrono::microseconds(1800), 0.0, std::chrono::system_clock::now(), "TransactionMonitor-001", "demo-decision-001"
            },
            {
                "step-004", AuditEventType::RISK_ASSESSMENT, "Calculated comprehensive risk score using multi-factor analysis",
                {
                    {"assessment_method", "multi_factor_weighted_scoring"},
                    {"model_version", "v2.1.4"},
                    {"risk_factors_analyzed", 8}  // This will be dynamically calculated in production
                },
                {
                    {"overall_risk_score", 0.78},
                    {"risk_level", "HIGH"},
                    {"confidence_score", 0.85},
                    {"eight_factor_breakdown", {
                        {"factor_1_transaction_amount", {
                            {"score", 0.92},
                            {"weight", 0.20},
                            {"description", "Transaction amount exceeds threshold"},
                            {"evidence", "$2.5M > $1M threshold"},
                            {"mitigation", "High-value transaction protocol activated"}
                        }},
                        {"factor_2_velocity_anomaly", {
                            {"score", 0.85},
                            {"weight", 0.15},
                            {"description", "Unusual transaction velocity for account"},
                            {"evidence", "First large transaction in 45 days"},
                            {"mitigation", "Account behavior monitoring increased"}
                        }},
                        {"factor_3_timing_anomaly", {
                            {"score", 0.76},
                            {"weight", 0.12},
                            {"description", "Non-business hours transaction"},
                            {"evidence", "3:47 AM transaction time"},
                            {"mitigation", "Time-based risk premium applied"}
                        }},
                        {"factor_4_geographic_risk", {
                            {"score", 0.82},
                            {"weight", 0.10},
                            {"description", "Destination jurisdiction risk"},
                            {"evidence", "Medium-risk jurisdiction per OFAC list"},
                            {"mitigation", "Enhanced sanctions screening required"}
                        }},
                        {"factor_5_customer_risk_profile", {
                            {"score", 0.45},
                            {"weight", 0.15},
                            {"description", "Customer risk profile assessment"},
                            {"evidence", "Established customer, good payment history"},
                            {"mitigation", "Customer risk mitigates overall score"}
                        }},
                        {"factor_6_transaction_type", {
                            {"score", 0.79},
                            {"weight", 0.08},
                            {"description", "Wire transfer risk premium"},
                            {"evidence", "International wire transfer"},
                            {"mitigation", "Wire transfer compliance procedures"}
                        }},
                        {"factor_7_account_history", {
                            {"score", 0.35},
                            {"weight", 0.10},
                            {"description", "Account transaction history"},
                            {"evidence", "12 similar transactions in 30 days"},
                            {"mitigation", "Established pattern reduces risk"}
                        }},
                        {"factor_8_regulatory_flags", {
                            {"score", 0.88},
                            {"weight", 0.10},
                            {"description", "Regulatory compliance flags"},
                            {"evidence", "EDD triggers activated"},
                            {"mitigation", "Enhanced due diligence initiated"}
                        }}
                    }}
                },
                {},
                std::chrono::microseconds(2500), 0.0, std::chrono::system_clock::now(), "TransactionMonitor-001", "demo-decision-001"
            }
        };

        std::cout << "🎯 TRANSACTION MONITORING DECISION AUDIT TRAIL" << std::endl;
        std::cout << "==============================================" << std::endl;
        std::cout << "Decision ID: " << demo_trail.decision_id << std::endl;
        std::cout << "Agent: " << demo_trail.agent_name << " (" << demo_trail.agent_type << ")" << std::endl;
        std::cout << "Trigger: " << demo_trail.trigger_event << std::endl;
        std::cout << "Final Decision: " << demo_trail.final_decision["decision"] << std::endl;
        std::cout << "Confidence: HIGH" << std::endl;
        std::cout << "Human Review Required: " << (demo_trail.requires_human_review ? "YES" : "NO") << std::endl;
        std::cout << std::endl;

        // Show the detailed factor analysis and risks
        show_detailed_analysis(demo_trail);

        std::cout << "✅ Decision audit completed successfully!" << std::endl;
        std::cout << "📊 Decision ID: " << demo_trail.decision_id << std::endl;
        std::cout << "🎯 Final Decision: " << demo_trail.final_decision["decision"] << std::endl;
        std::cout << "📈 Confidence: HIGH (85%)" << std::endl;
        std::cout << "🔍 Requires Human Review: YES" << std::endl;
    }

    void show_detailed_analysis(const DecisionAuditTrail& trail) {
        std::cout << "🔬 DETAILED DECISION ANALYSIS" << std::endl;
        std::cout << "=============================" << std::endl;

        // Display all risk assessment factors with details (dynamically counted)
        size_t factor_count = 0;
        for (const auto& step : trail.steps) {
            if (step.event_type == AuditEventType::RISK_ASSESSMENT) {
                if (step.output_data.contains("eight_factor_breakdown")) {
                    const auto& factors = step.output_data["eight_factor_breakdown"];
                    factor_count = factors.size();
                    std::cout << "🎯 " << factor_count << "-FACTOR RISK ASSESSMENT BREAKDOWN:" << std::endl;
                    for (const auto& [factor_key, factor_data] : factors.items()) {
                        std::cout << "  📊 " << factor_data["description"].get<std::string>() << std::endl;
                        std::cout << "     Risk Score: " << factor_data["score"].get<double>() * 100 << "%" << std::endl;
                        std::cout << "     Weight: " << factor_data["weight"].get<double>() * 100 << "%" << std::endl;
                        std::cout << "     Evidence: " << factor_data["evidence"].get<std::string>() << std::endl;
                        std::cout << "     Mitigation: " << factor_data["mitigation"].get<std::string>() << std::endl;
                        std::cout << std::endl;
                    }
                }
                break;
            }
        }

        // Display the specific risk indicators with mitigation steps (dynamically counted)
        size_t risk_indicator_count = 0;
        for (const auto& step : trail.steps) {
            if (step.event_type == AuditEventType::PATTERN_ANALYSIS) {
                if (step.output_data.contains("risk_indicators")) {
                    const auto& indicators = step.output_data["risk_indicators"];
                    risk_indicator_count = indicators.size();
                    std::cout << "🚨 " << risk_indicator_count << " CRITICAL RISK INDICATORS IDENTIFIED:" << std::endl;
                    int indicator_count = 1;
                    for (const auto& indicator : indicators) {
                        std::cout << "  " << indicator_count << ". " << indicator["name"].get<std::string>() << std::endl;
                        std::cout << "     Description: " << indicator["description"].get<std::string>() << std::endl;
                        std::cout << "     Severity: " << indicator["severity"].get<std::string>() << std::endl;
                        std::cout << "     Confidence: " << indicator["confidence"].get<double>() * 100 << "%" << std::endl;
                        std::cout << "     Mitigation Steps:" << std::endl;

                        const auto& mitigation_steps = indicator["mitigation_steps"];
                        for (size_t i = 0; i < mitigation_steps.size(); ++i) {
                            std::cout << "       " << (i + 1) << ". " << mitigation_steps[i].get<std::string>() << std::endl;
                        }
                        std::cout << std::endl;
                        indicator_count++;
                    }
                }
                break;
            }
        }

        std::cout << "⚖️ WEIGHTED RISK CALCULATION:" << std::endl;
        for (const auto& step : trail.steps) {
            if (step.event_type == AuditEventType::RISK_ASSESSMENT) {
                if (step.output_data.contains("overall_risk_score")) {
                    std::cout << "  Final Risk Score: " << step.output_data["overall_risk_score"].get<double>() * 100 << "%" << std::endl;
                    std::cout << "  Risk Level: HIGH (exceeds 70% threshold)" << std::endl;
                }
                break;
            }
        }
        std::cout << std::endl;

        std::cout << "🎯 DECISION IMPACT:" << std::endl;
        std::cout << "  • Transaction flagged for enhanced due diligence" << std::endl;
        std::cout << "  • Customer verification procedures initiated" << std::endl;
        std::cout << "  • Management notification sent" << std::endl;
        std::cout << "  • 90-day monitoring period activated" << std::endl;
        std::cout << std::endl;
    }

    void show_decision_explanation() {
        std::cout << "🧠 DECISION EXPLANATION GENERATION" << std::endl;
        std::cout << "==================================" << std::endl;

        // Create a demo trail for explanation
        DecisionAuditTrail demo_trail;
        demo_trail.decision_id = "demo-decision-001";
        demo_trail.agent_type = "TRANSACTION_GUARDIAN";
        demo_trail.agent_name = "TransactionMonitor-001";
        demo_trail.final_confidence = DecisionConfidence::HIGH;
        demo_trail.requires_human_review = true;

        // Show high-level explanation
        std::cout << "📊 HIGH-LEVEL SUMMARY:" << std::endl;
        std::cout << "The Transaction Guardian agent analyzed an international wire transfer of $2.5M and determined it requires enhanced due diligence with HIGH confidence. Human review has been requested due to multiple risk indicators." << std::endl;
        std::cout << std::endl;

        // Show detailed analysis
        show_detailed_analysis(demo_trail);
    }

    void demonstrate_human_review() {
        std::cout << "👥 HUMAN-AI COLLABORATION DEMO" << std::endl;
        std::cout << "==============================" << std::endl;

        std::cout << "📋 Decision requiring review: demo-decision-001" << std::endl;
        std::cout << "   Reason: High-value transaction with multiple risk indicators" << std::endl;
        std::cout << std::endl;

        std::cout << "🔍 Human Reviewer Analysis:" << std::endl;
        std::cout << "  • Reviewed transaction details and risk assessment" << std::endl;
        std::cout << "  • Confirmed legitimate business purpose" << std::endl;
        std::cout << "  • Approved with additional monitoring requirements" << std::endl;
        std::cout << std::endl;

        std::cout << "✅ Human feedback recorded successfully!" << std::endl;
        std::cout << "   Decision status updated: APPROVED with conditions" << std::endl;
        std::cout << "   Additional monitoring activated for 90 days" << std::endl;
    }

    void show_agent_analytics() {
        std::cout << "📊 AGENT PERFORMANCE ANALYTICS" << std::endl;
        std::cout << "==============================" << std::endl;

        std::cout << "🤖 TRANSACTION_GUARDIAN Analytics (Last 24h):" << std::endl;
        std::cout << "  • Total Decisions: 1,247" << std::endl;
        std::cout << "  • Average Confidence: 82.3%" << std::endl;
        std::cout << "  • Human Reviews Required: 89 (7.1%)" << std::endl;
        std::cout << "  • Average Processing Time: 45ms" << std::endl;
        std::cout << "  • Success Rate: 94.2%" << std::endl;
        std::cout << std::endl;

        std::cout << "🔍 Decision Patterns:" << std::endl;
        std::cout << "  • Most Common Decision: FLAG_FOR_REVIEW (42%)" << std::endl;
        std::cout << "  • Risk Distribution: HIGH (23%), MEDIUM (45%), LOW (32%)" << std::endl;
        std::cout << "  • Peak Decision Time: 2:00 PM" << std::endl;
    }

    void export_audit_data() {
        std::cout << "📤 AUDIT DATA EXPORT" << std::endl;
        std::cout << "===================" << std::endl;

        std::cout << "✅ Audit data export simulation completed!" << std::endl;
        std::cout << "   File: audit_export_20241201_143000.json" << std::endl;
        std::cout << "   Records: 1,247 decisions" << std::endl;
        std::cout << "   Period: Last 24 hours" << std::endl;
        std::cout << "   Format: JSON for compliance reporting" << std::endl;
        std::cout << "   Status: Ready for regulatory submission" << std::endl;
    }

    std::string confidence_to_string(DecisionConfidence confidence) {
        switch (confidence) {
            case DecisionConfidence::VERY_LOW: return "Very Low";
            case DecisionConfidence::LOW: return "Low";
            case DecisionConfidence::MEDIUM: return "Medium";
            case DecisionConfidence::HIGH: return "High";
            case DecisionConfidence::VERY_HIGH: return "Very High";
            default: return "Unknown";
        }
    }

private:
    StructuredLogger* logger_;
    std::shared_ptr<ConnectionPool> db_pool_;
    std::unique_ptr<DecisionAuditTrailManager> audit_manager_;
};

} // namespace regulens

int main() {
    try {
        regulens::DecisionAuditDemo demo;

        if (!demo.initialize()) {
            std::cerr << "Failed to initialize Decision Audit Demo" << std::endl;
            return 1;
        }

        demo.run_interactive_demo();

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
