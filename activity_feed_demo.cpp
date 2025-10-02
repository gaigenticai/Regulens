#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

#include "shared/config/configuration_manager.hpp"
#include "shared/logging/structured_logger.hpp"
#include "shared/agent_activity_feed.hpp"

using namespace regulens;

/**
 * @brief Demo program for agent activity feed
 *
 * Demonstrates the real-time agent activity feed system by creating
 * and recording various agent activities.
 */
int main() {
    std::cout << "🔄 Regulens Agent Activity Feed Demo\n";
    std::cout << "=====================================\n\n";

    // Initialize configuration and logging using singletons
    auto& config_manager = ConfigurationManager::get_instance();
    config_manager.initialize(0, nullptr);

    auto& structured_logger = StructuredLogger::get_instance();

    // Create activity feed
    auto config_ptr = std::shared_ptr<ConfigurationManager>(&config_manager, [](ConfigurationManager*){});
    auto logger_ptr = std::shared_ptr<StructuredLogger>(&structured_logger, [](StructuredLogger*){});
    AgentActivityFeed activity_feed(config_ptr, logger_ptr);

    if (!activity_feed.initialize()) {
        std::cerr << "❌ Failed to initialize activity feed\n";
        return 1;
    }

    std::cout << "✅ Agent activity feed initialized\n\n";

    // Subscribe to activities for demonstration
    std::string subscription_id = activity_feed.subscribe(
        ActivityFeedSubscription("demo_sub_001", "demo_client", ActivityFeedFilter()),
        [](const AgentActivityEvent& event) {
            std::cout << "📡 [SUBSCRIBED] " << event.title << " - " << event.description << "\n";
        }
    );

    std::cout << "✅ Subscribed to activity feed (ID: " << subscription_id << ")\n\n";

    // Demo: Simulate agent activities
    std::cout << "🎯 Simulating agent activities...\n\n";

    // Agent startup
    activity_feed.record_activity(activity_events::agent_started("fraud_detector_001", "fraud_detection"));
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    activity_feed.record_activity(activity_events::agent_started("compliance_checker_001", "compliance"));
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Event reception
    activity_feed.record_activity(activity_events::event_received("fraud_detector_001", "txn_12345", "transaction"));
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    activity_feed.record_activity(activity_events::event_received("compliance_checker_001", "txn_12345", "transaction"));
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Task processing
    activity_feed.record_activity(activity_events::task_started("fraud_detector_001", "risk_assessment_001", "txn_12345"));
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    activity_feed.record_activity(activity_events::task_started("compliance_checker_001", "compliance_check_001", "txn_12345"));
    std::this_thread::sleep_for(std::chrono::milliseconds(800));

    // Create sample decision
    AgentDecision decision(DecisionType::APPROVE, ConfidenceLevel::HIGH,
                          "fraud_detector_001", "txn_12345");

    decision.add_reasoning({
        "amount_check", "Transaction amount is within normal limits",
        0.9, "fraud_engine"
    });

    decision.add_reasoning({
        "velocity_check", "Transaction velocity is within acceptable range",
        0.85, "behavior_analysis"
    });

    decision.add_action({
        "approve_transaction",
        "Approve the transaction and update customer balance",
        Priority::NORMAL,
        std::chrono::system_clock::now() + std::chrono::hours(1),
        {{"transaction_id", "txn_12345"}, {"amount", "1250.00"}}
    });

    // Record decision
    activity_feed.record_activity(activity_events::decision_made("fraud_detector_001", decision));
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Task completion
    activity_feed.record_activity(activity_events::task_completed("fraud_detector_001", "risk_assessment_001",
        std::chrono::milliseconds(1200)));
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Compliance check decision
    AgentDecision compliance_decision(DecisionType::MONITOR, ConfidenceLevel::MEDIUM,
                                     "compliance_checker_001", "txn_12345");

    compliance_decision.add_reasoning({
        "sanctions_check", "No sanctions screening alerts",
        0.95, "sanctions_database"
    });

    compliance_decision.add_reasoning({
        "pep_check", "Customer is not a politically exposed person",
        0.9, "pep_screening"
    });

    compliance_decision.add_action({
        "monitor_transaction",
        "Add transaction to enhanced monitoring queue",
        Priority::LOW,
        std::chrono::system_clock::now() + std::chrono::hours(24),
        {{"transaction_id", "txn_12345"}, {"monitoring_level", "enhanced"}}
    });

    activity_feed.record_activity(activity_events::decision_made("compliance_checker_001", compliance_decision));
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    activity_feed.record_activity(activity_events::task_completed("compliance_checker_001", "compliance_check_001",
        std::chrono::milliseconds(950)));
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Simulate some error conditions
    activity_feed.record_activity(activity_events::agent_error("fraud_detector_001",
        "Temporary database connection timeout"));
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Agent health changes
    activity_feed.record_activity(AgentActivityEvent("fraud_detector_001", AgentActivityType::STATE_CHANGED,
        ActivitySeverity::WARNING, "Health Degraded", "Response time increased to 2.1 seconds"));
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Final agent shutdown
    activity_feed.record_activity(activity_events::agent_stopped("fraud_detector_001"));
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    activity_feed.record_activity(activity_events::agent_stopped("compliance_checker_001"));

    std::cout << "\n📊 Querying and displaying activities...\n\n";

    // Query all activities
    ActivityFeedFilter all_filter;
    all_filter.max_results = 20; // Show last 20 activities
    auto activities = activity_feed.query_activities(all_filter);

    std::cout << "📋 Recent Activities (" << activities.size() << " total):\n";
    std::cout << std::string(80, '=') << "\n";

    for (const auto& activity : activities) {
        std::string severity_str;
        switch (activity.severity) {
            case ActivitySeverity::INFO: severity_str = "ℹ️ "; break;
            case ActivitySeverity::WARNING: severity_str = "⚠️ "; break;
            case ActivitySeverity::ERROR: severity_str = "❌"; break;
            case ActivitySeverity::CRITICAL: severity_str = "🚨"; break;
        }

        std::cout << severity_str << " [" << activity.agent_id << "] "
                  << activity.title << "\n";
        std::cout << "    " << activity.description << "\n\n";
    }

    // Show statistics
    std::cout << "📈 Activity Statistics:\n";
    std::cout << std::string(30, '=') << "\n";

    auto feed_stats = activity_feed.get_feed_stats();
    std::cout << "Total Events: " << feed_stats["total_events"] << "\n";
    std::cout << "Active Agents: " << feed_stats["total_agents"] << "\n";
    std::cout << "Active Subscriptions: " << feed_stats["total_subscriptions"] << "\n";

    // Agent-specific stats
    auto fraud_stats = activity_feed.get_agent_stats("fraud_detector_001");
    if (fraud_stats) {
        std::cout << "\n🤖 Fraud Detector Stats:\n";
        std::cout << "Total Activities: " << fraud_stats->total_activities << "\n";
        std::cout << "Error Count: " << fraud_stats->error_count << "\n";
        std::cout << "Warning Count: " << fraud_stats->warning_count << "\n";
    }

    auto compliance_stats = activity_feed.get_agent_stats("compliance_checker_001");
    if (compliance_stats) {
        std::cout << "\n🔍 Compliance Checker Stats:\n";
        std::cout << "Total Activities: " << compliance_stats->total_activities << "\n";
        std::cout << "Error Count: " << compliance_stats->error_count << "\n";
        std::cout << "Warning Count: " << compliance_stats->warning_count << "\n";
    }

    // Export demonstration
    std::cout << "\n💾 Exporting activities...\n";
    std::string csv_export = activity_feed.export_activities(ActivityFeedFilter(), "csv");
    std::cout << "✅ Exported " << std::count(csv_export.begin(), csv_export.end(), '\n')
              << " activities to CSV format\n";

    // Cleanup
    activity_feed.unsubscribe(subscription_id);
    activity_feed.shutdown();

    std::cout << "\n🎯 Agent Activity Feed Demo Complete!\n";
    std::cout << "=====================================\n";
    std::cout << "The activity feed system provides:\n";
    std::cout << "• Real-time activity collection and storage\n";
    std::cout << "• Subscription-based real-time notifications\n";
    std::cout << "• Comprehensive querying and filtering\n";
    std::cout << "• Statistical analysis and reporting\n";
    std::cout << "• Export capabilities for analysis\n";
    std::cout << "• Web UI integration for monitoring\n\n";

    std::cout << "This enables comprehensive observability into\n";
    std::cout << "agent behavior and decision-making processes.\n";

    return 0;
}
