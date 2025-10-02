/**
 * Web Search Tool Implementation
 *
 * Production-grade web search integration supporting multiple search engines
 * with safety filters, caching, and rate limiting.
 */

#include "web_search_tool.hpp"
#include "../tool_interface.hpp"
#include <curl/curl.h>
#include <sstream>
#include <regex>
#include <algorithm>

namespace regulens {

// Web Search Tool Implementation
WebSearchTool::WebSearchTool(const ToolConfig& config, StructuredLogger* logger)
    : Tool(config, logger) {

    // Parse search configuration from tool config
    if (config.metadata.contains("search_engine")) {
        search_config_.search_engine = config.metadata["search_engine"];
    }
    if (config.metadata.contains("api_key")) {
        search_config_.api_key = config.metadata["api_key"];
    }
    if (config.metadata.contains("cse_id")) {
        search_config_.cse_id = config.metadata["cse_id"];
    }
    if (config.metadata.contains("safe_search")) {
        search_config_.safe_search = config.metadata["safe_search"];
    }
    if (config.metadata.contains("max_results")) {
        search_config_.max_results = config.metadata["max_results"];
    }
    if (config.metadata.contains("allowed_domains")) {
        search_config_.allowed_domains = config.metadata["allowed_domains"];
    }
    if (config.metadata.contains("blocked_domains")) {
        search_config_.blocked_domains = config.metadata["blocked_domains"];
    }
    if (config.metadata.contains("result_cache_ttl_seconds")) {
        search_config_.result_cache_ttl_seconds = config.metadata["result_cache_ttl_seconds"];
    }
}

ToolResult WebSearchTool::execute_operation(const std::string& operation,
                                          const nlohmann::json& parameters) {
    if (!is_authenticated()) {
        return ToolResult::create_error("Web search tool not authenticated");
    }

    try {
        if (operation == "search") {
            if (!parameters.contains("query")) {
                return ToolResult::create_error("Missing query parameter");
            }
            return perform_web_search(parameters["query"], parameters);
        } else if (operation == "cached_search") {
            if (!parameters.contains("query")) {
                return ToolResult::create_error("Missing query parameter");
            }
            return get_cached_results(parameters["query"]);
        } else {
            return ToolResult::create_error("Unknown web search operation: " + operation);
        }
    } catch (const std::exception& e) {
        record_operation_result("execute_operation", false, std::chrono::milliseconds(0));
        return ToolResult::create_error("Web search operation failed: " + std::string(e.what()));
    }
}

bool WebSearchTool::authenticate() {
    // Web search tools typically use API keys, validate configuration
    if (search_config_.search_engine == "google" && search_config_.api_key.empty()) {
        logger_->log(LogLevel::ERROR, "Google search requires API key");
        return false;
    }
    if (search_config_.search_engine == "bing" && search_config_.api_key.empty()) {
        logger_->log(LogLevel::ERROR, "Bing search requires API key");
        return false;
    }

    authenticated_ = true;
    logger_->log(LogLevel::INFO, "Web search tool authenticated for engine: " + search_config_.search_engine);
    return true;
}

bool WebSearchTool::disconnect() {
    authenticated_ = false;
    result_cache_.clear();
    logger_->log(LogLevel::INFO, "Web search tool disconnected");
    return true;
}

ToolResult WebSearchTool::perform_web_search(const std::string& query, const nlohmann::json& options) {
    auto start_time = std::chrono::steady_clock::now();

    try {
        // Check cache first
        auto cached_result = get_cached_results(query);
        if (cached_result.success) {
            logger_->log(LogLevel::INFO, "Returning cached search results for: " + query);
            return cached_result;
        }

        // Perform fresh search based on configured engine
        std::vector<SearchResult> results;
        int max_results = options.value("max_results", search_config_.max_results);

        if (search_config_.search_engine == "google") {
            results = search_google(query, max_results);
        } else if (search_config_.search_engine == "bing") {
            results = search_bing(query, max_results);
        } else if (search_config_.search_engine == "duckduckgo") {
            results = search_duckduckgo(query, max_results);
        } else {
            return ToolResult::create_error("Unsupported search engine: " + search_config_.search_engine);
        }

        // Filter and rank results
        nlohmann::json filters = options.value("filters", nlohmann::json{});
        results = filter_and_rank_results(results, filters);

        // Cache results
        cache_results(query, results);

        // Convert to JSON response
        nlohmann::json response = search_results_to_json(results);
        response["query"] = query;
        response["engine"] = search_config_.search_engine;
        response["total_results"] = results.size();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time);
        record_operation_result("perform_web_search", true, duration);

        return ToolResult::create_success(response);

    } catch (const std::exception& e) {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time);
        record_operation_result("perform_web_search", false, duration);
        return ToolResult::create_error("Web search failed: " + std::string(e.what()));
    }
}

ToolResult WebSearchTool::get_cached_results(const std::string& query) {
    auto it = result_cache_.find(query);
    if (it == result_cache_.end()) {
        return ToolResult::create_error("No cached results found");
    }

    // Check if cache is still valid
    auto now = std::chrono::system_clock::now();
    auto cache_age = now - it->second[0].cached_at;
    auto max_age = std::chrono::seconds(search_config_.result_cache_ttl_seconds);

    if (cache_age > max_age) {
        result_cache_.erase(it);
        return ToolResult::create_error("Cached results expired");
    }

    nlohmann::json response = search_results_to_json(it->second);
    response["cached"] = true;
    response["cache_age_seconds"] = std::chrono::duration_cast<std::chrono::seconds>(cache_age).count();

    record_operation_result("get_cached_results", true, std::chrono::milliseconds(1));
    return ToolResult::create_success(response);
}

void WebSearchTool::cache_results(const std::string& query, const std::vector<SearchResult>& results) {
    if (results.empty()) return;

    result_cache_[query] = results;

    // Clean up old cache entries if cache gets too large
    if (result_cache_.size() > 1000) {
        auto now = std::chrono::system_clock::now();
        auto max_age = std::chrono::seconds(search_config_.result_cache_ttl_seconds);

        for (auto it = result_cache_.begin(); it != result_cache_.end(); ) {
            auto cache_age = now - it->second[0].cached_at;
            if (cache_age > max_age) {
                it = result_cache_.erase(it);
            } else {
                ++it;
            }
        }
    }
}

// Search Engine Implementations

std::vector<SearchResult> WebSearchTool::search_google(const std::string& query, int max_results) {
    std::vector<SearchResult> results;

    try {
        std::string url = "https://www.googleapis.com/customsearch/v1";
        url += "?key=" + search_config_.api_key;
        url += "&cx=" + search_config_.cse_id;
        url += "&q=" + url_encode(query);
        url += "&num=" + std::to_string(std::min(max_results, 10));
        if (search_config_.safe_search) {
            url += "&safe=active";
        }

        // Perform HTTP request (simplified - would use HttpClient in production)
        CURL* curl = curl_easy_init();
        if (!curl) return results;

        std::string response_body;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res == CURLE_OK) {
            nlohmann::json response = nlohmann::json::parse(response_body);

            if (response.contains("items")) {
                for (const auto& item : response["items"]) {
                    SearchResult result;
                    result.title = item.value("title", "");
                    result.url = item.value("link", "");
                    result.snippet = item.value("snippet", "");
                    result.domain = extract_domain(result.url);
                    result.relevance_score = 0.8; // Google provides good relevance
                    result.cached_at = std::chrono::system_clock::now();

                    if (is_domain_allowed(result.domain)) {
                        results.push_back(result);
                    }
                }
            }
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Google search failed: " + std::string(e.what()));
    }

    return results;
}

std::vector<SearchResult> WebSearchTool::search_bing(const std::string& query, int max_results) {
    std::vector<SearchResult> results;

    try {
        std::string url = "https://api.bing.microsoft.com/v7.0/search";
        url += "?q=" + url_encode(query);
        url += "&count=" + std::to_string(std::min(max_results, 50));
        if (search_config_.safe_search) {
            url += "&safeSearch=Strict";
        }

        // Perform HTTP request with Bing API key
        CURL* curl = curl_easy_init();
        if (!curl) return results;

        std::string response_body;
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, ("Ocp-Apim-Subscription-Key: " + search_config_.api_key).c_str());

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);

        if (res == CURLE_OK) {
            nlohmann::json response = nlohmann::json::parse(response_body);

            if (response.contains("webPages") && response["webPages"].contains("value")) {
                for (const auto& item : response["webPages"]["value"]) {
                    SearchResult result;
                    result.title = item.value("name", "");
                    result.url = item.value("url", "");
                    result.snippet = item.value("snippet", "");
                    result.domain = extract_domain(result.url);
                    result.relevance_score = 0.7; // Bing provides good relevance
                    result.cached_at = std::chrono::system_clock::now();

                    if (is_domain_allowed(result.domain)) {
                        results.push_back(result);
                    }
                }
            }
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Bing search failed: " + std::string(e.what()));
    }

    return results;
}

std::vector<SearchResult> WebSearchTool::search_duckduckgo(const std::string& query, int max_results) {
    std::vector<SearchResult> results;

    try {
        std::string url = "https://api.duckduckgo.com/";
        url += "?q=" + url_encode(query);
        url += "&format=json";
        url += "&no_html=1";
        url += "&skip_disambig=1";

        // DuckDuckGo Instant Answer API
        CURL* curl = curl_easy_init();
        if (!curl) return results;

        std::string response_body;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res == CURLE_OK) {
            nlohmann::json response = nlohmann::json::parse(response_body);

            // DuckDuckGo provides instant answer and related topics
            if (response.contains("RelatedTopics")) {
                int count = 0;
                for (const auto& topic : response["RelatedTopics"]) {
                    if (count >= max_results) break;

                    if (topic.contains("Text") && topic.contains("FirstURL")) {
                        SearchResult result;
                        result.title = topic.value("Text", "");
                        result.url = topic.value("FirstURL", "");
                        result.snippet = topic.value("Text", "");
                        result.domain = extract_domain(result.url);
                        result.relevance_score = 0.6; // DuckDuckGo provides decent relevance
                        result.cached_at = std::chrono::system_clock::now();

                        if (is_domain_allowed(result.domain)) {
                            results.push_back(result);
                            count++;
                        }
                    }
                }
            }
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "DuckDuckGo search failed: " + std::string(e.what()));
    }

    return results;
}

// Result Processing Methods

std::vector<SearchResult> WebSearchTool::filter_and_rank_results(
    std::vector<SearchResult>& results,
    const nlohmann::json& filters
) {
    // Remove blocked domains
    results.erase(
        std::remove_if(results.begin(), results.end(),
            [this](const SearchResult& result) {
                return !is_domain_allowed(result.domain);
            }),
        results.end()
    );

    // Apply additional filters
    if (filters.contains("min_relevance")) {
        double min_score = filters["min_relevance"];
        results.erase(
            std::remove_if(results.begin(), results.end(),
                [min_score](const SearchResult& result) {
                    return result.relevance_score < min_score;
                }),
            results.end()
        );
    }

    // Sort by relevance score (highest first)
    std::sort(results.begin(), results.end(),
        [](const SearchResult& a, const SearchResult& b) {
            return a.relevance_score > b.relevance_score;
        });

    // Limit results
    if (results.size() > search_config_.max_results) {
        results.resize(search_config_.max_results);
    }

    return results;
}

bool WebSearchTool::is_domain_allowed(const std::string& domain) const {
    // Check blocked domains first
    for (const auto& blocked : search_config_.blocked_domains) {
        if (domain.find(blocked) != std::string::npos) {
            return false;
        }
    }

    // If allowed domains specified, must be in the list
    if (!search_config_.allowed_domains.empty()) {
        for (const auto& allowed : search_config_.allowed_domains) {
            if (domain.find(allowed) != std::string::npos) {
                return true;
            }
        }
        return false; // Not in allowed list
    }

    return true; // No restrictions
}

double WebSearchTool::calculate_relevance_score(const SearchResult& result, const std::string& query) {
    // Simple relevance scoring based on title and snippet matching
    double score = 0.0;

    std::string lower_query = query;
    std::string lower_title = result.title;
    std::string lower_snippet = result.snippet;

    std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);
    std::transform(lower_title.begin(), lower_title.end(), lower_title.begin(), ::tolower);
    std::transform(lower_snippet.begin(), lower_snippet.end(), lower_snippet.begin(), ::tolower);

    // Title matches are most important
    if (lower_title.find(lower_query) != std::string::npos) {
        score += 0.4;
    }

    // Snippet matches are also important
    if (lower_snippet.find(lower_query) != std::string::npos) {
        score += 0.3;
    }

    // Domain authority (simplified)
    if (result.domain.find(".edu") != std::string::npos ||
        result.domain.find(".gov") != std::string::npos) {
        score += 0.2;
    } else if (result.domain.find(".org") != std::string::npos) {
        score += 0.1;
    }

    return std::min(score, 1.0);
}

// Utility Methods

std::string WebSearchTool::url_encode(const std::string& str) const {
    CURL* curl = curl_easy_init();
    if (!curl) return str;

    char* encoded = curl_easy_escape(curl, str.c_str(), str.length());
    std::string result = encoded ? encoded : str;
    curl_free(encoded);
    curl_easy_cleanup(curl);
    return result;
}

std::string WebSearchTool::extract_domain(const std::string& url) const {
    std::regex domain_regex(R"(https?://(?:www\.)?([^/]+))");
    std::smatch match;
    if (std::regex_search(url, match, domain_regex) && match.size() > 1) {
        return match[1].str();
    }
    return url; // Fallback
}

nlohmann::json WebSearchTool::search_results_to_json(const std::vector<SearchResult>& results) const {
    nlohmann::json json_results = nlohmann::json::array();

    for (const auto& result : results) {
        json_results.push_back({
            {"title", result.title},
            {"url", result.url},
            {"snippet", result.snippet},
            {"domain", result.domain},
            {"relevance_score", result.relevance_score}
        });
    }

    return json_results;
}

size_t WebSearchTool::write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// Factory function
std::unique_ptr<Tool> create_web_search_tool(const ToolConfig& config, StructuredLogger* logger) {
    return std::make_unique<WebSearchTool>(config, logger);
}

} // namespace regulens
