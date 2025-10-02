#include <iostream>
#include <memory>

#include "shared/config/configuration_manager.hpp"
#include "shared/logging/structured_logger.hpp"
#include "shared/visualization/decision_tree_visualizer.hpp"
#include "shared/models/agent_decision.hpp"

using namespace regulens;

/**
 * @brief Demo program for decision tree visualization
 *
 * Demonstrates the visual decision tree functionality by creating
 * sample agent decisions and generating visual representations.
 */
int main() {
    std::cout << "🌳 Regulens Decision Tree Visualization Demo\n";
    std::cout << "============================================\n\n";

    // Initialize configuration and logging using singletons
    auto& config_manager = ConfigurationManager::get_instance();
    config_manager.initialize(0, nullptr);

    auto& structured_logger = StructuredLogger::get_instance();

    // Create decision tree visualizer using singleton references
    auto config_ptr = std::shared_ptr<ConfigurationManager>(&config_manager, [](ConfigurationManager*){});
    auto logger_ptr = std::shared_ptr<StructuredLogger>(&structured_logger, [](StructuredLogger*){});
    DecisionTreeVisualizer visualizer(config_ptr, logger_ptr);

    std::cout << "✅ Decision tree visualizer initialized\n";

    // Create sample agent decision
    AgentDecision decision(DecisionType::APPROVE, ConfidenceLevel::HIGH,
                          "fraud_detection_agent", "transaction_12345");

    // Add reasoning components
    decision.add_reasoning({
        "transaction_amount", "Amount ($1,250) is within normal range for customer",
        0.9, "fraud_detection_engine"
    });

    decision.add_reasoning({
        "customer_history", "Customer has 5+ years of good transaction history",
        0.95, "customer_database"
    });

    decision.add_reasoning({
        "location_check", "Transaction location matches customer's registered address",
        0.85, "geolocation_service"
    });

    // Add recommended actions
    decision.add_action({
        "approve_transaction",
        "Approve the transaction and update customer balance",
        Priority::NORMAL,
        std::chrono::system_clock::now() + std::chrono::hours(1),
        {{"transaction_id", "TXN_12345"}, {"amount", "1250.00"}}
    });

    decision.add_action({
        "send_notification",
        "Send approval confirmation to customer",
        Priority::LOW,
        std::chrono::system_clock::now() + std::chrono::minutes(5),
        {{"customer_email", "customer@example.com"}}
    });

    // Add risk assessment
    RiskAssessment risk;
    risk.risk_score = 0.15; // Low risk
    risk.risk_level = "low";
    risk.risk_factors = {"Amount within limits", "Good customer history"};
    risk.assessment_time = std::chrono::system_clock::now();
    decision.set_risk_assessment(risk);

    std::cout << "✅ Sample agent decision created with " << decision.get_reasoning().size()
              << " reasoning factors and " << decision.get_actions().size() << " actions\n";

    // Build decision tree
    DecisionTree tree = visualizer.build_decision_tree(decision);

    std::cout << "✅ Decision tree built with " << tree.nodes.size() << " nodes and "
              << tree.edges.size() << " edges\n";

    // Generate different visualizations
    std::cout << "\n📊 Generating visualizations...\n";

    // JSON visualization
    std::string json_viz = visualizer.generate_visualization(tree, VisualizationFormat::JSON);
    std::cout << "✅ JSON visualization generated (" << json_viz.length() << " characters)\n";

    // DOT visualization (for GraphViz)
    std::string dot_viz = visualizer.generate_visualization(tree, VisualizationFormat::DOT);
    std::cout << "✅ DOT visualization generated (" << dot_viz.length() << " characters)\n";

    // Interactive HTML visualization
    std::string html_viz = visualizer.generate_visualization(tree, VisualizationFormat::HTML);
    std::cout << "✅ Interactive HTML visualization generated (" << html_viz.length() << " characters)\n";

    // SVG visualization
    std::string svg_viz = visualizer.generate_visualization(tree, VisualizationFormat::SVG);
    std::cout << "✅ SVG visualization generated (" << svg_viz.length() << " characters)\n";

    // Get tree statistics
    nlohmann::json stats = visualizer.get_tree_statistics(tree);
    std::cout << "✅ Tree statistics calculated\n";

    // Validate tree
    bool is_valid = visualizer.validate_tree(tree);
    std::cout << "✅ Tree validation: " << (is_valid ? "PASSED" : "FAILED") << "\n";

    // Display summary
    std::cout << "\n📈 Decision Tree Summary:\n";
    std::cout << "========================\n";
    std::cout << "Tree ID: " << tree.tree_id << "\n";
    std::cout << "Agent ID: " << tree.agent_id << "\n";
    std::cout << "Decision: " << decision_type_to_string(decision.get_type()) << "\n";
    std::cout << "Confidence: " << confidence_to_string(decision.get_confidence()) << "\n";
    std::cout << "Total Nodes: " << stats["total_nodes"] << "\n";
    std::cout << "Total Edges: " << stats["total_edges"] << "\n";
    std::cout << "Tree Depth: " << stats["tree_depth"] << "\n";
    std::cout << "Valid Tree: " << (stats["is_valid"].get<bool>() ? "Yes" : "No") << "\n";

    std::cout << "\n🎯 Decision Tree Visualization Demo Complete!\n";
    std::cout << "=============================================\n";
    std::cout << "The decision tree visualizer can generate:\n";
    std::cout << "• Interactive HTML dashboards for web UI\n";
    std::cout << "• SVG graphics for reports\n";
    std::cout << "• JSON data for API integration\n";
    std::cout << "• DOT format for GraphViz rendering\n\n";

    std::cout << "This enables explainable AI by showing exactly how\n";
    std::cout << "agents reach their decisions through visual decision trees.\n";

    return 0;
}
