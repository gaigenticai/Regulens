#pragma once
#include <string>

namespace regulens {

// Canonical DecisionNodeType for all modules
// (Extend as needed for all use cases)
enum class DecisionNodeType {
    ROOT,           // Root of decision tree
    CONDITION,      // Conditional evaluation node
    ACTION,         // Action/recommendation node
    FACTOR,         // Contributing factor node
    EVIDENCE,       // Evidence supporting decision
    OUTCOME,        // Final decision outcome
    DECISION,       // Decision point with branches
    CHANCE,         // Chance/probability node
    TERMINAL,       // End node with outcome
    UTILITY         // Utility assessment node
};

} // namespace regulens
