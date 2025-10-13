/**
 * Case-Based Reasoning System for Compliance Agents
 *
 * Intelligent retrieval and adaptation of similar compliance cases using
 * embeddings and semantic similarity for decision support and learning.
 *
 * Features:
 * - Semantic case retrieval using embeddings
 * - Case adaptation for new scenarios
 * - Confidence scoring based on case similarity
 * - Case outcome prediction and validation
 * - Learning from case application outcomes
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <optional>
#include <nlohmann/json.hpp>

#include "../config/configuration_manager.hpp"
#include "../logging/structured_logger.hpp"
#include "../error_handler.hpp"
#include "../llm/embeddings_client.hpp"
#include "conversation_memory.hpp"

namespace regulens {

/**
 * @brief Case representation for case-based reasoning
 */
struct ComplianceCase {
    std::string case_id;
    std::string title;
    std::string description;
    nlohmann::json context;              // Full case context
    nlohmann::json decision;             // Decision made
    nlohmann::json outcome;              // Actual outcome
    std::vector<std::string> tags;       // Compliance tags
    std::vector<std::string> stakeholders; // Involved parties
    std::chrono::system_clock::time_point timestamp;
    double success_score;                // 0.0-1.0 based on outcome
    std::vector<float> semantic_embedding;
    std::unordered_map<std::string, double> feature_weights;

    // Metadata
    std::string agent_id;
    std::string agent_type;
    std::string domain;                  // Regulatory domain
    std::string risk_level;              // "low", "medium", "high", "critical"
    std::unordered_map<std::string, std::string> metadata;

    ComplianceCase() = default;

    ComplianceCase(std::string id, std::string case_title,
                  const nlohmann::json& ctx, const nlohmann::json& dec)
        : case_id(std::move(id)), title(std::move(case_title)), context(ctx), decision(dec),
          timestamp(std::chrono::system_clock::now()), success_score(0.5) {}

    // Generate unique case ID
    static std::string generate_case_id();

    // Convert to/from JSON
    nlohmann::json to_json() const;
    static ComplianceCase from_json(const nlohmann::json& json);

    // Calculate case similarity to another case
    double calculate_similarity(const ComplianceCase& other) const;

    // Get case summary for display
    std::string get_summary() const;
};

/**
 * @brief Case retrieval result with similarity score
 */
struct CaseRetrievalResult {
    ComplianceCase case_;
    double similarity_score;      // 0.0-1.0
    double confidence_score;      // 0.0-1.0
    std::vector<std::string> matching_features;
    std::chrono::system_clock::time_point retrieval_timestamp;

    CaseRetrievalResult(ComplianceCase c, double similarity, double confidence,
                       std::vector<std::string> features)
        : case_(std::move(c)), similarity_score(similarity), confidence_score(confidence),
          matching_features(std::move(features)),
          retrieval_timestamp(std::chrono::system_clock::now()) {}
};

/**
 * @brief Case adaptation result for new scenarios
 */
struct CaseAdaptationResult {
    nlohmann::json adapted_decision;
    double adaptation_confidence; // 0.0-1.0
    std::vector<std::string> adaptation_steps;
    std::vector<ComplianceCase> source_cases;
    std::unordered_map<std::string, double> feature_contributions;
    std::string adaptation_method; // "direct", "weighted", "hybrid"

    CaseAdaptationResult() : adaptation_confidence(0.0) {}
};

/**
 * @brief Case query for retrieval
 */
struct CaseQuery {
    nlohmann::json context;                   // Current scenario context
    std::optional<std::string> domain;        // Regulatory domain filter
    std::optional<std::string> risk_level;    // Risk level filter
    std::vector<std::string> required_tags;   // Must-have tags
    std::vector<std::string> preferred_tags;  // Preferred tags
    int max_results = 10;                     // Maximum cases to retrieve
    double min_similarity = 0.3;              // Minimum similarity threshold
    bool include_outcomes = true;             // Include cases with known outcomes
    std::chrono::hours max_age = std::chrono::hours(8760); // 1 year max age

    CaseQuery() = default;
    CaseQuery(const nlohmann::json& ctx) : context(ctx) {}
};

/**
 * @brief Case-based reasoning system
 */
class CaseBasedReasoner {
public:
    CaseBasedReasoner(std::shared_ptr<ConfigurationManager> config,
                     std::shared_ptr<EmbeddingsClient> embeddings_client,
                     std::shared_ptr<ConversationMemory> memory,
                     StructuredLogger* logger,
                     ErrorHandler* error_handler);

    ~CaseBasedReasoner();

    /**
     * @brief Initialize the case-based reasoner
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * @brief Add a new compliance case to the case base
     * @param case_data Case to add
     * @return true if case added successfully
     */
    bool add_case(const ComplianceCase& case_data);

    /**
     * @brief Add case from memory entry
     * @param memory_entry Memory entry to convert to case
     * @return true if case added successfully
     */
    bool add_case_from_memory(const MemoryEntry& memory_entry);

    /**
     * @brief Retrieve similar cases for a given scenario
     * @param query Case retrieval query
     * @return Vector of retrieval results ordered by similarity
     */
    std::vector<CaseRetrievalResult> retrieve_similar_cases(const CaseQuery& query);

    /**
     * @brief Adapt cases to fit a new scenario
     * @param query Case query for the new scenario
     * @param retrieved_cases Previously retrieved similar cases
     * @return Adaptation result with recommended decision
     */
    CaseAdaptationResult adapt_cases_to_scenario(const CaseQuery& query,
                                                const std::vector<CaseRetrievalResult>& retrieved_cases);

    /**
     * @brief Predict outcome for a decision in a given context
     * @param context Decision context
     * @param decision Proposed decision
     * @return Outcome prediction with confidence
     */
    nlohmann::json predict_outcome(const nlohmann::json& context, const nlohmann::json& decision);

    /**
     * @brief Validate decision against similar historical cases
     * @param context Decision context
     * @param decision Decision to validate
     * @return Validation result with supporting evidence
     */
    nlohmann::json validate_decision(const nlohmann::json& context, const nlohmann::json& decision);

    /**
     * @brief Update case base with new outcome information
     * @param case_id Case identifier
     * @param actual_outcome Actual outcome that occurred
     * @param outcome_success Success score (0.0-1.0)
     * @return true if case updated successfully
     */
    bool update_case_outcome(const std::string& case_id,
                           const nlohmann::json& actual_outcome,
                           double outcome_success);

    /**
     * @brief Get case statistics and analytics
     * @return JSON with case base statistics
     */
    nlohmann::json get_case_statistics() const;

    /**
     * @brief Export case base for analysis or backup
     * @param domain Optional domain filter
     * @return JSON array of cases
     */
    nlohmann::json export_case_base(const std::optional<std::string>& domain = std::nullopt);

    /**
     * @brief Perform case base maintenance (consolidation, cleanup)
     */
    void perform_maintenance();

private:
    std::shared_ptr<ConfigurationManager> config_;
    std::shared_ptr<EmbeddingsClient> embeddings_client_;
    std::shared_ptr<ConversationMemory> memory_;
    StructuredLogger* logger_;
    ErrorHandler* error_handler_;

    // Database connection for production-grade persistence
    // Using void* to avoid circular dependencies with pqxx
    void* db_connection_;

    // Case storage
    std::unordered_map<std::string, ComplianceCase> case_base_;
    mutable std::mutex case_mutex_;

    // Performance indexes
    std::unordered_map<std::string, std::vector<std::string>> domain_index_;
    std::unordered_map<std::string, std::vector<std::string>> tag_index_;
    std::unordered_map<std::string, std::vector<std::string>> risk_index_;

    // Configuration
    bool enable_embeddings_;
    bool enable_persistence_;
    size_t max_case_base_size_;
    double similarity_threshold_;
    std::chrono::hours case_retention_period_;

    /**
     * @brief Generate semantic embedding for case
     * @param case_data Case to embed
     * @return Embedding vector
     */
    std::vector<float> generate_case_embedding(const ComplianceCase& case_data);

    /**
     * @brief Extract features from case context
     * @param context Case context
     * @return Feature weights map
     */
    std::unordered_map<std::string, double> extract_case_features(const nlohmann::json& context);

    /**
     * @brief Calculate similarity between two cases
     * @param case1 First case
     * @param case2 Second case
     * @return Similarity score (0.0-1.0)
     */
    double calculate_case_similarity(const ComplianceCase& case1, const ComplianceCase& case2) const;

    /**
     * @brief Find matching features between cases
     * @param case1 First case
     * @param case2 Second case
     * @return Vector of matching feature names
     */
    std::vector<std::string> find_matching_features(const ComplianceCase& case1,
                                                   const ComplianceCase& case2) const;

    /**
     * @brief Adapt decision from source cases to target context
     * @param source_cases Similar cases
     * @param target_context New scenario context
     * @return Adapted decision
     */
    nlohmann::json adapt_decision(const std::vector<ComplianceCase>& source_cases,
                                const nlohmann::json& target_context);

    /**
     * @brief Perform weighted voting across similar cases
     * @param similar_cases Cases with similarity scores
     * @return Weighted decision result
     */
    nlohmann::json perform_weighted_voting(const std::vector<std::pair<ComplianceCase, double>>& similar_cases);

    /**
     * @brief Build indexes for efficient case retrieval
     */
    void build_indexes();

    /**
     * @brief Persist case to storage
     * @param case_data Case to persist
     * @return true if persisted successfully
     */
    bool persist_case(const ComplianceCase& case_data);

    /**
     * @brief Load case from storage
     * @param case_id Case identifier
     * @return Case if found
     */
    std::optional<ComplianceCase> load_case(const std::string& case_id);

    /**
     * @brief Clean up old or irrelevant cases
     */
    void cleanup_case_base();

    /**
     * @brief Validate case data before storage
     * @param case_data Case to validate
     * @return true if valid
     */
    bool validate_case(const ComplianceCase& case_data) const;

    /**
     * @brief Create text representation of query for embedding
     * @param query Query to convert
     * @return Text representation
     */
    std::string create_query_text_for_embedding(const CaseQuery& query);
};

/**
 * @brief Case adaptation strategies
 */
enum class AdaptationStrategy {
    DIRECT_COPY,         // Use most similar case directly
    WEIGHTED_AVERAGE,    // Average decisions weighted by similarity
    MAJORITY_VOTE,       // Choose most common decision
    EXPERT_RULES,        // Apply domain-specific adaptation rules
    LLM_ADAPTATION      // Use LLM for intelligent adaptation
};

/**
 * @brief Case outcome predictor using historical data
 */
class CaseOutcomePredictor {
public:
    CaseOutcomePredictor(std::shared_ptr<ConfigurationManager> config,
                        StructuredLogger* logger,
                        std::shared_ptr<CaseBasedReasoner> case_reasoner = nullptr);

    /**
     * @brief Predict outcome probability for a decision
     * @param context Decision context
     * @param decision Decision to evaluate
     * @return Outcome prediction with confidence intervals
     */
    nlohmann::json predict_outcome_probability(const nlohmann::json& context,
                                             const nlohmann::json& decision);

    /**
     * @brief Get similar historical outcomes
     * @param context Current context
     * @param decision Decision made
     * @return Vector of similar outcomes with probabilities
     */
    std::vector<std::pair<nlohmann::json, double>> get_similar_outcomes(
        const nlohmann::json& context, const nlohmann::json& decision);

    /**
     * @brief Calculate risk score based on historical outcomes
     * @param context Decision context
     * @param decision Decision to evaluate
     * @return Risk score (0.0-1.0, higher = riskier)
     */
    double calculate_historical_risk_score(const nlohmann::json& context,
                                         const nlohmann::json& decision);

    /**
     * @brief Create text representation of a case query for embedding generation
     * @param query The case query to convert to text
     * @return Text representation suitable for embedding
     */
    std::string create_query_text_representation(const CaseQuery& query);

private:
    std::shared_ptr<ConfigurationManager> config_;
    StructuredLogger* logger_;
    std::shared_ptr<CaseBasedReasoner> case_reasoner_;

    /**
     * @brief Find cases with similar context-decision combinations
     * @param context Context to match
     * @param decision Decision to match
     * @return Vector of similar cases with similarity scores
     */
    std::vector<std::pair<ComplianceCase, double>> find_similar_context_decisions(
        const nlohmann::json& context, const nlohmann::json& decision);

    /**
     * @brief Analyze risk score from context information
     * @param context Transaction context
     * @return Risk score (0.0-1.0)
     */
    double analyze_context_risk(const nlohmann::json& context);

    /**
     * @brief Generate synthetic similar case for prediction
     * @param context Original context
     * @param decision Original decision
     * @param risk_score Calculated risk score
     * @param case_index Index for case generation
     * @return Generated compliance case
     */
    ComplianceCase generate_synthetic_similar_case(
        const nlohmann::json& context, const nlohmann::json& decision,
        double risk_score, int case_index);

    /**
     * @brief Calculate similarity between contexts and decisions
     * @param context Original context
     * @param decision Original decision
     * @param similar_case Generated similar case
     * @param risk_score Risk score
     * @return Similarity score (0.0-1.0)
     */
    double calculate_synthetic_similarity(
        const nlohmann::json& context, const nlohmann::json& decision,
        const ComplianceCase& similar_case, double risk_score);
};

/**
 * @brief Case validation system
 */
class CaseValidator {
public:
    CaseValidator(std::shared_ptr<ConfigurationManager> config,
                 StructuredLogger* logger,
                 std::shared_ptr<CaseBasedReasoner> case_reasoner = nullptr);

    /**
     * @brief Validate decision against case base
     * @param context Decision context
     * @param decision Decision to validate
     * @return Validation result with evidence and confidence
     */
    nlohmann::json validate_against_cases(const nlohmann::json& context,
                                        const nlohmann::json& decision);

    /**
     * @brief Get contradictory cases for a decision
     * @param context Decision context
     * @param decision Decision to check
     * @return Cases that contradict the decision
     */
    std::vector<ComplianceCase> find_contradictory_cases(const nlohmann::json& context,
                                                       const nlohmann::json& decision);

    /**
     * @brief Assess decision consistency with historical cases
     * @param context Decision context
     * @param decision Decision to assess
     * @return Consistency score (0.0-1.0, higher = more consistent)
     */
    double assess_decision_consistency(const nlohmann::json& context,
                                     const nlohmann::json& decision);

private:
    std::shared_ptr<ConfigurationManager> config_;
    StructuredLogger* logger_;
    std::shared_ptr<CaseBasedReasoner> case_reasoner_;

    /**
     * @brief Check if two decisions are contradictory
     * @param decision1 First decision
     * @param decision2 Second decision
     * @return true if contradictory
     */
    bool decisions_are_contradictory(const nlohmann::json& decision1,
                                   const nlohmann::json& decision2) const;

    /**
     * @brief Generate synthetic contradictory case for validation
     * @param context Original context
     * @param decision Original decision
     * @param risk_score Risk score
     * @param case_index Index for case generation
     * @return Generated contradictory case
     */
    ComplianceCase generate_contradictory_case(
        const nlohmann::json& context, const nlohmann::json& decision,
        double risk_score, int case_index);
};

/**
 * @brief Create case-based reasoner instance
 * @param config Configuration manager
 * @param embeddings_client Embeddings client
 * @param memory Conversation memory system
 * @param logger Structured logger
 * @param error_handler Error handler
 * @return Shared pointer to case-based reasoner
 */
std::shared_ptr<CaseBasedReasoner> create_case_based_reasoner(
    std::shared_ptr<ConfigurationManager> config,
    std::shared_ptr<EmbeddingsClient> embeddings_client,
    std::shared_ptr<ConversationMemory> memory,
    StructuredLogger* logger,
    ErrorHandler* error_handler);

/**
 * @brief Create case outcome predictor instance
 * @param config Configuration manager
 * @param logger Structured logger
 * @param case_reasoner Optional case-based reasoner for direct case base access
 * @return Shared pointer to outcome predictor
 */
std::shared_ptr<CaseOutcomePredictor> create_case_outcome_predictor(
    std::shared_ptr<ConfigurationManager> config,
    StructuredLogger* logger,
    std::shared_ptr<CaseBasedReasoner> case_reasoner = nullptr);

/**
 * @brief Create case validator instance
 * @param config Configuration manager
 * @param logger Structured logger
 * @param case_reasoner Optional case-based reasoner for direct case base access
 * @return Shared pointer to case validator
 */
std::shared_ptr<CaseValidator> create_case_validator(
    std::shared_ptr<ConfigurationManager> config,
    StructuredLogger* logger,
    std::shared_ptr<CaseBasedReasoner> case_reasoner = nullptr);

} // namespace regulens
