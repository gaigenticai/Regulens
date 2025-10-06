/**
 * Prometheus Metrics Collection Implementation
 *
 * Production-grade metrics collection with circuit breaker monitoring,
 * LLM performance tracking, compliance metrics, and system observability.
 */

#include "prometheus_metrics.hpp"
#include "../resilience/circuit_breaker.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace regulens {

// MetricLabels Implementation

std::string MetricLabels::to_string() const {
    if (labels.empty()) return "";

    std::stringstream ss;
    bool first = true;
    for (const auto& [key, value] : labels) {
        if (!first) ss << ",";
        ss << key << "=\"" << value << "\"";
        first = false;
    }
    return "{" + ss.str() + "}";
}

// MetricDefinition Implementation

std::string MetricDefinition::to_prometheus_format() const {
    std::stringstream ss;

    // Add HELP comment
    ss << "# HELP " << name << " " << help << "\n";

    // Add TYPE comment
    std::string type_str;
    switch (type) {
        case MetricType::COUNTER: type_str = "counter"; break;
        case MetricType::GAUGE: type_str = "gauge"; break;
        case MetricType::HISTOGRAM: type_str = "histogram"; break;
        case MetricType::SUMMARY: type_str = "summary"; break;
    }
    ss << "# TYPE " << name << " " << type_str << "\n";

    // Add metric line
    ss << name;
    if (!labels.labels.empty()) {
        ss << labels.to_string();
    }
    ss << " " << value << "\n";

    return ss.str();
}

// CircuitBreakerMetricsCollector Implementation

CircuitBreakerMetricsCollector::CircuitBreakerMetricsCollector(std::shared_ptr<StructuredLogger> logger)
    : logger_(logger) {}

std::vector<MetricDefinition> CircuitBreakerMetricsCollector::collect_metrics() {
    std::vector<MetricDefinition> metrics;

    std::lock_guard<std::mutex> lock(breakers_mutex_);

    for (const auto& [name, breaker] : breakers_) {
        if (!breaker) continue;

        auto breaker_metrics = breaker->get_metrics();
        CircuitState current_state = breaker->get_current_state();

        // Circuit breaker state gauge
        metrics.emplace_back(
            "regulens_circuit_breaker_state",
            "Current state of circuit breaker (0=closed, 1=open, 2=half_open)",
            MetricType::GAUGE,
            MetricLabels{{"circuit_breaker", name}},
            std::to_string(static_cast<int>(current_state))
        );

        // Total requests counter
        metrics.emplace_back(
            "regulens_circuit_breaker_requests_total",
            "Total number of requests through circuit breaker",
            MetricType::COUNTER,
            MetricLabels{{"circuit_breaker", name}},
            std::to_string(breaker_metrics.total_requests)
        );

        // Successful requests counter
        metrics.emplace_back(
            "regulens_circuit_breaker_requests_successful_total",
            "Total number of successful requests through circuit breaker",
            MetricType::COUNTER,
            MetricLabels{{"circuit_breaker", name}},
            std::to_string(breaker_metrics.successful_requests)
        );

        // Failed requests counter
        metrics.emplace_back(
            "regulens_circuit_breaker_requests_failed_total",
            "Total number of failed requests through circuit breaker",
            MetricType::COUNTER,
            MetricLabels{{"circuit_breaker", name}},
            std::to_string(breaker_metrics.failed_requests)
        );

        // Rejected requests counter (when circuit is open)
        metrics.emplace_back(
            "regulens_circuit_breaker_requests_rejected_total",
            "Total number of requests rejected by open circuit breaker",
            MetricType::COUNTER,
            MetricLabels{{"circuit_breaker", name}},
            std::to_string(breaker_metrics.rejected_requests)
        );

        // State transitions counter
        metrics.emplace_back(
            "regulens_circuit_breaker_state_transitions_total",
            "Total number of circuit breaker state transitions",
            MetricType::COUNTER,
            MetricLabels{{"circuit_breaker", name}},
            std::to_string(breaker_metrics.state_transitions)
        );

        // Recovery attempts counter
        metrics.emplace_back(
            "regulens_circuit_breaker_recovery_attempts_total",
            "Total number of circuit breaker recovery attempts",
            MetricType::COUNTER,
            MetricLabels{{"circuit_breaker", name}},
            std::to_string(breaker_metrics.recovery_attempts)
        );

        // Successful recoveries counter
        metrics.emplace_back(
            "regulens_circuit_breaker_recoveries_successful_total",
            "Total number of successful circuit breaker recoveries",
            MetricType::COUNTER,
            MetricLabels{{"circuit_breaker", name}},
            std::to_string(breaker_metrics.successful_recoveries)
        );

        // Success rate gauge (calculated)
        double success_rate = breaker_metrics.total_requests > 0 ?
            static_cast<double>(breaker_metrics.successful_requests) / breaker_metrics.total_requests : 0.0;
        metrics.emplace_back(
            "regulens_circuit_breaker_success_rate",
            "Circuit breaker success rate (0.0 to 1.0)",
            MetricType::GAUGE,
            MetricLabels{{"circuit_breaker", name}},
            std::to_string(success_rate)
        );

        // Failure rate gauge (calculated)
        double failure_rate = breaker_metrics.total_requests > 0 ?
            static_cast<double>(breaker_metrics.failed_requests) / breaker_metrics.total_requests : 0.0;
        metrics.emplace_back(
            "regulens_circuit_breaker_failure_rate",
            "Circuit breaker failure rate (0.0 to 1.0)",
            MetricType::GAUGE,
            MetricLabels{{"circuit_breaker", name}},
            std::to_string(failure_rate)
        );
    }

    return metrics;
}

void CircuitBreakerMetricsCollector::register_circuit_breaker(std::shared_ptr<CircuitBreaker> breaker) {
    if (!breaker) return;

    std::lock_guard<std::mutex> lock(breakers_mutex_);
    breakers_[breaker->get_name()] = breaker;

    if (logger_) {
        logger_->info("Registered circuit breaker for metrics collection",
                     "CircuitBreakerMetricsCollector", "register_circuit_breaker",
                     {{"circuit_breaker", breaker->get_name()}});
    }
}

void CircuitBreakerMetricsCollector::unregister_circuit_breaker(const std::string& breaker_name) {
    std::lock_guard<std::mutex> lock(breakers_mutex_);
    breakers_.erase(breaker_name);

    if (logger_) {
        logger_->info("Unregistered circuit breaker from metrics collection",
                     "CircuitBreakerMetricsCollector", "unregister_circuit_breaker",
                     {{"circuit_breaker", breaker_name}});
    }
}

std::string CircuitBreakerMetricsCollector::circuit_state_to_string(CircuitState state) const {
    switch (state) {
        case CircuitState::CLOSED: return "closed";
        case CircuitState::OPEN: return "open";
        case CircuitState::HALF_OPEN: return "half_open";
        default: return "unknown";
    }
}

// LLMMetricsCollector Implementation

LLMMetricsCollector::LLMMetricsCollector(std::shared_ptr<StructuredLogger> logger)
    : logger_(logger) {}

void LLMMetricsCollector::record_api_call(const std::string& provider, const std::string& model,
                                        bool success, long response_time_ms,
                                        int input_tokens, int output_tokens, double cost_usd) {
    if (provider == "openai") {
        openai_calls_++;
        if (success) openai_successful_calls_++;

        // Update response time histogram
        if (response_time_ms > 0) {
            openai_response_time_histogram_.observe(static_cast<double>(response_time_ms));
        }

        openai_total_tokens_ += (input_tokens + output_tokens);
        openai_total_cost_ += cost_usd;

    } else if (provider == "anthropic") {
        anthropic_calls_++;
        if (success) anthropic_successful_calls_++;

        // Update response time histogram
        if (response_time_ms > 0) {
            anthropic_response_time_histogram_.observe(static_cast<double>(response_time_ms));
        }

        anthropic_total_tokens_ += (input_tokens + output_tokens);
        anthropic_total_cost_ += cost_usd;
    }

    if (logger_ && !success) {
        logger_->warn("LLM API call failed",
                     "LLMMetricsCollector", "record_api_call",
                     {{"provider", provider}, {"model", model}, {"response_time_ms", std::to_string(response_time_ms)}});
    }
}

void LLMMetricsCollector::record_rate_limit_hit(const std::string& provider) {
    if (provider == "openai") {
        openai_rate_limits_++;
    } else if (provider == "anthropic") {
        anthropic_rate_limits_++;
    }
}

void LLMMetricsCollector::record_circuit_breaker_event(const std::string& provider, const std::string& event_type) {
    if (provider == "openai") {
        if (event_type == "opened") openai_breaker_opened_++;
        else if (event_type == "closed") openai_breaker_closed_++;
    } else if (provider == "anthropic") {
        if (event_type == "opened") anthropic_breaker_opened_++;
        else if (event_type == "closed") anthropic_breaker_closed_++;
    }
}

std::vector<MetricDefinition> LLMMetricsCollector::collect_metrics() {
    std::vector<MetricDefinition> metrics;

    // OpenAI metrics
    metrics.emplace_back(
        "regulens_llm_openai_requests_total",
        "Total number of OpenAI API requests",
        MetricType::COUNTER,
        MetricLabels{{"provider", "openai"}},
        std::to_string(openai_calls_)
    );

    metrics.emplace_back(
        "regulens_llm_openai_requests_successful_total",
        "Total number of successful OpenAI API requests",
        MetricType::COUNTER,
        MetricLabels{{"provider", "openai"}},
        std::to_string(openai_successful_calls_)
    );

    metrics.emplace_back(
        "regulens_llm_openai_rate_limits_total",
        "Total number of OpenAI rate limit hits",
        MetricType::COUNTER,
        MetricLabels{{"provider", "openai"}},
        std::to_string(openai_rate_limits_)
    );

    // OpenAI response time histogram
    for (size_t i = 0; i < openai_response_time_histogram_.buckets.size(); ++i) {
        const auto& bucket = openai_response_time_histogram_.buckets[i];
        std::string bucket_label = (bucket.upper_bound == std::numeric_limits<double>::infinity()) ?
            "+Inf" : std::to_string(bucket.upper_bound);

        metrics.emplace_back(
            "regulens_llm_openai_response_time_ms_bucket",
            "OpenAI API response time histogram in milliseconds",
            MetricType::COUNTER,
            MetricLabels{{"provider", "openai"}, {"le", bucket_label}},
            std::to_string(bucket.count.load())
        );
    }

    metrics.emplace_back(
        "regulens_llm_openai_response_time_ms_count",
        "Total number of OpenAI API response time observations",
        MetricType::COUNTER,
        MetricLabels{{"provider", "openai"}},
        std::to_string(openai_response_time_histogram_.count.load())
    );

    metrics.emplace_back(
        "regulens_llm_openai_response_time_ms_sum",
        "Sum of OpenAI API response times in milliseconds",
        MetricType::COUNTER,
        MetricLabels{{"provider", "openai"}},
        std::to_string(openai_response_time_histogram_.sum.load())
    );

    metrics.emplace_back(
        "regulens_llm_openai_tokens_total",
        "Total OpenAI tokens used",
        MetricType::COUNTER,
        MetricLabels{{"provider", "openai"}},
        std::to_string(openai_total_tokens_)
    );

    metrics.emplace_back(
        "regulens_llm_openai_cost_usd_total",
        "Total OpenAI API cost in USD",
        MetricType::COUNTER,
        MetricLabels{{"provider", "openai"}},
        std::to_string(openai_total_cost_)
    );

    // Anthropic metrics
    metrics.emplace_back(
        "regulens_llm_anthropic_requests_total",
        "Total number of Anthropic API requests",
        MetricType::COUNTER,
        MetricLabels{{"provider", "anthropic"}},
        std::to_string(anthropic_calls_)
    );

    metrics.emplace_back(
        "regulens_llm_anthropic_requests_successful_total",
        "Total number of successful Anthropic API requests",
        MetricType::COUNTER,
        MetricLabels{{"provider", "anthropic"}},
        std::to_string(anthropic_successful_calls_)
    );

    metrics.emplace_back(
        "regulens_llm_anthropic_rate_limits_total",
        "Total number of Anthropic rate limit hits",
        MetricType::COUNTER,
        MetricLabels{{"provider", "anthropic"}},
        std::to_string(anthropic_rate_limits_)
    );

    // Anthropic response time histogram
    for (size_t i = 0; i < anthropic_response_time_histogram_.buckets.size(); ++i) {
        const auto& bucket = anthropic_response_time_histogram_.buckets[i];
        std::string bucket_label = (bucket.upper_bound == std::numeric_limits<double>::infinity()) ?
            "+Inf" : std::to_string(bucket.upper_bound);

        metrics.emplace_back(
            "regulens_llm_anthropic_response_time_ms_bucket",
            "Anthropic API response time histogram in milliseconds",
            MetricType::COUNTER,
            MetricLabels{{"provider", "anthropic"}, {"le", bucket_label}},
            std::to_string(bucket.count.load())
        );
    }

    metrics.emplace_back(
        "regulens_llm_anthropic_response_time_ms_count",
        "Total number of Anthropic API response time observations",
        MetricType::COUNTER,
        MetricLabels{{"provider", "anthropic"}},
        std::to_string(anthropic_response_time_histogram_.count.load())
    );

    metrics.emplace_back(
        "regulens_llm_anthropic_response_time_ms_sum",
        "Sum of Anthropic API response times in milliseconds",
        MetricType::COUNTER,
        MetricLabels{{"provider", "anthropic"}},
        std::to_string(anthropic_response_time_histogram_.sum.load())
    );

    metrics.emplace_back(
        "regulens_llm_anthropic_tokens_total",
        "Total Anthropic tokens used",
        MetricType::COUNTER,
        MetricLabels{{"provider", "anthropic"}},
        std::to_string(anthropic_total_tokens_)
    );

    metrics.emplace_back(
        "regulens_llm_anthropic_cost_usd_total",
        "Total Anthropic API cost in USD",
        MetricType::COUNTER,
        MetricLabels{{"provider", "anthropic"}},
        std::to_string(anthropic_total_cost_)
    );

    return metrics;
}

// ComplianceMetricsCollector Implementation

ComplianceMetricsCollector::ComplianceMetricsCollector(std::shared_ptr<StructuredLogger> logger)
    : logger_(logger) {}

void ComplianceMetricsCollector::record_decision(const std::string& agent_type, const std::string& decision_type,
                                               long processing_time_ms, double confidence_score, bool was_correct) {
    total_decisions_++;

    if (was_correct) correct_decisions_++;

    if (decision_type == "approve") approve_decisions_++;
    else if (decision_type == "deny") deny_decisions_++;
    else if (decision_type == "escalate") escalate_decisions_++;

    // Update average processing time using exponential moving average
    if (processing_time_ms > 0) {
        const double alpha = 0.1; // Smoothing factor for EMA
        if (avg_decision_time_ == 0.0) {
            avg_decision_time_ = processing_time_ms;
        } else {
            avg_decision_time_ = alpha * processing_time_ms + (1.0 - alpha) * avg_decision_time_;
        }
    }
}

void ComplianceMetricsCollector::record_regulatory_ingestion(const std::string& source, size_t documents_processed,
                                                           size_t new_regulations_found, long processing_time_ms) {
    new_regulations_found_ += new_regulations_found;

    if (source == "SEC") sec_documents_processed_ += documents_processed;
    else if (source == "FCA") fca_documents_processed_ += documents_processed;
    else if (source == "ECB") ecb_documents_processed_ += documents_processed;

    // Update average processing time using exponential moving average
    if (processing_time_ms > 0) {
        const double alpha = 0.1; // Smoothing factor for EMA
        if (avg_regulatory_processing_time_ == 0.0) {
            avg_regulatory_processing_time_ = processing_time_ms;
        } else {
            avg_regulatory_processing_time_ = alpha * processing_time_ms + (1.0 - alpha) * avg_regulatory_processing_time_;
        }
    }
}

void ComplianceMetricsCollector::record_risk_assessment(const std::string& entity_type, double risk_score,
                                                     const std::string& risk_level, long processing_time_ms) {
    if (risk_level == "LOW") risk_assessments_low_++;
    else if (risk_level == "MEDIUM") risk_assessments_medium_++;
    else if (risk_level == "HIGH") risk_assessments_high_++;
    else if (risk_level == "CRITICAL") risk_assessments_critical_++;

    // Update average processing time using exponential moving average
    if (processing_time_ms > 0) {
        const double alpha = 0.1; // Smoothing factor for EMA
        if (avg_risk_assessment_time_ == 0.0) {
            avg_risk_assessment_time_ = processing_time_ms;
        } else {
            avg_risk_assessment_time_ = alpha * processing_time_ms + (1.0 - alpha) * avg_risk_assessment_time_;
        }
    }
}

std::vector<MetricDefinition> ComplianceMetricsCollector::collect_metrics() {
    std::vector<MetricDefinition> metrics;

    // Decision metrics
    metrics.emplace_back(
        "regulens_compliance_decisions_total",
        "Total number of compliance decisions made",
        MetricType::COUNTER,
        MetricLabels{},
        std::to_string(total_decisions_)
    );

    metrics.emplace_back(
        "regulens_compliance_decisions_correct_total",
        "Total number of correct compliance decisions",
        MetricType::COUNTER,
        MetricLabels{},
        std::to_string(correct_decisions_)
    );

    metrics.emplace_back(
        "regulens_compliance_decisions_approve_total",
        "Total number of approve decisions",
        MetricType::COUNTER,
        MetricLabels{{"decision_type", "approve"}},
        std::to_string(approve_decisions_)
    );

    metrics.emplace_back(
        "regulens_compliance_decisions_deny_total",
        "Total number of deny decisions",
        MetricType::COUNTER,
        MetricLabels{{"decision_type", "deny"}},
        std::to_string(deny_decisions_)
    );

    metrics.emplace_back(
        "regulens_compliance_decisions_escalate_total",
        "Total number of escalate decisions",
        MetricType::COUNTER,
        MetricLabels{{"decision_type", "escalate"}},
        std::to_string(escalate_decisions_)
    );

    // Decision accuracy rate
    double accuracy_rate = total_decisions_ > 0 ?
        static_cast<double>(correct_decisions_) / total_decisions_ : 0.0;
    metrics.emplace_back(
        "regulens_compliance_decision_accuracy",
        "Compliance decision accuracy rate (0.0 to 1.0)",
        MetricType::GAUGE,
        MetricLabels{},
        std::to_string(accuracy_rate)
    );

    metrics.emplace_back(
        "regulens_compliance_avg_decision_time_ms",
        "Average compliance decision processing time in milliseconds",
        MetricType::GAUGE,
        MetricLabels{},
        std::to_string(avg_decision_time_)
    );

    // Regulatory ingestion metrics
    metrics.emplace_back(
        "regulens_regulatory_documents_processed_total",
        "Total regulatory documents processed",
        MetricType::COUNTER,
        MetricLabels{{"source", "sec"}},
        std::to_string(sec_documents_processed_)
    );

    metrics.emplace_back(
        "regulens_regulatory_documents_processed_total",
        "Total regulatory documents processed",
        MetricType::COUNTER,
        MetricLabels{{"source", "fca"}},
        std::to_string(fca_documents_processed_)
    );

    metrics.emplace_back(
        "regulens_regulatory_documents_processed_total",
        "Total regulatory documents processed",
        MetricType::COUNTER,
        MetricLabels{{"source", "ecb"}},
        std::to_string(ecb_documents_processed_)
    );

    metrics.emplace_back(
        "regulens_regulatory_new_regulations_found_total",
        "Total number of new regulations discovered",
        MetricType::COUNTER,
        MetricLabels{},
        std::to_string(new_regulations_found_)
    );

    // Risk assessment metrics
    metrics.emplace_back(
        "regulens_risk_assessments_total",
        "Total risk assessments by severity level",
        MetricType::COUNTER,
        MetricLabels{{"severity", "low"}},
        std::to_string(risk_assessments_low_)
    );

    metrics.emplace_back(
        "regulens_risk_assessments_total",
        "Total risk assessments by severity level",
        MetricType::COUNTER,
        MetricLabels{{"severity", "medium"}},
        std::to_string(risk_assessments_medium_)
    );

    metrics.emplace_back(
        "regulens_risk_assessments_total",
        "Total risk assessments by severity level",
        MetricType::COUNTER,
        MetricLabels{{"severity", "high"}},
        std::to_string(risk_assessments_high_)
    );

    metrics.emplace_back(
        "regulens_risk_assessments_total",
        "Total risk assessments by severity level",
        MetricType::COUNTER,
        MetricLabels{{"severity", "critical"}},
        std::to_string(risk_assessments_critical_)
    );

    return metrics;
}

// RedisMetricsCollector Implementation

RedisMetricsCollector::RedisMetricsCollector(std::shared_ptr<StructuredLogger> logger)
    : logger_(logger) {}

void RedisMetricsCollector::record_redis_operation(const std::string& operation_type, const std::string& cache_type,
                                                 bool success, long response_time_ms, bool hit) {
    redis_operations_total_++;

    if (success) {
        redis_operations_successful_++;
    }

    if (hit) {
        redis_cache_hits_++;
    } else if (operation_type == "GET") {
        redis_cache_misses_++;
    }

    // Update average response time using exponential moving average
    if (response_time_ms > 0) {
        const double alpha = 0.1; // Smoothing factor for EMA
        if (redis_avg_response_time_ == 0.0) {
            redis_avg_response_time_ = response_time_ms;
        } else {
            redis_avg_response_time_ = alpha * response_time_ms + (1.0 - alpha) * redis_avg_response_time_;
        }
    }

    // Update cache-specific counters
    if (cache_type == "llm") {
        llm_cache_operations_++;
    } else if (cache_type == "regulatory") {
        regulatory_cache_operations_++;
    } else if (cache_type == "session") {
        session_cache_operations_++;
    } else if (cache_type == "temp") {
        temp_cache_operations_++;
    } else if (cache_type == "preferences") {
        preferences_cache_operations_++;
    }
}

void RedisMetricsCollector::record_connection_pool_metrics(size_t total_connections, size_t active_connections,
                                                         size_t available_connections) {
    pool_total_connections_ = total_connections;
    pool_active_connections_ = active_connections;
    pool_available_connections_ = available_connections;
}

void RedisMetricsCollector::record_cache_eviction(const std::string& cache_type, size_t evicted_count) {
    cache_evictions_total_ += evicted_count;
}

void RedisMetricsCollector::record_cache_size(const std::string& cache_type, size_t entry_count, size_t memory_usage_bytes) {
    current_cache_entries_ = entry_count;
    current_memory_usage_ = memory_usage_bytes;
}

std::vector<MetricDefinition> RedisMetricsCollector::collect_metrics() {
    std::vector<MetricDefinition> metrics;

    // Redis operation metrics
    metrics.emplace_back(
        "regulens_redis_operations_total",
        "Total number of Redis operations",
        MetricType::COUNTER,
        MetricLabels{},
        std::to_string(redis_operations_total_)
    );

    metrics.emplace_back(
        "regulens_redis_operations_successful_total",
        "Total number of successful Redis operations",
        MetricType::COUNTER,
        MetricLabels{},
        std::to_string(redis_operations_successful_)
    );

    // Cache hit/miss metrics
    metrics.emplace_back(
        "regulens_redis_cache_hits_total",
        "Total number of Redis cache hits",
        MetricType::COUNTER,
        MetricLabels{},
        std::to_string(redis_cache_hits_)
    );

    metrics.emplace_back(
        "regulens_redis_cache_misses_total",
        "Total number of Redis cache misses",
        MetricType::COUNTER,
        MetricLabels{},
        std::to_string(redis_cache_misses_)
    );

    // Cache hit rate
    size_t total_cache_requests = redis_cache_hits_.load() + redis_cache_misses_.load();
    double cache_hit_rate = total_cache_requests > 0 ?
        static_cast<double>(redis_cache_hits_) / total_cache_requests : 0.0;

    metrics.emplace_back(
        "regulens_redis_cache_hit_rate",
        "Redis cache hit rate (0.0 to 1.0)",
        MetricType::GAUGE,
        MetricLabels{},
        std::to_string(cache_hit_rate)
    );

    // Response time metrics
    metrics.emplace_back(
        "regulens_redis_avg_response_time_ms",
        "Average Redis operation response time in milliseconds",
        MetricType::GAUGE,
        MetricLabels{},
        std::to_string(redis_avg_response_time_)
    );

    // Connection pool metrics
    metrics.emplace_back(
        "regulens_redis_pool_connections_total",
        "Total Redis connections in pool",
        MetricType::GAUGE,
        MetricLabels{},
        std::to_string(pool_total_connections_)
    );

    metrics.emplace_back(
        "regulens_redis_pool_connections_active",
        "Active Redis connections in pool",
        MetricType::GAUGE,
        MetricLabels{},
        std::to_string(pool_active_connections_)
    );

    metrics.emplace_back(
        "regulens_redis_pool_connections_available",
        "Available Redis connections in pool",
        MetricType::GAUGE,
        MetricLabels{},
        std::to_string(pool_available_connections_)
    );

    // Cache-specific operation metrics
    metrics.emplace_back(
        "regulens_redis_cache_operations_total",
        "Total Redis cache operations by type",
        MetricType::COUNTER,
        MetricLabels{{"cache_type", "llm"}},
        std::to_string(llm_cache_operations_)
    );

    metrics.emplace_back(
        "regulens_redis_cache_operations_total",
        "Total Redis cache operations by type",
        MetricType::COUNTER,
        MetricLabels{{"cache_type", "regulatory"}},
        std::to_string(regulatory_cache_operations_)
    );

    metrics.emplace_back(
        "regulens_redis_cache_operations_total",
        "Total Redis cache operations by type",
        MetricType::COUNTER,
        MetricLabels{{"cache_type", "session"}},
        std::to_string(session_cache_operations_)
    );

    metrics.emplace_back(
        "regulens_redis_cache_operations_total",
        "Total Redis cache operations by type",
        MetricType::COUNTER,
        MetricLabels{{"cache_type", "temp"}},
        std::to_string(temp_cache_operations_)
    );

    metrics.emplace_back(
        "regulens_redis_cache_operations_total",
        "Total Redis cache operations by type",
        MetricType::COUNTER,
        MetricLabels{{"cache_type", "preferences"}},
        std::to_string(preferences_cache_operations_)
    );

    // Eviction metrics
    metrics.emplace_back(
        "regulens_redis_cache_evictions_total",
        "Total number of Redis cache evictions",
        MetricType::COUNTER,
        MetricLabels{},
        std::to_string(cache_evictions_total_)
    );

    // Cache size metrics
    metrics.emplace_back(
        "regulens_redis_cache_entries_current",
        "Current number of entries in Redis cache",
        MetricType::GAUGE,
        MetricLabels{},
        std::to_string(current_cache_entries_)
    );

    metrics.emplace_back(
        "regulens_redis_memory_usage_bytes",
        "Current Redis memory usage in bytes",
        MetricType::GAUGE,
        MetricLabels{},
        std::to_string(current_memory_usage_)
    );

    return metrics;
}

// SystemMetricsCollector Implementation

SystemMetricsCollector::SystemMetricsCollector(std::shared_ptr<StructuredLogger> logger)
    : logger_(logger) {}

void SystemMetricsCollector::record_database_operation(const std::string& operation_type, const std::string& table_name,
                                                     bool success, long response_time_ms, size_t rows_affected) {
    db_queries_total_++;

    if (success) {
        db_queries_successful_++;
    }

    // Update average response time using exponential moving average
    if (response_time_ms > 0) {
        const double alpha = 0.1; // Smoothing factor for EMA
        if (db_avg_response_time_ == 0.0) {
            db_avg_response_time_ = response_time_ms;
        } else {
            db_avg_response_time_ = alpha * response_time_ms + (1.0 - alpha) * db_avg_response_time_;
        }
    }
}

void SystemMetricsCollector::record_cache_operation(const std::string& cache_type, const std::string& operation_type,
                                                  bool hit, long response_time_ms) {
    cache_requests_total_++;

    if (hit) {
        cache_hits_++;
    }

    // Update average response time using exponential moving average
    if (response_time_ms > 0) {
        const double alpha = 0.1; // Smoothing factor for EMA
        if (cache_avg_response_time_ == 0.0) {
            cache_avg_response_time_ = response_time_ms;
        } else {
            cache_avg_response_time_ = alpha * response_time_ms + (1.0 - alpha) * cache_avg_response_time_;
        }
    }
}

void SystemMetricsCollector::record_http_call(const std::string& endpoint, const std::string& method,
                                           int status_code, long response_time_ms) {
    http_requests_total_++;

    if (status_code >= 200 && status_code < 300) {
        http_requests_2xx_++;
    } else if (status_code >= 400 && status_code < 500) {
        http_requests_4xx_++;
    } else if (status_code >= 500) {
        http_requests_5xx_++;
    }

    // Update average response time using exponential moving average
    if (response_time_ms > 0) {
        const double alpha = 0.1; // Smoothing factor for EMA
        if (http_avg_response_time_ == 0.0) {
            http_avg_response_time_ = response_time_ms;
        } else {
            http_avg_response_time_ = alpha * response_time_ms + (1.0 - alpha) * http_avg_response_time_;
        }
    }
}

void SystemMetricsCollector::update_system_resources(double cpu_usage, double memory_usage, size_t active_connections) {
    current_cpu_usage_ = cpu_usage;
    current_memory_usage_ = memory_usage;
    current_active_connections_ = active_connections;
}

std::vector<MetricDefinition> SystemMetricsCollector::collect_metrics() {
    std::vector<MetricDefinition> metrics;

    // Database metrics
    metrics.emplace_back(
        "regulens_db_queries_total",
        "Total number of database queries",
        MetricType::COUNTER,
        MetricLabels{},
        std::to_string(db_queries_total_)
    );

    metrics.emplace_back(
        "regulens_db_queries_successful_total",
        "Total number of successful database queries",
        MetricType::COUNTER,
        MetricLabels{},
        std::to_string(db_queries_successful_)
    );

    metrics.emplace_back(
        "regulens_db_avg_response_time_ms",
        "Average database query response time in milliseconds",
        MetricType::GAUGE,
        MetricLabels{},
        std::to_string(db_avg_response_time_)
    );

    // Cache metrics
    metrics.emplace_back(
        "regulens_cache_requests_total",
        "Total number of cache requests",
        MetricType::COUNTER,
        MetricLabels{},
        std::to_string(cache_requests_total_)
    );

    metrics.emplace_back(
        "regulens_cache_hits_total",
        "Total number of cache hits",
        MetricType::COUNTER,
        MetricLabels{},
        std::to_string(cache_hits_)
    );

    double cache_hit_rate = cache_requests_total_ > 0 ?
        static_cast<double>(cache_hits_) / cache_requests_total_ : 0.0;
    metrics.emplace_back(
        "regulens_cache_hit_rate",
        "Cache hit rate (0.0 to 1.0)",
        MetricType::GAUGE,
        MetricLabels{},
        std::to_string(cache_hit_rate)
    );

    // HTTP metrics
    metrics.emplace_back(
        "regulens_http_requests_total",
        "Total number of HTTP requests",
        MetricType::COUNTER,
        MetricLabels{{"status_class", "2xx"}},
        std::to_string(http_requests_2xx_)
    );

    metrics.emplace_back(
        "regulens_http_requests_total",
        "Total number of HTTP requests",
        MetricType::COUNTER,
        MetricLabels{{"status_class", "4xx"}},
        std::to_string(http_requests_4xx_)
    );

    metrics.emplace_back(
        "regulens_http_requests_total",
        "Total number of HTTP requests",
        MetricType::COUNTER,
        MetricLabels{{"status_class", "5xx"}},
        std::to_string(http_requests_5xx_)
    );

    metrics.emplace_back(
        "regulens_http_avg_response_time_ms",
        "Average HTTP response time in milliseconds",
        MetricType::GAUGE,
        MetricLabels{},
        std::to_string(http_avg_response_time_)
    );

    // System resources
    metrics.emplace_back(
        "regulens_system_cpu_usage_percent",
        "Current CPU usage percentage",
        MetricType::GAUGE,
        MetricLabels{},
        std::to_string(current_cpu_usage_)
    );

    metrics.emplace_back(
        "regulens_system_memory_usage_percent",
        "Current memory usage percentage",
        MetricType::GAUGE,
        MetricLabels{},
        std::to_string(current_memory_usage_)
    );

    metrics.emplace_back(
        "regulens_system_active_connections",
        "Current number of active connections",
        MetricType::GAUGE,
        MetricLabels{},
        std::to_string(current_active_connections_)
    );

    return metrics;
}

// PrometheusMetricsCollector Implementation

PrometheusMetricsCollector::PrometheusMetricsCollector(std::shared_ptr<ConfigurationManager> config,
                                                     std::shared_ptr<StructuredLogger> logger,
                                                     std::shared_ptr<ErrorHandler> error_handler)
    : config_(config), logger_(logger), error_handler_(error_handler) {}

PrometheusMetricsCollector::~PrometheusMetricsCollector() {
    shutdown();
}

bool PrometheusMetricsCollector::initialize() {
    if (initialized_) return true;

    try {
        circuit_breaker_collector_ = std::make_unique<CircuitBreakerMetricsCollector>(logger_);
        llm_collector_ = std::make_unique<LLMMetricsCollector>(logger_);
        compliance_collector_ = std::make_unique<ComplianceMetricsCollector>(logger_);
        redis_collector_ = std::make_unique<RedisMetricsCollector>(logger_);
        system_collector_ = std::make_unique<SystemMetricsCollector>(logger_);

        initialized_ = true;

        if (logger_) {
            logger_->info("Prometheus metrics collector initialized successfully",
                         "PrometheusMetricsCollector", "initialize");
        }

        return true;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Failed to initialize Prometheus metrics collector: " + std::string(e.what()),
                          "PrometheusMetricsCollector", "initialize");
        }
        return false;
    }
}

void PrometheusMetricsCollector::shutdown() {
    if (!initialized_) return;

    initialized_ = false;

    // Clean up collectors
    circuit_breaker_collector_.reset();
    llm_collector_.reset();
    compliance_collector_.reset();
    system_collector_.reset();

    if (logger_) {
        logger_->info("Prometheus metrics collector shutdown complete",
                     "PrometheusMetricsCollector", "shutdown");
    }
}

std::string PrometheusMetricsCollector::collect_all_metrics() {
    if (!initialized_) return "";

    std::stringstream output;

    // Add Prometheus header
    output << generate_prometheus_header();

    // Collect metrics from all collectors
    auto circuit_metrics = circuit_breaker_collector_->collect_metrics();
    auto llm_metrics = llm_collector_->collect_metrics();
    auto compliance_metrics = compliance_collector_->collect_metrics();
    auto redis_metrics = redis_collector_->collect_metrics();
    auto system_metrics = system_collector_->collect_metrics();

    // Combine all metrics
    std::vector<MetricDefinition> all_metrics;
    all_metrics.reserve(circuit_metrics.size() + llm_metrics.size() +
                       compliance_metrics.size() + redis_metrics.size() + system_metrics.size());

    all_metrics.insert(all_metrics.end(), circuit_metrics.begin(), circuit_metrics.end());
    all_metrics.insert(all_metrics.end(), llm_metrics.begin(), llm_metrics.end());
    all_metrics.insert(all_metrics.end(), compliance_metrics.begin(), compliance_metrics.end());
    all_metrics.insert(all_metrics.end(), redis_metrics.begin(), redis_metrics.end());
    all_metrics.insert(all_metrics.end(), system_metrics.begin(), system_metrics.end());

    // Format as Prometheus output
    for (const auto& metric : all_metrics) {
        output << metric.to_prometheus_format() << "\n";
    }

    return output.str();
}

std::string PrometheusMetricsCollector::get_metrics_endpoint_response() {
    return collect_all_metrics();
}

std::string PrometheusMetricsCollector::generate_prometheus_header() const {
    std::stringstream header;
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();

    header << "# Regulens Prometheus Metrics\n";
    header << "# Generated at: " << timestamp << "\n";
    header << "# System: Enterprise Regulatory Compliance AI Platform\n";
    header << "\n";

    return header.str();
}

// Factory function

std::shared_ptr<PrometheusMetricsCollector> create_prometheus_metrics_collector(
    std::shared_ptr<ConfigurationManager> config,
    std::shared_ptr<StructuredLogger> logger,
    std::shared_ptr<ErrorHandler> error_handler) {

    auto collector = std::make_shared<PrometheusMetricsCollector>(config, logger, error_handler);
    if (collector->initialize()) {
        return collector;
    }
    return nullptr;
}

} // namespace regulens
