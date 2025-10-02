#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <nlohmann/json.hpp>

namespace regulens {

/**
 * @brief Impact level of regulatory changes
 */
enum class RegulatoryImpact {
    LOW,        // Minor clarifications, non-material changes
    MEDIUM,     // Process changes, new reporting requirements
    HIGH,       // Significant rule changes affecting operations
    CRITICAL    // Major regulatory reforms, immediate action required
};

/**
 * @brief Status of regulatory change processing
 */
enum class RegulatoryChangeStatus {
    DETECTED,       // Change detected but not analyzed
    ANALYZING,      // Currently being analyzed
    ANALYZED,       // Analysis complete
    DISTRIBUTED,    // Sent to relevant agents/systems
    ARCHIVED        // Processed and stored for reference
};

/**
 * @brief Affected business domains
 */
enum class BusinessDomain {
    FINANCIAL_REPORTING,
    RISK_MANAGEMENT,
    COMPLIANCE_MONITORING,
    DATA_PRIVACY,
    CONSUMER_PROTECTION,
    MARKET_CONDUCT,
    CAPITAL_REQUIREMENTS,
    ANTI_MONEY_LAUNDERING,
    CYBER_SECURITY,
    OPERATIONAL_RESILIENCE
};

/**
 * @brief Regulatory change metadata
 */
struct RegulatoryChangeMetadata {
    std::string regulatory_body;          // SEC, FCA, ECB, etc.
    std::string document_type;            // Rule, Guidance, Policy, etc.
    std::string document_number;          // Official document identifier
    std::vector<std::string> keywords;    // Important keywords extracted
    std::vector<std::string> affected_entities; // Companies/sectors affected
    std::unordered_map<std::string, std::string> custom_fields;

    nlohmann::json to_json() const {
        return {
            {"regulatory_body", regulatory_body},
            {"document_type", document_type},
            {"document_number", document_number},
            {"keywords", keywords},
            {"affected_entities", affected_entities},
            {"custom_fields", custom_fields}
        };
    }
};

/**
 * @brief Regulatory change analysis results
 */
struct RegulatoryChangeAnalysis {
    RegulatoryImpact impact_level;
    std::string executive_summary;
    std::vector<BusinessDomain> affected_domains;
    std::vector<std::string> required_actions;
    std::vector<std::string> compliance_deadlines;
    std::unordered_map<std::string, double> risk_scores; // Domain -> risk score
    std::chrono::system_clock::time_point analysis_timestamp;

    nlohmann::json to_json() const {
        nlohmann::json domains_json;
        for (auto domain : affected_domains) {
            domains_json.push_back(static_cast<int>(domain));
        }

        nlohmann::json risk_scores_json;
        for (const auto& [domain, score] : risk_scores) {
            risk_scores_json[domain] = score;
        }

        return {
            {"impact_level", static_cast<int>(impact_level)},
            {"executive_summary", executive_summary},
            {"affected_domains", domains_json},
            {"required_actions", required_actions},
            {"compliance_deadlines", compliance_deadlines},
            {"risk_scores", risk_scores_json},
            {"analysis_timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                analysis_timestamp.time_since_epoch()).count()}
        };
    }
};

/**
 * @brief Complete regulatory change record
 */
class RegulatoryChange {
public:
    RegulatoryChange(std::string source_id, std::string title,
                    std::string content_url, RegulatoryChangeMetadata metadata)
        : change_id_(generate_change_id()),
          source_id_(std::move(source_id)), title_(std::move(title)),
          content_url_(std::move(content_url)), metadata_(std::move(metadata)),
          status_(RegulatoryChangeStatus::DETECTED),
          detected_at_(std::chrono::system_clock::now()) {}

    // Getters
    const std::string& get_change_id() const { return change_id_; }
    const std::string& get_source_id() const { return source_id_; }
    const std::string& get_title() const { return title_; }
    const std::string& get_content_url() const { return content_url_; }
    const RegulatoryChangeMetadata& get_metadata() const { return metadata_; }
    RegulatoryChangeStatus get_status() const { return status_; }
    std::chrono::system_clock::time_point get_detected_at() const { return detected_at_; }

    // Analysis results
    const std::optional<RegulatoryChangeAnalysis>& get_analysis() const { return analysis_; }
    void set_analysis(const RegulatoryChangeAnalysis& analysis) {
        analysis_ = analysis;
        status_ = RegulatoryChangeStatus::ANALYZED;
        analyzed_at_ = std::chrono::system_clock::now();
    }

    // Status management
    void set_status(RegulatoryChangeStatus status) {
        status_ = status;
        if (status == RegulatoryChangeStatus::DISTRIBUTED) {
            distributed_at_ = std::chrono::system_clock::now();
        }
    }

    std::chrono::system_clock::time_point get_analyzed_at() const {
        return analyzed_at_.value_or(std::chrono::system_clock::time_point{});
    }

    std::chrono::system_clock::time_point get_distributed_at() const {
        return distributed_at_.value_or(std::chrono::system_clock::time_point{});
    }

    // Utility methods
    bool requires_immediate_action() const {
        return analysis_ && analysis_->impact_level == RegulatoryImpact::CRITICAL;
    }

    bool is_high_priority() const {
        return analysis_ &&
               (analysis_->impact_level == RegulatoryImpact::HIGH ||
                analysis_->impact_level == RegulatoryImpact::CRITICAL);
    }

    std::string get_impact_description() const {
        if (!analysis_) return "Not analyzed";

        switch (analysis_->impact_level) {
            case RegulatoryImpact::LOW: return "Low Impact";
            case RegulatoryImpact::MEDIUM: return "Medium Impact";
            case RegulatoryImpact::HIGH: return "High Impact";
            case RegulatoryImpact::CRITICAL: return "Critical Impact";
            default: return "Unknown Impact";
        }
    }

    // JSON serialization
    nlohmann::json to_json() const {
        nlohmann::json json = {
            {"change_id", change_id_},
            {"source_id", source_id_},
            {"title", title_},
            {"content_url", content_url_},
            {"metadata", metadata_.to_json()},
            {"status", static_cast<int>(status_)},
            {"detected_at", std::chrono::duration_cast<std::chrono::milliseconds>(
                detected_at_.time_since_epoch()).count()}
        };

        if (analysis_) {
            json["analysis"] = analysis_->to_json();
        }

        if (analyzed_at_) {
            json["analyzed_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                analyzed_at_->time_since_epoch()).count();
        }

        if (distributed_at_) {
            json["distributed_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                distributed_at_->time_since_epoch()).count();
        }

        return json;
    }

    // JSON deserialization
    static std::optional<RegulatoryChange> from_json(const nlohmann::json& json) {
        try {
            RegulatoryChangeMetadata metadata;
            if (json.contains("metadata")) {
                const auto& meta = json["metadata"];
                metadata.regulatory_body = meta["regulatory_body"].get<std::string>();
                metadata.document_type = meta["document_type"].get<std::string>();
                metadata.document_number = meta["document_number"].get<std::string>();
                metadata.keywords = meta["keywords"].get<std::vector<std::string>>();
                metadata.affected_entities = meta["affected_entities"].get<std::vector<std::string>>();

                if (meta.contains("custom_fields")) {
                    for (const auto& [key, value] : meta["custom_fields"].items()) {
                        metadata.custom_fields[key] = value.get<std::string>();
                    }
                }
            }

            RegulatoryChange change(
                json["source_id"].get<std::string>(),
                json["title"].get<std::string>(),
                json["content_url"].get<std::string>(),
                metadata
            );

            change.status_ = static_cast<RegulatoryChangeStatus>(json["status"].get<int>());
            change.detected_at_ = std::chrono::system_clock::time_point(
                std::chrono::milliseconds(json["detected_at"].get<int64_t>()));

            // Restore optional fields
            if (json.contains("analyzed_at")) {
                change.analyzed_at_ = std::chrono::system_clock::time_point(
                    std::chrono::milliseconds(json["analyzed_at"].get<int64_t>()));
            }

            if (json.contains("distributed_at")) {
                change.distributed_at_ = std::chrono::system_clock::time_point(
                    std::chrono::milliseconds(json["distributed_at"].get<int64_t>()));
            }

            if (json.contains("analysis")) {
                // Would need to parse analysis object - simplified for now
                change.analysis_ = RegulatoryChangeAnalysis{};
            }

            return change;
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }

private:
    static std::string generate_change_id() {
        static std::atomic<uint64_t> counter{0};
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch()).count();
        return "reg_change_" + std::to_string(timestamp) + "_" + std::to_string(++counter);
    }

    std::string change_id_;
    std::string source_id_;
    std::string title_;
    std::string content_url_;
    RegulatoryChangeMetadata metadata_;
    RegulatoryChangeStatus status_;
    std::chrono::system_clock::time_point detected_at_;

    std::optional<RegulatoryChangeAnalysis> analysis_;
    std::optional<std::chrono::system_clock::time_point> analyzed_at_;
    std::optional<std::chrono::system_clock::time_point> distributed_at_;
};

// Helper functions
inline std::string regulatory_impact_to_string(RegulatoryImpact impact) {
    switch (impact) {
        case RegulatoryImpact::LOW: return "LOW";
        case RegulatoryImpact::MEDIUM: return "MEDIUM";
        case RegulatoryImpact::HIGH: return "HIGH";
        case RegulatoryImpact::CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

inline std::string business_domain_to_string(BusinessDomain domain) {
    switch (domain) {
        case BusinessDomain::FINANCIAL_REPORTING: return "Financial Reporting";
        case BusinessDomain::RISK_MANAGEMENT: return "Risk Management";
        case BusinessDomain::COMPLIANCE_MONITORING: return "Compliance Monitoring";
        case BusinessDomain::DATA_PRIVACY: return "Data Privacy";
        case BusinessDomain::CONSUMER_PROTECTION: return "Consumer Protection";
        case BusinessDomain::MARKET_CONDUCT: return "Market Conduct";
        case BusinessDomain::CAPITAL_REQUIREMENTS: return "Capital Requirements";
        case BusinessDomain::ANTI_MONEY_LAUNDERING: return "Anti-Money Laundering";
        case BusinessDomain::CYBER_SECURITY: return "Cyber Security";
        case BusinessDomain::OPERATIONAL_RESILIENCE: return "Operational Resilience";
        default: return "Unknown Domain";
    }
}

} // namespace regulens

