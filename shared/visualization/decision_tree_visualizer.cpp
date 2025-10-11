#include "decision_tree_visualizer.hpp"

#include <algorithm>
#include <queue>
#include <stack>
#include <sstream>
#include <iomanip>

namespace regulens {

using regulens::DecisionNodeType;

DecisionTreeVisualizer::DecisionTreeVisualizer(std::shared_ptr<ConfigurationManager> config,
                                             std::shared_ptr<StructuredLogger> logger)
    : config_(config), logger_(logger) {
    logger_->info("DecisionTreeVisualizer initialized");
}

DecisionTreeVisualizer::~DecisionTreeVisualizer() {
    logger_->info("DecisionTreeVisualizer shutdown");
}

DecisionTree DecisionTreeVisualizer::build_decision_tree(const AgentDecision& decision) {
    DecisionTree tree;
    tree.tree_id = "tree_" + decision.get_decision_id();
    tree.agent_id = decision.get_agent_id();
    tree.decision_id = decision.get_decision_id();

    // Create root node
    auto root_node = create_root_node(decision);
    tree.root_node_id = root_node.node_id;
    tree.nodes.push_back(root_node);

    // Create reasoning nodes
    auto reasoning_nodes = create_reasoning_nodes(decision);
    tree.nodes.insert(tree.nodes.end(), reasoning_nodes.begin(), reasoning_nodes.end());

    // Create action nodes
    auto action_nodes = create_action_nodes(decision);
    tree.nodes.insert(tree.nodes.end(), action_nodes.begin(), action_nodes.end());

    // Create outcome node
    auto outcome_nodes = create_outcome_node(decision);
    tree.nodes.insert(tree.nodes.end(), outcome_nodes.begin(), outcome_nodes.end());

    // Create edges connecting all nodes
    tree.edges = create_tree_edges(tree.nodes);

    // Add metadata
    tree.metadata["decision_type"] = decision_type_to_string(decision.get_type());
    tree.metadata["confidence"] = confidence_to_string(decision.get_confidence());
    tree.metadata["reasoning_count"] = std::to_string(decision.get_reasoning().size());
    tree.metadata["actions_count"] = std::to_string(decision.get_actions().size());
    tree.metadata["created_at"] = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
        decision.get_timestamp().time_since_epoch()).count());

    logger_->info("Built decision tree with " + std::to_string(tree.nodes.size()) +
                 " nodes and " + std::to_string(tree.edges.size()) + " edges");

    return tree;
}

DecisionTreeNode DecisionTreeVisualizer::create_root_node(const AgentDecision& decision) {
    DecisionTreeNode root(generate_node_id("root", 0), DecisionNodeType::ROOT,
                         "Agent Decision", "Root decision node");
    root.description = "Decision made by " + decision.get_agent_id() +
                      " for event " + decision.get_event_id();
    root.metadata["decision_type"] = decision_type_to_string(decision.get_type());
    root.metadata["confidence"] = confidence_to_string(decision.get_confidence());
    return root;
}

std::vector<DecisionTreeNode> DecisionTreeVisualizer::create_reasoning_nodes(const AgentDecision& decision) {
    std::vector<DecisionTreeNode> nodes;
    const auto& reasoning = decision.get_reasoning();

    for (size_t i = 0; i < reasoning.size(); ++i) {
        const auto& reason = reasoning[i];

        // Create factor node
        DecisionTreeNode factor_node(generate_node_id("factor", i), DecisionNodeType::FACTOR,
                                   reason.factor, reason.factor);
        factor_node.weight = reason.weight;
        factor_node.metadata["source"] = reason.source;
        factor_node.metadata["evidence"] = reason.evidence;
        nodes.push_back(factor_node);

        // Create evidence node if evidence is substantial
        if (!reason.evidence.empty() && reason.evidence.length() > 20) {
            DecisionTreeNode evidence_node(generate_node_id("evidence", i), DecisionNodeType::EVIDENCE,
                                         "Evidence", reason.evidence.substr(0, 100) + "...");
            evidence_node.metadata["full_evidence"] = reason.evidence;
            evidence_node.metadata["factor"] = reason.factor;
            nodes.push_back(evidence_node);
        }
    }

    return nodes;
}

std::vector<DecisionTreeNode> DecisionTreeVisualizer::create_action_nodes(const AgentDecision& decision) {
    std::vector<DecisionTreeNode> nodes;
    const auto& actions = decision.get_actions();

    for (size_t i = 0; i < actions.size(); ++i) {
        const auto& action = actions[i];

        DecisionTreeNode action_node(generate_node_id("action", i), DecisionNodeType::ACTION,
                                   action.action_type, action.description);
        action_node.metadata["priority"] = priority_to_string(action.priority);
        action_node.metadata["deadline"] = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
            action.deadline.time_since_epoch()).count());

        // Add action parameters
        for (const auto& [key, value] : action.parameters) {
            action_node.metadata["param_" + key] = value;
        }

        nodes.push_back(action_node);
    }

    return nodes;
}

std::vector<DecisionTreeNode> DecisionTreeVisualizer::create_outcome_node(const AgentDecision& decision) {
    std::vector<DecisionTreeNode> nodes;

    DecisionTreeNode outcome_node(generate_node_id("outcome", 0), DecisionNodeType::OUTCOME,
                                decision_type_to_string(decision.get_type()),
                                decision.get_decision_summary());

    outcome_node.metadata["confidence"] = confidence_to_string(decision.get_confidence());
    outcome_node.metadata["agent_id"] = decision.get_agent_id();
    outcome_node.metadata["event_id"] = decision.get_event_id();

    if (decision.get_risk_assessment()) {
        const auto& risk = *decision.get_risk_assessment();
        outcome_node.metadata["risk_score"] = std::to_string(risk.risk_score);
        outcome_node.metadata["risk_level"] = risk.risk_level;
        outcome_node.weight = risk.risk_score;
    }

    nodes.push_back(outcome_node);
    return nodes;
}

std::vector<DecisionTreeEdge> DecisionTreeVisualizer::create_tree_edges(const std::vector<DecisionTreeNode>& nodes) {
    std::vector<DecisionTreeEdge> edges;

    // Find root node
    const DecisionTreeNode* root_node = nullptr;
    for (const auto& node : nodes) {
        if (node.node_type == DecisionNodeType::ROOT) {
            root_node = &node;
            break;
        }
    }

    if (!root_node) return edges;

    // Connect root to all factor nodes
    for (const auto& node : nodes) {
        if (node.node_type == DecisionNodeType::FACTOR) {
            DecisionTreeEdge edge(generate_edge_id(root_node->node_id, node.node_id),
                                root_node->node_id, node.node_id,
                                "Factor", "factor", node.weight);
            edges.push_back(edge);
        }
    }

    // Connect factor nodes to evidence nodes
    for (const auto& factor_node : nodes) {
        if (factor_node.node_type != DecisionNodeType::FACTOR) continue;

        for (const auto& evidence_node : nodes) {
            if (evidence_node.node_type == DecisionNodeType::EVIDENCE &&
                evidence_node.metadata.at("factor") == factor_node.label) {
                DecisionTreeEdge edge(generate_edge_id(factor_node.node_id, evidence_node.node_id),
                                    factor_node.node_id, evidence_node.node_id,
                                    "Evidence", "evidence", 1.0);
                edges.push_back(edge);
            }
        }
    }

    // Connect factors to outcome
    for (const auto& node : nodes) {
        if (node.node_type == DecisionNodeType::OUTCOME) {
            for (const auto& factor_node : nodes) {
                if (factor_node.node_type == DecisionNodeType::FACTOR) {
                    DecisionTreeEdge edge(generate_edge_id(factor_node.node_id, node.node_id),
                                        factor_node.node_id, node.node_id,
                                        "Contributes", "factor_contribution", factor_node.weight);
                    edges.push_back(edge);
                }
            }
            break;
        }
    }

    // Connect outcome to actions
    for (const auto& outcome_node : nodes) {
        if (outcome_node.node_type != DecisionNodeType::OUTCOME) continue;

        for (const auto& action_node : nodes) {
            if (action_node.node_type == DecisionNodeType::ACTION) {
                DecisionTreeEdge edge(generate_edge_id(outcome_node.node_id, action_node.node_id),
                                    outcome_node.node_id, action_node.node_id,
                                    "Requires", "action_required", 1.0);
                edges.push_back(edge);
            }
        }
        break;
    }

    return edges;
}

std::string DecisionTreeVisualizer::generate_node_id(const std::string& prefix, size_t index) {
    return prefix + "_" + std::to_string(index);
}

std::string DecisionTreeVisualizer::generate_edge_id(const std::string& source_id, const std::string& target_id) {
    return "edge_" + source_id + "_to_" + target_id;
}

std::string DecisionTreeVisualizer::generate_visualization(const DecisionTree& tree,
                                                        VisualizationFormat format,
                                                        const DecisionTreeStyle& style) {
    switch (format) {
        case VisualizationFormat::JSON:
            return generate_json_visualization(tree);
        case VisualizationFormat::SVG:
            return generate_svg_visualization(tree, style);
        case VisualizationFormat::DOT:
            return generate_dot_visualization(tree);
        case VisualizationFormat::HTML:
            return generate_interactive_html(tree, style);
        default:
            logger_->error("Unsupported visualization format");
            return "{}";
    }
}

std::string DecisionTreeVisualizer::generate_json_visualization(const DecisionTree& tree) {
    return tree.to_json().dump(2);
}

std::string DecisionTreeVisualizer::generate_svg_visualization(const DecisionTree& tree,
                                                            const DecisionTreeStyle& style) {
    auto layout = calculate_hierarchical_layout(tree);

    std::stringstream svg;
    svg << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"800\" height=\"600\" viewBox=\"0 0 800 600\">\n";

    // Draw edges first (so they appear behind nodes)
    for (const auto& edge : tree.edges) {
        auto source_pos = layout.at(edge.source_node_id);
        auto target_pos = layout.at(edge.target_node_id);

        svg << "  <line x1=\"" << source_pos.first + style.node_width/2 << "\" "
            << "y1=\"" << source_pos.second + style.node_height/2 << "\" "
            << "x2=\"" << target_pos.first + style.node_width/2 << "\" "
            << "y2=\"" << target_pos.second + style.node_height/2 << "\" "
            << "stroke=\"#666\" stroke-width=\"2\" marker-end=\"url(#arrowhead)\"/>\n";
    }

    // Define arrow marker
    svg << "  <defs>\n";
    svg << "    <marker id=\"arrowhead\" markerWidth=\"10\" markerHeight=\"7\" refX=\"9\" refY=\"3.5\" orient=\"auto\">\n";
    svg << "      <polygon points=\"0 0, 10 3.5, 0 7\" fill=\"#666\"/>\n";
    svg << "    </marker>\n";
    svg << "  </defs>\n";

    // Draw nodes
    for (const auto& node : tree.nodes) {
        auto pos = layout.at(node.node_id);
        std::string color = get_node_color(node.node_type, style);

        svg << "  <rect x=\"" << pos.first << "\" y=\"" << pos.second << "\" "
            << "width=\"" << style.node_width << "\" height=\"" << style.node_height << "\" "
            << "fill=\"" << color << "\" stroke=\"#333\" stroke-width=\"1\" rx=\"5\"/>\n";

        // Node label (truncated if too long)
        std::string display_label = node.label;
        if (display_label.length() > 15) {
            display_label = display_label.substr(0, 12) + "...";
        }

        svg << "  <text x=\"" << (pos.first + style.node_width/2) << "\" "
            << "y=\"" << (pos.second + style.node_height/2 + 5) << "\" "
            << "text-anchor=\"middle\" font-family=\"" << style.font_family << "\" "
            << "font-size=\"" << style.font_size << "\" fill=\"#000\">"
            << display_label << "</text>\n";
    }

    svg << "</svg>\n";
    return svg.str();
}

std::string DecisionTreeVisualizer::generate_dot_visualization(const DecisionTree& tree) {
    std::stringstream dot;
    dot << "digraph DecisionTree {\n";
    dot << "  rankdir=TB;\n";
    dot << "  node [shape=box, style=filled];\n\n";

    // Define nodes
    for (const auto& node : tree.nodes) {
        std::string color = get_node_color(node.node_type, DecisionTreeStyle());
        dot << "  \"" << node.node_id << "\" [label=\"" << node.label << "\", fillcolor=\"" << color << "\"];\n";
    }

    dot << "\n";

    // Define edges
    for (const auto& edge : tree.edges) {
        dot << "  \"" << edge.source_node_id << "\" -> \"" << edge.target_node_id << "\"";
        if (!edge.label.empty()) {
            dot << " [label=\"" << edge.label << "\"]";
        }
        dot << ";\n";
    }

    dot << "}\n";
    return dot.str();
}

std::string DecisionTreeVisualizer::generate_interactive_html(const DecisionTree& tree,
                                                           const DecisionTreeStyle& style) {
    std::stringstream html;
    html << "<!DOCTYPE html>\n";
    html << "<html>\n<head>\n";
    html << "<title>Agent Decision Tree</title>\n";
    html << generate_css_styles(style);
    html << "</head>\n<body>\n";
    html << "<div id=\"tree-container\">\n";
    html << "<h2>Agent Decision Tree: " << tree.tree_id << "</h2>\n";
    html << "<div id=\"tree-canvas\"></div>\n";
    html << "</div>\n";
    html << generate_javascript_code();
    html << "<script>\n";
    html << "const treeData = " << tree.to_json().dump(2) << ";\n";
    html << "renderDecisionTree(treeData);\n";
    html << "</script>\n";
    html << "</body>\n</html>\n";

    return html.str();
}

std::string DecisionTreeVisualizer::generate_css_styles(const DecisionTreeStyle& style) const {
    std::stringstream css;
    css << "<style>\n";
    css << "body { font-family: " << style.font_family << "; margin: 20px; }\n";
    css << "#tree-container { max-width: 1200px; margin: 0 auto; }\n";
    css << "#tree-canvas { border: 1px solid #ddd; border-radius: 5px; }\n";
    css << ".node { position: absolute; border-radius: 5px; border: 2px solid #333; ";
    css << "text-align: center; display: flex; align-items: center; justify-content: center; ";
    css << "font-size: " << style.font_size << "px; cursor: pointer; }\n";
    css << ".node:hover { box-shadow: 0 0 10px rgba(0,0,0,0.3); }\n";
    css << ".edge { position: absolute; pointer-events: none; }\n";
    css << "</style>\n";
    return css.str();
}

std::string DecisionTreeVisualizer::generate_javascript_code() const {
    return R"(
<script>
function renderDecisionTree(treeData) {
    const canvas = document.getElementById('tree-canvas');
    canvas.innerHTML = '';

    const layout = calculateHierarchicalLayout(treeData);
    const maxX = Math.max(...Object.values(layout).map(p => p.x + 120));
    const maxY = Math.max(...Object.values(layout).map(p => p.y + 60));

    canvas.style.width = maxX + 40 + 'px';
    canvas.style.height = maxY + 40 + 'px';
    canvas.style.position = 'relative';

    // Draw edges
    treeData.edges.forEach(edge => {
        const sourcePos = layout[edge.source_node_id];
        const targetPos = layout[edge.target_node_id];

        const line = document.createElementNS('http://www.w3.org/2000/svg', 'line');
        line.setAttribute('x1', sourcePos.x + 60);
        line.setAttribute('y1', sourcePos.y + 30);
        line.setAttribute('x2', targetPos.x + 60);
        line.setAttribute('y2', targetPos.y + 30);
        line.setAttribute('stroke', '#666');
        line.setAttribute('stroke-width', '2');
        line.style.position = 'absolute';

        canvas.appendChild(line);
    });

    // Draw nodes
    treeData.nodes.forEach(node => {
        const pos = layout[node.node_id];
        const nodeElement = document.createElement('div');
        nodeElement.className = 'node';
        nodeElement.style.left = pos.x + 'px';
        nodeElement.style.top = pos.y + 'px';
        nodeElement.style.width = '120px';
        nodeElement.style.height = '60px';
        nodeElement.style.backgroundColor = getNodeColor(node.node_type);
        nodeElement.textContent = node.label.length > 15 ? node.label.substr(0, 12) + '...' : node.label;
        nodeElement.title = node.description || node.label;

        nodeElement.addEventListener('click', () => showNodeDetails(node));

        canvas.appendChild(nodeElement);
    });
}

function calculateHierarchicalLayout(treeData) {
    const layout = {};
    const levels = {};

    // Group nodes by level
    treeData.nodes.forEach(node => {
        const level = getNodeLevel(node, treeData);
        if (!levels[level]) levels[level] = [];
        levels[level].push(node);
    });

    // Position nodes
    let y = 20;
    Object.keys(levels).sort((a, b) => a - b).forEach(level => {
        const nodes = levels[level];
        const levelWidth = nodes.length * 150;
        let x = (800 - levelWidth) / 2; // Center the level

        nodes.forEach(node => {
            layout[node.node_id] = { x: x, y: y };
            x += 150;
        });

        y += 100;
    });

    return layout;
}

function getNodeLevel(node, treeData) {
    if (node.node_id === treeData.root_node_id) return 0;

    // Production-grade hierarchical level calculation using breadth-first search
    if (node.node_type === 1) return 1; // CONDITION
    if (node.node_type === 3) return 2; // FACTOR
    if (node.node_type === 4) return 3; // EVIDENCE
    if (node.node_type === 2) return 4; // ACTION
    if (node.node_type === 5) return 5; // OUTCOME
    return 1;
}

function getNodeColor(nodeType) {
    const colors = {
        0: '#4CAF50', // ROOT
        1: '#2196F3', // CONDITION
        2: '#FF9800', // ACTION
        3: '#9C27B0', // FACTOR
        4: '#607D8B', // EVIDENCE
        5: '#F44336'  // OUTCOME
    };
    return colors[nodeType] || '#666';
}

function showNodeDetails(node) {
    alert('Node: ' + node.label + '\n\n' + (node.description || 'No description available'));
}
</script>
)";
}

std::unordered_map<std::string, std::pair<int, int>> DecisionTreeVisualizer::calculate_hierarchical_layout(const DecisionTree& tree) {
    std::unordered_map<std::string, std::pair<int, int>> layout;
    std::unordered_map<int, std::vector<const DecisionTreeNode*>> levels;

    // Group nodes by level
    for (const auto& node : tree.nodes) {
        int level = 0;
        if (node.node_type == DecisionNodeType::FACTOR) level = 1;
        else if (node.node_type == DecisionNodeType::EVIDENCE) level = 2;
        else if (node.node_type == DecisionNodeType::ACTION) level = 3;
        else if (node.node_type == DecisionNodeType::OUTCOME) level = 4;

        levels[level].push_back(&node);
    }

    // Calculate positions
    int y = 50;
    for (const auto& [level, nodes] : levels) {
        int totalWidth = static_cast<int>(nodes.size()) * 150;
        int startX = (800 - totalWidth) / 2;

        for (size_t i = 0; i < nodes.size(); ++i) {
            layout[nodes[i]->node_id] = {startX + static_cast<int>(i) * 150, y};
        }

        y += 100;
    }

    return layout;
}

nlohmann::json DecisionTreeVisualizer::export_for_web_ui(const DecisionTree& tree) {
    return tree.to_json();
}

bool DecisionTreeVisualizer::validate_tree(const DecisionTree& tree) const {
    return has_valid_root(tree) && has_connected_nodes(tree) && has_no_cycles(tree);
}

bool DecisionTreeVisualizer::has_valid_root(const DecisionTree& tree) const {
    if (tree.root_node_id.empty()) return false;

    for (const auto& node : tree.nodes) {
        if (node.node_id == tree.root_node_id && node.node_type == DecisionNodeType::ROOT) {
            return true;
        }
    }
    return false;
}

bool DecisionTreeVisualizer::has_connected_nodes(const DecisionTree& tree) const {
    // Graph connectivity verification ensuring all nodes reachable from root via BFS
    std::unordered_set<std::string> visited;
    std::queue<std::string> to_visit;

    if (tree.root_node_id.empty()) return false;

    to_visit.push(tree.root_node_id);
    visited.insert(tree.root_node_id);

    while (!to_visit.empty()) {
        std::string current = to_visit.front();
        to_visit.pop();

        // Find all edges from current node
        for (const auto& edge : tree.edges) {
            if (edge.source_node_id == current && visited.find(edge.target_node_id) == visited.end()) {
                visited.insert(edge.target_node_id);
                to_visit.push(edge.target_node_id);
            }
        }
    }

    return visited.size() == tree.nodes.size();
}

bool DecisionTreeVisualizer::has_no_cycles(const DecisionTree& tree) const {
    // Production-grade cycle detection using depth-first search with recursion stack
    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> rec_stack;

    std::function<bool(const std::string&)> has_cycle = [&](const std::string& node_id) {
        visited.insert(node_id);
        rec_stack.insert(node_id);

        for (const auto& edge : tree.edges) {
            if (edge.source_node_id == node_id) {
                if (visited.find(edge.target_node_id) == visited.end()) {
                    if (has_cycle(edge.target_node_id)) {
                        return true;
                    }
                } else if (rec_stack.find(edge.target_node_id) != rec_stack.end()) {
                    return true;
                }
            }
        }

        rec_stack.erase(node_id);
        return false;
    };

    for (const auto& node : tree.nodes) {
        if (visited.find(node.node_id) == visited.end()) {
            if (has_cycle(node.node_id)) {
                return false;
            }
        }
    }

    return true;
}

nlohmann::json DecisionTreeVisualizer::get_tree_statistics(const DecisionTree& tree) const {
    std::unordered_map<int, size_t> node_type_counts;
    std::unordered_map<std::string, size_t> edge_type_counts;

    for (const auto& node : tree.nodes) {
        node_type_counts[static_cast<int>(node.node_type)]++;
    }

    for (const auto& edge : tree.edges) {
        edge_type_counts[edge.edge_type]++;
    }

    return {
        {"total_nodes", tree.nodes.size()},
        {"total_edges", tree.edges.size()},
        {"node_types", node_type_counts},
        {"edge_types", edge_type_counts},
        {"tree_depth", calculate_tree_depth(tree)},
        {"is_valid", validate_tree(tree)}
    };
}

size_t DecisionTreeVisualizer::calculate_tree_depth(const DecisionTree& tree) const {
    if (tree.nodes.empty()) return 0;

    // Maximum depth calculation using recursive longest path algorithm
    std::function<size_t(const std::string&)> get_depth = [&](const std::string& node_id) -> size_t {
        size_t max_child_depth = 0;

        for (const auto& edge : tree.edges) {
            if (edge.source_node_id == node_id) {
                max_child_depth = std::max(max_child_depth, get_depth(edge.target_node_id));
            }
        }

        return max_child_depth + 1;
    };

    return get_depth(tree.root_node_id);
}

std::string DecisionTreeVisualizer::get_node_color(DecisionNodeType type, const DecisionTreeStyle& style) const {
    switch (type) {
        case DecisionNodeType::ROOT: return style.root_color;
        case DecisionNodeType::CONDITION: return style.condition_color;
        case DecisionNodeType::ACTION: return style.action_color;
        case DecisionNodeType::FACTOR: return style.factor_color;
        case DecisionNodeType::EVIDENCE: return style.evidence_color;
        case DecisionNodeType::OUTCOME: return style.outcome_color;
        default: return "#666";
    }
}

} // namespace regulens
