#include "audit_intelligence_agent.hpp"

#include <algorithm>
#include <sstream>
#include <iomanip>
#include <random>
#include <cmath>
#include <variant>
#include <regex>

namespace regulens {

AuditIntelligenceAgent::AuditIntelligenceAgent(
    std::shared_ptr<ConfigurationManager> config,
    std::shared_ptr<StructuredLogger> logger,
                                             std::shared_ptr<ConnectionPool> db_pool,
    std::shared_ptr<AnthropicClient> llm_client,
    std::shared_ptr<DecisionAuditTrailManager> audit_trail)
    : config_(config), logger_(logger), db_pool_(db_pool), llm_client_(llm_client),
      audit_trail_(audit_trail), running_(false), total_audits_processed_(0),
      anomaly_threshold_(0.85), analysis_interval_(15), critical_severity_risk_(0.8),
      high_severity_risk_(0.6), medium_severity_risk_(0.4), low_severity_risk_(0.2) {
}

AuditIntelligenceAgent::~AuditIntelligenceAgent() {
    stop();
}

bool AuditIntelligenceAgent::initialize() {
    try {
        logger_->log(LogLevel::INFO, "Initializing Audit Intelligence Agent");

        // Load configuration parameters
        anomaly_threshold_ = config_->get_double("AUDIT_ANOMALY_THRESHOLD").value_or(0.85);
        critical_severity_risk_ = config_->get_double("AUDIT_CRITICAL_SEVERITY_RISK").value_or(0.8);
        high_severity_risk_ = config_->get_double("AUDIT_HIGH_SEVERITY_RISK").value_or(0.6);
        medium_severity_risk_ = config_->get_double("AUDIT_MEDIUM_SEVERITY_RISK").value_or(0.4);
        low_severity_risk_ = config_->get_double("AUDIT_LOW_SEVERITY_RISK").value_or(0.2);
        analysis_interval_ = std::chrono::minutes(
            config_->get_int("AUDIT_ANALYSIS_INTERVAL_MINUTES").value_or(15));

        logger_->log(LogLevel::INFO, "Audit Intelligence Agent initialized successfully");
        return true;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to initialize Audit Intelligence Agent: " + std::string(e.what()));
        return false;
    }
}

bool AuditIntelligenceAgent::load_configuration_from_database(const std::string& agent_id) {
    try {
        logger_->log(LogLevel::INFO, "Loading Audit Intelligence agent configuration from database: " + agent_id);
        
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
        auto result = conn->execute_query_multi(query, {agent_id});
        
        db_pool_->return_connection(conn);
        
        if (result.empty()) {
            logger_->log(LogLevel::WARN, "No configuration found in database for agent: " + agent_id);
            return false;
        }
        
        // Parse configuration JSON from database
        std::string config_json_str = result[0]["configuration"].get<std::string>();
        nlohmann::json db_config = nlohmann::json::parse(config_json_str);
        
        // Override anomaly threshold with database value (NO HARDCODED VALUES!)
        if (db_config.contains("anomaly_threshold")) {
            anomaly_threshold_ = db_config["anomaly_threshold"].get<double>();
            logger_->log(LogLevel::INFO, "Loaded anomaly_threshold from database: " + 
                std::to_string(anomaly_threshold_));
        } else if (db_config.contains("risk_threshold")) {
            // User might have set risk_threshold in UI for Audit Intelligence
            anomaly_threshold_ = db_config["risk_threshold"].get<double>();
            logger_->log(LogLevel::INFO, "Loaded anomaly_threshold from risk_threshold field: " + 
                std::to_string(anomaly_threshold_));
        }
        
        if (db_config.contains("region")) {
            region_ = db_config["region"].get<std::string>();
            logger_->log(LogLevel::INFO, "Loaded region from database: " + region_);
            
            // Apply region-specific adjustments for audit sensitivity
            if (region_ == "EU") {
                // EU GDPR requires more thorough auditing
                if (!db_config.contains("anomaly_threshold")) {
                    anomaly_threshold_ = std::min(anomaly_threshold_ + 0.05, 0.95);
                    logger_->log(LogLevel::INFO, "Applied EU GDPR adjustment to anomaly_threshold: " + 
                        std::to_string(anomaly_threshold_));
                }
            }
        }
        
        if (db_config.contains("alert_email")) {
            alert_email_ = db_config["alert_email"].get<std::string>();
            logger_->log(LogLevel::INFO, "Loaded alert_email from database: " + alert_email_);
        }
        
        config_loaded_from_db_ = true;
        
        logger_->log(LogLevel::INFO, std::string("Successfully loaded Audit Intelligence agent configuration from database - ") +
            "agent_id: " + agent_id + ", region: " + region_ + 
            ", anomaly_threshold: " + std::to_string(anomaly_threshold_));
        
        return true;
        
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to load Audit Intelligence configuration from database: " + std::string(e.what()));
        return false;
    }
}

void AuditIntelligenceAgent::start() {
    if (running_) {
        logger_->log(LogLevel::WARN, "Audit Intelligence Agent is already running");
        return;
    }

    running_ = true;
    audit_thread_ = std::thread(&AuditIntelligenceAgent::audit_processing_loop, this);

    logger_->log(LogLevel::INFO, "Audit Intelligence Agent started");
}

void AuditIntelligenceAgent::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    if (audit_thread_.joinable()) {
        audit_thread_.join();
    }

    logger_->log(LogLevel::INFO, "Audit Intelligence Agent stopped");
}

std::vector<ComplianceEvent> AuditIntelligenceAgent::analyze_audit_trails(int time_window_hours) {
    std::vector<ComplianceEvent> anomalies;

    try {
        auto now = std::chrono::system_clock::now();
        auto start_time = now - std::chrono::hours(time_window_hours);

        // Get audit trails from all agents using the audit trail manager
        auto audit_trails = audit_trail_->get_audit_trail_for_compliance(start_time, now);

        // Convert audit trails to analysis data
        std::vector<nlohmann::json> audit_data;
        for (const auto& trail : audit_trails) {
            audit_data.push_back({
                {"trail_id", trail["trail_id"]},
                {"agent_name", trail["agent_name"]},
                {"agent_type", trail["agent_type"]},
                {"final_confidence", trail["final_confidence"]},
                {"started_at", trail["started_at"]},
                {"total_processing_time_ms", trail["total_processing_time_ms"]},
                {"final_decision", trail["final_decision"]},
                {"risk_assessment", trail["risk_assessment"]}
            });
        }

        // Detect anomalies using advanced pattern recognition
        // Combine multiple anomaly detection methods
        std::vector<nlohmann::json> detected_anomalies;
        auto temporal = detect_temporal_anomalies(audit_data);
        auto behavioral = detect_behavioral_anomalies(audit_data);
        auto correlation = detect_risk_correlation_anomalies(audit_data);
        detected_anomalies.insert(detected_anomalies.end(), temporal.begin(), temporal.end());
        detected_anomalies.insert(detected_anomalies.end(), behavioral.begin(), behavioral.end());
        detected_anomalies.insert(detected_anomalies.end(), correlation.begin(), correlation.end());
        for (const auto& anomaly : detected_anomalies) {
            EventSource source{
                "audit_intelligence_agent",
                "audit_trail_analysis",
                "internal"
            };
            
            EventMetadata metadata;
            metadata["anomaly_data"] = anomaly.dump();
            
            ComplianceEvent event(
                EventType::AUDIT_LOG_ENTRY,
                EventSeverity::HIGH,
                "Audit Intelligence detected anomalous pattern: " + anomaly["description"].get<std::string>(),
                source,
                metadata
            );
            anomalies.push_back(event);
        }

        total_audits_processed_ += audit_data.size();
        logger_->log(LogLevel::INFO, "Processed " + std::to_string(audit_data.size()) +
                   " audit records, detected " + std::to_string(anomalies.size()) + " anomalies");

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to analyze audit trails: " + std::string(e.what()));
    }

    return anomalies;
}

AgentDecision AuditIntelligenceAgent::perform_compliance_monitoring(const ComplianceEvent& event) {
    // Determine decision type based on risk assessment
    DecisionType decision_type = DecisionType::MONITOR; // Default to monitoring
    ConfidenceLevel confidence = ConfidenceLevel::MEDIUM;

    // Analyze the event using AI-powered compliance monitoring
    nlohmann::json analysis_data = {
        {"event_type", static_cast<int>(event.get_type())},
        {"severity", static_cast<int>(event.get_severity())},
        {"description", event.get_description()},
        {"source", event.get_source().to_json()},
        {"metadata", nlohmann::json::object()}
    };
    // Add metadata to analysis
    for (const auto& [key, value] : event.get_metadata()) {
        std::visit([&](const auto& v) {
            analysis_data["metadata"][key] = v;
        }, value);
    }

    // Calculate risk score using advanced ML analysis
    double risk_score = calculate_advanced_risk_score(analysis_data);

    // Determine decision type and confidence based on risk score
    if (risk_score > anomaly_threshold_) {
        decision_type = DecisionType::ALERT;
        confidence = ConfidenceLevel::HIGH;
    } else if (risk_score > anomaly_threshold_ * 0.7) {
        decision_type = DecisionType::INVESTIGATE;
        confidence = ConfidenceLevel::MEDIUM;
    }

    AgentDecision decision(decision_type, confidence, "AuditIntelligenceAgent", event.get_event_id());

    try {
        // Add reasoning based on the analysis
        DecisionReasoning reasoning;
        reasoning.factor = "Risk-based compliance monitoring";
        reasoning.evidence = "Event severity: " + std::to_string(static_cast<int>(event.get_severity())) + 
                           ", Type: " + std::to_string(static_cast<int>(event.get_type()));
        reasoning.weight = risk_score;
        reasoning.source = "AuditIntelligenceAgent_ML_Analysis";
        decision.add_reasoning(reasoning);

        // Add recommended actions based on decision type
        RecommendedAction action;
        action.deadline = std::chrono::system_clock::now() + std::chrono::hours(24);
        action.parameters = {{"event_id", event.get_event_id()}};

        if (decision_type == DecisionType::ALERT) {
            action.action_type = "escalate";
            action.description = "Immediate compliance review required - high risk anomaly detected";
            action.priority = Priority::CRITICAL;
        } else if (decision_type == DecisionType::INVESTIGATE) {
            action.action_type = "investigate";
            action.description = "Conduct detailed compliance investigation";
            action.priority = Priority::HIGH;
        } else {
            action.action_type = "monitor";
            action.description = "Continue routine compliance monitoring";
            action.priority = Priority::NORMAL;
        }
        decision.add_action(action);

        // Set risk assessment
        RiskAssessment risk_assessment;
        risk_assessment.risk_score = risk_score;
        risk_assessment.risk_level = risk_score > anomaly_threshold_ ? "HIGH" :
                                   (risk_score > anomaly_threshold_ * 0.7 ? "MEDIUM" : "LOW");
        risk_assessment.risk_factors = {"Event severity", "Event type", "Historical patterns"};
        risk_assessment.assessment_time = std::chrono::system_clock::now();
        decision.set_risk_assessment(risk_assessment);

        logger_->log(LogLevel::INFO, "Completed compliance monitoring for event " + event.get_event_id() +
                   " with risk score: " + std::to_string(risk_score));

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to perform compliance monitoring: " + std::string(e.what()));

        // Set low confidence on error
        DecisionReasoning error_reasoning;
        error_reasoning.factor = "Analysis error";
        error_reasoning.evidence = "Failed to complete risk analysis: " + std::string(e.what());
        error_reasoning.weight = 0.0;
        error_reasoning.source = "AuditIntelligenceAgent_Error_Handler";
        decision.add_reasoning(error_reasoning);

        RecommendedAction error_action;
        error_action.action_type = "manual_review";
        error_action.description = "Manual compliance review required due to analysis error";
        error_action.priority = Priority::HIGH;
        error_action.deadline = std::chrono::system_clock::now() + std::chrono::hours(4);
        error_action.parameters = {{"event_id", event.get_event_id()}, {"error", std::string(e.what())}};
        decision.add_action(error_action);
    }

    return decision;
}

nlohmann::json AuditIntelligenceAgent::generate_audit_report(
    const std::chrono::system_clock::time_point& start_time,
    const std::chrono::system_clock::time_point& end_time) {

    nlohmann::json report = {
        {"report_type", "audit_intelligence_summary"},
        {"generated_at", std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()},
        {"time_period", {
            {"start", std::chrono::duration_cast<std::chrono::seconds>(
                start_time.time_since_epoch()).count()},
            {"end", std::chrono::duration_cast<std::chrono::seconds>(
                end_time.time_since_epoch()).count()}
        }}
    };

    try {
        // Gather audit statistics using available methods
        auto compliance_trails = audit_trail_->get_audit_trail_for_compliance(start_time, end_time);
        auto agent_analytics = audit_trail_->get_agent_performance_analytics("all", start_time);
        auto pattern_analysis_data = audit_trail_->get_decision_pattern_analysis("all", start_time);

        // Build audit statistics from available data
        nlohmann::json audit_stats = {
            {"total_audit_trails", compliance_trails.size()},
            {"time_period_days", std::chrono::duration_cast<std::chrono::hours>(end_time - start_time).count() / 24.0},
            {"agent_analytics", agent_analytics},
            {"pattern_analysis", pattern_analysis_data}
        };

        report["audit_statistics"] = audit_stats;

        // Get all agent decisions for pattern analysis
        std::vector<AgentDecision> all_decisions = convert_audit_trails_to_decisions(compliance_trails);
        auto pattern_analysis = analyze_decision_patterns(all_decisions);

        report["pattern_analysis"] = pattern_analysis;

        // Generate AI-powered insights
        std::vector<nlohmann::json> audit_data = {audit_stats, pattern_analysis};
        std::string insights = generate_compliance_insights(audit_data);

        report["ai_insights"] = insights;
        report["total_audits_processed"] = total_audits_processed_.load();

        logger_->log(LogLevel::INFO, "Generated comprehensive audit intelligence report");

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to generate audit report: " + std::string(e.what()));
        report["error"] = std::string(e.what());
    }

    return report;
}

nlohmann::json AuditIntelligenceAgent::detect_fraud_patterns(const nlohmann::json& transaction_data) {
    nlohmann::json fraud_analysis = {
        {"analysis_type", "fraud_pattern_detection"},
        {"transaction_analyzed", true}
    };

    try {
        // Use AI to analyze transaction patterns for fraud indicators
        std::string analysis_prompt = "Analyze this transaction data for potential fraud patterns: " +
                                    transaction_data.dump();

        auto llm_response = llm_client_->complex_reasoning_task(
            "fraud_pattern_analysis",
            transaction_data,
            3  // reasoning steps
        );

        if (llm_response) {
            fraud_analysis["fraud_risk_assessment"] = *llm_response;

            // Extract and calculate real risk score from LLM response using advanced pattern analysis
            double calculated_risk = extract_risk_score_from_llm_response(*llm_response);
            // Apply transaction-specific risk adjustments
            calculated_risk = adjust_risk_for_transaction_characteristics(calculated_risk, transaction_data);
            fraud_analysis["risk_score"] = calculated_risk;

            // Generate dynamic recommendations based on risk level
            fraud_analysis["recommendations"] = generate_fraud_recommendations(calculated_risk, transaction_data);

            // Add fraud indicators found
            fraud_analysis["fraud_indicators"] = identify_fraud_indicators(transaction_data, *llm_response);
        } else {
            fraud_analysis["error"] = "Failed to get AI analysis for fraud detection";
            fraud_analysis["risk_score"] = calculate_baseline_fraud_risk(transaction_data); // Dynamic baseline
            fraud_analysis["recommendations"] = generate_basic_fraud_recommendations(transaction_data);
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to detect fraud patterns: " + std::string(e.what()));
        fraud_analysis["error"] = std::string(e.what());
    }

    return fraud_analysis;
}

nlohmann::json AuditIntelligenceAgent::analyze_decision_patterns(const std::vector<AgentDecision>& decisions) {
    nlohmann::json pattern_analysis = {
        {"analysis_type", "decision_pattern_analysis"},
        {"decisions_analyzed", decisions.size()}
    };

    try {
        // Analyze decision patterns for consistency and potential bias
        std::unordered_map<std::string, int> decision_type_counts;
        std::unordered_map<std::string, std::vector<double>> confidence_scores;
        std::unordered_map<std::string, int> agent_decision_counts;

        for (const auto& decision : decisions) {
            std::string decision_type_str = decision_type_to_string(decision.get_type());
            std::string confidence_str = confidence_to_string(decision.get_confidence());

            decision_type_counts[decision_type_str]++;
            confidence_scores[decision_type_str].push_back(
                decision.get_confidence() == ConfidenceLevel::HIGH ? 1.0 :
                decision.get_confidence() == ConfidenceLevel::MEDIUM ? 0.5 : 0.0
            );
            agent_decision_counts[decision.get_agent_id()]++;
        }

        // Calculate pattern metrics
        pattern_analysis["decision_type_distribution"] = decision_type_counts;
        pattern_analysis["agent_activity_distribution"] = agent_decision_counts;

        // Calculate average confidence scores
        nlohmann::json avg_confidence;
        for (const auto& [type, scores] : confidence_scores) {
            double sum = 0.0;
            for (double score : scores) sum += score;
            avg_confidence[type] = scores.empty() ? 0.0 : sum / scores.size();
        }
        pattern_analysis["average_confidence_by_type"] = avg_confidence;

        // Detect potential bias patterns using statistical analysis
        pattern_analysis["bias_indicators"] = detect_bias_patterns(decision_type_counts, agent_decision_counts);

        logger_->log(LogLevel::INFO, "Analyzed decision patterns for " + std::to_string(decisions.size()) + " decisions");

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to analyze decision patterns: " + std::string(e.what()));
        pattern_analysis["error"] = std::string(e.what());
    }

    return pattern_analysis;
}

nlohmann::json AuditIntelligenceAgent::analyze_decision_patterns_from_audit_trails(const std::vector<nlohmann::json>& audit_trails) {
    nlohmann::json pattern_analysis = {
        {"analysis_type", "audit_trail_pattern_analysis"},
        {"trails_analyzed", audit_trails.size()}
    };

    try {
        std::unordered_map<std::string, int> agent_type_counts;
        std::unordered_map<std::string, int> confidence_distribution;
        std::unordered_map<std::string, std::vector<long long>> processing_times;

        for (const auto& trail : audit_trails) {
            std::string agent_type = trail["agent_type"];
            int confidence = trail["final_confidence"];
            long long processing_time = trail["total_processing_time_ms"];

            agent_type_counts[agent_type]++;
            confidence_distribution[std::to_string(confidence)]++;
            processing_times[agent_type].push_back(processing_time);
        }

        pattern_analysis["agent_type_distribution"] = agent_type_counts;
        pattern_analysis["confidence_distribution"] = confidence_distribution;

        // Calculate processing time statistics
        nlohmann::json processing_stats;
        for (const auto& [agent_type, times] : processing_times) {
            if (!times.empty()) {
                long long sum = 0;
                long long min_time = *std::min_element(times.begin(), times.end());
                long long max_time = *std::max_element(times.begin(), times.end());
                for (long long time : times) sum += time;

                processing_stats[agent_type] = {
                    {"average_ms", static_cast<double>(sum) / times.size()},
                    {"min_ms", min_time},
                    {"max_ms", max_time},
                    {"count", times.size()}
                };
            }
        }
        pattern_analysis["processing_time_statistics"] = processing_stats;

        // Detect performance anomalies
        pattern_analysis["performance_anomalies"] = detect_performance_anomalies(processing_times);

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to analyze audit trail patterns: " + std::string(e.what()));
        pattern_analysis["error"] = std::string(e.what());
    }

    return pattern_analysis;
}

void AuditIntelligenceAgent::audit_processing_loop() {
    logger_->log(LogLevel::INFO, "Starting audit intelligence processing loop");

    while (running_) {
        try {
            // Perform continuous audit analysis
            auto anomalies = analyze_audit_trails(1); // Last hour

            if (!anomalies.empty()) {
                logger_->log(LogLevel::WARN, "Detected " + std::to_string(anomalies.size()) +
                           " audit anomalies in the last hour");
            }

            // Wait for next analysis interval
            std::this_thread::sleep_for(analysis_interval_);

        } catch (const std::exception& e) {
            logger_->log(LogLevel::ERROR, "Error in audit processing loop: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(30)); // Back off on error
        }
    }
}

std::vector<nlohmann::json> AuditIntelligenceAgent::perform_advanced_pattern_recognition(const std::vector<nlohmann::json>& audit_data) {
    std::vector<nlohmann::json> anomalies;

    try {
        // Advanced ML-based pattern recognition for audit anomalies
        // Analyze temporal patterns, agent behavior consistency, and risk correlations

        // 1. Temporal anomaly detection
        auto temporal_anomalies = detect_temporal_anomalies(audit_data);
        anomalies.insert(anomalies.end(), temporal_anomalies.begin(), temporal_anomalies.end());

        // 2. Behavioral consistency analysis
        auto behavioral_anomalies = detect_behavioral_anomalies(audit_data);
        anomalies.insert(anomalies.end(), behavioral_anomalies.begin(), behavioral_anomalies.end());

        // 3. Risk correlation analysis
        auto risk_anomalies = detect_risk_correlation_anomalies(audit_data);
        anomalies.insert(anomalies.end(), risk_anomalies.begin(), risk_anomalies.end());

        // 4. Use LLM for advanced pattern interpretation if available
        if (!anomalies.empty()) {
            for (auto& anomaly : anomalies) {
                anomaly["llm_insights"] = generate_anomaly_insights(anomaly);
            }
        }

        logger_->log(LogLevel::INFO, "Advanced pattern recognition completed, detected " +
                   std::to_string(anomalies.size()) + " anomalies");

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed in advanced pattern recognition: " + std::string(e.what()));

        // Fallback to basic detection
        anomalies.push_back({
            {"pattern_type", "fallback_detection"},
            {"description", "Pattern recognition failed, using basic anomaly detection"},
            {"confidence", 0.5},
            {"severity", "LOW"},
            {"error", std::string(e.what())}
        });
    }

    return anomalies;
}

double AuditIntelligenceAgent::calculate_advanced_risk_score(const nlohmann::json& analysis_data) {
    try {
        // Advanced ML-based risk scoring with multiple factors

        double base_risk = 0.0;
        std::vector<std::pair<std::string, double>> risk_factors;

        // 1. Severity-based risk assessment
        if (analysis_data.contains("severity")) {
            std::string severity = analysis_data["severity"];
            if (severity == "CRITICAL") {
                base_risk += critical_severity_risk_;
                risk_factors.emplace_back("Critical severity event", critical_severity_risk_);
            } else if (severity == "HIGH") {
                base_risk += high_severity_risk_;
                risk_factors.emplace_back("High severity event", high_severity_risk_);
            } else if (severity == "MEDIUM") {
                base_risk += medium_severity_risk_;
                risk_factors.emplace_back("Medium severity event", medium_severity_risk_);
            } else if (severity == "LOW") {
                base_risk += low_severity_risk_;
                risk_factors.emplace_back("Low severity event", low_severity_risk_);
            }
        }

        // 2. Event type risk analysis using pattern matching
        if (analysis_data.contains("event_type")) {
            std::string event_type = analysis_data["event_type"];

            // High-risk event types
            if (event_type.find("FRAUD") != std::string::npos ||
                event_type.find("BREACH") != std::string::npos) {
                base_risk += 0.7;
                risk_factors.emplace_back("High-risk event type: " + event_type, 0.7);
            } else if (event_type.find("VIOLATION") != std::string::npos ||
                      event_type.find("NON_COMPLIANCE") != std::string::npos) {
                base_risk += 0.5;
                risk_factors.emplace_back("Compliance violation: " + event_type, 0.5);
            } else if (event_type.find("SUSPICIOUS") != std::string::npos ||
                      event_type.find("ANOMALY") != std::string::npos) {
                base_risk += 0.3;
                risk_factors.emplace_back("Suspicious activity: " + event_type, 0.3);
            }
        }

        // 3. Historical pattern analysis using statistical ML models
        double historical_risk = analyze_historical_patterns(analysis_data);
        if (historical_risk > 0) {
            base_risk += historical_risk * 0.4; // Weight historical patterns at 40%
            risk_factors.emplace_back("Historical pattern risk", historical_risk * 0.4);
        }

        // 4. Contextual risk assessment using LLM if available
        double contextual_risk = 0.0;
        try {
            contextual_risk = assess_contextual_risk_with_llm(analysis_data);
            if (contextual_risk > 0) {
                base_risk += contextual_risk * 0.3; // Weight contextual analysis at 30%
                risk_factors.emplace_back("Contextual AI analysis", contextual_risk * 0.3);
            }
        } catch (const std::exception& e) {
            logger_->log(LogLevel::DEBUG, "LLM contextual analysis failed, using rule-based only: " + std::string(e.what()));
        }

        // 5. Apply risk normalization and bounds checking
        double final_risk = std::min(std::max(base_risk, 0.0), 1.0);

        // Log risk assessment for audit trail
        std::string risk_factors_str;
        for (size_t i = 0; i < risk_factors.size(); ++i) {
            if (i > 0) risk_factors_str += ", ";
            risk_factors_str += risk_factors[i].first + ": " + std::to_string(risk_factors[i].second);
        }

        logger_->log(LogLevel::INFO, "Calculated advanced risk score: " + std::to_string(final_risk) +
                   " based on factors: " + risk_factors_str);

        return final_risk;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to calculate advanced risk score: " + std::string(e.what()));
        // Fallback to basic risk assessment
        return calculate_basic_risk_score(analysis_data);
    }
}

double AuditIntelligenceAgent::calculate_basic_risk_score(const nlohmann::json& audit_data) {
    double base_score = 0.2; // Conservative base risk

    if (audit_data.contains("severity")) {
        std::string severity = audit_data["severity"];
        if (severity == "CRITICAL") base_score += 0.5;
        else if (severity == "HIGH") base_score += 0.3;
        else if (severity == "MEDIUM") base_score += 0.1;
    }

    if (audit_data.contains("event_type")) {
        std::string event_type = audit_data["event_type"];
        if (event_type.find("FRAUD") != std::string::npos) base_score += 0.4;
        if (event_type.find("VIOLATION") != std::string::npos) base_score += 0.3;
    }

    return std::min(base_score, 1.0);
}

double AuditIntelligenceAgent::analyze_historical_patterns(const nlohmann::json& analysis_data) {
    try {
        // Production-grade pattern analysis using statistical methods and embeddings
        
        // Get recent audit trails for pattern comparison
        auto now = std::chrono::system_clock::now();
        auto week_ago = now - std::chrono::hours(24 * 7);

        auto recent_trails = audit_trail_->get_audit_trail_for_compliance(week_ago, now);

        if (recent_trails.empty()) return 0.0;

        // Extract features from current event for comparison
        std::string current_event_type = analysis_data.value("event_type", "");
        std::string current_severity = analysis_data.value("severity", "");
        double current_amount = analysis_data.value("amount", 0.0);
        std::string current_entity = analysis_data.value("entity_id", "");
        
        // Calculate pattern-based risk scores using multiple similarity metrics
        std::vector<double> similarity_scores;
        std::vector<double> event_severities;
        
        for (const auto& trail : recent_trails) {
            if (!trail.contains("final_decision")) continue;
            
            // Feature extraction from historical event
            std::string hist_event_type;
            std::string hist_severity;
            double hist_amount = 0.0;
            std::string hist_entity;
            
            if (trail.contains("event_type")) {
                hist_event_type = trail["event_type"];
            }
            if (trail.contains("severity")) {
                hist_severity = trail["severity"];
            }
            if (trail.contains("amount")) {
                hist_amount = trail["amount"];
            }
            if (trail.contains("entity_id")) {
                hist_entity = trail["entity_id"];
            }
            
            // Production-grade similarity calculation using multiple metrics
            double feature_similarity = 0.0;
            int feature_count = 0;
            
            // Event type similarity (categorical)
            if (!current_event_type.empty() && !hist_event_type.empty()) {
                feature_similarity += (current_event_type == hist_event_type) ? 1.0 : 0.0;
                feature_count++;
            }
            
            // Severity similarity (ordinal)
            if (!current_severity.empty() && !hist_severity.empty()) {
                // Map severity levels to numerical values
                std::unordered_map<std::string, int> severity_map = {
                    {"low", 1}, {"medium", 2}, {"high", 3}, {"critical", 4}
                };
                
                int curr_sev_val = severity_map[current_severity];
                int hist_sev_val = severity_map[hist_severity];
                
                if (curr_sev_val > 0 && hist_sev_val > 0) {
                    // Normalized distance: 1.0 (same), 0.0 (max difference)
                    double sev_similarity = 1.0 - (std::abs(curr_sev_val - hist_sev_val) / 3.0);
                    feature_similarity += sev_similarity;
                    feature_count++;
                    
                    // Track severity for weighting
                    event_severities.push_back(hist_sev_val / 4.0);
                }
            }
            
            // Amount similarity (numerical with log scaling)
            if (current_amount > 0 && hist_amount > 0) {
                double log_curr = std::log10(current_amount + 1);
                double log_hist = std::log10(hist_amount + 1);
                double log_diff = std::abs(log_curr - log_hist);
                
                // Use Gaussian similarity kernel: exp(-0.5 * (diff/sigma)^2)
                double sigma = 1.0;  // One order of magnitude standard deviation
                double amount_similarity = std::exp(-0.5 * (log_diff / sigma) * (log_diff / sigma));
                feature_similarity += amount_similarity;
                feature_count++;
            }
            
            // Entity similarity (same entity = higher risk)
            if (!current_entity.empty() && !hist_entity.empty()) {
                feature_similarity += (current_entity == hist_entity) ? 1.0 : 0.3;
                feature_count++;
            }
            
            // Calculate average feature similarity
            if (feature_count > 0) {
                double event_similarity = feature_similarity / feature_count;
                similarity_scores.push_back(event_similarity);
            }
        }

        if (similarity_scores.empty()) return 0.0;

        // Advanced aggregation of similarity scores
        
        // 1. Calculate mean similarity (baseline)
        double mean_similarity = 0.0;
        for (double score : similarity_scores) {
            mean_similarity += score;
        }
        mean_similarity /= similarity_scores.size();
        
        // 2. Calculate max similarity (closest match)
        double max_similarity = *std::max_element(similarity_scores.begin(), similarity_scores.end());
        
        // 3. Calculate weighted average (weight by severity if available)
        double weighted_similarity = mean_similarity;
        if (!event_severities.empty() && event_severities.size() == similarity_scores.size()) {
            double total_weight = 0.0;
            weighted_similarity = 0.0;
            
            for (size_t i = 0; i < similarity_scores.size(); ++i) {
                double weight = event_severities[i];
                weighted_similarity += similarity_scores[i] * weight;
                total_weight += weight;
            }
            
            if (total_weight > 0) {
                weighted_similarity /= total_weight;
            }
        }
        
        // 4. Calculate density (how many similar events)
        int high_similarity_count = 0;
        for (double score : similarity_scores) {
            if (score > 0.7) high_similarity_count++;
        }
        double density_factor = static_cast<double>(high_similarity_count) / similarity_scores.size();
        
        // Final risk score combines multiple factors
        double risk_score = 0.0;
        risk_score += mean_similarity * 0.30;        // 30% weight on average
        risk_score += max_similarity * 0.35;         // 35% weight on closest match
        risk_score += weighted_similarity * 0.20;    // 20% weight on severity-weighted
        risk_score += density_factor * 0.15;         // 15% weight on clustering

        // Scale to appropriate risk range
        return std::min(risk_score * 0.85, 0.75);

    } catch (const std::exception& e) {
        logger_->log(LogLevel::DEBUG, "Historical pattern analysis failed: " + std::string(e.what()));
        return 0.0;
    }
}

double AuditIntelligenceAgent::assess_contextual_risk_with_llm(const nlohmann::json& analysis_data) {
    try {
        // Use LLM for contextual risk assessment
        nlohmann::json context_data = {
            {"task", "contextual_risk_assessment"},
            {"event_data", analysis_data},
            {"analysis_type", "compliance_risk_evaluation"}
        };

        auto llm_response = llm_client_->complex_reasoning_task(
            "contextual_risk_analysis",
            context_data,
            2  // reasoning steps
        );

        if (llm_response && !llm_response->empty()) {
            // Parse structured LLM response for risk score
            return parse_structured_risk_response(*llm_response);
        }

        return 0.0; // Neutral if LLM analysis inconclusive

    } catch (const std::exception& e) {
        logger_->log(LogLevel::DEBUG, "LLM contextual risk assessment failed: " + std::string(e.what()));
        return 0.0;
    }
}

std::vector<nlohmann::json> AuditIntelligenceAgent::detect_temporal_anomalies(const std::vector<nlohmann::json>& audit_data) {
    std::vector<nlohmann::json> anomalies;

    try {
        // Analyze temporal patterns for anomalies
        std::map<std::string, std::vector<std::chrono::system_clock::time_point>> agent_timestamps;

        // Group timestamps by agent
        for (const auto& record : audit_data) {
            if (record.contains("agent_name") && record.contains("started_at")) {
                std::string agent_name = record["agent_name"];
                // Parse timestamp properly
                auto timestamp = parse_iso_timestamp(record["started_at"]);
                if (timestamp) {
                    agent_timestamps[agent_name].push_back(*timestamp);
                }
            }
        }

        // Detect unusual activity spikes
        for (const auto& [agent_name, timestamps] : agent_timestamps) {
            if (timestamps.size() < 5) continue; // Need minimum data points

            // Calculate activity rate (decisions per hour)
            auto time_span = timestamps.back() - timestamps.front();
            double hours = std::chrono::duration_cast<std::chrono::hours>(time_span).count();
            double rate = timestamps.size() / std::max(hours, 1.0);

            // Flag unusually high activity (more than 10 decisions/hour sustained)
            if (rate > 10.0 && timestamps.size() > 20) {
                anomalies.push_back({
                    {"pattern_type", "temporal_spike"},
                    {"description", "Unusual activity spike detected for agent: " + agent_name +
                                   " (" + std::to_string(rate) + " decisions/hour)"},
                    {"confidence", std::min(rate / 20.0, 0.95)}, // Confidence based on rate
                    {"severity", "HIGH"},
                    {"agent_name", agent_name},
                    {"activity_rate", rate}
                });
            }
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::DEBUG, "Temporal anomaly detection failed: " + std::string(e.what()));
    }

    return anomalies;
}

std::vector<nlohmann::json> AuditIntelligenceAgent::detect_behavioral_anomalies(const std::vector<nlohmann::json>& audit_data) {
    std::vector<nlohmann::json> anomalies;

    try {
        // Analyze behavioral consistency across agents
        std::map<std::string, std::vector<int>> agent_confidences;

        // Collect confidence scores by agent
        for (const auto& record : audit_data) {
            if (record.contains("agent_name") && record.contains("final_confidence")) {
                std::string agent_name = record["agent_name"];
                int confidence = record["final_confidence"];
                agent_confidences[agent_name].push_back(confidence);
            }
        }

        // Detect confidence score anomalies
        for (const auto& [agent_name, confidences] : agent_confidences) {
            if (confidences.size() < 10) continue; // Need sufficient data

            // Calculate statistics
            double sum = 0.0;
            for (int conf : confidences) sum += conf;
            double mean = sum / confidences.size();

            double variance = 0.0;
            for (int conf : confidences) {
                variance += std::pow(conf - mean, 2);
            }
            variance /= confidences.size();
            double std_dev = std::sqrt(variance);

            // Flag unusually inconsistent confidence scores
            if (std_dev > 2.0) { // High variance indicates inconsistency
                anomalies.push_back({
                    {"pattern_type", "behavioral_inconsistency"},
                    {"description", "Inconsistent decision confidence detected for agent: " + agent_name +
                                   " (std_dev: " + std::to_string(std_dev) + ")"},
                    {"confidence", std::min(std_dev / 3.0, 0.9)},
                    {"severity", "MEDIUM"},
                    {"agent_name", agent_name},
                    {"confidence_std_dev", std_dev}
                });
            }

            // Flag consistently low confidence (possible malfunction)
            if (mean < 1.0 && confidences.size() > 20) {
                anomalies.push_back({
                    {"pattern_type", "low_confidence_pattern"},
                    {"description", "Persistently low confidence scores for agent: " + agent_name +
                                   " (mean: " + std::to_string(mean) + ")"},
                    {"confidence", 0.8},
                    {"severity", "MEDIUM"},
                    {"agent_name", agent_name},
                    {"mean_confidence", mean}
                });
            }
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::DEBUG, "Behavioral anomaly detection failed: " + std::string(e.what()));
    }

    return anomalies;
}

std::vector<nlohmann::json> AuditIntelligenceAgent::detect_risk_correlation_anomalies(const std::vector<nlohmann::json>& audit_data) {
    std::vector<nlohmann::json> anomalies;

    try {
        // Analyze risk correlations and patterns
        std::vector<std::pair<int, int>> confidence_risk_pairs;

        for (const auto& record : audit_data) {
            if (record.contains("final_confidence") && record.contains("risk_assessment")) {
                int confidence = record["final_confidence"];

                // Extract risk score from risk_assessment JSON
                double risk_score = 0.5; // default
                if (record["risk_assessment"].contains("overall_risk_score")) {
                    risk_score = record["risk_assessment"]["overall_risk_score"];
                }

                confidence_risk_pairs.emplace_back(confidence, static_cast<int>(risk_score * 4)); // Scale to 0-4
            }
        }

        if (confidence_risk_pairs.size() < 20) return anomalies; // Need sufficient data

        // Calculate correlation between confidence and risk
        double correlation = calculate_correlation(confidence_risk_pairs);

        // Flag unusual correlations (very high negative correlation might indicate gaming)
        if (correlation < -0.7) {
            anomalies.push_back({
                {"pattern_type", "risk_confidence_correlation"},
                {"description", "Unusual negative correlation between confidence and risk scores: " +
                               std::to_string(correlation)},
                {"confidence", std::min(std::abs(correlation), 0.9)},
                {"severity", "HIGH"},
                {"correlation_coefficient", correlation}
            });
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::DEBUG, "Risk correlation anomaly detection failed: " + std::string(e.what()));
    }

    return anomalies;
}

std::string AuditIntelligenceAgent::generate_anomaly_insights(const nlohmann::json& anomaly) {
    try {
        nlohmann::json insight_data = {
            {"task", "anomaly_insight_generation"},
            {"anomaly_data", anomaly},
            {"analysis_type", "compliance_anomaly_interpretation"}
        };

        auto llm_response = llm_client_->complex_reasoning_task(
            "anomaly_insights",
            insight_data,
            1  // Single reasoning step for efficiency
        );

        return llm_response ? *llm_response : "AI analysis unavailable for this anomaly.";

    } catch (const std::exception& e) {
        return "Failed to generate AI insights: " + std::string(e.what());
    }
}

nlohmann::json AuditIntelligenceAgent::detect_bias_patterns(
    const std::unordered_map<std::string, int>& decision_counts,
    const std::unordered_map<std::string, int>& agent_counts) {

    nlohmann::json bias_analysis = {
        {"bias_detected", false},
        {"bias_indicators", nlohmann::json::array()},
        {"recommendations", nlohmann::json::array()}
    };

    try {
        // Analyze decision distribution for potential bias
        if (decision_counts.size() > 1) {
            // Check for skewed decision distributions
            int total_decisions = 0;
            for (const auto& [type, count] : decision_counts) {
                total_decisions += count;
            }

            for (const auto& [decision_type, count] : decision_counts) {
                double proportion = static_cast<double>(count) / total_decisions;

                // Flag if any decision type dominates (>80% of decisions)
                if (proportion > 0.8) {
                    bias_analysis["bias_detected"] = true;
                    bias_analysis["bias_indicators"].push_back({
                        {"type", "decision_distribution_bias"},
                        {"description", "Decision type '" + decision_type + "' dominates at " +
                                       std::to_string(proportion * 100) + "% of all decisions"},
                        {"severity", "MEDIUM"}
                    });
                    bias_analysis["recommendations"].push_back(
                        "Review decision logic for potential bias toward " + decision_type + " outcomes"
                    );
                }
            }
        }

        // Analyze agent activity distribution
        if (agent_counts.size() > 1) {
            int total_agent_decisions = 0;
            for (const auto& [agent, count] : agent_counts) {
                total_agent_decisions += count;
            }

            for (const auto& [agent, count] : agent_counts) {
                double proportion = static_cast<double>(count) / total_agent_decisions;

                // Flag if one agent handles >70% of decisions (potential single point of failure)
                if (proportion > 0.7) {
                    bias_analysis["bias_indicators"].push_back({
                        {"type", "agent_concentration_bias"},
                        {"description", "Agent '" + agent + "' handles " +
                                       std::to_string(proportion * 100) + "% of all decisions"},
                        {"severity", "LOW"}
                    });
                    bias_analysis["recommendations"].push_back(
                        "Consider redistributing workload to reduce single agent dependency"
                    );
                }
            }
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::DEBUG, "Bias pattern detection failed: " + std::string(e.what()));
        bias_analysis["error"] = std::string(e.what());
    }

    return bias_analysis;
}

nlohmann::json AuditIntelligenceAgent::detect_performance_anomalies(
    const std::unordered_map<std::string, std::vector<long long>>& processing_times) {

    nlohmann::json performance_analysis = {
        {"anomalies_detected", nlohmann::json::array()},
        {"performance_summary", nlohmann::json::object()}
    };

    try {
        for (const auto& [agent_type, times] : processing_times) {
            if (times.size() < 5) continue; // Need minimum samples

            // Calculate statistics
            long long sum = 0;
            long long min_time = *std::min_element(times.begin(), times.end());
            long long max_time = *std::max_element(times.begin(), times.end());
            for (long long time : times) sum += time;
            double mean = static_cast<double>(sum) / times.size();

            // Calculate standard deviation
            double variance = 0.0;
            for (long long time : times) {
                variance += std::pow(time - mean, 2);
            }
            variance /= times.size();
            double std_dev = std::sqrt(variance);

            // Detect performance anomalies
            double outlier_threshold = mean + 3 * std_dev;

            int outlier_count = 0;
            for (long long time : times) {
                if (time > outlier_threshold) outlier_count++;
            }

            if (outlier_count > times.size() * 0.1) { // More than 10% outliers
                performance_analysis["anomalies_detected"].push_back({
                    {"agent_type", agent_type},
                    {"anomaly_type", "performance_outliers"},
                    {"description", "High number of performance outliers detected: " +
                                   std::to_string(outlier_count) + "/" + std::to_string(times.size())},
                    {"severity", "MEDIUM"},
                    {"mean_time_ms", mean},
                    {"outlier_threshold_ms", outlier_threshold}
                });
            }

            // Flag consistently slow performance
            if (mean > 5000 && times.size() > 10) { // Over 5 seconds average
                performance_analysis["anomalies_detected"].push_back({
                    {"agent_type", agent_type},
                    {"anomaly_type", "slow_performance"},
                    {"description", "Consistently slow performance detected (mean: " +
                                   std::to_string(mean) + "ms)"},
                    {"severity", "HIGH"},
                    {"mean_time_ms", mean}
                });
            }

            performance_analysis["performance_summary"][agent_type] = {
                {"mean_ms", mean},
                {"min_ms", min_time},
                {"max_ms", max_time},
                {"std_dev_ms", std_dev},
                {"sample_count", times.size()}
            };
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::DEBUG, "Performance anomaly detection failed: " + std::string(e.what()));
        performance_analysis["error"] = std::string(e.what());
    }

    return performance_analysis;
}

double AuditIntelligenceAgent::calculate_correlation(const std::vector<std::pair<int, int>>& data_points) {
    if (data_points.size() < 2) return 0.0;

    // Calculate Pearson correlation coefficient
    double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0;
    double sum_x2 = 0.0, sum_y2 = 0.0;
    size_t n = data_points.size();

    for (const auto& [x, y] : data_points) {
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_x2 += x * x;
        sum_y2 += y * y;
    }

    double numerator = n * sum_xy - sum_x * sum_y;
    double denominator = std::sqrt((n * sum_x2 - sum_x * sum_x) * (n * sum_y2 - sum_y * sum_y));

    return denominator != 0.0 ? numerator / denominator : 0.0;
}

std::string AuditIntelligenceAgent::generate_compliance_insights(const std::vector<nlohmann::json>& audit_data) {
    try {
        nlohmann::json insight_request = {
            {"task", "compliance_insights_generation"},
            {"audit_data", audit_data},
            {"analysis_focus", "compliance_patterns_and_recommendations"}
        };

        auto insights = llm_client_->complex_reasoning_task("compliance_insights_generation", insight_request, 3);

        return insights ? *insights : "Unable to generate AI-powered compliance insights at this time.";

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to generate compliance insights: " + std::string(e.what()));
        return "Error generating compliance insights: " + std::string(e.what());
    }
}

double AuditIntelligenceAgent::extract_risk_score_from_llm_response(const std::string& llm_response) {
    try {
        // Advanced pattern matching to extract risk score from LLM response
        std::string response = llm_response;
        std::transform(response.begin(), response.end(), response.begin(), ::tolower);

        // Look for explicit risk score mentions
        std::regex risk_pattern(R"(risk[_ ]?score[_ ]?:?\s*([0-9]*\.?[0-9]+))");
        std::smatch matches;
        if (std::regex_search(response, matches, risk_pattern) && matches.size() > 1) {
            try {
                double extracted_score = std::stod(matches[1].str());
                return std::min(std::max(extracted_score, 0.0), 1.0); // Clamp to [0,1]
            } catch (const std::exception&) {
                // Continue to keyword analysis
            }
        }

        // Keyword-based risk assessment
        double keyword_risk = 0.0;
        if (response.find("high risk") != std::string::npos ||
            response.find("critical") != std::string::npos ||
            response.find("severe") != std::string::npos) {
            keyword_risk += 0.7;
        }
        if (response.find("medium risk") != std::string::npos ||
            response.find("moderate") != std::string::npos) {
            keyword_risk += 0.5;
        }
        if (response.find("low risk") != std::string::npos ||
            response.find("minimal") != std::string::npos) {
            keyword_risk += 0.2;
        }

        // Look for fraud indicators
        if (response.find("fraud") != std::string::npos ||
            response.find("suspicious") != std::string::npos ||
            response.find("anomal") != std::string::npos) {
            keyword_risk += 0.3;
        }

        return std::min(keyword_risk, 1.0);

    } catch (const std::exception& e) {
        logger_->log(LogLevel::DEBUG, "Failed to extract risk score from LLM response: " + std::string(e.what()));
        return 0.5; // Conservative default
    }
}

nlohmann::json AuditIntelligenceAgent::generate_fraud_recommendations(double risk_score, const nlohmann::json& transaction_data) {
    nlohmann::json recommendations = nlohmann::json::array();

    try {
        // Generate risk-appropriate recommendations based on actual risk calculation
        if (risk_score > 0.8) {
            recommendations.push_back("🚨 CRITICAL: Immediately freeze transaction and initiate emergency fraud investigation protocol");
            recommendations.push_back("📞 Contact customer via multiple verified channels within 30 minutes for verification");
            recommendations.push_back("👥 Escalate to senior fraud analyst and legal team immediately");
            recommendations.push_back("🔒 Implement enhanced security measures for account and similar transaction patterns");
            recommendations.push_back("📊 Generate detailed forensic analysis report for regulatory compliance");
        } else if (risk_score > 0.6) {
            recommendations.push_back("⚠️ HIGH PRIORITY: Enhanced verification required before processing - do not auto-approve");
            recommendations.push_back("📞 Contact customer for additional verification using secondary authentication");
            recommendations.push_back("👁️ Monitor account activity for 48 hours post-transaction with enhanced scrutiny");
            recommendations.push_back("🔍 Review transaction against customer's complete historical pattern database");
            recommendations.push_back("📝 Document all verification steps and rationales");
        } else if (risk_score > 0.4) {
            recommendations.push_back("🟡 MEDIUM PRIORITY: Additional verification recommended - consider manual review");
            recommendations.push_back("📱 Send verification code to all registered contact methods and require response");
            recommendations.push_back("🏷️ Flag transaction for senior review with detailed risk assessment attached");
            recommendations.push_back("📊 Monitor for related suspicious activity patterns across the platform");
            recommendations.push_back("⏰ Allow processing only after verification completion (maximum 4 hours)");
        } else if (risk_score > 0.2) {
            recommendations.push_back("🟢 LOW PRIORITY: Standard verification sufficient but monitor closely");
            recommendations.push_back("📝 Log transaction for ongoing pattern analysis and model training");
            recommendations.push_back("👁️ Continue standard post-transaction monitoring protocols");
            recommendations.push_back("📊 Include in regular risk assessment reports");
        } else {
            recommendations.push_back("✅ VERY LOW RISK: Process normally with standard protocols");
            recommendations.push_back("📝 No additional verification required - maintain routine monitoring");
            recommendations.push_back("📊 Use as positive training example for fraud detection models");
        }

        // Add transaction-specific recommendations based on actual transaction data
        if (transaction_data.contains("amount")) {
            double amount = transaction_data["amount"].get<double>();
            if (amount > config_->get_double("TRANSACTION_MAX_AMOUNT_INSTITUTION").value_or(100000.0)) {
                recommendations.push_back("💰 EXTREME HIGH-VALUE TRANSACTION: Requires C-suite approval regardless of risk score");
            } else if (amount > config_->get_double("TRANSACTION_MAX_AMOUNT_BUSINESS").value_or(50000.0)) {
                recommendations.push_back("💰 HIGH-VALUE TRANSACTION: Requires senior management approval for processing");
            } else if (amount > config_->get_double("TRANSACTION_MAX_AMOUNT_INDIVIDUAL").value_or(10000.0)) {
                recommendations.push_back("💰 ELEVATED AMOUNT: Enhanced verification required for high-value transaction");
            }
        }

        if (transaction_data.contains("location") && transaction_data.contains("usual_location")) {
            std::string location = transaction_data["location"];
            std::string usual_location = transaction_data["usual_location"];
            if (location != usual_location) {
                recommendations.push_back("🌍 GEOGRAPHIC ANOMALY: Transaction from unusual location - verify legitimacy and check for account compromise");
                recommendations.push_back("📍 Cross-border transaction detected - apply enhanced regulatory compliance checks");
            }
        }

        // Add velocity-based recommendations
        if (transaction_data.contains("recent_transactions")) {
            int recent_count = transaction_data["recent_transactions"];
            double velocity_threshold = config_->get_double("TRANSACTION_VELOCITY_THRESHOLD").value_or(0.7);
            if (recent_count > 10) { // High velocity
                recommendations.push_back("⚡ HIGH VELOCITY: Unusual transaction frequency detected - investigate for automated attacks");
            }
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to generate fraud recommendations: " + std::string(e.what()));
        recommendations = {"🚨 MANUAL REVIEW REQUIRED: Error in automated fraud assessment - immediate human review mandatory"};
    }

    return recommendations;
}

double AuditIntelligenceAgent::adjust_risk_for_transaction_characteristics(double base_risk, const nlohmann::json& transaction_data) {
    double adjusted_risk = base_risk;

    try {
        // Amount-based risk adjustment
        if (transaction_data.contains("amount")) {
            double amount = transaction_data["amount"].get<double>();
            double max_individual = config_->get_double("TRANSACTION_MAX_AMOUNT_INDIVIDUAL").value_or(10000.0);
            double max_business = config_->get_double("TRANSACTION_MAX_AMOUNT_BUSINESS").value_or(50000.0);

            if (amount > max_business) {
                adjusted_risk += 0.3; // Institutional amounts significantly increase risk
            } else if (amount > max_individual) {
                adjusted_risk += 0.2; // Business amounts moderately increase risk
            } else if (amount > max_individual * 0.5) {
                adjusted_risk += 0.1; // Large individual amounts slightly increase risk
            }
        }

        // Geographic risk adjustment
        if (transaction_data.contains("location") && transaction_data.contains("usual_location")) {
            std::string location = transaction_data["location"];
            std::string usual_location = transaction_data["usual_location"];

            if (location != usual_location) {
                adjusted_risk += 0.25; // Geographic anomalies significantly impact risk

                // Check for sanctioned countries
                std::string sanctioned_countries = config_->get_string("SANCTIONED_COUNTRIES").value_or("IR,KP,SY,CU");
                if (sanctioned_countries.find(location) != std::string::npos) {
                    adjusted_risk += 0.4; // Sanctioned countries massively increase risk
                }
            }
        }

        // Transaction velocity risk adjustment
        if (transaction_data.contains("recent_transactions")) {
            int recent_count = transaction_data["recent_transactions"];
            if (recent_count > 20) {
                adjusted_risk += 0.35; // Very high velocity indicates potential fraud
            } else if (recent_count > 10) {
                adjusted_risk += 0.2; // High velocity increases risk
            } else if (recent_count > 5) {
                adjusted_risk += 0.1; // Moderate velocity slightly increases risk
            }
        }

        // Time-of-day risk adjustment (unusual hours)
        if (transaction_data.contains("timestamp")) {
            // Parse timestamp properly to extract hour
            auto timestamp_opt = parse_iso_timestamp(transaction_data["timestamp"]);
            if (timestamp_opt) {
                auto time_t_val = std::chrono::system_clock::to_time_t(*timestamp_opt);
                std::tm* tm_ptr = std::localtime(&time_t_val);
                if (tm_ptr) {
                    int hours = tm_ptr->tm_hour;

                    if (hours < 6 || hours > 22) { // Unusual hours (2 AM - 6 AM)
                        adjusted_risk += 0.15; // Unusual timing increases fraud risk
                    }
                }
            }
        }

        // Ensure risk stays within bounds
        adjusted_risk = std::min(std::max(adjusted_risk, 0.0), 1.0);

        logger_->log(LogLevel::DEBUG, "Risk adjusted from " + std::to_string(base_risk) +
                   " to " + std::to_string(adjusted_risk) + " based on transaction characteristics");

    } catch (const std::exception& e) {
        logger_->log(LogLevel::DEBUG, "Failed to adjust risk for transaction characteristics: " + std::string(e.what()));
    }

    return adjusted_risk;
}

nlohmann::json AuditIntelligenceAgent::identify_fraud_indicators(const nlohmann::json& transaction_data, const std::string& llm_response) {
    nlohmann::json indicators = nlohmann::json::array();

    try {
        // Extract fraud indicators from transaction data and LLM analysis
        std::string llm_lower = llm_response;
        std::transform(llm_lower.begin(), llm_lower.end(), llm_lower.begin(), ::tolower);

        // Amount-based indicators
        if (transaction_data.contains("amount")) {
            double amount = transaction_data["amount"].get<double>();
            if (amount > config_->get_double("TRANSACTION_MAX_AMOUNT_INSTITUTION").value_or(100000.0)) {
                indicators.push_back({
                    {"type", "amount_anomaly"},
                    {"description", "Transaction amount exceeds institutional limits"},
                    {"severity", "critical"},
                    {"amount", amount}
                });
            }
        }

        // Geographic indicators
        if (transaction_data.contains("location") && transaction_data.contains("usual_location")) {
            std::string location = transaction_data["location"];
            std::string usual_location = transaction_data["usual_location"];
            if (location != usual_location) {
                indicators.push_back({
                    {"type", "geographic_anomaly"},
                    {"description", "Transaction from unusual geographic location"},
                    {"severity", "high"},
                    {"location", location},
                    {"usual_location", usual_location}
                });
            }
        }

        // LLM-detected indicators
        if (llm_lower.find("suspicious") != std::string::npos) {
            indicators.push_back({
                {"type", "llm_suspicious_pattern"},
                {"description", "AI detected suspicious patterns in transaction analysis"},
                {"severity", "high"},
                {"source", "llm_analysis"}
            });
        }

        if (llm_lower.find("unusual") != std::string::npos ||
            llm_lower.find("anomal") != std::string::npos) {
            indicators.push_back({
                {"type", "llm_anomaly_detected"},
                {"description", "AI detected anomalous transaction characteristics"},
                {"severity", "medium"},
                {"source", "llm_analysis"}
            });
        }

        // Velocity indicators
        if (transaction_data.contains("recent_transactions")) {
            int recent_count = transaction_data["recent_transactions"];
            if (recent_count > 15) {
                indicators.push_back({
                    {"type", "high_velocity"},
                    {"description", "Unusually high transaction velocity detected"},
                    {"severity", "high"},
                    {"transaction_count", recent_count}
                });
            }
        }

        // Time-based indicators
        if (transaction_data.contains("timestamp")) {
            // Check for rapid succession transactions
            if (transaction_data.contains("time_since_last_transaction")) {
                double time_diff = transaction_data["time_since_last_transaction"];
                if (time_diff < 60) { // Less than 1 minute between transactions
                    indicators.push_back({
                        {"type", "rapid_succession"},
                        {"description", "Multiple transactions in rapid succession"},
                        {"severity", "medium"},
                        {"time_difference_seconds", time_diff}
                    });
                }
            }
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::DEBUG, "Failed to identify fraud indicators: " + std::string(e.what()));
        indicators.push_back({
            {"type", "analysis_error"},
            {"description", "Failed to complete fraud indicator analysis"},
            {"severity", "unknown"},
            {"error", std::string(e.what())}
        });
    }

    return indicators;
}

double AuditIntelligenceAgent::calculate_baseline_fraud_risk(const nlohmann::json& transaction_data) {
    double baseline_risk = 0.3; // Conservative baseline

    try {
        // Calculate baseline risk without LLM analysis using rule-based heuristics

        // Amount-based risk
        if (transaction_data.contains("amount")) {
            double amount = transaction_data["amount"].get<double>();
            if (amount > 50000) baseline_risk += 0.4;
            else if (amount > 10000) baseline_risk += 0.2;
            else if (amount > 1000) baseline_risk += 0.1;
        }

        // Geographic risk
        if (transaction_data.contains("location") && transaction_data.contains("usual_location")) {
            if (transaction_data["location"] != transaction_data["usual_location"]) {
                baseline_risk += 0.25;
            }
        }

        // Velocity risk
        if (transaction_data.contains("recent_transactions")) {
            int count = transaction_data["recent_transactions"];
            if (count > 10) baseline_risk += 0.2;
            else if (count > 5) baseline_risk += 0.1;
        }

        // Time-based risk using transaction timestamp
        if (transaction_data.contains("timestamp")) {
            auto timestamp_opt = parse_iso_timestamp(transaction_data["timestamp"]);
            if (timestamp_opt) {
                auto time_t_val = std::chrono::system_clock::to_time_t(*timestamp_opt);
                std::tm* tm_ptr = std::localtime(&time_t_val);
                if (tm_ptr) {
                    int hours = tm_ptr->tm_hour;
                    if (hours < 6 || hours > 22) { // Unusual hours
                        baseline_risk += 0.1;
                    }
                }
            }
        } else {
            // Fallback to current time if no timestamp
            auto now = std::chrono::system_clock::now();
            auto hours = std::chrono::duration_cast<std::chrono::hours>(
                now.time_since_epoch()).count() % 24;
            if (hours < 6 || hours > 22) {
                baseline_risk += 0.1;
            }
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::DEBUG, "Failed to calculate baseline fraud risk: " + std::string(e.what()));
    }

    return std::min(baseline_risk, 1.0);
}

nlohmann::json AuditIntelligenceAgent::generate_basic_fraud_recommendations(const nlohmann::json& transaction_data) {
    nlohmann::json recommendations = nlohmann::json::array();

    try {
        recommendations.push_back("⚠️ AI ANALYSIS UNAVAILABLE: Conduct manual fraud review with enhanced scrutiny");
        recommendations.push_back("📞 Contact customer using primary and secondary verification methods");
        recommendations.push_back("👁️ Implement enhanced monitoring for account and similar transactions");

        // Amount-specific recommendations
        if (transaction_data.contains("amount")) {
            double amount = transaction_data["amount"].get<double>();
            if (amount > 10000) {
                recommendations.push_back("💰 HIGH-VALUE TRANSACTION: Requires senior approval for processing");
            }
        }

        // Geographic recommendations
        if (transaction_data.contains("location") && transaction_data.contains("usual_location")) {
            if (transaction_data["location"] != transaction_data["usual_location"]) {
                recommendations.push_back("🌍 GEOGRAPHIC ANOMALY: Verify transaction legitimacy thoroughly");
            }
        }

        recommendations.push_back("📝 Document all manual review steps and decision rationales");
        recommendations.push_back("⏰ Complete review within 4 hours or escalate to supervisor");

    } catch (const std::exception& e) {
        logger_->log(LogLevel::DEBUG, "Failed to generate basic fraud recommendations: " + std::string(e.what()));
        recommendations = {"🚨 CRITICAL: Manual review failed - escalate immediately to fraud team"};
    }

    return recommendations;
}

std::vector<ComplianceEvent> AuditIntelligenceAgent::analyze_decision_anomalies() {
    // This method is deprecated - use analyze_audit_trails instead
    return analyze_audit_trails(24);
}

std::vector<nlohmann::json> AuditIntelligenceAgent::perform_pattern_recognition() {
    // This method is deprecated - use perform_advanced_pattern_recognition instead
    return perform_advanced_pattern_recognition({});
}

std::vector<AgentDecision> AuditIntelligenceAgent::convert_audit_trails_to_decisions(const std::vector<nlohmann::json>& audit_trails) {
    std::vector<AgentDecision> decisions;

    for (const auto& trail : audit_trails) {
        try {
            AgentDecision decision(
                string_to_decision_type(trail.value("final_decision", "MONITOR")),
                int_to_confidence_level(trail.value("final_confidence", 2)),
                trail.value("agent_name", "unknown_agent"),
                trail.value("trail_id", "unknown_trail")
            );

            // Add reasoning based on audit trail data
            DecisionReasoning reasoning;
            reasoning.factor = "Audit trail analysis";
            reasoning.evidence = "Processing time: " + std::to_string(trail.value("total_processing_time_ms", 0)) + "ms";
            reasoning.weight = trail.value("final_confidence", 50) / 100.0;
            reasoning.source = "AuditIntelligenceAgent";
            decision.add_reasoning(reasoning);

            // Set risk assessment if available
            if (trail.contains("risk_assessment")) {
                RiskAssessment risk_assessment;
                auto risk_data = trail["risk_assessment"];
                if (risk_data.contains("overall_risk_score")) {
                    risk_assessment.risk_score = risk_data["overall_risk_score"];
                }
                if (risk_data.contains("risk_level")) {
                    risk_assessment.risk_level = risk_data["risk_level"];
                }
                decision.set_risk_assessment(risk_assessment);
            }

            decisions.push_back(decision);

        } catch (const std::exception& e) {
            logger_->log(LogLevel::WARN, "Failed to convert audit trail to decision: " + std::string(e.what()));
        }
    }

    return decisions;
}

DecisionType AuditIntelligenceAgent::string_to_decision_type(const std::string& decision_str) {
    if (decision_str == "APPROVE") return DecisionType::APPROVE;
    if (decision_str == "DENY") return DecisionType::DENY;
    if (decision_str == "ESCALATE") return DecisionType::ESCALATE;
    if (decision_str == "INVESTIGATE") return DecisionType::INVESTIGATE;
    if (decision_str == "ALERT") return DecisionType::ALERT;
    return DecisionType::MONITOR;
}

ConfidenceLevel AuditIntelligenceAgent::int_to_confidence_level(int confidence_int) {
    if (confidence_int >= 80) return ConfidenceLevel::HIGH;
    if (confidence_int >= 60) return ConfidenceLevel::MEDIUM;
    return ConfidenceLevel::LOW;
}

double AuditIntelligenceAgent::parse_structured_risk_response(const std::string& llm_response) {
    try {
        // Try to parse as JSON first (structured output)
        nlohmann::json parsed_response = nlohmann::json::parse(llm_response);

        if (parsed_response.contains("risk_score")) {
            double score = parsed_response["risk_score"];
            return std::min(std::max(score, 0.0), 1.0);
        }

        if (parsed_response.contains("risk_level")) {
            std::string level = parsed_response["risk_level"];
            std::transform(level.begin(), level.end(), level.begin(), ::tolower);

            if (level == "critical" || level == "high") return 0.8;
            if (level == "medium") return 0.5;
            if (level == "low") return 0.2;
        }

        if (parsed_response.contains("confidence")) {
            double confidence = parsed_response["confidence"];
            return std::min(std::max(confidence, 0.0), 1.0);
        }

    } catch (const nlohmann::json::parse_error&) {
        // Not JSON, fall back to text parsing
    }

    // Fallback to text parsing with improved pattern matching
    std::string response = llm_response;
    std::transform(response.begin(), response.end(), response.begin(), ::tolower);

    // Look for explicit risk score patterns
    std::regex score_pattern(R"(risk[_ ]?score[_ ]?:?\s*([0-9]*\.?[0-9]+))");
    std::smatch matches;
    if (std::regex_search(response, matches, score_pattern) && matches.size() > 1) {
        try {
            double score = std::stod(matches[1].str());
            return std::min(std::max(score, 0.0), 1.0);
        } catch (const std::exception&) {
            // Continue to keyword analysis
        }
    }

    // Enhanced keyword analysis with context
    double keyword_score = 0.0;
    int keyword_count = 0;

    // Critical/high risk indicators
    if (response.find("critical") != std::string::npos ||
        response.find("severe") != std::string::npos ||
        response.find("extremely high") != std::string::npos) {
        keyword_score += 0.9;
        keyword_count++;
    } else if (response.find("high risk") != std::string::npos ||
               response.find("very high") != std::string::npos) {
        keyword_score += 0.8;
        keyword_count++;
    }

    // Medium risk indicators
    if (response.find("medium risk") != std::string::npos ||
        response.find("moderate") != std::string::npos ||
        response.find("concerning") != std::string::npos) {
        keyword_score += 0.5;
        keyword_count++;
    }

    // Low risk indicators
    if (response.find("low risk") != std::string::npos ||
        response.find("minimal") != std::string::npos ||
        response.find("very low") != std::string::npos) {
        keyword_score += 0.1;
        keyword_count++;
    }

    // Additional context-based scoring
    if (response.find("suspicious") != std::string::npos ||
        response.find("anomal") != std::string::npos) {
        keyword_score += 0.2;
    }

    if (response.find("normal") != std::string::npos ||
        response.find("typical") != std::string::npos) {
        keyword_score -= 0.1;
    }

    // Return average if keywords found, otherwise neutral
    return keyword_count > 0 ? std::min(keyword_score / keyword_count, 1.0) : 0.0;
}

std::optional<std::chrono::system_clock::time_point> AuditIntelligenceAgent::parse_iso_timestamp(const std::string& timestamp_str) {
    try {
        // Handle different timestamp formats
        std::string ts = timestamp_str;

        // If it's a numeric timestamp (seconds since epoch)
        if (ts.find_first_not_of("0123456789.") == std::string::npos) {
            try {
                time_t seconds = static_cast<time_t>(std::stod(ts));
                return std::chrono::system_clock::from_time_t(seconds);
            } catch (const std::exception&) {
                // Continue to ISO parsing
            }
        }

        // Try ISO 8601 parsing (YYYY-MM-DDTHH:MM:SSZ or similar)
        std::tm tm = {};
        std::istringstream ss(ts);

        // Try different ISO formats
        if (ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S")) {
            // Check for timezone indicator
            std::string remainder;
            std::getline(ss, remainder);
            if (remainder.find('Z') != std::string::npos || remainder.find('+') != std::string::npos) {
                // UTC or offset - tm is already in correct timezone
            } else if (remainder.find('-') != std::string::npos) {
                // Handle timezone offset if needed
            }

            time_t time = timegm(&tm); // Use thread-safe version
            return std::chrono::system_clock::from_time_t(time);
        }

        // Try alternative timestamp format (YYYY-MM-DD HH:MM:SS)
        ss.clear();
        ss.str(ts);
        if (ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S")) {
            time_t time = timegm(&tm);
            return std::chrono::system_clock::from_time_t(time);
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::DEBUG, "Failed to parse timestamp '" + timestamp_str + "': " + std::string(e.what()));
    }

    return std::nullopt;
}

double AuditIntelligenceAgent::analyze_time_based_risk_patterns(const nlohmann::json& analysis_data) {
    try {
        // Analyze risk patterns over time using historical audit trails
        auto now = std::chrono::system_clock::now();
        auto week_ago = now - std::chrono::hours(24 * 7);

        auto recent_trails = audit_trail_->get_audit_trail_for_compliance(week_ago, now);

        if (recent_trails.empty()) return 0.0;

        // Group by time periods (hourly)
        std::map<int, std::vector<nlohmann::json>> hourly_patterns;
        std::map<std::string, std::vector<nlohmann::json>> agent_patterns;

        for (const auto& trail : recent_trails) {
            // Parse timestamp and group by hour
            if (trail.contains("started_at")) {
                auto timestamp_opt = parse_iso_timestamp(trail["started_at"]);
                if (timestamp_opt) {
                    auto time_t_val = std::chrono::system_clock::to_time_t(*timestamp_opt);
                    std::tm* tm_ptr = std::gmtime(&time_t_val);
                    if (tm_ptr) {
                        int hour = tm_ptr->tm_hour;
                        hourly_patterns[hour].push_back(trail);
                    }
                }
            }

            // Group by agent
            if (trail.contains("agent_name")) {
                agent_patterns[trail["agent_name"]].push_back(trail);
            }
        }

        double time_based_risk = 0.0;
        int risk_factors = 0;

        // Analyze hourly patterns for unusual activity spikes
        for (const auto& [hour, trails] : hourly_patterns) {
            if (trails.size() > 50) { // High activity hour
                time_based_risk += 0.2;
                risk_factors++;
            }
        }

        // Analyze agent consistency
        for (const auto& [agent, trails] : agent_patterns) {
            if (trails.size() > 10) {
                // Calculate confidence variance
                std::vector<double> confidences;
                for (const auto& trail : trails) {
                    if (trail.contains("final_confidence")) {
                        confidences.push_back(trail["final_confidence"].get<double>() / 100.0);
                    }
                }

                if (confidences.size() > 5) {
                    double mean = 0.0;
                    for (double conf : confidences) mean += conf;
                    mean /= confidences.size();

                    double variance = 0.0;
                    for (double conf : confidences) {
                        variance += std::pow(conf - mean, 2);
                    }
                    variance /= confidences.size();
                    double std_dev = std::sqrt(variance);

                    if (std_dev > 0.3) { // High variance in agent confidence
                        time_based_risk += 0.15;
                        risk_factors++;
                    }
                }
            }
        }

        // Analyze escalation patterns over time
        int recent_escalations = 0;
        for (const auto& trail : recent_trails) {
            if (trail.contains("final_decision") &&
                trail["final_decision"] == "ESCALATE") {
                recent_escalations++;
            }
        }

        if (recent_escalations > recent_trails.size() * 0.1) { // >10% escalations
            time_based_risk += 0.25;
            risk_factors++;
        }

        return risk_factors > 0 ? std::min(time_based_risk / risk_factors, 0.5) : 0.0;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::DEBUG, "Time-based risk pattern analysis failed: " + std::string(e.what()));
        return 0.0;
    }
}

} // namespace regulens
