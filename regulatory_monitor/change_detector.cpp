#include "change_detector.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <regex>
#include <openssl/sha.h>
#include <set>
#include <cmath>
#include <queue>
#include <unordered_set>
#include <numeric>

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
    // Cleanup resources with proper locking
    std::lock_guard<std::mutex> lock(baselines_mutex_);
    content_hashes_.clear();
    baseline_content_.clear();
    baseline_metadata_.clear();
}

bool ChangeDetector::initialize() {
    try {
        logger_->info("Initializing ChangeDetector with advanced algorithms", "ChangeDetector", "initialize");

        // Load configuration parameters with defaults
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

        // Add comprehensive default ignored patterns
        if (ignored_patterns_.empty()) {
            ignored_patterns_ = {
                // Timestamps and dates
                "Last Updated:\\s*\\d{2}/\\d{2}/\\d{4}",
                "Last Modified:\\s*[^\\n]+",
                "Retrieved on:\\s*[^\\n]+",
                "Accessed on:\\s*[^\\n]+",
                "Published:\\s*\\d{2}/\\d{2}/\\d{4}",
                "\\d{2}/\\d{2}/\\d{4}\\s+\\d{2}:\\d{2}:\\d{2}",

                // Page metadata
                "Page\\s+\\d+\\s+of\\s+\\d+",
                "\\[Page\\s+\\d+\\]",

                // Copyright and legal boilerplate
                "Copyright\\s+\\d{4}",
                "©\\s*\\d{4}",
                "All rights reserved",

                // Common HTML/Web artifacts
                "<script[^>]*>.*?</script>",
                "<style[^>]*>.*?</style>",
                "<!-- .* -->",

                // Document IDs that change per version
                "Version:\\s*[\\d\\.]+",
                "Revision:\\s*[\\d\\.]+",
                "Document ID:\\s*[A-Z0-9-]+"
            };
        }

        logger_->info("ChangeDetector initialized successfully with advanced features",
                     "ChangeDetector", "initialize", {
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
        // Validate input
        if (new_content.length() < min_content_length_) {
            logger_->debug("Content too short for analysis", "ChangeDetector", "detect_changes", {
                {"source_id", source_id},
                {"content_length", std::to_string(new_content.length())}
            });
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - start_time);
            return ChangeDetectionResult(false, {}, "skipped_short_content", 0.0, elapsed);
        }

        // Normalize content to remove noise and insignificant changes
        std::string normalized_baseline = normalize_content(baseline_content);
        std::string normalized_new = normalize_content(new_content);

        // Phase 1: Fast hash-based detection
        std::string baseline_hash = calculate_content_hash(normalized_baseline);
        std::string new_hash = calculate_content_hash(normalized_new);

        if (!detect_hash_changes(baseline_hash, new_hash)) {
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

        // Phase 2: Advanced structural diff using Myers algorithm and LCS
        auto diff_chunks = compute_advanced_diff(normalized_baseline, normalized_new);

        if (diff_chunks.empty()) {
            false_positives_++;
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - start_time);

            logger_->debug("Hash changed but no significant structural changes",
                          "ChangeDetector", "detect_changes", {
                {"source_id", source_id}
            });

            return ChangeDetectionResult(false, {}, "structural_analysis", 0.5, elapsed);
        }

        structural_detections_++;

        // Step 3: Semantic change analysis
        double semantic_score = detect_semantic_changes(normalized_baseline, normalized_new, metadata);
        semantic_detections_++;

        // Step 4: Convert diff chunks to regulatory changes
        std::vector<RegulatoryChange> detected_changes;
        auto change_summaries = analyze_diff_chunks(diff_chunks, metadata);

        for (const auto& summary : change_summaries) {
            RegulatoryChangeMetadata change_metadata = metadata;
            change_metadata.keywords.push_back("structural_change");
            change_metadata.keywords.push_back(summary.category);

            // Add impact indicators to metadata
            if (summary.impact_score > 0.7) {
                change_metadata.keywords.push_back("high_impact");
            }

            RegulatoryChange change(
                source_id,
                summary.title,
                metadata.custom_fields.count("url") ? metadata.custom_fields.at("url") : "",
                change_metadata
            );

            detected_changes.push_back(change);
        }

        // Calculate overall confidence score
        double structural_confidence = calculate_structural_confidence(diff_chunks);
        double confidence = (structural_confidence * 0.6) + (semantic_score * 0.4);

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start_time);

        last_detection_time_ = std::chrono::system_clock::now();

        logger_->info("Changes detected successfully", "ChangeDetector", "detect_changes", {
            {"source_id", source_id},
            {"changes_count", std::to_string(detected_changes.size())},
            {"confidence", std::to_string(confidence)},
            {"semantic_score", std::to_string(semantic_score)},
            {"structural_confidence", std::to_string(structural_confidence)},
            {"diff_chunks", std::to_string(diff_chunks.size())},
            {"processing_time_ms", std::to_string(elapsed.count())}
        });

        return ChangeDetectionResult(
            true,
            detected_changes,
            "advanced_multi_phase_analysis",
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

    size_t total = total_detections_.load();
    size_t hash_based = hash_based_detections_.load();
    size_t structural = structural_detections_.load();
    size_t semantic = semantic_detections_.load();
    size_t false_pos = false_positives_.load();

    double accuracy = total > 0 ?
        static_cast<double>(total - false_pos) / static_cast<double>(total) : 1.0;

    return {
        {"total_detections", total},
        {"hash_based_detections", hash_based},
        {"structural_detections", structural},
        {"semantic_detections", semantic},
        {"false_positives", false_pos},
        {"accuracy", accuracy},
        {"baselines_stored", baseline_content_.size()},
        {"last_detection_time", std::chrono::duration_cast<std::chrono::milliseconds>(
            last_detection_time_.time_since_epoch()).count()},
        {"semantic_threshold", semantic_threshold_},
        {"min_content_length", min_content_length_}
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

    // Use Myers diff algorithm for optimal diff
    auto edit_script = compute_myers_diff(baseline_lines, new_lines);

    // Convert edit script to change descriptions
    for (const auto& edit : edit_script) {
        std::string change_desc;
        switch (edit.operation) {
            case ChangeDetector::EditOp::INSERT:
                change_desc = "+ " + edit.content;
                break;
            case ChangeDetector::EditOp::DELETE:
                change_desc = "- " + edit.content;
                break;
            case ChangeDetector::EditOp::REPLACE:
                change_desc = "~ " + edit.content;
                break;
            default:
                continue;
        }
        changes.push_back(change_desc);
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

    // Calculate multiple semantic similarity metrics

    // 1. Jaccard similarity for keyword overlap
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

    // 2. Cosine similarity using term frequency
    auto baseline_tf = calculate_term_frequency(baseline_content);
    auto new_tf = calculate_term_frequency(new_content);
    double cosine_sim = calculate_cosine_similarity(baseline_tf, new_tf);

    // 3. Structural similarity based on document structure
    double structural_sim = calculate_structural_similarity(baseline_content, new_content);

    // 4. Length-based change indicator
    double length_ratio = std::abs(static_cast<double>(new_content.length()) -
                                   static_cast<double>(baseline_content.length())) /
                         std::max(static_cast<double>(baseline_content.length()), 1.0);

    // Combine metrics with weighted average
    double semantic_change_score =
        (1.0 - jaccard_similarity) * 0.35 +
        (1.0 - cosine_sim) * 0.35 +
        (1.0 - structural_sim) * 0.20 +
        std::min(length_ratio, 1.0) * 0.10;

    return std::max(0.0, std::min(1.0, semantic_change_score));
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

    // Comprehensive regulatory keywords
    static const std::vector<std::string> regulatory_terms = {
        // Core regulatory terms
        "regulation", "compliance", "requirement", "obligation", "prohibition",
        "mandate", "directive", "guideline", "standard", "policy", "procedure",
        "rule", "law", "statute", "ordinance", "framework",

        // Enforcement and violations
        "enforcement", "penalty", "sanction", "violation", "breach", "infringement",
        "fine", "censure", "suspension", "revocation",

        // Risk and capital
        "risk", "capital", "liquidity", "leverage", "solvency", "adequacy",
        "buffer", "tier 1", "tier 2", "basel", "stress test",

        // Reporting and disclosure
        "reporting", "disclosure", "filing", "submission", "notification",
        "audit", "assessment", "review", "examination", "inspection",

        // Governance and controls
        "governance", "oversight", "supervision", "monitoring", "control",
        "internal control", "risk management", "compliance program",

        // Operational terms
        "implementation", "effective date", "deadline", "timeline", "transition",
        "phase-in", "exemption", "waiver", "exception"
    };

    // Convert content to lowercase for case-insensitive matching
    std::string lower_content = to_lowercase(content);

    // Find all regulatory terms in content
    for (const auto& term : regulatory_terms) {
        if (lower_content.find(term) != std::string::npos) {
            keywords.push_back(term);
        }
    }

    // Extract capitalized phrases (likely important regulatory terms)
    std::regex capitalized_phrase(R"(\b[A-Z][a-z]+(?:\s+[A-Z][a-z]+){1,4}\b)");
    std::sregex_iterator iter(content.begin(), content.end(), capitalized_phrase);
    std::sregex_iterator end;

    while (iter != end) {
        std::string phrase = iter->str();
        if (phrase.length() > 5 && phrase.length() < 50) {  // Reasonable phrase length
            keywords.push_back(phrase);
        }
        ++iter;
    }

    // Extract numeric values with context (critical regulatory values)
    std::regex numeric_patterns(
        R"(\b(?:\d+(?:\.\d+)?%|\$\d+(?:,\d{3})*(?:\.\d{2})?|"
        R"(\d{1,2}/\d{1,2}/\d{2,4}|\d+\s*(?:days|months|years|basis points))\b)"
    );
    iter = std::sregex_iterator(content.begin(), content.end(), numeric_patterns);

    while (iter != end) {
        keywords.push_back(iter->str());
        ++iter;
    }

    // Remove duplicates while preserving order
    std::unordered_set<std::string> seen;
    std::vector<std::string> unique_keywords;
    for (const auto& kw : keywords) {
        if (seen.insert(kw).second) {
            unique_keywords.push_back(kw);
        }
    }

    return unique_keywords;
}

bool ChangeDetector::are_changes_significant(
    const std::vector<std::string>& changes,
    const RegulatoryChangeMetadata& metadata) const {

    if (changes.empty()) {
        return false;
    }

    // Multi-factor significance assessment

    // Factor 1: Number of changes
    if (changes.size() >= 5) {
        return true;
    }

    // Factor 2: Individual change significance
    for (const auto& change : changes) {
        if (change.length() > 100) {  // Substantial change
            return true;
        }
    }

    // Factor 3: Total change volume
    size_t total_change_chars = 0;
    for (const auto& change : changes) {
        total_change_chars += change.length();
    }

    if (total_change_chars > 500) {
        return true;
    }

    // Factor 4: Regulatory keyword density
    size_t regulatory_keyword_count = 0;
    for (const auto& change : changes) {
        auto keywords = extract_keywords(change);
        regulatory_keyword_count += keywords.size();
    }

    if (regulatory_keyword_count >= 3) {
        return true;
    }

    // Factor 5: High-priority regulatory body changes
    static const std::unordered_set<std::string> high_priority_bodies = {
        "SEC", "FCA", "ECB", "FINRA", "CFTC", "FDIC", "FRB"
    };

    if (high_priority_bodies.count(metadata.regulatory_body)) {
        // Lower threshold for high-priority bodies
        return changes.size() >= 2 || total_change_chars > 200;
    }

    return false;
}

double ChangeDetector::calculate_confidence_score(
    ChangeDetectionMethod method,
    const std::vector<std::string>& changes) const {

    double base_confidence = 0.0;

    switch (method) {
        case ChangeDetectionMethod::CONTENT_HASH:
            base_confidence = 0.95;
            break;
        case ChangeDetectionMethod::STRUCTURAL_DIFF:
            base_confidence = 0.85;
            break;
        case ChangeDetectionMethod::SEMANTIC_ANALYSIS:
            base_confidence = 0.75;
            break;
        case ChangeDetectionMethod::TIMESTAMP_BASED:
            base_confidence = 0.60;
            break;
    }

    // Adjust confidence based on change volume and consistency
    double change_factor = std::min(1.0, changes.size() / 10.0);
    double adjusted_confidence = base_confidence * (0.8 + (change_factor * 0.2));

    return std::max(0.0, std::min(1.0, adjusted_confidence));
}

// ==================== Advanced Diff Algorithms ====================

/**
 * @brief Myers diff algorithm implementation
 * Computes optimal edit script using Myers O(ND) algorithm
 */
std::vector<ChangeDetector::Edit> ChangeDetector::compute_myers_diff(
    const std::vector<std::string>& baseline_lines,
    const std::vector<std::string>& new_lines) const {

    size_t n = baseline_lines.size();
    size_t m = new_lines.size();
    size_t max_d = n + m;

    // V array for storing endpoints of furthest reaching D-paths
    std::vector<int> v(2 * max_d + 1, 0);
    std::vector<std::vector<int>> trace;

    // Find shortest edit script
    for (size_t d = 0; d <= max_d; ++d) {
        trace.push_back(v);

        for (int k = -static_cast<int>(d); k <= static_cast<int>(d); k += 2) {
            int x;
            if (k == -static_cast<int>(d) ||
                (k != static_cast<int>(d) && v[max_d + k - 1] < v[max_d + k + 1])) {
                x = v[max_d + k + 1];
            } else {
                x = v[max_d + k - 1] + 1;
            }

            int y = x - k;

            while (static_cast<size_t>(x) < n && static_cast<size_t>(y) < m &&
                   baseline_lines[x] == new_lines[y]) {
                x++;
                y++;
            }

            v[max_d + k] = x;

            if (static_cast<size_t>(x) >= n && static_cast<size_t>(y) >= m) {
                // Found the end, backtrack to build edit script
                return backtrack_myers_diff(baseline_lines, new_lines, trace, max_d);
            }
        }
    }

    // Fallback to simple diff if Myers fails
    return compute_simple_diff(baseline_lines, new_lines);
}

/**
 * @brief Backtrack through Myers diff trace to build edit script
 */
std::vector<ChangeDetector::Edit> ChangeDetector::backtrack_myers_diff(
    const std::vector<std::string>& baseline_lines,
    const std::vector<std::string>& new_lines,
    const std::vector<std::vector<int>>& trace,
    size_t max_d) const {

    std::vector<Edit> edits;
    int x = baseline_lines.size();
    int y = new_lines.size();

    for (int d = trace.size() - 1; d >= 0; --d) {
        const auto& v = trace[d];
        int k = x - y;

        int prev_k;
        if (k == -d || (k != static_cast<int>(d) &&
            v[max_d + k - 1] < v[max_d + k + 1])) {
            prev_k = k + 1;
        } else {
            prev_k = k - 1;
        }

        int prev_x = v[max_d + prev_k];
        int prev_y = prev_x - prev_k;

        while (x > prev_x && y > prev_y) {
            edits.push_back(Edit(ChangeDetector::EditOp::MATCH, x - 1, y - 1, baseline_lines[x - 1]));
            x--;
            y--;
        }

        if (d > 0) {
            if (x > prev_x) {
                edits.push_back(Edit(ChangeDetector::EditOp::DELETE, x - 1, y, baseline_lines[x - 1]));
                x--;
            } else {
                edits.push_back(Edit(ChangeDetector::EditOp::INSERT, x, y - 1, new_lines[y - 1]));
                y--;
            }
        }
    }

    std::reverse(edits.begin(), edits.end());
    return edits;
}

/**
 * @brief Compute longest common subsequence for diff
 */
std::vector<ChangeDetector::Edit> ChangeDetector::compute_simple_diff(
    const std::vector<std::string>& baseline_lines,
    const std::vector<std::string>& new_lines) const {

    size_t n = baseline_lines.size();
    size_t m = new_lines.size();

    // LCS dynamic programming table
    std::vector<std::vector<size_t>> dp(n + 1, std::vector<size_t>(m + 1, 0));

    for (size_t i = 1; i <= n; ++i) {
        for (size_t j = 1; j <= m; ++j) {
            if (baseline_lines[i - 1] == new_lines[j - 1]) {
                dp[i][j] = dp[i - 1][j - 1] + 1;
            } else {
                dp[i][j] = std::max(dp[i - 1][j], dp[i][j - 1]);
            }
        }
    }

    // Backtrack to build edit script
    std::vector<Edit> edits;
    size_t i = n, j = m;

    while (i > 0 || j > 0) {
        if (i > 0 && j > 0 && baseline_lines[i - 1] == new_lines[j - 1]) {
            edits.push_back(Edit(ChangeDetector::EditOp::MATCH, i - 1, j - 1, baseline_lines[i - 1]));
            i--;
            j--;
        } else if (j > 0 && (i == 0 || dp[i][j - 1] >= dp[i - 1][j])) {
            edits.push_back(Edit(ChangeDetector::EditOp::INSERT, i, j - 1, new_lines[j - 1]));
            j--;
        } else if (i > 0) {
            edits.push_back(Edit(ChangeDetector::EditOp::DELETE, i - 1, j, baseline_lines[i - 1]));
            i--;
        }
    }

    std::reverse(edits.begin(), edits.end());
    return edits;
}

/**
 * @brief Compute advanced diff with chunking and significance scoring
 */
std::vector<ChangeDetector::DiffChunk> ChangeDetector::compute_advanced_diff(
    const std::string& baseline_content,
    const std::string& new_content) const {

    auto baseline_lines = split_into_lines(baseline_content);
    auto new_lines = split_into_lines(new_content);

    auto edit_script = compute_myers_diff(baseline_lines, new_lines);

    // Group edits into chunks
    std::vector<DiffChunk> chunks;
    DiffChunk current_chunk;
    bool in_chunk = false;

    for (size_t i = 0; i < edit_script.size(); ++i) {
        const auto& edit = edit_script[i];

        if (edit.operation != ChangeDetector::EditOp::MATCH) {
            if (!in_chunk) {
                current_chunk = DiffChunk();
                current_chunk.baseline_start = edit.baseline_index;
                current_chunk.new_start = edit.new_index;
                in_chunk = true;
            }

            current_chunk.baseline_end = edit.baseline_index;
            current_chunk.new_end = edit.new_index;

            if (edit.operation == ChangeDetector::EditOp::DELETE) {
                current_chunk.deleted_lines.push_back(edit.content);
            } else if (edit.operation == ChangeDetector::EditOp::INSERT) {
                current_chunk.inserted_lines.push_back(edit.content);
            }
        } else {
            if (in_chunk) {
                // Calculate significance score for this chunk
                current_chunk.significance_score = calculate_chunk_significance(current_chunk);

                if (current_chunk.significance_score > 0.1) {  // Filter out trivial changes
                    chunks.push_back(current_chunk);
                }
                in_chunk = false;
            }
        }
    }

    // Don't forget last chunk
    if (in_chunk) {
        current_chunk.significance_score = calculate_chunk_significance(current_chunk);
        if (current_chunk.significance_score > 0.1) {
            chunks.push_back(current_chunk);
        }
    }

    return chunks;
}

/**
 * @brief Calculate significance score for a diff chunk
 */
double ChangeDetector::calculate_chunk_significance(const ChangeDetector::DiffChunk& chunk) const {
    // Factors for significance:
    // 1. Volume of change (number of lines)
    size_t total_lines = chunk.deleted_lines.size() + chunk.inserted_lines.size();
    double volume_score = std::min(1.0, total_lines / 10.0);

    // 2. Content importance (regulatory keywords)
    size_t keyword_count = 0;
    for (const auto& line : chunk.deleted_lines) {
        keyword_count += extract_keywords(line).size();
    }
    for (const auto& line : chunk.inserted_lines) {
        keyword_count += extract_keywords(line).size();
    }
    double keyword_score = std::min(1.0, keyword_count / 5.0);

    // 3. Change type (replacement vs pure addition/deletion)
    double change_type_score = 0.5;
    if (!chunk.deleted_lines.empty() && !chunk.inserted_lines.empty()) {
        change_type_score = 0.8;  // Replacements are more significant
    }

    // Combined score
    return (volume_score * 0.4) + (keyword_score * 0.4) + (change_type_score * 0.2);
}

/**
 * @brief Calculate structural confidence based on diff chunks
 */
double ChangeDetector::calculate_structural_confidence(
    const std::vector<ChangeDetector::DiffChunk>& chunks) const {

    if (chunks.empty()) {
        return 0.0;
    }

    // Average significance across chunks
    double total_significance = 0.0;
    for (const auto& chunk : chunks) {
        total_significance += chunk.significance_score;
    }

    double avg_significance = total_significance / chunks.size();

    // Confidence increases with number of consistent changes
    double chunk_factor = std::min(1.0, chunks.size() / 5.0);

    return avg_significance * (0.7 + chunk_factor * 0.3);
}

// ==================== Semantic Analysis Methods ====================

/**
 * @brief Calculate term frequency for cosine similarity
 */
std::unordered_map<std::string, double> ChangeDetector::calculate_term_frequency(
    const std::string& content) const {

    std::unordered_map<std::string, size_t> term_counts;
    std::unordered_map<std::string, double> term_freq;

    // Tokenize content
    std::regex word_regex(R"(\b\w+\b)");
    std::sregex_iterator iter(content.begin(), content.end(), word_regex);
    std::sregex_iterator end;

    size_t total_terms = 0;
    while (iter != end) {
        std::string term = to_lowercase(iter->str());
        if (term.length() > 2) {  // Ignore very short words
            term_counts[term]++;
            total_terms++;
        }
        ++iter;
    }

    // Calculate normalized frequency
    for (const auto& [term, count] : term_counts) {
        term_freq[term] = static_cast<double>(count) / static_cast<double>(total_terms);
    }

    return term_freq;
}

/**
 * @brief Calculate cosine similarity between two term frequency maps
 */
double ChangeDetector::calculate_cosine_similarity(
    const std::unordered_map<std::string, double>& tf1,
    const std::unordered_map<std::string, double>& tf2) const {

    double dot_product = 0.0;
    double norm1 = 0.0;
    double norm2 = 0.0;

    // Calculate dot product and norm for tf1
    for (const auto& [term, freq] : tf1) {
        norm1 += freq * freq;
        auto it = tf2.find(term);
        if (it != tf2.end()) {
            dot_product += freq * it->second;
        }
    }

    // Calculate norm for tf2
    for (const auto& [term, freq] : tf2) {
        norm2 += freq * freq;
    }

    norm1 = std::sqrt(norm1);
    norm2 = std::sqrt(norm2);

    if (norm1 == 0.0 || norm2 == 0.0) {
        return 0.0;
    }

    return dot_product / (norm1 * norm2);
}

/**
 * @brief Calculate structural similarity based on document organization
 */
double ChangeDetector::calculate_structural_similarity(
    const std::string& baseline_content,
    const std::string& new_content) const {

    // Extract structural elements (headers, sections, etc.)
    auto baseline_structure = extract_structural_elements(baseline_content);
    auto new_structure = extract_structural_elements(new_content);

    if (baseline_structure.empty() && new_structure.empty()) {
        return 1.0;  // Both have no structure
    }

    if (baseline_structure.empty() || new_structure.empty()) {
        return 0.0;  // One has structure, other doesn't
    }

    // Calculate Jaccard similarity of structural elements
    std::set<std::string> baseline_set(baseline_structure.begin(), baseline_structure.end());
    std::set<std::string> new_set(new_structure.begin(), new_structure.end());

    std::vector<std::string> intersection;
    std::set_intersection(baseline_set.begin(), baseline_set.end(),
                         new_set.begin(), new_set.end(),
                         std::back_inserter(intersection));

    std::vector<std::string> union_set;
    std::set_union(baseline_set.begin(), baseline_set.end(),
                  new_set.begin(), new_set.end(),
                  std::back_inserter(union_set));

    return static_cast<double>(intersection.size()) / static_cast<double>(union_set.size());
}

/**
 * @brief Extract structural elements from content
 */
std::vector<std::string> ChangeDetector::extract_structural_elements(
    const std::string& content) const {

    std::vector<std::string> elements;

    // Extract section headers (various formats)
    std::vector<std::regex> header_patterns = {
        std::regex(R"(^#+\s+(.+)$)", std::regex::multiline),  // Markdown headers
        std::regex(R"(^Section\s+[\d\.]+\s*[:-]\s*(.+)$)", std::regex::icase | std::regex::multiline),
        std::regex(R"(^[IVX]+\.\s+(.+)$)", std::regex::multiline),  // Roman numerals
        std::regex(R"(^\d+\.\s+(.+)$)", std::regex::multiline),  // Numbered sections
        std::regex(R"(^[A-Z][^a-z\n]{5,}$)", std::regex::multiline)  // ALL CAPS headers
    };

    for (const auto& pattern : header_patterns) {
        std::sregex_iterator iter(content.begin(), content.end(), pattern);
        std::sregex_iterator end;

        while (iter != end) {
            if (iter->size() > 1) {
                elements.push_back(iter->str(1));
            } else {
                elements.push_back(iter->str());
            }
            ++iter;
        }
    }

    return elements;
}

// ==================== Change Analysis Methods ====================

/**
 * @brief Analyze diff chunks and create change summaries
 */
std::vector<ChangeDetector::ChangeSummary> ChangeDetector::analyze_diff_chunks(
    const std::vector<ChangeDetector::DiffChunk>& chunks,
    const RegulatoryChangeMetadata& metadata) const {

    std::vector<ChangeSummary> summaries;
    std::unordered_map<std::string, std::vector<const DiffChunk*>> categorized_chunks;

    // Categorize chunks
    for (const auto& chunk : chunks) {
        std::string category = categorize_chunk(chunk);
        categorized_chunks[category].push_back(&chunk);
    }

    // Create summary for each category
    for (const auto& [category, category_chunks] : categorized_chunks) {
        if (category_chunks.empty()) continue;

        ChangeSummary summary;
        summary.category = category;
        summary.title = create_category_title(category, category_chunks.size());

        // Calculate aggregate impact score
        double total_impact = 0.0;
        for (const auto* chunk : category_chunks) {
            total_impact += chunk->significance_score;
        }
        summary.impact_score = total_impact / category_chunks.size();

        // Extract key details
        for (const auto* chunk : category_chunks) {
            if (chunk->inserted_lines.size() <= 3 && chunk->deleted_lines.size() <= 3) {
                // Small changes - include details
                for (const auto& line : chunk->inserted_lines) {
                    if (line.length() < 200) {
                        summary.details.push_back("Added: " + line);
                    }
                }
                for (const auto& line : chunk->deleted_lines) {
                    if (line.length() < 200) {
                        summary.details.push_back("Removed: " + line);
                    }
                }
            }
        }

        summaries.push_back(summary);
    }

    return summaries;
}

/**
 * @brief Categorize a diff chunk based on content
 */
std::string ChangeDetector::categorize_chunk(const ChangeDetector::DiffChunk& chunk) const {
    // Combine all text from chunk
    std::string chunk_text;
    for (const auto& line : chunk.deleted_lines) {
        chunk_text += line + " ";
    }
    for (const auto& line : chunk.inserted_lines) {
        chunk_text += line + " ";
    }

    std::string lower = to_lowercase(chunk_text);

    // Category detection with priority
    if (lower.find("capital") != std::string::npos ||
        lower.find("tier 1") != std::string::npos ||
        lower.find("leverage") != std::string::npos ||
        lower.find("basel") != std::string::npos) {
        return "capital_requirements";
    }

    if (lower.find("report") != std::string::npos ||
        lower.find("disclosure") != std::string::npos ||
        lower.find("filing") != std::string::npos ||
        lower.find("submission") != std::string::npos) {
        return "reporting_requirements";
    }

    if (lower.find("risk") != std::string::npos ||
        lower.find("assessment") != std::string::npos ||
        lower.find("stress test") != std::string::npos) {
        return "risk_management";
    }

    if (lower.find("compliance") != std::string::npos ||
        lower.find("obligation") != std::string::npos ||
        lower.find("requirement") != std::string::npos) {
        return "compliance_obligations";
    }

    if (lower.find("deadline") != std::string::npos ||
        lower.find("effective date") != std::string::npos ||
        lower.find("timeline") != std::string::npos ||
        lower.find("phase") != std::string::npos) {
        return "timeline_changes";
    }

    if (lower.find("penalty") != std::string::npos ||
        lower.find("sanction") != std::string::npos ||
        lower.find("enforcement") != std::string::npos ||
        lower.find("violation") != std::string::npos) {
        return "enforcement";
    }

    if (lower.find("liquidity") != std::string::npos ||
        lower.find("funding") != std::string::npos) {
        return "liquidity_requirements";
    }

    return "general_regulatory";
}

/**
 * @brief Create human-readable title for category
 */
std::string ChangeDetector::create_category_title(
    const std::string& category,
    size_t change_count) const {

    // Convert category to readable format
    std::string title = category;
    std::replace(title.begin(), title.end(), '_', ' ');

    // Capitalize each word
    bool capitalize_next = true;
    for (char& c : title) {
        if (capitalize_next && std::isalpha(c)) {
            c = std::toupper(c);
            capitalize_next = false;
        } else if (c == ' ') {
            capitalize_next = true;
        }
    }

    // Add change count
    std::ostringstream oss;
    oss << title << " Update";
    if (change_count > 1) {
        oss << " (" << change_count << " changes)";
    }

    return oss.str();
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

std::string ChangeDetector::normalize_content(const std::string& content) const {
    std::string normalized = content;

    // Remove ignored patterns
    for (const auto& pattern : ignored_patterns_) {
        try {
            std::regex pattern_regex(pattern, std::regex::icase);
            normalized = std::regex_replace(normalized, pattern_regex, "");
        } catch (const std::regex_error& e) {
            logger_->debug("Invalid regex pattern", "ChangeDetector", "normalize_content", {
                {"pattern", pattern},
                {"error", e.what()}
            });
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
        logger_->info("Initializing DocumentParser with advanced patterns",
                     "DocumentParser", "initialize");

        // Initialize comprehensive regulatory body patterns
        regulatory_body_patterns_ = {
            {"SEC", {
                "Securities and Exchange Commission", "SEC", "U.S. Securities",
                "Securities Exchange Commission", "sec.gov"
            }},
            {"FCA", {
                "Financial Conduct Authority", "FCA", "UK Financial",
                "fca.org.uk", "Financial Services Authority"
            }},
            {"ECB", {
                "European Central Bank", "ECB", "Eurozone",
                "ecb.europa.eu", "Eurosystem"
            }},
            {"FINRA", {
                "Financial Industry Regulatory Authority", "FINRA",
                "finra.org"
            }},
            {"CFTC", {
                "Commodity Futures Trading Commission", "CFTC",
                "cftc.gov"
            }},
            {"OCC", {
                "Office of the Comptroller of the Currency", "OCC",
                "occ.gov", "Comptroller of the Currency"
            }},
            {"FDIC", {
                "Federal Deposit Insurance Corporation", "FDIC",
                "fdic.gov"
            }},
            {"FRB", {
                "Federal Reserve Board", "Federal Reserve", "FRB",
                "federalreserve.gov", "Board of Governors"
            }},
            {"EBA", {
                "European Banking Authority", "EBA",
                "eba.europa.eu"
            }},
            {"ESMA", {
                "European Securities and Markets Authority", "ESMA",
                "esma.europa.eu"
            }},
            {"BCBS", {
                "Basel Committee on Banking Supervision", "BCBS",
                "Bank for International Settlements", "bis.org"
            }},
            {"PRA", {
                "Prudential Regulation Authority", "PRA",
                "bankofengland.co.uk/pra"
            }}
        };

        // Initialize comprehensive document type patterns
        document_type_patterns_ = {
            {"rule", {
                "final rule", "proposed rule", "interim final rule",
                "regulation", "regulatory rule", "implementing rule"
            }},
            {"guidance", {
                "guidance", "guideline", "advisory", "bulletin",
                "supervisory guidance", "regulatory guidance"
            }},
            {"order", {
                "order", "enforcement action", "cease and desist",
                "administrative order", "consent order"
            }},
            {"release", {
                "release", "press release", "announcement",
                "regulatory release", "staff release"
            }},
            {"report", {
                "report", "study", "analysis", "white paper",
                "research report", "regulatory report"
            }},
            {"policy", {
                "policy statement", "policy", "framework",
                "regulatory framework", "policy position"
            }},
            {"directive", {
                "directive", "european directive", "eu directive",
                "commission directive"
            }},
            {"standard", {
                "regulatory standard", "technical standard",
                "implementing standard", "binding technical standard"
            }}
        };

        logger_->info("DocumentParser initialized successfully with comprehensive patterns",
                     "DocumentParser", "initialize", {
            {"regulatory_bodies", std::to_string(regulatory_body_patterns_.size())},
            {"document_types", std::to_string(document_type_patterns_.size())}
        });

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
            {"document_type", metadata.document_type},
            {"keywords_count", std::to_string(metadata.keywords.size())}
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
            return strip_html_tags(match[1].str());
        }

        // Try H1 tag
        std::regex h1_regex(R"(<h1[^>]*>(.*?)</h1>)", std::regex::icase);
        if (std::regex_search(content, match, h1_regex)) {
            return strip_html_tags(match[1].str());
        }
    }

    // Extract first non-empty line as title
    auto lines = split_into_lines(content);
    for (const auto& line : lines) {
        if (line.length() > 10 && line.length() < 200) {
            return line;
        }
    }

    return "";
}

std::optional<std::chrono::system_clock::time_point>
DocumentParser::extract_effective_date(const std::string& content) const {

    // Comprehensive date pattern matching
    std::vector<std::regex> date_patterns = {
        std::regex(R"(effective\s+date[:\s]+(\d{1,2}[/-]\d{1,2}[/-]\d{2,4}))",
                  std::regex::icase),
        std::regex(R"(effective\s+on[:\s]+(\d{1,2}[/-]\d{1,2}[/-]\d{2,4}))",
                  std::regex::icase),
        std::regex(R"(effective[:\s]+([A-Za-z]+\s+\d{1,2},?\s+\d{4}))",
                  std::regex::icase),
        std::regex(R"(shall\s+be\s+effective\s+(\d{1,2}[/-]\d{1,2}[/-]\d{2,4}))",
                  std::regex::icase),
        std::regex(R"((\d{4}-\d{2}-\d{2}))", std::regex::icase)
    };

    for (const auto& pattern : date_patterns) {
        std::smatch match;
        if (std::regex_search(content, match, pattern)) {
            // Found a date pattern - parse it properly
            std::string date_str = match[1].str();
            
            // Try multiple date format parsing strategies
            std::tm tm = {};
            std::vector<std::string> formats = {
                "%m/%d/%Y",    // MM/DD/YYYY
                "%m-%d-%Y",    // MM-DD-YYYY
                "%d/%m/%Y",    // DD/MM/YYYY
                "%d-%m-%Y",    // DD-MM-YYYY
                "%Y-%m-%d",    // YYYY-MM-DD (ISO format)
                "%m/%d/%y",    // MM/DD/YY
                "%B %d, %Y",   // Month DD, YYYY
                "%B %d %Y"     // Month DD YYYY
            };
            
            for (const auto& fmt : formats) {
                std::istringstream ss(date_str);
                ss >> std::get_time(&tm, fmt.c_str());
                
                if (!ss.fail()) {
                    // Successfully parsed
                    auto time_point = std::chrono::system_clock::from_time_t(std::mktime(&tm));
                    logger_->debug("Successfully parsed effective date", "DocumentParser", "extract_effective_date", {
                        {"date_string", date_str},
                        {"format", fmt}
                    });
                    return time_point;
                }
                
                // Reset for next attempt
                tm = {};
            }
            
            // If we couldn't parse with any format, log warning and return nullopt
            logger_->warn("Found date pattern but failed to parse", "DocumentParser", "extract_effective_date", {
                {"date_string", date_str}
            });
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
        {"parsing_errors", parsing_errors_.load()},
        {"error_rate", documents_parsed_.load() > 0 ?
            static_cast<double>(parsing_errors_.load()) / documents_parsed_.load() : 0.0}
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

    // Extract affected entities from HTML meta tags and content
    metadata.affected_entities = extract_affected_entities(html);

    return metadata;
}

RegulatoryChangeMetadata DocumentParser::parse_xml(const std::string& xml) {
    RegulatoryChangeMetadata metadata;

    // Extract from common XML/RSS fields
    metadata.regulatory_body = extract_xml_field(xml, "source|publisher|author|dc:creator");
    metadata.document_type = extract_xml_field(xml, "type|category|dc:type");
    metadata.document_number = extract_xml_field(xml, "id|guid|identifier|dc:identifier");

    // Extract text content
    std::string text_content = strip_xml_tags(xml);
    metadata.keywords = extract_keywords_from_text(text_content);

    // Extract affected entities
    metadata.affected_entities = extract_affected_entities(xml);

    return metadata;
}

RegulatoryChangeMetadata DocumentParser::parse_text(const std::string& text) {
    RegulatoryChangeMetadata metadata;

    metadata.regulatory_body = extract_regulatory_body(text);
    metadata.document_type = extract_document_type(text);
    metadata.document_number = extract_document_number(text);
    metadata.keywords = extract_keywords_from_text(text);
    metadata.affected_entities = extract_affected_entities(text);

    return metadata;
}

std::string DocumentParser::extract_regulatory_body(const std::string& content) const {
    std::string lower_content = to_lowercase(content);

    // Score each regulatory body based on pattern matches
    std::unordered_map<std::string, int> scores;

    for (const auto& [body, patterns] : regulatory_body_patterns_) {
        for (const auto& pattern : patterns) {
            std::string lower_pattern = to_lowercase(pattern);
            size_t pos = 0;
            while ((pos = lower_content.find(lower_pattern, pos)) != std::string::npos) {
                scores[body]++;
                pos += lower_pattern.length();
            }
        }
    }

    // Return body with highest score
    std::string best_body = "Unknown";
    int best_score = 0;

    for (const auto& [body, score] : scores) {
        if (score > best_score) {
            best_score = score;
            best_body = body;
        }
    }

    return best_body;
}

std::string DocumentParser::extract_document_type(const std::string& content) const {
    std::string lower_content = to_lowercase(content);

    // Score each document type
    std::unordered_map<std::string, int> scores;

    for (const auto& [type, patterns] : document_type_patterns_) {
        for (const auto& pattern : patterns) {
            if (lower_content.find(pattern) != std::string::npos) {
                scores[type]++;
            }
        }
    }

    // Return type with highest score
    std::string best_type = "general";
    int best_score = 0;

    for (const auto& [type, score] : scores) {
        if (score > best_score) {
            best_score = score;
            best_type = type;
        }
    }

    return best_type;
}

std::string DocumentParser::extract_document_number(const std::string& content) const {
    // Comprehensive document number patterns
    std::vector<std::regex> number_patterns = {
        std::regex(R"(Release\s+No\.?\s*([A-Z0-9-]+))", std::regex::icase),
        std::regex(R"(File\s+No\.?\s*([A-Z0-9-]+))", std::regex::icase),
        std::regex(R"(Document\s+No\.?\s*([A-Z0-9-]+))", std::regex::icase),
        std::regex(R"(Ref(?:erence)?\.?\s*([A-Z0-9-]+))", std::regex::icase),
        std::regex(R"(Docket\s+No\.?\s*([A-Z0-9-]+))", std::regex::icase),
        std::regex(R"(Case\s+No\.?\s*([A-Z0-9-]+))", std::regex::icase),
        std::regex(R"(RIN\s+([A-Z0-9-]+))", std::regex::icase),
        std::regex(R"(FR\s+Doc\.?\s*([A-Z0-9-]+))", std::regex::icase)
    };

    for (const auto& pattern : number_patterns) {
        std::smatch match;
        if (std::regex_search(content, match, pattern)) {
            return match[1].str();
        }
    }

    return "";
}

std::vector<std::string> DocumentParser::extract_affected_entities(
    const std::string& content) const {

    std::vector<std::string> entities;
    std::unordered_set<std::string> unique_entities;

    // Entity type patterns
    std::vector<std::pair<std::regex, std::string>> entity_patterns = {
        {std::regex(R"(\b(?:all\s+)?(?:banks|banking\s+institutions))", std::regex::icase),
         "banks"},
        {std::regex(R"(\b(?:broker-dealers?|brokers?))", std::regex::icase),
         "broker-dealers"},
        {std::regex(R"(\b(?:investment\s+advisers?|investment\s+advisors?))", std::regex::icase),
         "investment_advisers"},
        {std::regex(R"(\b(?:insurance\s+companies|insurers?))", std::regex::icase),
         "insurance_companies"},
        {std::regex(R"(\b(?:credit\s+unions?))", std::regex::icase),
         "credit_unions"},
        {std::regex(R"(\b(?:depository\s+institutions?))", std::regex::icase),
         "depository_institutions"},
        {std::regex(R"(\b(?:systemically\s+important|SIFIs?))", std::regex::icase),
         "systemically_important_institutions"},
        {std::regex(R"(\b(?:public\s+companies|issuers?))", std::regex::icase),
         "public_companies"}
    };

    for (const auto& [pattern, entity_type] : entity_patterns) {
        if (std::regex_search(content, pattern)) {
            if (unique_entities.insert(entity_type).second) {
                entities.push_back(entity_type);
            }
        }
    }

    return entities;
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
    std::string result = html;

    // Remove script and style tags with content
    result = std::regex_replace(result,
        std::regex("<script[^>]*>.*?</script>", std::regex::icase), " ");
    result = std::regex_replace(result,
        std::regex("<style[^>]*>.*?</style>", std::regex::icase), " ");

    // Remove all other tags
    result = std::regex_replace(result, std::regex("<[^>]*>"), " ");

    // Decode common HTML entities
    result = std::regex_replace(result, std::regex("&nbsp;"), " ");
    result = std::regex_replace(result, std::regex("&amp;"), "&");
    result = std::regex_replace(result, std::regex("&lt;"), "<");
    result = std::regex_replace(result, std::regex("&gt;"), ">");
    result = std::regex_replace(result, std::regex("&quot;"), "\"");

    // Normalize whitespace
    result = std::regex_replace(result, std::regex(R"(\s+)"), " ");

    return result;
}

std::string DocumentParser::strip_xml_tags(const std::string& xml) const {
    std::string result = xml;

    // Remove CDATA sections
    result = std::regex_replace(result,
        std::regex(R"(<!\[CDATA\[.*?\]\]>)"), " ");

    // Remove comments
    result = std::regex_replace(result,
        std::regex("<!--.*?-->"), " ");

    // Remove all tags
    result = std::regex_replace(result, std::regex("<[^>]*>"), " ");

    // Normalize whitespace
    result = std::regex_replace(result, std::regex(R"(\s+)"), " ");

    return result;
}

std::string DocumentParser::extract_xml_field(const std::string& xml,
                                              const std::string& field_pattern) const {
    std::regex field_regex("<(?:" + field_pattern + ")[^>]*>([^<]*)</(?:" + field_pattern + ")>",
                          std::regex::icase);
    std::smatch match;

    if (std::regex_search(xml, match, field_regex)) {
        return match[1].str();
    }

    // Try with CDATA
    std::regex cdata_regex("<(?:" + field_pattern + ")[^>]*><!\\[CDATA\\[([^\\]]*)\\]\\]></(?:" +
                          field_pattern + ")>", std::regex::icase);

    if (std::regex_search(xml, match, cdata_regex)) {
        return match[1].str();
    }

    return "";
}

std::vector<std::string> DocumentParser::extract_keywords_from_text(
    const std::string& text) const {

    std::vector<std::string> keywords;
    std::unordered_set<std::string> unique_keywords;

    // Comprehensive regulatory keywords
    static const std::vector<std::string> regulatory_terms = {
        // Core regulatory
        "regulation", "compliance", "requirement", "obligation", "prohibition",
        "mandate", "directive", "guideline", "standard", "policy", "procedure",

        // Enforcement
        "enforcement", "penalty", "sanction", "violation", "breach",
        "fine", "censure", "suspension",

        // Capital and liquidity
        "capital", "liquidity", "leverage", "tier 1", "tier 2",
        "buffer", "basel", "stress test", "adequacy",

        // Risk
        "risk", "credit risk", "market risk", "operational risk",
        "systemic risk", "counterparty risk",

        // Reporting
        "reporting", "disclosure", "filing", "submission",
        "audit", "examination", "review",

        // Governance
        "governance", "oversight", "supervision", "monitoring",
        "internal control", "risk management"
    };

    std::string lower_text = to_lowercase(text);

    for (const auto& term : regulatory_terms) {
        if (lower_text.find(term) != std::string::npos) {
            if (unique_keywords.insert(term).second) {
                keywords.push_back(term);
            }
        }
    }

    return keywords;
}

} // namespace regulens
