#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>

#include "agent_decision.hpp"

namespace regulens {


#include "decision_tree_types.hpp"

/**
 * @brief Decision tree node
 */
struct DecisionTreeNode {
    std::string node_id;
    regulens::DecisionNodeType node_type;
    std::string label;
    std::string description;
    double weight = 1.0;  // Importance/relevance weight
    std::string data_type; // "boolean", "numeric", "categorical", "text"

    // Node relationships
    std::vector<std::string> parent_ids;
    std::vector<std::string> child_ids;

    // Node metadata
    std::unordered_map<std::string, std::string> metadata;
    std::chrono::system_clock::time_point timestamp;

    DecisionTreeNode(std::string id, regulens::DecisionNodeType type, std::string lbl,
                                        std::string desc = "", double w = 1.0)
                : node_id(std::move(id)), node_type(type), label(std::move(lbl)),
                    description(std::move(desc)), weight(w),
                    timestamp(std::chrono::system_clock::now()) {}

    nlohmann::json to_json() const {
        nlohmann::json metadata_json;
        for (const auto& [key, value] : metadata) {
            metadata_json[key] = value;
        }

        return {
            {"node_id", node_id},
            {"node_type", static_cast<int>(node_type)},
            {"label", label},
            {"description", description},
            {"weight", weight},
            {"data_type", data_type},
            {"parent_ids", parent_ids},
            {"child_ids", child_ids},
            {"metadata", metadata_json},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                timestamp.time_since_epoch()).count()}
        };
    }
};

/**
 * @brief Decision tree edge representing flow between nodes
 */
struct DecisionTreeEdge {
    std::string edge_id;
    std::string source_node_id;
    std::string target_node_id;
    std::string label;  // Edge label (e.g., "true", "false", "weight: 0.8")
    std::string edge_type; // "condition_true", "condition_false", "factor", "evidence"
    double weight = 1.0;

    nlohmann::json to_json() const {
        return {
            {"edge_id", edge_id},
            {"source_node_id", source_node_id},
            {"target_node_id", target_node_id},
            {"label", label},
            {"edge_type", edge_type},
            {"weight", weight}
        };
    }
};

/**
 * @brief Complete decision tree representation
 */
struct DecisionTree {
    std::string tree_id;
    std::string agent_id;
    std::string decision_id;
    std::string root_node_id;
    std::vector<DecisionTreeNode> nodes;
    std::vector<DecisionTreeEdge> edges;
    std::unordered_map<std::string, std::string> metadata;

    nlohmann::json to_json() const {
        nlohmann::json nodes_json = nlohmann::json::array();
        for (const auto& node : nodes) {
            nodes_json.push_back(node.to_json());
        }

        nlohmann::json edges_json = nlohmann::json::array();
        for (const auto& edge : edges) {
            edges_json.push_back(edge.to_json());
        }

        nlohmann::json metadata_json;
        for (const auto& [key, value] : metadata) {
            metadata_json[key] = value;
        }

        return {
            {"tree_id", tree_id},
            {"agent_id", agent_id},
            {"decision_id", decision_id},
            {"root_node_id", root_node_id},
            {"nodes", nodes_json},
            {"edges", edges_json},
            {"metadata", metadata_json}
        };
    }
};

/**
 * @brief Visual styling for decision tree rendering
 */
struct DecisionTreeStyle {
    // Node styling
    std::string root_color = "#4CAF50";      // Green
    std::string condition_color = "#2196F3"; // Blue
    std::string action_color = "#FF9800";    // Orange
    std::string factor_color = "#9C27B0";    // Purple
    std::string evidence_color = "#607D8B";  // Blue Grey
    std::string outcome_color = "#F44336";   // Red

    // Layout settings
    int node_width = 120;
    int node_height = 60;
    int horizontal_spacing = 150;
    int vertical_spacing = 100;

    // Font settings
    std::string font_family = "Arial, sans-serif";
    int font_size = 12;

    nlohmann::json to_json() const {
        return {
            {"root_color", root_color},
            {"condition_color", condition_color},
            {"action_color", action_color},
            {"factor_color", factor_color},
            {"evidence_color", evidence_color},
            {"outcome_color", outcome_color},
            {"node_width", node_width},
            {"node_height", node_height},
            {"horizontal_spacing", horizontal_spacing},
            {"vertical_spacing", vertical_spacing},
            {"font_family", font_family},
            {"font_size", font_size}
        };
    }
};

} // namespace regulens
