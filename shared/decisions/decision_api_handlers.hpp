/**
 * Decision Analysis API Handlers - Header File
 * Production-grade decision tree and MCDA endpoint declarations
 * Uses DecisionTreeOptimizer for sophisticated decision analysis
 */

#ifndef REGULENS_DECISION_API_HANDLERS_HPP
#define REGULENS_DECISION_API_HANDLERS_HPP

#include <string>
#include <map>
#include <memory>
#include <libpq-fe.h>
#include "../decision_tree_optimizer.hpp"

namespace regulens {
namespace decisions {

// Initialize decision tree optimizer (should be called at startup)
bool initialize_decision_engine(
    std::shared_ptr<DecisionTreeOptimizer> optimizer
);

// Get shared decision engine instance
std::shared_ptr<DecisionTreeOptimizer> get_decision_engine();

// Decision Tree Retrieval
std::string get_decision_tree(
    PGconn* db_conn,
    const std::map<std::string, std::string>& query_params
);

// Decision Visualization
std::string visualize_decision(
    PGconn* db_conn,
    const std::string& request_body,
    const std::string& user_id
);

// Create Decision with MCDA
std::string create_decision(
    PGconn* db_conn,
    const std::string& request_body,
    const std::string& user_id
);

} // namespace decisions
} // namespace regulens

#endif // REGULENS_DECISION_API_HANDLERS_HPP

