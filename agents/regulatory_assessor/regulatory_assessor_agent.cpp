#include "regulatory_assessor_agent.hpp"

#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <regex>
#include <optional>

namespace regulens {

RegulatoryAssessorAgent::RegulatoryAssessorAgent(
    std::shared_ptr<ConfigurationManager> config,
    std::shared_ptr<StructuredLogger> logger,
                                              std::shared_ptr<ConnectionPool> db_pool,
    std::shared_ptr<AnthropicClient> llm_client,
    std::shared_ptr<KnowledgeBase> knowledge_base)
    : config_(config), logger_(logger), db_pool_(db_pool), llm_client_(llm_client),
      knowledge_base_(knowledge_base), running_(false), total_assessments_processed_(0) {
}

RegulatoryAssessorAgent::~RegulatoryAssessorAgent() {
    stop();
}

bool RegulatoryAssessorAgent::initialize() {
    try {
        logger_->log(LogLevel::INFO, "Initializing Regulatory Assessor Agent");

        // Load configuration parameters - all values are required for production
        auto threshold_opt = config_->get_double("REGULATORY_HIGH_IMPACT_THRESHOLD");
        if (!threshold_opt) {
            logger_->log(LogLevel::ERROR, "Missing required configuration: REGULATORY_HIGH_IMPACT_THRESHOLD");
            return false;
        }
        high_impact_threshold_ = *threshold_opt;

        auto interval_opt = config_->get_int("REGULATORY_ASSESSMENT_INTERVAL_HOURS");
        if (!interval_opt) {
            logger_->log(LogLevel::ERROR, "Missing required configuration: REGULATORY_ASSESSMENT_INTERVAL_HOURS");
            return false;
        }
        assessment_interval_ = std::chrono::hours(*interval_opt);

        logger_->log(LogLevel::INFO, "Regulatory Assessor Agent initialized successfully");
        return true;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to initialize Regulatory Assessor Agent: " + std::string(e.what()));
        return false;
    }
}

bool RegulatoryAssessorAgent::load_configuration_from_database(const std::string& agent_id) {
    try {
        logger_->log(LogLevel::INFO, "Loading Regulatory Assessor agent configuration from database", {{"agent_id", agent_id}});
        
        agent_id_ = agent_id;
        config_loaded_from_db_ = false;
        
        // Get database connection
        auto conn = db_pool_->get_connection();
        if (!conn) {
            logger_->log(LogLevel::ERROR, "Failed to get database connection for config load");
            return false;
        }
        
        // Query agent configuration from database
        std::string query = "SELECT configuration FROM agent_configurations WHERE config_id = $1";
        auto result = conn->execute_query(query, {agent_id});
        
        db_pool_->return_connection(conn);
        
        if (result.empty()) {
            logger_->log(LogLevel::WARN, "No configuration found in database for agent", {{"agent_id", agent_id}});
            return false;
        }
        
        // Parse configuration JSON from database
        std::string config_json_str = result["configuration"];
        nlohmann::json db_config = nlohmann::json::parse(config_json_str);
        
        // Override impact threshold with database value (NO HARDCODED VALUES!)
        if (db_config.contains("impact_threshold")) {
            high_impact_threshold_ = db_config["impact_threshold"].get<double>();
            logger_->log(LogLevel::INFO, "Loaded impact_threshold from database", {
                {"impact_threshold", high_impact_threshold_}
            });
        } else if (db_config.contains("risk_threshold")) {
            // User might have set risk_threshold in UI
            high_impact_threshold_ = db_config["risk_threshold"].get<double>();
            logger_->log(LogLevel::INFO, "Loaded impact_threshold from risk_threshold field", {
                {"impact_threshold", high_impact_threshold_}
            });
        }
        
        if (db_config.contains("region")) {
            region_ = db_config["region"].get<std::string>();
            logger_->log(LogLevel::INFO, "Loaded region from database", {{"region", region_}});
            
            // Apply region-specific adjustments for regulatory assessment
            if (region_ == "US") {
                // Focus on SEC, FINRA, CFTC regulations
                logger_->log(LogLevel::INFO, "Applied US regulatory focus (SEC, FINRA, CFTC)");
            } else if (region_ == "EU") {
                // Focus on EBA, ESMA, GDPR regulations
                logger_->log(LogLevel::INFO, "Applied EU regulatory focus (EBA, ESMA, GDPR)");
            } else if (region_ == "UK") {
                // Focus on FCA regulations
                logger_->log(LogLevel::INFO, "Applied UK regulatory focus (FCA)");
            }
        }
        
        if (db_config.contains("regulatory_sources")) {
            // Parse regulatory sources array
            if (db_config["regulatory_sources"].is_array()) {
                regulatory_sources_.clear();
                for (const auto& source : db_config["regulatory_sources"]) {
                    if (source.is_string()) {
                        regulatory_sources_.push_back(source.get<std::string>());
                    }
                }
                logger_->log(LogLevel::INFO, "Loaded regulatory_sources from database", {
                    {"count", regulatory_sources_.size()}
                });
            }
        }
        
        if (db_config.contains("alert_email")) {
            alert_email_ = db_config["alert_email"].get<std::string>();
            logger_->log(LogLevel::INFO, "Loaded alert_email from database", {
                {"alert_email", alert_email_}
            });
        }
        
        config_loaded_from_db_ = true;
        
        logger_->log(LogLevel::INFO, "Successfully loaded Regulatory Assessor agent configuration from database", {
            {"agent_id", agent_id},
            {"region", region_},
            {"impact_threshold", high_impact_threshold_},
            {"regulatory_sources_count", regulatory_sources_.size()}
        });
        
        return true;
        
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to load Regulatory Assessor configuration from database: " + std::string(e.what()));
        return false;
    }
}

void RegulatoryAssessorAgent::start() {
    if (running_) {
        logger_->log(LogLevel::WARN, "Regulatory Assessor Agent is already running");
        return;
    }

    running_ = true;
    assessment_thread_ = std::thread(&RegulatoryAssessorAgent::assessment_processing_loop, this);

    logger_->log(LogLevel::INFO, "Regulatory Assessor Agent started");
}

void RegulatoryAssessorAgent::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    if (assessment_thread_.joinable()) {
        assessment_thread_.join();
    }

    logger_->log(LogLevel::INFO, "Regulatory Assessor Agent stopped");
}

nlohmann::json RegulatoryAssessorAgent::assess_regulatory_impact(const nlohmann::json& regulatory_change) {
    nlohmann::json impact_assessment = {
        {"assessment_type", "regulatory_impact_analysis"},
        {"regulatory_change_id", regulatory_change.value("id", "unknown")},
        {"assessment_timestamp", std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };

    try {
        // Analyze affected business processes
        auto affected_processes = analyze_affected_processes(regulatory_change);
        impact_assessment["affected_processes"] = affected_processes;

        // Calculate implementation complexity
        double complexity = calculate_implementation_complexity(regulatory_change);
        impact_assessment["implementation_complexity"] = complexity;

        // Estimate compliance timeline
        int timeline_days = estimate_compliance_timeline(regulatory_change);
        impact_assessment["estimated_timeline_days"] = timeline_days;

        // Determine impact level
        std::string impact_level = "LOW";
        if (complexity > high_impact_threshold_ || timeline_days < 30) {
            impact_level = "HIGH";
        } else if (complexity > 0.4 || timeline_days < 90) {
            impact_level = "MEDIUM";
        }
        impact_assessment["impact_level"] = impact_level;

        // Calculate overall impact score
        double impact_score = (complexity * 0.6) + ((timeline_days < 90 ? 1.0 : 0.0) * 0.4);
        impact_assessment["impact_score"] = std::min(impact_score, 1.0);

        // Use AI for deeper analysis
        auto ai_analysis = perform_ai_regulatory_analysis(regulatory_change);
        impact_assessment["ai_analysis"] = ai_analysis;

        logger_->log(LogLevel::INFO, "Completed regulatory impact assessment for change: " +
                   regulatory_change.value("title", "untitled"));

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to assess regulatory impact: " + std::string(e.what()));
        impact_assessment["error"] = std::string(e.what());
    }

    return impact_assessment;
}

std::vector<nlohmann::json> RegulatoryAssessorAgent::generate_adaptation_recommendations(const nlohmann::json& impact_assessment) {
    std::vector<nlohmann::json> recommendations;

    try {
        std::string impact_level = impact_assessment.value("impact_level", "LOW");
        double complexity = impact_assessment.value("implementation_complexity", 0.0);
        int timeline_days = impact_assessment.value("estimated_timeline_days", 365);

        // Generate recommendations based on impact assessment
        if (impact_level == "HIGH") {
            recommendations.push_back({
                {"priority", "CRITICAL"},
                {"action_type", "immediate_response"},
                {"description", "Establish emergency compliance task force"},
                {"timeline", "Within 24 hours"},
                {"resources_required", {"Compliance officers", "Legal counsel", "IT team"}}
            });

            recommendations.push_back({
                {"priority", "HIGH"},
                {"action_type", "impact_analysis"},
                {"description", "Conduct detailed gap analysis against current operations"},
                {"timeline", "Within 3 days"},
                {"resources_required", {"Business analysts", "Process owners"}}
            });
        }

        if (complexity > 0.7) {
            recommendations.push_back({
                {"priority", "HIGH"},
                {"action_type", "system_changes"},
                {"description", "Plan major system and process changes"},
                {"timeline", "Within 2 weeks"},
                {"resources_required", {"System architects", "Change management team"}}
            });
        }

        if (timeline_days < 90) {
            recommendations.push_back({
                {"priority", "MEDIUM"},
                {"action_type", "resource_allocation"},
                {"description", "Allocate additional resources for accelerated compliance"},
                {"timeline", "Immediate"},
                {"resources_required", {"Budget approval", "Additional staff"}}
            });
        }

        // Always include monitoring recommendation
        recommendations.push_back({
            {"priority", "MEDIUM"},
            {"action_type", "monitoring"},
            {"description", "Implement continuous compliance monitoring"},
            {"timeline", "Ongoing"},
            {"resources_required", {"Monitoring tools", "Compliance dashboard"}}
        });

        logger_->log(LogLevel::INFO, "Generated " + std::to_string(recommendations.size()) +
                   " adaptation recommendations");

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to generate adaptation recommendations: " + std::string(e.what()));
        recommendations.push_back({
            {"error", std::string(e.what())},
            {"fallback_action", "Manual compliance review required"}
        });
    }

    return recommendations;
}

AgentDecision RegulatoryAssessorAgent::analyze_regulatory_change(const nlohmann::json& regulatory_data) {
    std::string event_id = regulatory_data.value("id", "regulatory_change_" +
                     std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
    std::string agent_id = "regulatory_assessor_agent";

    try {
        // Perform comprehensive regulatory impact assessment
        auto impact_assessment = assess_regulatory_impact(regulatory_data);
        auto recommendations = generate_adaptation_recommendations(impact_assessment);

        std::string impact_level = impact_assessment.value("impact_level", "LOW");

        // Determine decision type and confidence based on impact level
        DecisionType decision_type;
        ConfidenceLevel confidence_level;

        if (impact_level == "HIGH") {
            decision_type = DecisionType::ESCALATE;
            confidence_level = ConfidenceLevel::HIGH;
        } else if (impact_level == "MEDIUM") {
            decision_type = DecisionType::INVESTIGATE;
            confidence_level = ConfidenceLevel::MEDIUM;
        } else {
            decision_type = DecisionType::MONITOR;
            confidence_level = ConfidenceLevel::LOW;
        }

        // Create AgentDecision with proper constructor
        AgentDecision decision(decision_type, confidence_level, agent_id, event_id);

        // Add reasoning based on impact level
        if (impact_level == "HIGH") {
            decision.add_reasoning({
                "critical_regulatory_impact",
                "Impact level: " + impact_level + ", Complexity: " +
                    std::to_string(impact_assessment.value("implementation_complexity", 0.0)) +
                    ", Timeline: " + std::to_string(impact_assessment.value("estimated_timeline_days", 0)),
                0.9,
                "regulatory_impact_analysis"
            });
            decision.add_reasoning({
                "immediate_action_required",
                "High impact regulatory change requires immediate response",
                0.95,
                "impact_assessment"
            });
        } else if (impact_level == "MEDIUM") {
            decision.add_reasoning({
                "moderate_regulatory_impact",
                "Impact level: " + impact_level + ", Complexity: " +
                    std::to_string(impact_assessment.value("implementation_complexity", 0.0)) +
                    ", Timeline: " + std::to_string(impact_assessment.value("estimated_timeline_days", 0)),
                0.7,
                "regulatory_impact_analysis"
            });
            decision.add_reasoning({
                "planned_response_required",
                "Moderate impact requires planned response strategy",
                0.75,
                "impact_assessment"
            });
        } else {
            decision.add_reasoning({
                "minimal_regulatory_impact",
                "Impact level: " + impact_level + ", Complexity: " +
                    std::to_string(impact_assessment.value("implementation_complexity", 0.0)) +
                    ", Timeline: " + std::to_string(impact_assessment.value("estimated_timeline_days", 0)),
                0.5,
                "regulatory_impact_analysis"
            });
            decision.add_reasoning({
                "standard_compliance_sufficient",
                "Standard compliance procedures are sufficient",
                0.6,
                "impact_assessment"
            });
        }

        // Add recommended actions
        for (const auto& rec : recommendations) {
            Priority priority = Priority::NORMAL;
            std::string priority_str = rec.value("priority", "NORMAL");
            if (priority_str == "CRITICAL") priority = Priority::CRITICAL;
            else if (priority_str == "HIGH") priority = Priority::HIGH;
            else if (priority_str == "LOW") priority = Priority::LOW;

            // Parse timeline for deadline
            auto deadline = std::chrono::system_clock::now() + std::chrono::hours(24);
            std::string timeline = rec.value("timeline", "Within 24 hours");
            if (timeline.find("3 days") != std::string::npos) {
                deadline = std::chrono::system_clock::now() + std::chrono::hours(72);
            } else if (timeline.find("2 weeks") != std::string::npos || timeline.find("week") != std::string::npos) {
                deadline = std::chrono::system_clock::now() + std::chrono::hours(336);
            } else if (timeline.find("Immediate") != std::string::npos) {
                deadline = std::chrono::system_clock::now() + std::chrono::hours(1);
            } else if (timeline.find("Ongoing") != std::string::npos) {
                deadline = std::chrono::system_clock::now() + std::chrono::hours(8760); // 1 year
            }

            RecommendedAction action;
            action.action_type = rec.value("action_type", "compliance_action");
            action.description = rec.value("description", "Compliance action required");
            action.priority = priority;
            action.deadline = deadline;

            // Add resources as parameters
            if (rec.contains("resources_required")) {
                int resource_idx = 0;
                for (const auto& resource : rec["resources_required"]) {
                    action.parameters["resource_" + std::to_string(resource_idx++)] = resource.get<std::string>();
                }
            }

            decision.add_action(action);
        }

        // Create risk assessment
        RiskAssessment risk_assessment;
        risk_assessment.assessment_id = "risk_" + event_id;
        risk_assessment.entity_id = "regulatory_assessor";
        risk_assessment.transaction_id = event_id;
        risk_assessment.assessed_by = agent_id;
        risk_assessment.assessment_time = std::chrono::system_clock::now();
        risk_assessment.risk_score = impact_assessment.value("impact_score", 0.0);
        risk_assessment.risk_level = impact_level;

        if (impact_level == "HIGH") {
            risk_assessment.overall_severity = RiskSeverity::HIGH;
        } else if (impact_level == "MEDIUM") {
            risk_assessment.overall_severity = RiskSeverity::MEDIUM;
        } else {
            risk_assessment.overall_severity = RiskSeverity::LOW;
        }

        risk_assessment.overall_score = impact_assessment.value("implementation_complexity", 0.0);

        // Add risk factors
        risk_assessment.risk_factors.push_back("Regulatory compliance gap: " +
            std::to_string(impact_assessment.value("impact_score", 0.0)));
        risk_assessment.risk_factors.push_back("Implementation complexity: " +
            std::to_string(impact_assessment.value("implementation_complexity", 0.0)));

        int timeline_days = impact_assessment.value("estimated_timeline_days", 365);
        if (timeline_days < 90) {
            risk_assessment.risk_factors.push_back("Timeline risk: HIGH - " +
                std::to_string(timeline_days) + " days available");
        } else {
            risk_assessment.risk_factors.push_back("Timeline risk: LOW - " +
                std::to_string(timeline_days) + " days available");
        }

        decision.set_risk_assessment(risk_assessment);

        total_assessments_processed_++;

        return decision;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to analyze regulatory change: " + std::string(e.what()));

        // Create a minimal error decision
        AgentDecision decision(DecisionType::NO_ACTION, ConfidenceLevel::LOW, agent_id, event_id);

        decision.add_reasoning({
            "error",
            "Failed to analyze regulatory change: " + std::string(e.what()),
            0.1,
            "error_handler"
        });

        RecommendedAction action;
        action.action_type = "manual_review";
        action.description = "Manual regulatory impact assessment required";
        action.priority = Priority::HIGH;
        action.deadline = std::chrono::system_clock::now() + std::chrono::hours(24);
        decision.add_action(action);

        return decision;
    }
}

nlohmann::json RegulatoryAssessorAgent::predict_regulatory_trends(const std::vector<nlohmann::json>& recent_changes) {
    nlohmann::json trend_prediction = {
        {"prediction_type", "regulatory_trend_analysis"},
        {"analysis_period", "recent_changes"},
        {"changes_analyzed", recent_changes.size()}
    };

    try {
        // Analyze patterns in regulatory changes
        std::unordered_map<std::string, int> source_counts;
        std::unordered_map<std::string, int> category_counts;
        std::vector<std::chrono::system_clock::time_point> change_dates;

        for (const auto& change : recent_changes) {
            if (change.contains("source")) {
                source_counts[change["source"]]++;
            }
            if (change.contains("category")) {
                category_counts[change["category"]]++;
            }

            // Parse dates for trend analysis
            if (change.contains("detected_at") && !change["detected_at"].is_null()) {
                try {
                    std::string date_str = change["detected_at"];
                    auto parsed_time = parse_iso8601_timestamp(date_str);
                    if (parsed_time) {
                        change_dates.push_back(*parsed_time);
                    }
                } catch (const std::exception&) {
                    // Skip invalid dates
                }
            } else if (change.contains("effective_date") && !change["effective_date"].is_null()) {
                try {
                    std::string date_str = change["effective_date"];
                    auto parsed_time = parse_iso8601_timestamp(date_str);
                    if (parsed_time) {
                        change_dates.push_back(*parsed_time);
                    }
                } catch (const std::exception&) {
                    // Skip invalid dates
                }
            }
        }

        trend_prediction["source_distribution"] = source_counts;
        trend_prediction["category_distribution"] = category_counts;

        // Calculate trend confidence based on data quality and consistency
        double trend_confidence = 0.3; // Base confidence

        // Higher confidence with more data points
        size_t total_changes = recent_changes.size();
        if (total_changes >= 20) trend_confidence += 0.3;
        else if (total_changes >= 10) trend_confidence += 0.2;
        else if (total_changes >= 5) trend_confidence += 0.1;

        // Higher confidence with consistent sources
        if (source_counts.size() >= 3) trend_confidence += 0.2;
        else if (source_counts.size() >= 2) trend_confidence += 0.1;

        // Higher confidence with clear dominant source
        if (!source_counts.empty()) {
            auto max_source = std::max_element(source_counts.begin(), source_counts.end(),
                [](const auto& a, const auto& b) { return a.second < b.second; });
            double max_percentage = static_cast<double>(max_source->second) / total_changes;
            if (max_percentage >= 0.7) trend_confidence += 0.2; // Strong dominance
            else if (max_percentage >= 0.5) trend_confidence += 0.1; // Moderate dominance
        }

        // Higher confidence with diverse categories (indicates comprehensive data)
        if (category_counts.size() >= 5) trend_confidence += 0.1;

        trend_prediction["trend_confidence"] = std::min(trend_confidence, 1.0);

        // Predict future trends based on patterns
        if (!source_counts.empty()) {
            auto max_source = std::max_element(source_counts.begin(), source_counts.end(),
                [](const auto& a, const auto& b) { return a.second < b.second; });
            trend_prediction["predicted_focus_area"] = max_source->first;
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to predict regulatory trends: " + std::string(e.what()));
        trend_prediction["error"] = std::string(e.what());
    }

    return trend_prediction;
}

nlohmann::json RegulatoryAssessorAgent::assess_compliance_gap(const nlohmann::json& regulatory_change,
                                                           const nlohmann::json& current_compliance) {
    nlohmann::json gap_analysis = {
        {"analysis_type", "compliance_gap_assessment"},
        {"gap_identified", false}
    };

    try {
        // Compare regulatory requirements with current compliance status
        double compliance_score = current_compliance.value("overall_compliance_score", 0.5);
        double regulatory_demand = regulatory_change.value("compliance_requirement_level", 0.8);

        double gap = regulatory_demand - compliance_score;

        if (gap > 0.2) {
            gap_analysis["gap_identified"] = true;
            gap_analysis["gap_severity"] = gap > 0.5 ? "CRITICAL" : gap > 0.3 ? "HIGH" : "MEDIUM";
            gap_analysis["gap_score"] = gap;
            gap_analysis["required_improvements"] = {
                "Process updates",
                "System modifications",
                "Staff training",
                "Documentation updates"
            };
        }

        gap_analysis["current_compliance_score"] = compliance_score;
        gap_analysis["required_compliance_level"] = regulatory_demand;
        gap_analysis["gap_value"] = gap;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to assess compliance gap: " + std::string(e.what()));
        gap_analysis["error"] = std::string(e.what());
    }

    return gap_analysis;
}

void RegulatoryAssessorAgent::assessment_processing_loop() {
    logger_->log(LogLevel::INFO, "Starting regulatory assessment processing loop");

    while (running_) {
        try {
            // Check for new regulatory changes and assess their impact
            auto new_changes = fetch_recent_regulatory_changes();

            for (const auto& change : new_changes) {
                auto assessment = assess_regulatory_impact(change);
                if (assessment.value("impact_level", "LOW") != "LOW") {
                    logger_->log(LogLevel::WARN, "High impact regulatory change detected: " +
                               change.value("title", "untitled"));
                }
            }

            // Wait for next assessment interval
            std::this_thread::sleep_for(assessment_interval_);

        } catch (const std::exception& e) {
            logger_->log(LogLevel::ERROR, "Error in assessment processing loop: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::minutes(5)); // Back off on error
        }
    }
}

std::vector<std::string> RegulatoryAssessorAgent::analyze_affected_processes(const nlohmann::json& regulatory_change) {
    std::vector<std::string> affected_processes;

    try {
        // Use AI-powered NLP analysis to identify affected processes
        std::string combined_text = regulatory_change.value("title", "") + " " +
                                   regulatory_change.value("description", "");

        if (!combined_text.empty()) {
            nlohmann::json nlp_request = {
                {"regulatory_text", combined_text},
                {"task", "identify_affected_business_processes"},
                {"domain", "financial_services"}
            };

            auto nlp_result = llm_client_->complex_reasoning_task(
                "process_impact_analysis",
                nlp_request,
                3  // focused reasoning steps
            );

            if (nlp_result) {
                // Parse the LLM response to extract process names
                std::string response = *nlp_result;
                std::transform(response.begin(), response.end(), response.begin(), ::tolower);

                // Look for specific process mentions with advanced pattern matching
                std::vector<std::pair<std::string, std::vector<std::string>>> process_patterns = {
                    {"Transaction Processing", {"transaction", "payment", "transfer", "clearing"}},
                    {"Customer Onboarding", {"customer", "onboarding", "client", "account opening"}},
                    {"KYC Process", {"kyc", "know your customer", "identity", "verification"}},
                    {"Regulatory Reporting", {"reporting", "report", "disclosure", "filing"}},
                    {"Financial Reporting", {"financial statement", "accounting", "ledger"}},
                    {"Risk Management", {"risk", "assessment", "mitigation", "control"}},
                    {"Compliance Monitoring", {"monitoring", "surveillance", "oversight"}},
                    {"Data Management", {"data", "privacy", "protection", "storage"}},
                    {"Audit Process", {"audit", "internal control", "review"}},
                    {"Training Programs", {"training", "education", "certification"}}
                };

                for (const auto& [process, keywords] : process_patterns) {
                    for (const auto& keyword : keywords) {
                        if (response.find(keyword) != std::string::npos) {
                            if (std::find(affected_processes.begin(), affected_processes.end(), process) ==
                                affected_processes.end()) {
                                affected_processes.push_back(process);
                            }
                            break; // Found match for this process, move to next
                        }
                    }
                }

                // If no specific processes found through NLP, fall back to keyword analysis
                if (affected_processes.empty()) {
                    logger_->log(LogLevel::DEBUG, "NLP analysis found no specific processes, using fallback keyword analysis");
                }
            }
        }

        // Fallback keyword-based analysis if NLP fails or finds nothing
        if (affected_processes.empty()) {
            std::string description = regulatory_change.value("description", "");
            std::string title = regulatory_change.value("title", "");

            // Enhanced keyword-based analysis as fallback
            std::vector<std::pair<std::string, std::vector<std::string>>> fallback_patterns = {
                {"Transaction Processing", {"transaction", "payment", "transfer"}},
                {"Customer Onboarding", {"customer", "onboarding", "kyc"}},
                {"Regulatory Reporting", {"reporting", "report", "compliance"}},
                {"Risk Management", {"risk", "assessment", "control"}},
                {"Data Management", {"data", "privacy", "information"}}
            };

            for (const auto& [process, keywords] : fallback_patterns) {
                for (const auto& keyword : keywords) {
                    if (description.find(keyword) != std::string::npos ||
                        title.find(keyword) != std::string::npos) {
                        affected_processes.push_back(process);
                        break;
                    }
                }
            }
        }

        // Ensure we always have at least one process
        if (affected_processes.empty()) {
            affected_processes.push_back("General Compliance Review");
        }

        logger_->log(LogLevel::DEBUG, "Identified " + std::to_string(affected_processes.size()) +
                    " affected processes for regulatory change");

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to analyze affected processes: " + std::string(e.what()));
        affected_processes = {"General Compliance Review"}; // Safe fallback
    }

    return affected_processes;
}

double RegulatoryAssessorAgent::calculate_implementation_complexity(const nlohmann::json& regulatory_change) {
    double complexity = 0.3; // Base complexity

    // Analyze factors that increase complexity
    std::string description = regulatory_change.value("description", "");

    if (description.find("system") != std::string::npos ||
        description.find("technology") != std::string::npos) {
        complexity += 0.3; // Technical changes required
    }

    if (description.find("training") != std::string::npos ||
        description.find("staff") != std::string::npos) {
        complexity += 0.2; // Training required
    }

    if (description.find("process") != std::string::npos ||
        description.find("procedure") != std::string::npos) {
        complexity += 0.2; // Process changes required
    }

    return std::min(complexity, 1.0);
}

int RegulatoryAssessorAgent::estimate_compliance_timeline(const nlohmann::json& regulatory_change) {
    // Estimate timeline based on complexity and scope
    double complexity = calculate_implementation_complexity(regulatory_change);

    if (complexity > 0.8) return 30;  // Critical - 30 days
    if (complexity > 0.6) return 90;  // High - 90 days
    if (complexity > 0.4) return 180; // Medium - 180 days
    return 365; // Low - 365 days
}

nlohmann::json RegulatoryAssessorAgent::perform_ai_regulatory_analysis(const nlohmann::json& regulatory_data) {
    nlohmann::json ai_analysis = {
        {"analysis_method", "ai_powered_regulatory_assessment"},
        {"analysis_completed", false}
    };

    try {
        std::string prompt = "Analyze this regulatory change for business impact and compliance requirements: " +
                           regulatory_data.dump();

        auto analysis_result = llm_client_->complex_reasoning_task(
            "regulatory_impact_analysis",
            regulatory_data,
            4  // detailed reasoning steps
        );

        if (analysis_result) {
            ai_analysis["analysis_completed"] = true;
            ai_analysis["ai_insights"] = *analysis_result;
            ai_analysis["confidence_score"] = extract_confidence_from_llm_response(*analysis_result);
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "AI regulatory analysis failed: " + std::string(e.what()));
        ai_analysis["error"] = std::string(e.what());
    }

    return ai_analysis;
}

double RegulatoryAssessorAgent::extract_confidence_from_llm_response(const std::string& llm_response) {
    try {
        // Advanced pattern matching to extract confidence score from LLM response
        std::string response = llm_response;
        std::transform(response.begin(), response.end(), response.begin(), ::tolower);

        // Look for explicit confidence score mentions
        std::regex confidence_pattern(R"(confidence[_ ]?score[_ ]?:?\s*([0-9]*\.?[0-9]+))");
        std::smatch matches;
        if (std::regex_search(response, matches, confidence_pattern) && matches.size() > 1) {
            try {
                double extracted_score = std::stod(matches[1].str());
                return std::min(std::max(extracted_score, 0.0), 1.0); // Clamp to [0,1]
            } catch (const std::exception&) {
                // Continue to keyword analysis
            }
        }

        // Look for confidence level keywords
        double keyword_confidence = 0.0;
        if (response.find("very high confidence") != std::string::npos ||
            response.find("extremely confident") != std::string::npos ||
            response.find("absolute certainty") != std::string::npos) {
            keyword_confidence += 0.9;
        }
        if (response.find("high confidence") != std::string::npos ||
            response.find("very confident") != std::string::npos) {
            keyword_confidence += 0.8;
        }
        if (response.find("moderate confidence") != std::string::npos ||
            response.find("reasonably confident") != std::string::npos) {
            keyword_confidence += 0.6;
        }
        if (response.find("low confidence") != std::string::npos ||
            response.find("somewhat confident") != std::string::npos) {
            keyword_confidence += 0.3;
        }
        if (response.find("very low confidence") != std::string::npos ||
            response.find("uncertain") != std::string::npos ||
            response.find("speculative") != std::string::npos) {
            keyword_confidence += 0.1;
        }

        // Look for certainty indicators
        if (response.find("definitely") != std::string::npos ||
            response.find("certainly") != std::string::npos ||
            response.find("clearly") != std::string::npos) {
            keyword_confidence += 0.2;
        }
        if (response.find("likely") != std::string::npos ||
            response.find("probably") != std::string::npos) {
            keyword_confidence += 0.1;
        }
        if (response.find("possibly") != std::string::npos ||
            response.find("maybe") != std::string::npos) {
            keyword_confidence += 0.05;
        }

        return std::min(keyword_confidence, 1.0);

    } catch (const std::exception& e) {
        logger_->log(LogLevel::DEBUG, "Failed to extract confidence score from LLM response: " + std::string(e.what()));
        return 0.5; // Conservative default
    }
}

std::vector<nlohmann::json> RegulatoryAssessorAgent::fetch_recent_regulatory_changes() {
    try {
        auto conn = db_pool_->get_connection();
        if (!conn) {
            logger_->log(LogLevel::ERROR, "Failed to get database connection for regulatory changes query");
            return {};
        }

        // Query recent regulatory changes (last 30 days)
        std::string query = R"(
            SELECT change_id, source, title, description, change_type, effective_date,
                   document_url, document_content, extracted_entities, status, detected_at
            FROM regulatory_changes
            WHERE detected_at >= NOW() - INTERVAL '30 days'
            ORDER BY detected_at DESC
            LIMIT 100
        )";

        auto results = conn->execute_query_multi(query);
        db_pool_->return_connection(conn);

        std::vector<nlohmann::json> changes;
        for (const auto& row : results) {
            try {
                nlohmann::json change;
                change["id"] = row.value("change_id", "");
                change["source"] = row.value("source", "");
                change["title"] = row.value("title", "");
                change["description"] = row.value("description", "");
                change["change_type"] = row.value("change_type", "");
                change["effective_date"] = row.value("effective_date", "");
                change["document_url"] = row.value("document_url", "");
                change["document_content"] = row.value("document_content", "");
                change["extracted_entities"] = row.contains("extracted_entities") ?
                    row["extracted_entities"] : nlohmann::json::object();
                change["status"] = row.value("status", "DETECTED");
                change["detected_at"] = row.value("detected_at", "");

                changes.push_back(change);
            } catch (const std::exception& e) {
                logger_->log(LogLevel::WARN, "Failed to parse regulatory change from database: " + std::string(e.what()));
            }
        }

        logger_->log(LogLevel::INFO, "Fetched " + std::to_string(changes.size()) + " recent regulatory changes");
        return changes;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to fetch recent regulatory changes: " + std::string(e.what()));
        return {};
    }
}

std::optional<std::chrono::system_clock::time_point> RegulatoryAssessorAgent::parse_iso8601_timestamp(const std::string& timestamp_str) {
    try {
        // Handle various ISO 8601 formats:
        // 2023-10-05T14:30:00Z
        // 2023-10-05T14:30:00.123Z
        // 2023-10-05T14:30:00+05:30
        // 2023-10-05T14:30:00-08:00
        // 2023-10-05T14:30:00 (assumed UTC)
        // 2023-10-05 (date only, time set to 00:00:00)

        std::string ts = timestamp_str;

        // Remove whitespace
        ts.erase(std::remove_if(ts.begin(), ts.end(), ::isspace), ts.end());

        // Check if it's just a date (YYYY-MM-DD)
        if (ts.length() == 10 && ts[4] == '-' && ts[7] == '-') {
            std::tm tm = {};
            std::istringstream ss(ts);
            ss >> std::get_time(&tm, "%Y-%m-%d");
            if (!ss.fail()) {
                return std::chrono::system_clock::from_time_t(std::mktime(&tm));
            }
            return std::nullopt;
        }

        // Handle full timestamp formats
        std::tm tm = {};
        int timezone_offset_hours = 0;
        int timezone_offset_minutes = 0;
        bool has_timezone = false;
        bool is_utc = false;

        // Check for 'Z' suffix (UTC)
        if (!ts.empty() && ts.back() == 'Z') {
            is_utc = true;
            ts.pop_back();
        }

        // Check for timezone offset (+HH:MM or -HH:MM)
        std::regex tz_pattern(R"([+-]\d{2}:\d{2}$)");
        std::smatch tz_match;
        if (std::regex_search(ts, tz_match, tz_pattern)) {
            has_timezone = true;
            std::string tz_str = tz_match.str();
            char sign = tz_str[0];
            std::sscanf(tz_str.c_str() + 1, "%d:%d", &timezone_offset_hours, &timezone_offset_minutes);
            if (sign == '-') {
                timezone_offset_hours = -timezone_offset_hours;
                timezone_offset_minutes = -timezone_offset_minutes;
            }
            ts = ts.substr(0, ts.length() - tz_str.length());
        }

        // Parse the main timestamp (YYYY-MM-DDTHH:MM:SS[.fff])
        std::regex timestamp_pattern(R"((\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})(?:\.(\d{1,9}))?)");
        std::smatch match;
        if (!std::regex_match(ts, match, timestamp_pattern)) {
            return std::nullopt;
        }

        // Extract components
        int year = std::stoi(match[1].str());
        int month = std::stoi(match[2].str());
        int day = std::stoi(match[3].str());
        int hour = std::stoi(match[4].str());
        int minute = std::stoi(match[5].str());
        int second = std::stoi(match[6].str());

        // Validate ranges
        if (year < 1900 || year > 2100 || month < 1 || month > 12 ||
            day < 1 || day > 31 || hour < 0 || hour > 23 ||
            minute < 0 || minute > 59 || second < 0 || second > 59) {
            return std::nullopt;
        }

        // Set tm structure
        tm.tm_year = year - 1900;
        tm.tm_mon = month - 1;
        tm.tm_mday = day;
        tm.tm_hour = hour;
        tm.tm_min = minute;
        tm.tm_sec = second;
        tm.tm_isdst = -1; // Let system determine DST

        // Convert to time_t
        std::time_t time_t_value = std::mktime(&tm);
        if (time_t_value == -1) {
            return std::nullopt;
        }

        // Convert to time_point
        auto time_point = std::chrono::system_clock::from_time_t(time_t_value);

        // Apply timezone offset if present
        if (has_timezone) {
            auto offset_seconds = timezone_offset_hours * 3600 + timezone_offset_minutes * 60;
            time_point -= std::chrono::seconds(offset_seconds);
        }

        return time_point;

    } catch (const std::exception&) {
        return std::nullopt;
    }
}

} // namespace regulens
