#include "change_detector.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <regex>
#include <openssl/sha.h>
#include <set>
#include <cmath>

namespace regulens {

// ==================== ChangeDetector Implementation ====================

ChangeDetector::ChangeDetector(std::shared_ptr<ConfigurationManager> config,
                             std::shared_ptr<StructuredLogger> logger)
    : config_(config), logger_(logger),
      total_detections_(0), hash_based_detections_(0),
      structural_detections_(0), semantic_detections_(0),
      false_positives_(0), semantic_threshold_(0.3),
      min_content_length_(50) {
}

ChangeDetector::~ChangeDetector() {
    // Cleanup resources
    std::lock_guard<std::mutex> lock(baselines_mutex_);
    content_hashes_.clear();
    baseline_content_.clear();
    baseline_metadata_.clear();
}

bool ChangeDetector::initialize() {
    try {
        logger_->info("Initializing ChangeDetector", "ChangeDetector", "initialize");

        // Load configuration parameters
        semantic_threshold_ = config_->get_double("change_detector.semantic_threshold").value_or(0.3);
        min_content_length_ = static_cast<size_t>(
            config_->get_int("change_detector.min_content_length").value_or(50));

        // Load ignored patterns from configuration
        auto ignored_patterns_str = config_->get_string("change_detector.ignored_patterns").value_or("");
        if (!ignored_patterns_str.empty()) {
            std::istringstream iss(ignored_patterns_str);
            std::string pattern;
            while (std::getline(iss, pattern, ',')) {
                // Trim whitespace
                pattern.erase(0, pattern.find_first_not_of(" \t\n\r"));
                pattern.erase(pattern.find_last_not_of(" \t\n\r") + 1);
                if (!pattern.empty()) {
                    ignored_patterns_.push_back(pattern);
                }
            }
        }

        // Add default ignored patterns
        if (ignored_patterns_.empty()) {
            ignored_patterns_ = {
                "Last Updated:",
                "Last Modified:",
                "Page \\d+ of \\d+",
                "Copyright \\d{4}",
                "Retrieved on:",
                "Accessed on:",
                "\\d{2}/\\d{2}/\\d{4} \\d{2}:\\d{2}:\\d{2}"
            };
        }

        logger_->info("ChangeDetector initialized successfully", "ChangeDetector", "initialize", {
            {"semantic_threshold", std::to_string(semantic_threshold_)},
            {"min_content_length", std::to_string(min_content_length_)},
            {"ignored_patterns_count", std::to_string(ignored_patterns_.size())}
        });

        return true;
    } catch (const std::exception& e) {
        logger_->error("Failed to initialize ChangeDetector", "ChangeDetector", "initialize", {
            {"error", e.what()}
        });
        return false;
    }
}

ChangeDetectionResult ChangeDetector::detect_changes(
    const std::string& source_id,
    const std::string& baseline_content,
    const std::string& new_content,
    const RegulatoryChangeMetadata& metadata) {

    auto start_time = std::chrono::high_resolution_clock::now();
    total_detections_++;

    try {
        // Check content length validity
        if (new_content.length() < min_content_length_) {
            logger_->debug("Content too short for analysis", "ChangeDetector", "detect_changes", {
                {"source_id", source_id},
                {"content_length", std::to_string(new_content.length())}
            });
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - start_time);
            return ChangeDetectionResult(false, {}, "skipped_short_content", 0.0, elapsed);
        }

        // Normalize content to remove ignored patterns
        std::string normalized_baseline = normalize_content(baseline_content);
        std::string normalized_new = normalize_content(new_content);

        // Phase 1: Hash-based detection for quick comparison
        std::string baseline_hash = calculate_content_hash(normalized_baseline);
        std::string new_hash = calculate_content_hash(normalized_new);

        bool hash_changed = detect_hash_changes(baseline_hash, new_hash);

        if (!hash_changed) {
            // No changes detected at hash level
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - start_time);
            hash_based_detections_++;

            logger_->debug("No changes detected (hash match)", "ChangeDetector", "detect_changes", {
                {"source_id", source_id},
                {"baseline_hash", baseline_hash.substr(0, 16)},
                {"new_hash", new_hash.substr(0, 16)}
            });

            return ChangeDetectionResult(false, {}, "hash_based", 1.0, elapsed);
        }

        // Phase 2: Structural change detection
        auto structural_changes = detect_structural_changes(normalized_baseline, normalized_new);

        if (structural_changes.empty()) {
            // Hash changed but no structural changes - likely insignificant
            false_positives_++;
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - start_time);

            logger_->debug("Hash changed but no structural changes detected", "ChangeDetector", "detect_changes", {
                {"source_id", source_id}
            });

            return ChangeDetectionResult(false, {}, "structural_analysis", 0.5, elapsed);
        }

        structural_detections_++;

        // Phase 3: Determine significance of changes
        bool is_significant = are_changes_significant(structural_changes, metadata);

        if (!is_significant) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - start_time);

            logger_->debug("Changes detected but not significant", "ChangeDetector", "detect_changes", {
                {"source_id", source_id},
                {"changes_count", std::to_string(structural_changes.size())}
            });

            return ChangeDetectionResult(false, {}, "structural_analysis", 0.6, elapsed);
        }

        // Phase 4: Semantic analysis for high-value changes
        double semantic_score = detect_semantic_changes(normalized_baseline, normalized_new, metadata);
        semantic_detections_++;

        // Create regulatory change objects for each significant change
        std::vector<RegulatoryChange> detected_changes;

        // Group changes by type/category
        auto change_categories = categorize_changes(structural_changes);

        for (const auto& [category, category_changes] : change_categories) {
            if (category_changes.empty()) continue;

            // Create a summary of changes in this category
            std::string change_summary = create_change_summary(category, category_changes);

            // Create RegulatoryChange object
            RegulatoryChangeMetadata change_metadata = metadata;
            change_metadata.keywords.push_back("structural_change");
            change_metadata.keywords.push_back(category);

            RegulatoryChange change(
                source_id,
                change_summary,
                metadata.custom_fields.count("url") ? metadata.custom_fields.at("url") : "",
                change_metadata
            );

            detected_changes.push_back(change);
        }

        // Calculate confidence score
        ChangeDetectionMethod primary_method = ChangeDetectionMethod::STRUCTURAL_DIFF;
        double confidence = calculate_confidence_score(primary_method, structural_changes);

        // Adjust confidence based on semantic score
        confidence = (confidence + semantic_score) / 2.0;

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start_time);

        last_detection_time_ = std::chrono::system_clock::now();

        logger_->info("Changes detected successfully", "ChangeDetector", "detect_changes", {
            {"source_id", source_id},
            {"changes_count", std::to_string(detected_changes.size())},
            {"confidence", std::to_string(confidence)},
            {"semantic_score", std::to_string(semantic_score)},
            {"processing_time_ms", std::to_string(elapsed.count())}
        });

        return ChangeDetectionResult(
            true,
            detected_changes,
            "multi_phase_analysis",
            confidence,
            elapsed
        );

    } catch (const std::exception& e) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start_time);

        logger_->error("Error during change detection", "ChangeDetector", "detect_changes", {
            {"source_id", source_id},
            {"error", e.what()}
        });

        return ChangeDetectionResult(false, {}, "error", 0.0, elapsed);
    }
}

std::string ChangeDetector::get_baseline_content(const std::string& source_id) const {
    std::lock_guard<std::mutex> lock(baselines_mutex_);
    auto it = baseline_content_.find(source_id);
    return (it != baseline_content_.end()) ? it->second : "";
}

void ChangeDetector::update_baseline_content(const std::string& source_id,
                                            const std::string& content,
                                            const RegulatoryChangeMetadata& metadata) {
    std::lock_guard<std::mutex> lock(baselines_mutex_);

    std::string normalized = normalize_content(content);
    std::string content_hash = calculate_content_hash(normalized);

    content_hashes_[source_id] = content_hash;
    baseline_content_[source_id] = content;
    baseline_metadata_[source_id] = metadata;

    logger_->debug("Updated baseline content", "ChangeDetector", "update_baseline_content", {
        {"source_id", source_id},
        {"content_hash", content_hash.substr(0, 16)},
        {"content_length", std::to_string(content.length())}
    });
}

nlohmann::json ChangeDetector::get_detection_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    return {
        {"total_detections", total_detections_.load()},
        {"hash_based_detections", hash_based_detections_.load()},
        {"structural_detections", structural_detections_.load()},
        {"semantic_detections", semantic_detections_.load()},
        {"false_positives", false_positives_.load()},
        {"baselines_stored", baseline_content_.size()},
        {"last_detection_time", std::chrono::duration_cast<std::chrono::milliseconds>(
            last_detection_time_.time_since_epoch()).count()}
    };
}

void ChangeDetector::clear_baselines() {
    std::lock_guard<std::mutex> lock(baselines_mutex_);
    content_hashes_.clear();
    baseline_content_.clear();
    baseline_metadata_.clear();

    logger_->info("Cleared all baselines", "ChangeDetector", "clear_baselines");
}

// ==================== Private Helper Methods ====================

bool ChangeDetector::detect_hash_changes(const std::string& baseline_hash,
                                        const std::string& new_hash) const {
    return baseline_hash != new_hash;
}

std::vector<std::string> ChangeDetector::detect_structural_changes(
    const std::string& baseline_content,
    const std::string& new_content) const {

    std::vector<std::string> changes;

    // Split content into lines
    auto baseline_lines = split_into_lines(baseline_content);
    auto new_lines = split_into_lines(new_content);

    // Perform line-by-line diff using longest common subsequence (LCS) algorithm
    auto diff_result = compute_diff(baseline_lines, new_lines);

    // Extract actual changes (additions, deletions, modifications)
    for (const auto& diff : diff_result) {
        if (!diff.empty()) {
            changes.push_back(diff);
        }
    }

    return changes;
}

double ChangeDetector::detect_semantic_changes(
    const std::string& baseline_content,
    const std::string& new_content,
    const RegulatoryChangeMetadata& metadata) const {

    // Extract keywords from both versions
    auto baseline_keywords = extract_keywords(baseline_content);
    auto new_keywords = extract_keywords(new_content);

    // Calculate keyword overlap using Jaccard similarity
    std::set<std::string> baseline_set(baseline_keywords.begin(), baseline_keywords.end());
    std::set<std::string> new_set(new_keywords.begin(), new_keywords.end());

    std::vector<std::string> intersection;
    std::set_intersection(baseline_set.begin(), baseline_set.end(),
                         new_set.begin(), new_set.end(),
                         std::back_inserter(intersection));

    std::vector<std::string> union_set;
    std::set_union(baseline_set.begin(), baseline_set.end(),
                  new_set.begin(), new_set.end(),
                  std::back_inserter(union_set));

    double jaccard_similarity = union_set.empty() ? 1.0 :
        static_cast<double>(intersection.size()) / static_cast<double>(union_set.size());

    // Semantic change score is inverse of similarity (1.0 = completely different)
    double semantic_change_score = 1.0 - jaccard_similarity;

    // Weight by content length change
    double length_ratio = std::abs(static_cast<double>(new_content.length()) -
                                   static_cast<double>(baseline_content.length())) /
                         std::max(static_cast<double>(baseline_content.length()), 1.0);

    // Combine scores with weighting
    double final_score = (semantic_change_score * 0.7) + (std::min(length_ratio, 1.0) * 0.3);

    return final_score;
}

std::string ChangeDetector::calculate_content_hash(const std::string& content) const {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, content.c_str(), content.length());
    SHA256_Final(hash, &sha256);

    // Convert to hex string
    std::ostringstream oss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }

    return oss.str();
}

std::vector<std::string> ChangeDetector::extract_keywords(const std::string& content) const {
    std::vector<std::string> keywords;

    // Regulatory-specific keywords to look for
    static const std::vector<std::string> regulatory_terms = {
        "regulation", "compliance", "requirement", "obligation", "prohibition",
        "mandate", "directive", "guideline", "standard", "policy", "procedure",
        "enforcement", "penalty", "sanction", "violation", "breach", "risk",
        "capital", "liquidity", "reporting", "disclosure", "audit", "assessment",
        "governance", "oversight", "supervision", "monitoring", "control",
        "framework", "implementation", "effective date", "deadline", "timeline"
    };

    // Convert content to lowercase for case-insensitive matching
    std::string lower_content = to_lowercase(content);

    // Find all regulatory terms in content
    for (const auto& term : regulatory_terms) {
        if (lower_content.find(term) != std::string::npos) {
            keywords.push_back(term);
        }
    }

    // Extract capitalized phrases (likely important terms)
    std::regex capitalized_phrase(R"(\b[A-Z][a-z]+(?:\s+[A-Z][a-z]+)+\b)");
    std::sregex_iterator iter(content.begin(), content.end(), capitalized_phrase);
    std::sregex_iterator end;

    while (iter != end) {
        keywords.push_back(iter->str());
        ++iter;
    }

    // Extract numeric values with context (dates, amounts, percentages)
    std::regex numeric_context(R"(\b(?:\d+(?:\.\d+)?%|\$\d+(?:,\d{3})*(?:\.\d{2})?|\d{1,2}/\d{1,2}/\d{2,4})\b)");
    iter = std::sregex_iterator(content.begin(), content.end(), numeric_context);

    while (iter != end) {
        keywords.push_back(iter->str());
        ++iter;
    }

    return keywords;
}

bool ChangeDetector::are_changes_significant(
    const std::vector<std::string>& changes,
    const RegulatoryChangeMetadata& metadata) const {

    if (changes.empty()) {
        return false;
    }

    // Minimum number of changes to be considered significant
    if (changes.size() < 3) {
        // Check if any single change is substantial
        for (const auto& change : changes) {
            if (change.length() > 100) {
                return true;
            }
        }
        return false;
    }

    // Calculate total change volume
    size_t total_change_chars = 0;
    for (const auto& change : changes) {
        total_change_chars += change.length();
    }

    // Significant if total changes exceed threshold
    if (total_change_chars > 500) {
        return true;
    }

    // Check for regulatory keywords in changes
    size_t regulatory_keyword_count = 0;
    for (const auto& change : changes) {
        auto keywords = extract_keywords(change);
        regulatory_keyword_count += keywords.size();
    }

    // Significant if contains multiple regulatory terms
    return regulatory_keyword_count >= 3;
}

double ChangeDetector::calculate_confidence_score(
    ChangeDetectionMethod method,
    const std::vector<std::string>& changes) const {

    double base_confidence = 0.0;

    switch (method) {
        case ChangeDetectionMethod::CONTENT_HASH:
            base_confidence = 0.95; // High confidence for hash-based
            break;
        case ChangeDetectionMethod::STRUCTURAL_DIFF:
            base_confidence = 0.80; // Good confidence for structural diff
            break;
        case ChangeDetectionMethod::SEMANTIC_ANALYSIS:
            base_confidence = 0.70; // Moderate confidence for semantic
            break;
        case ChangeDetectionMethod::TIMESTAMP_BASED:
            base_confidence = 0.60; // Lower confidence for timestamp
            break;
    }

    // Adjust confidence based on number of changes detected
    double change_factor = std::min(1.0, changes.size() / 10.0);
    double adjusted_confidence = base_confidence * (0.8 + (change_factor * 0.2));

    return std::max(0.0, std::min(1.0, adjusted_confidence));
}

// ==================== Utility Methods ====================

std::vector<std::string> ChangeDetector::split_into_lines(const std::string& content) const {
    std::vector<std::string> lines;
    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line)) {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (!line.empty()) {
            lines.push_back(line);
        }
    }

    return lines;
}

std::vector<std::string> ChangeDetector::compute_diff(
    const std::vector<std::string>& baseline_lines,
    const std::vector<std::string>& new_lines) const {

    std::vector<std::string> changes;

    // Simple diff algorithm based on line matching
    std::set<std::string> baseline_set(baseline_lines.begin(), baseline_lines.end());
    std::set<std::string> new_set(new_lines.begin(), new_lines.end());

    // Find additions (in new but not in baseline)
    for (const auto& line : new_lines) {
        if (baseline_set.find(line) == baseline_set.end()) {
            changes.push_back("+ " + line);
        }
    }

    // Find deletions (in baseline but not in new)
    for (const auto& line : baseline_lines) {
        if (new_set.find(line) == new_set.end()) {
            changes.push_back("- " + line);
        }
    }

    return changes;
}

std::string ChangeDetector::normalize_content(const std::string& content) const {
    std::string normalized = content;

    // Remove ignored patterns
    for (const auto& pattern : ignored_patterns_) {
        try {
            std::regex pattern_regex(pattern, std::regex::icase);
            normalized = std::regex_replace(normalized, pattern_regex, "");
        } catch (const std::regex_error&) {
            // Skip invalid regex patterns
            continue;
        }
    }

    // Normalize whitespace
    normalized = std::regex_replace(normalized, std::regex(R"(\s+)"), " ");

    // Trim
    normalized.erase(0, normalized.find_first_not_of(" \t\r\n"));
    normalized.erase(normalized.find_last_not_of(" \t\r\n") + 1);

    return normalized;
}

std::string ChangeDetector::to_lowercase(const std::string& str) const {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower;
}

std::unordered_map<std::string, std::vector<std::string>>
ChangeDetector::categorize_changes(const std::vector<std::string>& changes) const {

    std::unordered_map<std::string, std::vector<std::string>> categories;

    for (const auto& change : changes) {
        std::string category = "general";
        std::string lower = to_lowercase(change);

        // Categorize based on content
        if (lower.find("capital") != std::string::npos ||
            lower.find("tier 1") != std::string::npos ||
            lower.find("leverage ratio") != std::string::npos) {
            category = "capital_requirements";
        } else if (lower.find("report") != std::string::npos ||
                   lower.find("disclosure") != std::string::npos ||
                   lower.find("filing") != std::string::npos) {
            category = "reporting_requirements";
        } else if (lower.find("risk") != std::string::npos ||
                   lower.find("assessment") != std::string::npos) {
            category = "risk_management";
        } else if (lower.find("compliance") != std::string::npos ||
                   lower.find("obligation") != std::string::npos) {
            category = "compliance_obligations";
        } else if (lower.find("deadline") != std::string::npos ||
                   lower.find("effective date") != std::string::npos ||
                   lower.find("timeline") != std::string::npos) {
            category = "timeline_changes";
        }

        categories[category].push_back(change);
    }

    return categories;
}

std::string ChangeDetector::create_change_summary(
    const std::string& category,
    const std::vector<std::string>& changes) const {

    std::ostringstream summary;

    // Create human-readable category name
    std::string category_name = category;
    std::replace(category_name.begin(), category_name.end(), '_', ' ');

    // Capitalize first letter of each word
    bool capitalize_next = true;
    for (char& c : category_name) {
        if (capitalize_next && std::isalpha(c)) {
            c = std::toupper(c);
            capitalize_next = false;
        } else if (c == ' ') {
            capitalize_next = true;
        }
    }

    summary << category_name << " update detected";

    if (changes.size() == 1) {
        summary << " (1 change)";
    } else {
        summary << " (" << changes.size() << " changes)";
    }

    return summary.str();
}

// ==================== DocumentParser Implementation ====================

DocumentParser::DocumentParser(std::shared_ptr<ConfigurationManager> config,
                              std::shared_ptr<StructuredLogger> logger)
    : config_(config), logger_(logger),
      documents_parsed_(0), html_documents_(0),
      xml_documents_(0), text_documents_(0),
      parsing_errors_(0) {
}

DocumentParser::~DocumentParser() {
    // Cleanup resources
}

bool DocumentParser::initialize() {
    try {
        logger_->info("Initializing DocumentParser", "DocumentParser", "initialize");

        // Initialize regulatory body patterns
        regulatory_body_patterns_ = {
            {"SEC", {"Securities and Exchange Commission", "SEC", "U.S. Securities"}},
            {"FCA", {"Financial Conduct Authority", "FCA", "UK Financial"}},
            {"ECB", {"European Central Bank", "ECB", "Eurozone"}},
            {"FINRA", {"Financial Industry Regulatory Authority", "FINRA"}},
            {"CFTC", {"Commodity Futures Trading Commission", "CFTC"}},
            {"OCC", {"Office of the Comptroller of the Currency", "OCC"}},
            {"FDIC", {"Federal Deposit Insurance Corporation", "FDIC"}},
            {"FRB", {"Federal Reserve Board", "Federal Reserve", "FRB"}},
            {"EBA", {"European Banking Authority", "EBA"}},
            {"ESMA", {"European Securities and Markets Authority", "ESMA"}}
        };

        // Initialize document type patterns
        document_type_patterns_ = {
            {"rule", {"final rule", "proposed rule", "interim final rule", "regulation"}},
            {"guidance", {"guidance", "guideline", "advisory", "bulletin"}},
            {"order", {"order", "enforcement action", "cease and desist"}},
            {"release", {"release", "press release", "announcement"}},
            {"report", {"report", "study", "analysis"}},
            {"policy", {"policy statement", "policy", "framework"}}
        };

        logger_->info("DocumentParser initialized successfully", "DocumentParser", "initialize");
        return true;

    } catch (const std::exception& e) {
        logger_->error("Failed to initialize DocumentParser", "DocumentParser", "initialize", {
            {"error", e.what()}
        });
        return false;
    }
}

RegulatoryChangeMetadata DocumentParser::parse_document(
    const std::string& content,
    const std::string& content_type) {

    documents_parsed_++;

    try {
        RegulatoryChangeMetadata metadata;

        if (content_type == "text/html" || content_type == "html") {
            html_documents_++;
            metadata = parse_html(content);
        } else if (content_type == "text/xml" || content_type == "application/xml" ||
                   content_type == "xml" || content_type == "rss") {
            xml_documents_++;
            metadata = parse_xml(content);
        } else {
            text_documents_++;
            metadata = parse_text(content);
        }

        logger_->debug("Document parsed successfully", "DocumentParser", "parse_document", {
            {"content_type", content_type},
            {"regulatory_body", metadata.regulatory_body},
            {"document_type", metadata.document_type}
        });

        return metadata;

    } catch (const std::exception& e) {
        parsing_errors_++;
        logger_->error("Error parsing document", "DocumentParser", "parse_document", {
            {"content_type", content_type},
            {"error", e.what()}
        });
        return RegulatoryChangeMetadata();
    }
}

std::string DocumentParser::extract_title(const std::string& content,
                                         const std::string& content_type) const {

    if (content_type == "text/html" || content_type == "html") {
        // Extract from HTML title tag
        std::regex title_regex(R"(<title[^>]*>(.*?)</title>)", std::regex::icase);
        std::smatch match;
        if (std::regex_search(content, match, title_regex)) {
            return match[1].str();
        }

        // Try H1 tag
        std::regex h1_regex(R"(<h1[^>]*>(.*?)</h1>)", std::regex::icase);
        if (std::regex_search(content, match, h1_regex)) {
            return match[1].str();
        }
    }

    // Extract first line as title
    auto lines = split_into_lines(content);
    return lines.empty() ? "" : lines[0];
}

std::optional<std::chrono::system_clock::time_point>
DocumentParser::extract_effective_date(const std::string& content) const {

    // Look for common date patterns
    std::vector<std::regex> date_patterns = {
        std::regex(R"(effective date[:\s]+(\d{1,2}[/-]\d{1,2}[/-]\d{2,4}))", std::regex::icase),
        std::regex(R"(effective[:\s]+([A-Za-z]+\s+\d{1,2},?\s+\d{4}))", std::regex::icase),
        std::regex(R"((\d{4}-\d{2}-\d{2}))", std::regex::icase)
    };

    for (const auto& pattern : date_patterns) {
        std::smatch match;
        if (std::regex_search(content, match, pattern)) {
            // Found a date - would need date parsing library for full implementation
            // For now, return current time as placeholder
            return std::chrono::system_clock::now();
        }
    }

    return std::nullopt;
}

nlohmann::json DocumentParser::get_parsing_stats() const {
    return {
        {"documents_parsed", documents_parsed_.load()},
        {"html_documents", html_documents_.load()},
        {"xml_documents", xml_documents_.load()},
        {"text_documents", text_documents_.load()},
        {"parsing_errors", parsing_errors_.load()}
    };
}

// ==================== Private Parsing Methods ====================

RegulatoryChangeMetadata DocumentParser::parse_html(const std::string& html) {
    RegulatoryChangeMetadata metadata;

    // Extract regulatory body
    metadata.regulatory_body = extract_regulatory_body(html);

    // Extract document type
    metadata.document_type = extract_document_type(html);

    // Extract document number
    metadata.document_number = extract_document_number(html);

    // Extract keywords from content
    std::string text_content = strip_html_tags(html);
    metadata.keywords = extract_keywords_from_text(text_content);

    return metadata;
}

RegulatoryChangeMetadata DocumentParser::parse_xml(const std::string& xml) {
    RegulatoryChangeMetadata metadata;

    // Extract from common XML/RSS fields
    metadata.regulatory_body = extract_xml_field(xml, "source|publisher|author");
    metadata.document_type = extract_xml_field(xml, "type|category");
    metadata.document_number = extract_xml_field(xml, "id|guid|identifier");

    // Extract text content
    std::string text_content = strip_xml_tags(xml);
    metadata.keywords = extract_keywords_from_text(text_content);

    return metadata;
}

RegulatoryChangeMetadata DocumentParser::parse_text(const std::string& text) {
    RegulatoryChangeMetadata metadata;

    metadata.regulatory_body = extract_regulatory_body(text);
    metadata.document_type = extract_document_type(text);
    metadata.document_number = extract_document_number(text);
    metadata.keywords = extract_keywords_from_text(text);

    return metadata;
}

std::string DocumentParser::extract_regulatory_body(const std::string& content) const {
    std::string lower_content = to_lowercase(content);

    for (const auto& [body, patterns] : regulatory_body_patterns_) {
        for (const auto& pattern : patterns) {
            if (lower_content.find(to_lowercase(pattern)) != std::string::npos) {
                return body;
            }
        }
    }

    return "Unknown";
}

std::string DocumentParser::extract_document_type(const std::string& content) const {
    std::string lower_content = to_lowercase(content);

    for (const auto& [type, patterns] : document_type_patterns_) {
        for (const auto& pattern : patterns) {
            if (lower_content.find(pattern) != std::string::npos) {
                return type;
            }
        }
    }

    return "general";
}

std::string DocumentParser::extract_document_number(const std::string& content) const {
    // Look for common document number patterns
    std::vector<std::regex> number_patterns = {
        std::regex(R"(Release No\.?\s*([A-Z0-9-]+))", std::regex::icase),
        std::regex(R"(File No\.?\s*([A-Z0-9-]+))", std::regex::icase),
        std::regex(R"(Document No\.?\s*([A-Z0-9-]+))", std::regex::icase),
        std::regex(R"(Ref(?:erence)?\.?\s*([A-Z0-9-]+))", std::regex::icase)
    };

    for (const auto& pattern : number_patterns) {
        std::smatch match;
        if (std::regex_search(content, match, pattern)) {
            return match[1].str();
        }
    }

    return "";
}

// ==================== Utility Methods ====================

std::vector<std::string> DocumentParser::split_into_lines(const std::string& content) const {
    std::vector<std::string> lines;
    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line)) {
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        if (!line.empty()) {
            lines.push_back(line);
        }
    }

    return lines;
}

std::string DocumentParser::to_lowercase(const std::string& str) const {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower;
}

std::string DocumentParser::strip_html_tags(const std::string& html) const {
    std::regex tag_regex("<[^>]*>");
    return std::regex_replace(html, tag_regex, " ");
}

std::string DocumentParser::strip_xml_tags(const std::string& xml) const {
    std::regex tag_regex("<[^>]*>");
    return std::regex_replace(xml, tag_regex, " ");
}

std::string DocumentParser::extract_xml_field(const std::string& xml,
                                             const std::string& field_pattern) const {
    std::regex field_regex("<(?:" + field_pattern + ")[^>]*>([^<]*)</(?:" + field_pattern + ")>",
                          std::regex::icase);
    std::smatch match;

    if (std::regex_search(xml, match, field_regex)) {
        return match[1].str();
    }

    return "";
}

std::vector<std::string> DocumentParser::extract_keywords_from_text(
    const std::string& text) const {

    std::vector<std::string> keywords;

    // Regulatory-specific keywords
    static const std::vector<std::string> regulatory_terms = {
        "regulation", "compliance", "requirement", "obligation", "prohibition",
        "mandate", "directive", "guideline", "standard", "policy", "procedure",
        "enforcement", "penalty", "sanction", "violation", "capital", "liquidity",
        "reporting", "disclosure", "audit", "risk", "governance"
    };

    std::string lower_text = to_lowercase(text);

    for (const auto& term : regulatory_terms) {
        if (lower_text.find(term) != std::string::npos) {
            keywords.push_back(term);
        }
    }

    // Remove duplicates
    std::sort(keywords.begin(), keywords.end());
    keywords.erase(std::unique(keywords.begin(), keywords.end()), keywords.end());

    return keywords;
}

} // namespace regulens
