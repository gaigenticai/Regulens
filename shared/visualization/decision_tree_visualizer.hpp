#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <nlohmann/json.hpp>

#include "../models/decision_tree.hpp"
#include "../models/agent_decision.hpp"
#include "../logging/structured_logger.hpp"
#include "../config/configuration_manager.hpp"

namespace regulens {

using regulens::DecisionNodeType;

/**
 * @brief Output formats for decision tree visualization
 */
enum class VisualizationFormat {
    JSON,       // JSON representation for web rendering
    SVG,        // Scalable Vector Graphics
    DOT,        // GraphViz DOT format
    HTML        // Interactive HTML with JavaScript
};

/**
 * @brief Layout algorithms for tree positioning
 */
enum class LayoutAlgorithm {
    HIERARCHICAL,   // Top-down hierarchical layout
    RADIAL,        // Radial/circular layout
    FORCE_DIRECTED // Force-directed layout
};

/**
 * @brief Decision tree visualizer
 *
 * Converts agent decisions into visual decision trees that can be rendered
 * in web interfaces to show the reasoning process behind agent decisions.
 */
class DecisionTreeVisualizer {
public:
    DecisionTreeVisualizer(std::shared_ptr<ConfigurationManager> config,
                          std::shared_ptr<StructuredLogger> logger);

    ~DecisionTreeVisualizer();

    /**
     * @brief Build decision tree from agent decision
     * @param decision Agent decision to visualize
     * @return Decision tree structure
     */
    DecisionTree build_decision_tree(const AgentDecision& decision);

    /**
     * @brief Generate visual representation of decision tree
     * @param tree Decision tree to visualize
     * @param format Output format
     * @param style Visual styling options
     * @return String representation in requested format
     */
    std::string generate_visualization(const DecisionTree& tree,
                                     VisualizationFormat format,
                                     const DecisionTreeStyle& style = DecisionTreeStyle());

    /**
     * @brief Generate interactive HTML visualization
     * @param tree Decision tree to visualize
     * @param style Visual styling options
     * @return Complete HTML page with interactive visualization
     */
    std::string generate_interactive_html(const DecisionTree& tree,
                                        const DecisionTreeStyle& style = DecisionTreeStyle());

    /**
     * @brief Export decision tree for web UI consumption
     * @param tree Decision tree to export
     * @return JSON suitable for web UI rendering
     */
    nlohmann::json export_for_web_ui(const DecisionTree& tree);

    /**
     * @brief Validate decision tree structure
     * @param tree Tree to validate
     * @return true if tree structure is valid
     */
    bool validate_tree(const DecisionTree& tree) const;

    /**
     * @brief Get tree statistics
     * @param tree Decision tree
     * @return JSON with tree statistics
     */
    nlohmann::json get_tree_statistics(const DecisionTree& tree) const;

private:
    std::shared_ptr<ConfigurationManager> config_;
    std::shared_ptr<StructuredLogger> logger_;

    /**
     * @brief Calculate the depth of a decision tree
     * @param tree Decision tree
     * @return Maximum depth of the tree
     */
    size_t calculate_tree_depth(const DecisionTree& tree) const;

    // Tree building helpers
    DecisionTreeNode create_root_node(const AgentDecision& decision);
    std::vector<DecisionTreeNode> create_reasoning_nodes(const AgentDecision& decision);
    std::vector<DecisionTreeNode> create_action_nodes(const AgentDecision& decision);
    std::vector<DecisionTreeNode> create_outcome_node(const AgentDecision& decision);
    std::vector<DecisionTreeEdge> create_tree_edges(const std::vector<DecisionTreeNode>& nodes);

    std::string generate_node_id(const std::string& prefix, size_t index);
    std::string generate_edge_id(const std::string& source_id, const std::string& target_id);

    // Visualization helpers
    std::string generate_svg_visualization(const DecisionTree& tree, const DecisionTreeStyle& style);
    std::string generate_dot_visualization(const DecisionTree& tree);
    std::string generate_json_visualization(const DecisionTree& tree);

    // Layout calculation
    std::unordered_map<std::string, std::pair<int, int>> calculate_layout(
        const DecisionTree& tree, LayoutAlgorithm algorithm);

    std::unordered_map<std::string, std::pair<int, int>> calculate_hierarchical_layout(
        const DecisionTree& tree);

    // HTML generation
    std::string generate_html_template() const;
    std::string generate_javascript_code() const;
    std::string generate_css_styles(const DecisionTreeStyle& style) const;

    // Node type helpers
    std::string get_node_color(DecisionNodeType type, const DecisionTreeStyle& style) const;
    std::string get_node_shape(DecisionNodeType type) const;
    std::string get_node_type_label(DecisionNodeType type) const;

    // Validation helpers
    bool has_valid_root(const DecisionTree& tree) const;
    bool has_connected_nodes(const DecisionTree& tree) const;
    bool has_no_cycles(const DecisionTree& tree) const;
};

} // namespace regulens
