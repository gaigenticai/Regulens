/**
 * Compliance Function Library - Domain-Specific Functions for Regulatory Compliance
 *
 * Pre-built function library providing regulatory lookup, risk assessment,
 * compliance checking, and other compliance-specific operations for function calling.
 *
 * Functions included:
 * - search_regulations: Search regulatory databases
 * - assess_risk: Perform risk assessments
 * - check_compliance: Validate compliance status
 * - get_regulatory_updates: Fetch regulatory changes
 * - analyze_transaction: Transaction analysis and flagging
 * - validate_document: Document compliance validation
 */

#pragma once

#include "function_calling.hpp"
#include "../knowledge_base.hpp"
#include "../risk_assessment.hpp"
#include <memory>

namespace regulens {

/**
 * @brief Compliance Function Library - Registry of compliance-specific functions
 */
class ComplianceFunctionLibrary {
public:
    ComplianceFunctionLibrary(std::shared_ptr<KnowledgeBase> knowledge_base,
                             std::shared_ptr<RiskAssessmentEngine> risk_engine,
                             std::shared_ptr<ConfigurationManager> config,
                             StructuredLogger* logger,
                             ErrorHandler* error_handler);

    /**
     * @brief Register all compliance functions with the registry
     * @param registry Function registry to register with
     * @return true if all functions registered successfully
     */
    bool register_all_functions(FunctionRegistry& registry);

    /**
     * @brief Get function definitions for a specific category
     * @param category Function category
     * @return Vector of function definitions
     */
    std::vector<FunctionDefinition> get_functions_by_category(const std::string& category) const;

private:
    std::shared_ptr<KnowledgeBase> knowledge_base_;
    std::shared_ptr<RiskAssessmentEngine> risk_engine_;
    std::shared_ptr<ConfigurationManager> config_;
    StructuredLogger* logger_;
    ErrorHandler* error_handler_;

    // Function implementations

    /**
     * @brief Search regulations function
     * Searches regulatory knowledge base for specific terms or topics
     */
    FunctionResult search_regulations(const nlohmann::json& args, const FunctionContext& context);

    /**
     * @brief Assess risk function
     * Performs risk assessment on transactions or entities
     */
    FunctionResult assess_risk(const nlohmann::json& args, const FunctionContext& context);

    /**
     * @brief Check compliance function
     * Validates compliance status for given criteria
     */
    FunctionResult check_compliance(const nlohmann::json& args, const FunctionContext& context);

    /**
     * @brief Get regulatory updates function
     * Fetches recent regulatory changes and updates
     */
    FunctionResult get_regulatory_updates(const nlohmann::json& args, const FunctionContext& context);

    /**
     * @brief Analyze transaction function
     * Performs detailed analysis of financial transactions
     */
    FunctionResult analyze_transaction(const nlohmann::json& args, const FunctionContext& context);

    /**
     * @brief Validate document function
     * Validates document compliance against regulatory requirements
     */
    FunctionResult validate_document(const nlohmann::json& args, const FunctionContext& context);

    /**
     * @brief Get compliance report function
     * Generates compliance status reports
     */
    FunctionResult get_compliance_report(const nlohmann::json& args, const FunctionContext& context);

    /**
     * @brief Search compliance precedents function
     * Searches for similar compliance cases and precedents
     */
    FunctionResult search_compliance_precedents(const nlohmann::json& args, const FunctionContext& context);

    // Helper functions

    /**
     * @brief Validate search parameters
     * @param args Function arguments
     * @return true if parameters are valid
     */
    bool validate_search_params(const nlohmann::json& args) const;

    /**
     * @brief Validate risk assessment parameters
     * @param args Function arguments
     * @return true if parameters are valid
     */
    bool validate_risk_params(const nlohmann::json& args) const;

    /**
     * @brief Format regulatory results
     * @param results Raw search results
     * @return Formatted results for API response
     */
    nlohmann::json format_regulatory_results(const std::vector<std::string>& results) const;

    /**
     * @brief Format risk assessment results
     * @param assessment Raw risk assessment
     * @return Formatted results for API response
     */
    nlohmann::json format_risk_assessment(const RiskAssessment& assessment) const;

    /**
     * @brief Determine regulatory category from content
     * @param title Regulatory change title
     * @param content Regulatory change content
     * @return Category string
     */
    std::string determine_regulatory_category(const std::string& title, const std::string& content) const;

    /**
     * @brief Generate regulatory summary
     * @param change Regulatory change
     * @return Summary string
     */
    std::string generate_regulatory_summary(const SimpleRegulatoryChange& change) const;

    /**
     * @brief Assess regulatory impact level
     * @param change Regulatory change
     * @return Impact level string
     */
    std::string assess_regulatory_impact(const SimpleRegulatoryChange& change) const;

    /**
     * @brief Extract affected entities from regulatory change
     * @param change Regulatory change
     * @return JSON array of affected entities
     */
    nlohmann::json extract_affected_entities(const SimpleRegulatoryChange& change) const;

    /**
     * @brief Format timestamp for API response
     * @param tp Time point to format
     * @return ISO 8601 formatted string
     */
    std::string format_timestamp(std::chrono::system_clock::time_point tp) const;

    /**
     * @brief Calculate relevance score for regulatory content
     * @param content The content to analyze
     * @param keywords Compliance-related keywords to check for
     * @return Relevance score between 0.0 and 1.0
     */
    double calculate_relevance_score(const std::string& content, const std::vector<std::string>& keywords) const;
};

/**
 * @brief Create compliance function library instance
 * @param knowledge_base Knowledge base for regulatory data
 * @param risk_engine Risk assessment engine
 * @param config Configuration manager
 * @param logger Structured logger
 * @param error_handler Error handler
 * @return Shared pointer to compliance function library
 */
std::shared_ptr<ComplianceFunctionLibrary> create_compliance_function_library(
    std::shared_ptr<KnowledgeBase> knowledge_base,
    std::shared_ptr<RiskAssessmentEngine> risk_engine,
    std::shared_ptr<ConfigurationManager> config,
    StructuredLogger* logger,
    ErrorHandler* error_handler);

} // namespace regulens
