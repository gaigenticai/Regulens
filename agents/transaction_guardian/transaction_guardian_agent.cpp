#include "transaction_guardian_agent.hpp"

#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <random>
#include <cctype>

namespace regulens {

TransactionGuardianAgent::TransactionGuardianAgent(
    std::shared_ptr<ConfigurationManager> config,
    std::shared_ptr<StructuredLogger> logger,
                                               std::shared_ptr<ConnectionPool> db_pool,
    std::shared_ptr<AnthropicClient> llm_client,
    std::shared_ptr<RiskAssessmentEngine> risk_engine)
    : config_(config), logger_(logger), db_pool_(db_pool), llm_client_(llm_client),
      risk_engine_(risk_engine), running_(false), transactions_processed_(0),
      suspicious_transactions_detected_(0) {
}

TransactionGuardianAgent::~TransactionGuardianAgent() {
    stop();
}

bool TransactionGuardianAgent::initialize() {
    try {
        logger_->log(LogLevel::INFO, "Initializing Transaction Guardian Agent");

        // Load configuration parameters - all values are required for production
        auto fraud_threshold_opt = config_->get_double("TRANSACTION_FRAUD_THRESHOLD");
        if (!fraud_threshold_opt) {
            logger_->log(LogLevel::ERROR, "Missing required configuration: TRANSACTION_FRAUD_THRESHOLD");
            return false;
        }
        fraud_threshold_ = *fraud_threshold_opt;

        auto velocity_threshold_opt = config_->get_double("TRANSACTION_VELOCITY_THRESHOLD");
        if (!velocity_threshold_opt) {
            logger_->log(LogLevel::ERROR, "Missing required configuration: TRANSACTION_VELOCITY_THRESHOLD");
            return false;
        }
        velocity_threshold_ = *velocity_threshold_opt;

        auto high_risk_threshold_opt = config_->get_double("TRANSACTION_HIGH_RISK_THRESHOLD");
        if (!high_risk_threshold_opt) {
            logger_->log(LogLevel::ERROR, "Missing required configuration: TRANSACTION_HIGH_RISK_THRESHOLD");
            return false;
        }
        high_risk_threshold_ = *high_risk_threshold_opt;

        auto analysis_window_opt = config_->get_int("TRANSACTION_ANALYSIS_WINDOW_MINUTES");
        if (!analysis_window_opt) {
            logger_->log(LogLevel::ERROR, "Missing required configuration: TRANSACTION_ANALYSIS_WINDOW_MINUTES");
            return false;
        }
        analysis_window_ = std::chrono::minutes(*analysis_window_opt);

        // Load risk calculation parameters - all values are required for production
        auto risk_amount_100k_opt = config_->get_double("TRANSACTION_RISK_AMOUNT_100K");
        if (!risk_amount_100k_opt) {
            logger_->log(LogLevel::ERROR, "Missing required configuration: TRANSACTION_RISK_AMOUNT_100K");
            return false;
        }
        risk_amount_100k_ = *risk_amount_100k_opt;

        auto risk_amount_50k_opt = config_->get_double("TRANSACTION_RISK_AMOUNT_50K");
        if (!risk_amount_50k_opt) {
            logger_->log(LogLevel::ERROR, "Missing required configuration: TRANSACTION_RISK_AMOUNT_50K");
            return false;
        }
        risk_amount_50k_ = *risk_amount_50k_opt;

        auto risk_amount_10k_opt = config_->get_double("TRANSACTION_RISK_AMOUNT_10K");
        if (!risk_amount_10k_opt) {
            logger_->log(LogLevel::ERROR, "Missing required configuration: TRANSACTION_RISK_AMOUNT_10K");
            return false;
        }
        risk_amount_10k_ = *risk_amount_10k_opt;

        auto risk_international_opt = config_->get_double("TRANSACTION_RISK_INTERNATIONAL");
        if (!risk_international_opt) {
            logger_->log(LogLevel::ERROR, "Missing required configuration: TRANSACTION_RISK_INTERNATIONAL");
            return false;
        }
        risk_international_ = *risk_international_opt;

        auto risk_crypto_opt = config_->get_double("TRANSACTION_RISK_CRYPTO");
        if (!risk_crypto_opt) {
            logger_->log(LogLevel::ERROR, "Missing required configuration: TRANSACTION_RISK_CRYPTO");
            return false;
        }
        risk_crypto_ = *risk_crypto_opt;

        auto velocity_critical_threshold_opt = config_->get_double("TRANSACTION_VELOCITY_CRITICAL_THRESHOLD");
        if (!velocity_critical_threshold_opt) {
            logger_->log(LogLevel::ERROR, "Missing required configuration: TRANSACTION_VELOCITY_CRITICAL_THRESHOLD");
            return false;
        }
        velocity_critical_threshold_ = *velocity_critical_threshold_opt;

        auto velocity_high_threshold_opt = config_->get_double("TRANSACTION_VELOCITY_HIGH_THRESHOLD");
        if (!velocity_high_threshold_opt) {
            logger_->log(LogLevel::ERROR, "Missing required configuration: TRANSACTION_VELOCITY_HIGH_THRESHOLD");
            return false;
        }
        velocity_high_threshold_ = *velocity_high_threshold_opt;

        auto velocity_moderate_threshold_opt = config_->get_double("TRANSACTION_VELOCITY_MODERATE_THRESHOLD");
        if (!velocity_moderate_threshold_opt) {
            logger_->log(LogLevel::ERROR, "Missing required configuration: TRANSACTION_VELOCITY_MODERATE_THRESHOLD");
            return false;
        }
        velocity_moderate_threshold_ = *velocity_moderate_threshold_opt;

        auto velocity_ratio_5x_opt = config_->get_double("TRANSACTION_VELOCITY_RATIO_5X");
        if (!velocity_ratio_5x_opt) {
            logger_->log(LogLevel::ERROR, "Missing required configuration: TRANSACTION_VELOCITY_RATIO_5X");
            return false;
        }
        velocity_ratio_5x_ = *velocity_ratio_5x_opt;

        auto velocity_ratio_3x_opt = config_->get_double("TRANSACTION_VELOCITY_RATIO_3X");
        if (!velocity_ratio_3x_opt) {
            logger_->log(LogLevel::ERROR, "Missing required configuration: TRANSACTION_VELOCITY_RATIO_3X");
            return false;
        }
        velocity_ratio_3x_ = *velocity_ratio_3x_opt;

        auto velocity_ratio_2x_opt = config_->get_double("TRANSACTION_VELOCITY_RATIO_2X");
        if (!velocity_ratio_2x_opt) {
            logger_->log(LogLevel::ERROR, "Missing required configuration: TRANSACTION_VELOCITY_RATIO_2X");
            return false;
        }
        velocity_ratio_2x_ = *velocity_ratio_2x_opt;

        auto ai_confidence_weight_opt = config_->get_double("TRANSACTION_AI_CONFIDENCE_WEIGHT");
        if (!ai_confidence_weight_opt) {
            logger_->log(LogLevel::ERROR, "Missing required configuration: TRANSACTION_AI_CONFIDENCE_WEIGHT");
            return false;
        }
        ai_confidence_weight_ = *ai_confidence_weight_opt;

        auto customer_risk_update_weight_opt = config_->get_double("TRANSACTION_CUSTOMER_RISK_UPDATE_WEIGHT");
        if (!customer_risk_update_weight_opt) {
            logger_->log(LogLevel::ERROR, "Missing required configuration: TRANSACTION_CUSTOMER_RISK_UPDATE_WEIGHT");
            return false;
        }
        customer_risk_update_weight_ = *customer_risk_update_weight_opt;

        auto unusual_amount_multiplier_opt = config_->get_double("TRANSACTION_UNUSUAL_AMOUNT_MULTIPLIER");
        if (!unusual_amount_multiplier_opt) {
            logger_->log(LogLevel::ERROR, "Missing required configuration: TRANSACTION_UNUSUAL_AMOUNT_MULTIPLIER");
            return false;
        }
        unusual_amount_multiplier_ = *unusual_amount_multiplier_opt;

        auto unusual_amount_risk_weight_opt = config_->get_double("TRANSACTION_UNUSUAL_AMOUNT_RISK_WEIGHT");
        if (!unusual_amount_risk_weight_opt) {
            logger_->log(LogLevel::ERROR, "Missing required configuration: TRANSACTION_UNUSUAL_AMOUNT_RISK_WEIGHT");
            return false;
        }
        unusual_amount_risk_weight_ = *unusual_amount_risk_weight_opt;

        auto off_hours_risk_weight_opt = config_->get_double("TRANSACTION_OFF_HOURS_RISK_WEIGHT");
        if (!off_hours_risk_weight_opt) {
            logger_->log(LogLevel::ERROR, "Missing required configuration: TRANSACTION_OFF_HOURS_RISK_WEIGHT");
            return false;
        }
        off_hours_risk_weight_ = *off_hours_risk_weight_opt;

        auto weekend_risk_weight_opt = config_->get_double("TRANSACTION_WEEKEND_RISK_WEIGHT");
        if (!weekend_risk_weight_opt) {
            logger_->log(LogLevel::ERROR, "Missing required configuration: TRANSACTION_WEEKEND_RISK_WEIGHT");
            return false;
        }
        weekend_risk_weight_ = *weekend_risk_weight_opt;

        auto risk_update_current_weight_opt = config_->get_double("TRANSACTION_RISK_UPDATE_CURRENT_WEIGHT");
        if (!risk_update_current_weight_opt) {
            logger_->log(LogLevel::ERROR, "Missing required configuration: TRANSACTION_RISK_UPDATE_CURRENT_WEIGHT");
            return false;
        }
        risk_update_current_weight_ = *risk_update_current_weight_opt;

        auto risk_update_transaction_weight_opt = config_->get_double("TRANSACTION_RISK_UPDATE_TRANSACTION_WEIGHT");
        if (!risk_update_transaction_weight_opt) {
            logger_->log(LogLevel::ERROR, "Missing required configuration: TRANSACTION_RISK_UPDATE_TRANSACTION_WEIGHT");
            return false;
        }
        risk_update_transaction_weight_ = *risk_update_transaction_weight_opt;

        auto base_time_risk_weight_opt = config_->get_double("TRANSACTION_BASE_TIME_RISK_WEIGHT");
        if (!base_time_risk_weight_opt) {
            logger_->log(LogLevel::ERROR, "Missing required configuration: TRANSACTION_BASE_TIME_RISK_WEIGHT");
            return false;
        }
        base_time_risk_weight_ = *base_time_risk_weight_opt;

        // Load sanctioned countries from configuration
        auto sanctioned_countries_opt = config_->get_string("SANCTIONED_COUNTRIES");
        std::string sanctioned_countries_str = sanctioned_countries_opt.value_or("IR,KP,SY,CU");
        std::stringstream ss(sanctioned_countries_str);
        std::string country;
        while (std::getline(ss, country, ',')) {
            // Trim whitespace
            country.erase(country.begin(), std::find_if(country.begin(), country.end(), [](unsigned char ch) {
                return !std::isspace(ch);
            }));
            country.erase(std::find_if(country.rbegin(), country.rend(), [](unsigned char ch) {
                return !std::isspace(ch);
            }).base(), country.end());
            if (!country.empty()) {
                sanctioned_countries_.push_back(country);
            }
        }

        logger_->log(LogLevel::INFO, "Transaction Guardian Agent initialized successfully");
        return true;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to initialize Transaction Guardian Agent: " + std::string(e.what()));
        return false;
    }
}

void TransactionGuardianAgent::start() {
    if (running_) {
        logger_->log(LogLevel::WARN, "Transaction Guardian Agent is already running");
        return;
    }

    running_ = true;
    processing_thread_ = std::thread(&TransactionGuardianAgent::transaction_processing_loop, this);

    logger_->log(LogLevel::INFO, "Transaction Guardian Agent started");
}

void TransactionGuardianAgent::stop() {
    if (!running_) {
        return;
    }

    running_ = false;
    queue_cv_.notify_all();

    if (processing_thread_.joinable()) {
        processing_thread_.join();
    }

    logger_->log(LogLevel::INFO, "Transaction Guardian Agent stopped");
}

AgentDecision TransactionGuardianAgent::process_transaction(const nlohmann::json& transaction_data) {
    std::string event_id = "transaction_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    std::string agent_id = "transaction_guardian_agent";

    try {
        // Queue transaction for processing
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            transaction_queue_.push(transaction_data);
        }
        queue_cv_.notify_one();

        // Perform immediate risk assessment
        double risk_score = calculate_transaction_risk_score(transaction_data, {});

        // Perform compliance checks
        auto compliance_check = check_compliance(transaction_data);
        auto fraud_detection = detect_fraud(transaction_data);

        // Determine overall decision
        bool transaction_approved = true;
        std::string risk_level = "LOW";

        if (risk_score > fraud_threshold_ || compliance_check.value("blocked", false)) {
            transaction_approved = false;
            risk_level = "CRITICAL";
            suspicious_transactions_detected_++;
        } else if (risk_score > high_risk_threshold_ || fraud_detection.value("suspicious", false)) {
            risk_level = "HIGH";
            // May still approve but flag for review
        } else if (risk_score > velocity_threshold_) {
            risk_level = "MEDIUM";
        }

        // Determine decision type and confidence
        DecisionType decision_type;
        ConfidenceLevel confidence_level;

        if (transaction_approved) {
            decision_type = risk_score > high_risk_threshold_ ? DecisionType::MONITOR : DecisionType::APPROVE;
            confidence_level = risk_score > high_risk_threshold_ ? ConfidenceLevel::MEDIUM : ConfidenceLevel::HIGH;
        } else {
            decision_type = DecisionType::DENY;
            confidence_level = ConfidenceLevel::HIGH;
        }

        // Create AgentDecision with proper constructor
        AgentDecision decision(decision_type, confidence_level, agent_id, event_id);

        // Add reasoning
        if (transaction_approved) {
            decision.add_reasoning({
                "transaction_approved",
                "Transaction approved with risk level: " + risk_level + ", risk score: " + std::to_string(risk_score),
                risk_score > high_risk_threshold_ ? 0.7 : 0.9,
                "transaction_risk_assessment"
            });

            decision.add_reasoning({
                "compliance_check_passed",
                "Compliance checks passed successfully",
                0.95,
                "compliance_engine"
            });

            if (!fraud_detection.value("suspicious", false)) {
                decision.add_reasoning({
                    "fraud_detection_passed",
                    "No fraud indicators detected",
                    0.85,
                    "fraud_detection_engine"
                });
            }
        } else {
            decision.add_reasoning({
                "transaction_blocked",
                "Transaction blocked due to: " + compliance_check.value("block_reason", "High risk transaction"),
                0.95,
                "compliance_engine"
            });

            decision.add_reasoning({
                "high_risk_detected",
                "Risk score (" + std::to_string(risk_score) + ") exceeds threshold",
                0.9,
                "transaction_risk_assessment"
            });

            decision.add_reasoning({
                "requires_investigation",
                "Transaction flagged for fraud investigation",
                0.85,
                "fraud_detection_engine"
            });
        }

        // Add recommended actions
        if (transaction_approved) {
            RecommendedAction action;
            action.action_type = "approve_transaction";
            action.description = "Process transaction normally";
            action.priority = Priority::NORMAL;
            action.deadline = std::chrono::system_clock::now() + std::chrono::minutes(5);
            decision.add_action(action);

            if (risk_level != "LOW") {
                RecommendedAction monitoring_action;
                monitoring_action.action_type = "flag_for_monitoring";
                monitoring_action.description = "Flag for additional monitoring";
                monitoring_action.priority = Priority::HIGH;
                monitoring_action.deadline = std::chrono::system_clock::now() + std::chrono::hours(1);
                decision.add_action(monitoring_action);
            }
        } else {
            RecommendedAction block_action;
            block_action.action_type = "block_transaction";
            block_action.description = "Block transaction immediately";
            block_action.priority = Priority::CRITICAL;
            block_action.deadline = std::chrono::system_clock::now() + std::chrono::seconds(30);
            decision.add_action(block_action);

            RecommendedAction investigation_action;
            investigation_action.action_type = "initiate_investigation";
            investigation_action.description = "Initiate fraud investigation";
            investigation_action.priority = Priority::HIGH;
            investigation_action.deadline = std::chrono::system_clock::now() + std::chrono::hours(2);
            decision.add_action(investigation_action);

            RecommendedAction notify_action;
            notify_action.action_type = "notify_compliance";
            notify_action.description = "Notify compliance team";
            notify_action.priority = Priority::HIGH;
            notify_action.deadline = std::chrono::system_clock::now() + std::chrono::minutes(30);
            decision.add_action(notify_action);

            RecommendedAction verification_action;
            verification_action.action_type = "customer_verification";
            verification_action.description = "Customer verification required";
            verification_action.priority = Priority::CRITICAL;
            verification_action.deadline = std::chrono::system_clock::now() + std::chrono::hours(1);
            decision.add_action(verification_action);
        }

        // Create risk assessment
        RiskAssessment risk_assessment;
        risk_assessment.assessment_id = "risk_" + event_id;
        risk_assessment.entity_id = transaction_data.value("customer_id", "unknown");
        risk_assessment.transaction_id = transaction_data.value("transaction_id", event_id);
        risk_assessment.assessed_by = agent_id;
        risk_assessment.assessment_time = std::chrono::system_clock::now();
        risk_assessment.risk_score = risk_score;
        risk_assessment.risk_level = risk_level;

        if (risk_level == "CRITICAL") {
            risk_assessment.overall_severity = RiskSeverity::CRITICAL;
        } else if (risk_level == "HIGH") {
            risk_assessment.overall_severity = RiskSeverity::HIGH;
        } else if (risk_level == "MEDIUM") {
            risk_assessment.overall_severity = RiskSeverity::MEDIUM;
        } else {
            risk_assessment.overall_severity = RiskSeverity::LOW;
        }

        risk_assessment.overall_score = risk_score;

        // Add risk factors
        risk_assessment.risk_factors.push_back("Overall risk score: " + std::to_string(risk_score));
        risk_assessment.risk_factors.push_back("Fraud probability: " +
            std::to_string(fraud_detection.value("fraud_probability", 0.0)));

        double velocity_risk = monitor_velocity(transaction_data.value("customer_id", ""),
                                               transaction_data.value("amount", 0.0));
        risk_assessment.risk_factors.push_back("Velocity risk: " + std::to_string(velocity_risk));

        // Add compliance violations if any
        if (compliance_check.contains("violations")) {
            for (const auto& violation : compliance_check["violations"]) {
                risk_assessment.risk_indicators.push_back(violation.get<std::string>());
            }
        }

        decision.set_risk_assessment(risk_assessment);

        transactions_processed_++;

        logger_->log(LogLevel::INFO, "Processed transaction with risk score: " + std::to_string(risk_score));

        return decision;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to process transaction: " + std::string(e.what()));

        // Create a minimal error decision
        AgentDecision decision(DecisionType::NO_ACTION, ConfidenceLevel::LOW, agent_id, event_id);

        decision.add_reasoning({
            "error",
            "Failed to process transaction: " + std::string(e.what()),
            0.1,
            "error_handler"
        });

        RecommendedAction action;
        action.action_type = "manual_review";
        action.description = "Manual transaction review required";
        action.priority = Priority::HIGH;
        action.deadline = std::chrono::system_clock::now() + std::chrono::hours(1);
        decision.add_action(action);

        return decision;
    }
}

nlohmann::json TransactionGuardianAgent::detect_fraud(const nlohmann::json& transaction_data) {
    nlohmann::json fraud_analysis = {
        {"analysis_type", "fraud_detection"},
        {"transaction_analyzed", true},
        {"suspicious", false}
    };

    try {
        double amount = transaction_data.value("amount", 0.0);
        std::string customer_id = transaction_data.value("customer_id", "");
        std::string transaction_type = transaction_data.value("type", "unknown");

        // Basic fraud detection rules
        bool suspicious = false;
        double fraud_probability = 0.0;

        // High amount transactions
        if (amount > 50000.0) {
            fraud_probability += 0.3;
            suspicious = true;
        }

        // Unusual transaction types
        if (transaction_type == "international" && amount > 100000.0) {
            fraud_probability += 0.2;
        }

        // Check velocity patterns
        if (!customer_id.empty()) {
            auto velocity_check = monitor_velocity(customer_id, amount);
            double velocity_risk = velocity_check.value("risk_score", 0.0);
            fraud_probability += velocity_risk * 0.25;
        }

        // Use AI for advanced fraud detection (with circuit breaker)
        if (is_circuit_breaker_open(last_llm_failure_, consecutive_llm_failures_)) {
            logger_->log(LogLevel::WARN, "LLM circuit breaker is open. Skipping AI fraud analysis.");
            fraud_probability += 0.1; // Conservative fallback increase
        } else {
            std::string analysis_prompt = R"(
            Analyze this transaction for potential fraud indicators. Consider:
            - Unusual patterns compared to customer history
            - Suspicious transaction characteristics
            - Known fraud patterns
            - Risk factors

            Transaction data: )" + transaction_data.dump() + R"(

            Return a JSON response with:
            - fraud_probability: number between 0-1
            - risk_factors: array of identified risk factors
            - confidence: confidence in the analysis (0-1)
            - reasoning: brief explanation
            )";

            auto ai_analysis = llm_client_->complex_reasoning_task(
                "fraud_detection",
                {{"prompt", analysis_prompt}, {"transaction_data", transaction_data}},
                3
            );

            if (ai_analysis) {
                try {
                    // Parse AI response for fraud indicators
                    auto ai_response = nlohmann::json::parse(*ai_analysis);

                    double ai_fraud_prob = ai_response.value("fraud_probability", 0.0);
                    double ai_confidence = ai_response.value("confidence", 0.5);

                    // Weight AI analysis by configurable confidence weight
                    fraud_probability += ai_fraud_prob * ai_confidence * ai_confidence_weight_;

                    // Add AI-identified risk factors to the analysis
                    if (ai_response.contains("risk_factors")) {
                        fraud_analysis["ai_risk_factors"] = ai_response["risk_factors"];
                    }

                    fraud_analysis["ai_confidence"] = ai_confidence;
                    fraud_analysis["ai_reasoning"] = ai_response.value("reasoning", "");

                    record_operation_success(consecutive_llm_failures_);
                    logger_->log(LogLevel::DEBUG, "AI fraud analysis completed with confidence: " + std::to_string(ai_confidence));

                } catch (const std::exception& e) {
                    record_operation_failure(consecutive_llm_failures_, last_llm_failure_);
                    logger_->log(LogLevel::WARN, "Failed to parse AI fraud analysis response: " + std::string(e.what()));
                    // Fallback: conservative increase in fraud probability
                    fraud_probability += 0.05;
                }
            } else {
                record_operation_failure(consecutive_llm_failures_, last_llm_failure_);
                logger_->log(LogLevel::WARN, "AI fraud analysis failed or returned no response");
                // Fallback: conservative small increase in fraud probability
                fraud_probability += 0.02;
            }
        }

        fraud_analysis["fraud_probability"] = std::min(fraud_probability, 1.0);
        fraud_analysis["suspicious"] = suspicious || fraud_probability > fraud_threshold_;
        fraud_analysis["detection_method"] = "hybrid_ai_rules";

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to detect fraud: " + std::string(e.what()));
        fraud_analysis["error"] = std::string(e.what());
    }

    return fraud_analysis;
}

nlohmann::json TransactionGuardianAgent::check_compliance(const nlohmann::json& transaction_data) {
    nlohmann::json compliance_check = {
        {"compliance_check_passed", true},
        {"blocked", false},
        {"violations", nlohmann::json::array()}
    };

    try {
        std::string customer_id = transaction_data.value("customer_id", "");
        std::string destination_country = transaction_data.value("destination_country", "");

        // Check business rules
        if (!validate_business_rules(transaction_data)) {
            compliance_check["compliance_check_passed"] = false;
            compliance_check["blocked"] = true;
            compliance_check["violations"].push_back("Business rule violation");
            compliance_check["block_reason"] = "Transaction violates business rules";
        }

        // Check AML compliance if customer data available
        if (!customer_id.empty()) {
            // Fetch real customer profile from database
            nlohmann::json customer_profile = fetch_customer_profile(customer_id);
            auto aml_check = check_aml_compliance(transaction_data, customer_profile);

            if (aml_check.value("blocked", false)) {
                compliance_check["compliance_check_passed"] = false;
                compliance_check["blocked"] = true;
                compliance_check["violations"].push_back("AML compliance violation");
                compliance_check["block_reason"] = aml_check.value("reason", "AML violation");
            }
        }

        // Check for sanctioned countries
        if (std::find(sanctioned_countries_.begin(), sanctioned_countries_.end(), destination_country)
            != sanctioned_countries_.end()) {
            compliance_check["compliance_check_passed"] = false;
            compliance_check["blocked"] = true;
            compliance_check["violations"].push_back("Sanctioned country transaction");
            compliance_check["block_reason"] = "Transaction to sanctioned country not allowed";
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to check compliance: " + std::string(e.what()));
        compliance_check["error"] = std::string(e.what());
    }

    return compliance_check;
}

nlohmann::json TransactionGuardianAgent::monitor_velocity(const std::string& customer_id, double transaction_amount) {
    nlohmann::json velocity_analysis = {
        {"customer_id", customer_id},
        {"analysis_type", "velocity_monitoring"},
        {"risk_score", 0.0}
    };

    try {
        if (customer_id.empty()) {
            return velocity_analysis;
        }

        // Get customer transaction history from database
        auto recent_transactions = fetch_customer_transaction_history(customer_id, analysis_window_);

        // Calculate velocity metrics
        double total_recent_amount = 0.0;
        int transaction_count = 0;

        for (const auto& tx : recent_transactions) {
            total_recent_amount += tx.value("amount", 0.0);
            transaction_count++;
        }

        // Simple velocity risk calculation
        double avg_transaction = transaction_count > 0 ? total_recent_amount / transaction_count : 0.0;
        double velocity_ratio = avg_transaction > 0 ? transaction_amount / avg_transaction : 1.0;

        double velocity_risk = 0.0;
        if (velocity_ratio > velocity_critical_threshold_) velocity_risk = velocity_ratio_5x_;      // Much higher than usual
        else if (velocity_ratio > velocity_high_threshold_) velocity_risk = velocity_ratio_3x_; // Significantly higher
        else if (velocity_ratio > velocity_moderate_threshold_) velocity_risk = velocity_ratio_2x_; // Moderately higher

        velocity_analysis["risk_score"] = velocity_risk;
        velocity_analysis["recent_transaction_count"] = transaction_count;
        velocity_analysis["average_transaction_amount"] = avg_transaction;
        velocity_analysis["velocity_ratio"] = velocity_ratio;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to monitor velocity: " + std::string(e.what()));
        velocity_analysis["error"] = std::string(e.what());
    }

    return velocity_analysis;
}

nlohmann::json TransactionGuardianAgent::generate_compliance_report(
    const std::chrono::system_clock::time_point& start_time,
    const std::chrono::system_clock::time_point& end_time) {

    nlohmann::json report = {
        {"report_type", "transaction_compliance_summary"},
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
        // Gather transaction statistics
        report["total_transactions_processed"] = transactions_processed_.load();
        report["suspicious_transactions_detected"] = suspicious_transactions_detected_.load();
        report["compliance_rate"] = transactions_processed_ > 0 ?
            (1.0 - static_cast<double>(suspicious_transactions_detected_.load()) / transactions_processed_.load()) : 1.0;

        // Fetch real risk distribution from database
        report["risk_distribution"] = fetch_risk_distribution(start_time, end_time);

        // Fetch real compliance violations from database
        report["top_violations"] = fetch_top_violations(start_time, end_time);

        logger_->log(LogLevel::INFO, "Generated comprehensive transaction compliance report");

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to generate compliance report: " + std::string(e.what()));
        report["error"] = std::string(e.what());
    }

    return report;
}

void TransactionGuardianAgent::escalate_suspicious_transaction(const nlohmann::json& transaction_data, double risk_score) {
    try {
        logger_->log(LogLevel::WARN, "Escalating suspicious transaction - Risk Score: " + std::to_string(risk_score));

        // Create compliance event for escalation
        EventSource source{
            "transaction_guardian_agent",
            "fraud_detection",
            "system"
        };

        EventMetadata metadata;
        metadata["transaction_id"] = transaction_data.value("transaction_id", "unknown");
        metadata["risk_score"] = risk_score;
        metadata["customer_id"] = transaction_data.value("customer_id", "unknown");

        ComplianceEvent event(
            EventType::SUSPICIOUS_ACTIVITY_DETECTED,
            risk_score > fraud_threshold_ ? EventSeverity::CRITICAL : EventSeverity::HIGH,
            "Suspicious transaction detected with risk score: " + std::to_string(risk_score),
            source,
            metadata
        );

        // In production, would queue this event for human review
        // For now, just log it
        logger_->log(LogLevel::WARN, "Suspicious transaction escalated for review: " +
                   transaction_data.value("transaction_id", "unknown"));

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to escalate suspicious transaction: " + std::string(e.what()));
    }
}

void TransactionGuardianAgent::transaction_processing_loop() {
    logger_->log(LogLevel::INFO, "Starting transaction processing loop");

    while (running_) {
        try {
            std::unique_lock<std::mutex> lock(queue_mutex_);

            // Wait for transactions or timeout
            if (transaction_queue_.empty()) {
                queue_cv_.wait_for(lock, std::chrono::seconds(1));
                continue;
            }

            // Process available transactions
            process_transaction_queue();

        } catch (const std::exception& e) {
            logger_->log(LogLevel::ERROR, "Error in transaction processing loop: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(5)); // Back off on error
        }
    }
}

void TransactionGuardianAgent::process_transaction_queue() {
    while (!transaction_queue_.empty()) {
        nlohmann::json transaction = transaction_queue_.front();
        transaction_queue_.pop();

        // Release lock while processing
        queue_mutex_.unlock();

        try {
            // Process the transaction
            auto decision = process_transaction(transaction);

            // Handle high-risk transactions
            auto risk_assessment_opt = decision.get_risk_assessment();
            if (risk_assessment_opt) {
                double risk_score = risk_assessment_opt->risk_score;
                if (risk_score > high_risk_threshold_) {
                    escalate_suspicious_transaction(transaction, risk_score);
                }
            }

        } catch (const std::exception& e) {
            logger_->log(LogLevel::ERROR, "Failed to process queued transaction: " + std::string(e.what()));
        }

        // Re-acquire lock for next iteration
        queue_mutex_.lock();
    }
}

double TransactionGuardianAgent::calculate_transaction_risk_score(const nlohmann::json& transaction_data,
                                                               const std::vector<nlohmann::json>& customer_history) {
    double risk_score = 0.0;

    try {
        double amount = transaction_data.value("amount", 0.0);
        std::string transaction_type = transaction_data.value("type", "domestic");

        // Amount-based risk
        if (amount > 100000) risk_score += risk_amount_100k_;
        else if (amount > 50000) risk_score += risk_amount_50k_;
        else if (amount > 10000) risk_score += risk_amount_10k_;

        // Type-based risk
        if (transaction_type == "international") risk_score += risk_international_;
        if (transaction_type == "crypto") risk_score += risk_crypto_;

        // Customer history analysis (skip if circuit breaker is open)
        if (!customer_history.empty() && !is_circuit_breaker_open(last_db_failure_, consecutive_db_failures_)) {
            // Analyze patterns from customer history
            double avg_amount = 0.0;
            for (const auto& tx : customer_history) {
                avg_amount += tx.value("amount", 0.0);
            }
            avg_amount /= customer_history.size();

            if (amount > avg_amount * unusual_amount_multiplier_) risk_score += unusual_amount_risk_weight_; // Unusual amount
        }

        // Time-based risk (end of day, weekends, etc.)
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm* tm = std::localtime(&time);

        if (tm->tm_hour >= 18 || tm->tm_hour <= 6) risk_score += off_hours_risk_weight_; // Off-hours
        if (tm->tm_wday == 0 || tm->tm_wday == 6) risk_score += weekend_risk_weight_; // Weekend

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to calculate risk score: " + std::string(e.what()));
        // Use fallback risk scoring
        return get_fallback_risk_score(transaction_data);
    }

    return std::min(risk_score, 1.0);
}

nlohmann::json TransactionGuardianAgent::check_aml_compliance(const nlohmann::json& transaction_data,
                                                           const nlohmann::json& customer_profile) {
    nlohmann::json aml_check = {
        {"aml_check_passed", true},
        {"blocked", false}
    };

    try {
        // Check customer AML status
        std::string aml_status = customer_profile.value("aml_status", "unknown");

        if (aml_status == "blocked" || aml_status == "high_risk") {
            aml_check["aml_check_passed"] = false;
            aml_check["blocked"] = true;
            aml_check["reason"] = "Customer has poor AML status: " + aml_status;
        }

        // Check transaction amount against customer profile
        double transaction_amount = transaction_data.value("amount", 0.0);
        double customer_limit = customer_profile.value("daily_limit", 50000.0);

        if (transaction_amount > customer_limit) {
            aml_check["aml_check_passed"] = false;
            aml_check["blocked"] = true;
            aml_check["reason"] = "Transaction exceeds customer daily limit";
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to check AML compliance: " + std::string(e.what()));
        aml_check["error"] = std::string(e.what());
    }

    return aml_check;
}

bool TransactionGuardianAgent::validate_business_rules(const nlohmann::json& transaction_data) {
    try {
        double amount = transaction_data.value("amount", 0.0);

        // Basic business rules
        if (amount <= 0) return false;
        if (amount > 1000000) return false; // Maximum transaction limit

        // Check required fields
        if (!transaction_data.contains("customer_id")) return false;
        if (!transaction_data.contains("amount")) return false;

        return true;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to validate business rules: " + std::string(e.what()));
        return false;
    }
}

void TransactionGuardianAgent::update_customer_risk_profile(const std::string& customer_id, double transaction_risk) {
    if (customer_id.empty()) return;

    std::unique_lock<std::mutex> lock(profiles_mutex_);

    auto& profile = customer_risk_profiles_[customer_id];
    double current_risk = profile.value("risk_score", 0.0);

    // Update risk score (configurable exponential moving average)
    double new_risk = (current_risk * risk_update_current_weight_) + (transaction_risk * risk_update_transaction_weight_);
    profile["risk_score"] = new_risk;
    profile["last_updated"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

nlohmann::json TransactionGuardianAgent::fetch_customer_profile(const std::string& customer_id) {
    nlohmann::json customer_profile;

    // Check circuit breaker for database operations
    if (is_circuit_breaker_open(last_db_failure_, consecutive_db_failures_)) {
        logger_->log(LogLevel::WARN, "Database circuit breaker is open. Using fallback profile.");
        return get_fallback_customer_profile(customer_id);
    }

    try {
        auto conn = db_pool_->get_connection();
        if (!conn) {
            record_operation_failure(consecutive_db_failures_, last_db_failure_);
            logger_->log(LogLevel::ERROR, "Failed to get database connection for customer profile fetch");
            return get_fallback_customer_profile(customer_id);
        }

        // Fetch customer profile with AML/KYC information
        std::string query = R"(
            SELECT
                customer_id,
                customer_type,
                full_name,
                business_name,
                tax_id,
                date_of_birth,
                nationality,
                residency_country,
                risk_rating,
                kyc_status,
                kyc_completed_at,
                last_review_date,
                watchlist_flags,
                sanctions_screening,
                pep_status,
                adverse_media,
                created_at,
                updated_at
            FROM customer_profiles
            WHERE customer_id = $1
        )";

        auto result = conn->execute_query_multi(query, {customer_id});

        if (!result.empty()) {
            const auto& row = result[0];

            customer_profile = {
                {"customer_id", row.value("customer_id", "")},
                {"customer_type", row.value("customer_type", "")},
                {"full_name", row.value("full_name", "")},
                {"business_name", row.value("business_name", "")},
                {"tax_id", row.value("tax_id", "")},
                {"nationality", row.value("nationality", "")},
                {"residency_country", row.value("residency_country", "")},
                {"risk_rating", row.value("risk_rating", "")},
                {"kyc_status", row.value("kyc_status", "")},
                {"aml_status", determine_aml_status_from_json(row)},
                {"daily_limit", calculate_daily_limit_from_json(row)},
                {"watchlist_flags", row.value("watchlist_flags", nlohmann::json::array())},
                {"pep_status", row.value("pep_status", false)},
                {"sanctions_screening", row.value("sanctions_screening", nlohmann::json())}
            };

            logger_->log(LogLevel::DEBUG, "Successfully fetched customer profile for ID: " + customer_id);
            record_operation_success(consecutive_db_failures_);
        } else {
            logger_->log(LogLevel::WARN, "Customer profile not found for ID: " + customer_id);
            record_operation_success(consecutive_db_failures_); // Query succeeded, just no data
        }

    } catch (const std::exception& e) {
        record_operation_failure(consecutive_db_failures_, last_db_failure_);
        logger_->log(LogLevel::ERROR, "Failed to fetch customer profile: " + std::string(e.what()));
        return get_fallback_customer_profile(customer_id);
    }

    return customer_profile;
}

std::vector<nlohmann::json> TransactionGuardianAgent::fetch_customer_transaction_history(
    const std::string& customer_id, const std::chrono::minutes& analysis_window) {

    std::vector<nlohmann::json> transactions;

    // Check circuit breaker for database operations
    if (is_circuit_breaker_open(last_db_failure_, consecutive_db_failures_)) {
        logger_->log(LogLevel::WARN, "Database circuit breaker is open. Using fallback transaction history.");
        // Convert fallback JSON array to vector
        auto fallback_data = get_fallback_transaction_history();
        for (const auto& tx : fallback_data) {
            transactions.push_back(tx);
        }
        return transactions;
    }

    try {
        auto conn = db_pool_->get_connection();
        if (!conn) {
            record_operation_failure(consecutive_db_failures_, last_db_failure_);
            logger_->log(LogLevel::ERROR, "Failed to get database connection for transaction history fetch");
            auto fallback_data = get_fallback_transaction_history();
            for (const auto& tx : fallback_data) {
                transactions.push_back(tx);
            }
            return transactions;
        }

        // Calculate the start time for analysis window
        auto now = std::chrono::system_clock::now();
        auto start_time = now - analysis_window;

        std::string query = R"(
            SELECT
                transaction_id,
                transaction_type,
                amount,
                currency,
                transaction_date,
                description,
                channel,
                receiver_country,
                risk_score
            FROM transactions
            WHERE customer_id = $1
              AND transaction_date >= $2
              AND transaction_date <= $3
            ORDER BY transaction_date DESC
            LIMIT 100
        )";

        auto start_timestamp = std::chrono::duration_cast<std::chrono::seconds>(start_time.time_since_epoch()).count();
        auto end_timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

        auto result = conn->execute_query_multi(query, {customer_id, std::to_string(start_timestamp), std::to_string(end_timestamp)});

        for (const auto& row : result) {
            nlohmann::json transaction = {
                {"transaction_id", row.value("transaction_id", "")},
                {"type", row.value("transaction_type", "")},
                {"amount", row.value("amount", 0.0)},
                {"currency", row.value("currency", "USD")},
                {"timestamp", row.value("transaction_date", "")},
                {"description", row.value("description", "")},
                {"channel", row.value("channel", "")},
                {"receiver_country", row.value("receiver_country", "")},
                {"risk_score", row.value("risk_score", 0.0)}
            };
            transactions.push_back(transaction);
        }

        logger_->log(LogLevel::DEBUG, "Fetched " + std::to_string(transactions.size()) +
                   " transactions for customer ID: " + customer_id);
        record_operation_success(consecutive_db_failures_);

    } catch (const std::exception& e) {
        record_operation_failure(consecutive_db_failures_, last_db_failure_);
        logger_->log(LogLevel::ERROR, "Failed to fetch customer transaction history: " + std::string(e.what()));
        // Return fallback data
        auto fallback_data = get_fallback_transaction_history();
        for (const auto& tx : fallback_data) {
            transactions.push_back(tx);
        }
    }

    return transactions;
}

nlohmann::json TransactionGuardianAgent::fetch_risk_distribution(
    const std::chrono::system_clock::time_point& start_time,
    const std::chrono::system_clock::time_point& end_time) {

    nlohmann::json risk_distribution = {
        {"low_risk", 0},
        {"medium_risk", 0},
        {"high_risk", 0},
        {"critical_risk", 0}
    };

    try {
        auto conn = db_pool_->get_connection();
        if (!conn) {
            logger_->log(LogLevel::ERROR, "Failed to get database connection for risk distribution fetch");
            return risk_distribution;
        }

        std::string query = R"(
            SELECT
                CASE
                    WHEN risk_score < 0.3 THEN 'low_risk'
                    WHEN risk_score < 0.6 THEN 'medium_risk'
                    WHEN risk_score < 0.8 THEN 'high_risk'
                    ELSE 'critical_risk'
                END as risk_category,
                COUNT(*) as count
            FROM transaction_risk_assessments tra
            JOIN transactions t ON tra.transaction_id = t.transaction_id
            WHERE tra.assessed_at >= $1 AND tra.assessed_at <= $2
            GROUP BY risk_category
        )";

        auto start_timestamp = std::chrono::duration_cast<std::chrono::seconds>(start_time.time_since_epoch()).count();
        auto end_timestamp = std::chrono::duration_cast<std::chrono::seconds>(end_time.time_since_epoch()).count();

        auto result = conn->execute_query_multi(query, {std::to_string(start_timestamp), std::to_string(end_timestamp)});

        for (const auto& row : result) {
            std::string category = row.value("risk_category", "");
            int count = row.value("count", 0);
            risk_distribution[category] = count;
        }

        logger_->log(LogLevel::DEBUG, "Fetched risk distribution statistics");

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to fetch risk distribution: " + std::string(e.what()));
    }

    return risk_distribution;
}

nlohmann::json TransactionGuardianAgent::fetch_top_violations(
    const std::chrono::system_clock::time_point& start_time,
    const std::chrono::system_clock::time_point& end_time) {

    nlohmann::json top_violations = {
        {"AML violations", 0},
        {"Velocity violations", 0},
        {"Business rule violations", 0},
        {"Sanctions violations", 0}
    };

    try {
        auto conn = db_pool_->get_connection();
        if (!conn) {
            logger_->log(LogLevel::ERROR, "Failed to get database connection for violations fetch");
            return top_violations;
        }

        std::string query = R"(
            SELECT
                CASE
                    WHEN risk_factors->>'type' = 'aml' THEN 'AML violations'
                    WHEN risk_factors->>'type' = 'velocity' THEN 'Velocity violations'
                    WHEN risk_factors->>'type' = 'business_rule' THEN 'Business rule violations'
                    WHEN risk_factors->>'type' = 'sanctions' THEN 'Sanctions violations'
                    ELSE 'Other violations'
                END as violation_type,
                COUNT(*) as count
            FROM transaction_risk_assessments
            WHERE risk_score >= 0.6
              AND assessed_at >= $1 AND assessed_at <= $2
            GROUP BY violation_type
            ORDER BY count DESC
            LIMIT 5
        )";

        auto start_timestamp = std::chrono::duration_cast<std::chrono::seconds>(start_time.time_since_epoch()).count();
        auto end_timestamp = std::chrono::duration_cast<std::chrono::seconds>(end_time.time_since_epoch()).count();

        auto result = conn->execute_query_multi(query, {std::to_string(start_timestamp), std::to_string(end_timestamp)});

        for (const auto& row : result) {
            std::string violation_type = row.value("violation_type", "");
            int count = row.value("count", 0);
            if (top_violations.contains(violation_type)) {
                top_violations[violation_type] = count;
            } else {
                top_violations[violation_type] = count;
            }
        }

        logger_->log(LogLevel::DEBUG, "Fetched top violations statistics");

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to fetch top violations: " + std::string(e.what()));
    }

    return top_violations;
}

std::string TransactionGuardianAgent::determine_aml_status_from_json(const nlohmann::json& customer_row) {
    std::string kyc_status = customer_row.value("kyc_status", "");
    std::string risk_rating = customer_row.value("risk_rating", "");
    bool pep_status = customer_row.value("pep_status", false);

    // Determine AML status based on KYC, risk rating, and PEP status
    if (kyc_status == "REJECTED" || kyc_status == "EXPIRED") {
        return "blocked";
    }

    if (pep_status || risk_rating == "VERY_HIGH") {
        return "high_risk";
    }

    if (kyc_status == "VERIFIED" && risk_rating == "LOW") {
        return "cleared";
    }

    return "under_review";
}

double TransactionGuardianAgent::calculate_daily_limit_from_json(const nlohmann::json& customer_row) {
    std::string risk_rating = customer_row.value("risk_rating", "");
    std::string customer_type = customer_row.value("customer_type", "");

    // Load configurable base limits from configuration with fallback values
    auto individual_limit_opt = config_->get_double("TRANSACTION_MAX_AMOUNT_INDIVIDUAL");
    double base_limit_individual = individual_limit_opt.value_or(10000.0);

    auto business_limit_opt = config_->get_double("TRANSACTION_MAX_AMOUNT_BUSINESS");
    double base_limit_business = business_limit_opt.value_or(50000.0);

    auto institution_limit_opt = config_->get_double("TRANSACTION_MAX_AMOUNT_INSTITUTION");
    double base_limit_institution = institution_limit_opt.value_or(100000.0);

    // Base limits by customer type
    double base_limit = (customer_type == "INDIVIDUAL") ? base_limit_individual :
                       (customer_type == "BUSINESS") ? base_limit_business : base_limit_institution;

    // Adjust based on risk rating
    double risk_multiplier = (risk_rating == "LOW") ? 1.0 :
                           (risk_rating == "MEDIUM") ? 0.5 :
                           (risk_rating == "HIGH") ? 0.25 : 0.1;

    return base_limit * risk_multiplier;
}

nlohmann::json TransactionGuardianAgent::get_fallback_customer_profile(const std::string& customer_id) const {
    logger_->log(LogLevel::WARN, "Using fallback customer profile for ID: " + customer_id);

    // Return a minimal profile that allows processing but flags for review
    return {
        {"customer_id", customer_id},
        {"customer_type", "UNKNOWN"},
        {"full_name", "Unknown Customer"},
        {"risk_rating", "HIGH"}, // Conservative fallback
        {"kyc_status", "UNKNOWN"},
        {"aml_status", "under_review"},
        {"daily_limit", 1000.0}, // Conservative low limit
        {"watchlist_flags", nlohmann::json::array()},
        {"pep_status", false},
        {"sanctions_screening", nlohmann::json()}
    };
}

nlohmann::json TransactionGuardianAgent::get_fallback_transaction_history() const {
    logger_->log(LogLevel::WARN, "Using fallback transaction history");

    // Return minimal transaction history
    return nlohmann::json::array({
        {
            {"transaction_id", "fallback_001"},
            {"type", "domestic"},
            {"amount", 100.0},
            {"timestamp", "2024-01-01T10:00:00Z"},
            {"description", "Fallback transaction"},
            {"channel", "ONLINE"},
            {"receiver_country", ""},
            {"risk_score", 0.0}
        }
    });
}

double TransactionGuardianAgent::get_fallback_risk_score(const nlohmann::json& transaction_data) const {
    logger_->log(LogLevel::WARN, "Using fallback risk scoring");

    // Simple fallback risk calculation without database/LLM
    double amount = transaction_data.value("amount", 0.0);
    std::string transaction_type = transaction_data.value("type", "domestic");

    double risk_score = 0.0;

    // Amount-based risk (using configurable parameters)
    if (amount > 100000) risk_score += risk_amount_100k_;
    else if (amount > 50000) risk_score += risk_amount_50k_;
    else if (amount > 10000) risk_score += risk_amount_10k_;

    // Type-based risk (using configurable parameters)
    if (transaction_type == "international") risk_score += risk_international_;
    if (transaction_type == "crypto") risk_score += risk_crypto_;

    // Time-based risk (configurable conservative fallback)
    risk_score += base_time_risk_weight_;

    return std::min(risk_score, 1.0);
}

bool TransactionGuardianAgent::is_circuit_breaker_open(std::chrono::steady_clock::time_point last_failure,
                                                     size_t consecutive_failures) const {
    if (consecutive_failures < MAX_CONSECUTIVE_FAILURES) {
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    auto time_since_failure = now - last_failure;

    return time_since_failure < CIRCUIT_BREAKER_TIMEOUT;
}

void TransactionGuardianAgent::record_operation_failure(std::atomic<size_t>& failure_counter,
                                                       std::chrono::steady_clock::time_point& last_failure) {
    failure_counter++;
    last_failure = std::chrono::steady_clock::now();

    logger_->log(LogLevel::WARN, "Operation failure recorded. Consecutive failures: " +
               std::to_string(failure_counter.load()));
}

void TransactionGuardianAgent::record_operation_success(std::atomic<size_t>& failure_counter) {
    if (failure_counter > 0) {
        failure_counter = 0;
        logger_->log(LogLevel::INFO, "Operation success recorded. Reset failure counter.");
    }
}

} // namespace regulens
