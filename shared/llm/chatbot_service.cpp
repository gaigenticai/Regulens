/**
 * GPT-4 Chatbot Service Implementation
 * Production-grade conversational AI with RAG integration
 */

#include "chatbot_service.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <openssl/sha.h>
#include <uuid/uuid.h>
#include <spdlog/spdlog.h>

namespace regulens {

ChatbotService::ChatbotService(
    std::shared_ptr<PostgreSQLConnection> db_conn,
    std::shared_ptr<VectorKnowledgeBase> knowledge_base,
    std::shared_ptr<OpenAIClient> openai_client
) : db_conn_(db_conn), knowledge_base_(knowledge_base), openai_client_(openai_client) {

    if (!db_conn_) {
        throw std::runtime_error("Database connection is required for ChatbotService");
    }
    if (!knowledge_base_) {
        throw std::runtime_error("Knowledge base is required for ChatbotService");
    }
    if (!openai_client_) {
        throw std::runtime_error("OpenAI client is required for ChatbotService");
    }

    spdlog::info("ChatbotService initialized with RAG enabled");
}

ChatbotService::~ChatbotService() {
    spdlog::info("ChatbotService shutting down");
}

ChatbotResponse ChatbotService::process_message(const ChatbotRequest& request) {
    auto start_time = std::chrono::high_resolution_clock::now();

    try {
        // Validate request
        if (request.user_message.empty()) {
            return create_fallback_response("Empty message received");
        }

        if (request.user_id.empty()) {
            return create_fallback_response("User ID is required");
        }

        // Check rate limits
        if (!check_rate_limits(request.user_id, usage_limits_)) {
            spdlog::warn("Rate limit exceeded for user {}", request.user_id);
            return create_fallback_response("Rate limit exceeded. Please try again later.");
        }

        // Create or get conversation
        std::string conversation_id = create_or_get_conversation(request);
        if (conversation_id.empty()) {
            return create_fallback_response("Failed to create conversation");
        }

        // Get conversation history for context
        std::vector<ChatbotMessage> conversation_history = get_conversation_history(conversation_id, max_context_messages_);

        // Retrieve relevant knowledge context (RAG)
        KnowledgeContext knowledge_context;
        if (rag_enabled_) {
            knowledge_context = retrieve_relevant_context(request.user_message);
        }

        // Generate GPT-4 response
        ChatbotResponse response = generate_gpt4_response(conversation_history, knowledge_context, request);
        response.conversation_id = conversation_id;

        // Calculate processing time
        auto end_time = std::chrono::high_resolution_clock::now();
        response.processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        // Store the conversation messages
        store_message(conversation_id, "user", request.user_message, response.tokens_used / 2, 0.0, default_model_); // Estimate input tokens
        store_message(conversation_id, "assistant", response.response_text, response.tokens_used / 2, response.cost,
                     default_model_, response.confidence_score, response.sources_used, response.processing_time);

        // Update conversation statistics
        update_conversation_stats(conversation_id, response.tokens_used, response.cost);

        // Record usage for rate limiting
        record_usage(request.user_id, response.tokens_used, response.cost);

        spdlog::info("Chatbot response generated for user {} in conversation {} ({} tokens, ${:.6f})",
                    request.user_id, conversation_id, response.tokens_used, response.cost);

        return response;

    } catch (const std::exception& e) {
        spdlog::error("Exception in process_message: {}", e.what());

        auto end_time = std::chrono::high_resolution_clock::now();
        auto processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        ChatbotResponse error_response = create_fallback_response("An error occurred while processing your message");
        error_response.processing_time = processing_time;
        return error_response;
    }
}

KnowledgeContext ChatbotService::retrieve_relevant_context(const std::string& query, int max_results) {
    KnowledgeContext context;

    try {
        // Generate query hash for caching
        std::string query_hash = hash_string(query);

        // Check cache first
        auto cached_context = get_cached_context(query_hash);
        if (cached_context) {
            spdlog::debug("Using cached knowledge context for query hash: {}", query_hash);
            return *cached_context;
        }

    // Create semantic query
    SemanticQuery semantic_query;
    semantic_query.query_text = query;
    semantic_query.max_results = max_results;
    semantic_query.similarity_threshold = 0.7f;
    semantic_query.domain_filter = KnowledgeDomain::REGULATORY_COMPLIANCE; // Focus on regulatory knowledge

    // Search knowledge base
    auto search_results = knowledge_base_->semantic_search(semantic_query);

        // Build context from results
        std::stringstream context_stream;
        context_stream << "Relevant regulatory information:\n\n";

        for (size_t i = 0; i < search_results.size(); ++i) {
            const auto& result = search_results[i];

            // Extract relevant fields from knowledge base entry
            nlohmann::json doc_entry = {
                {"title", result.entity.title},
                {"content", result.entity.content},
                {"relevance_score", result.similarity_score},
                {"doc_id", result.entity.entity_id},
                {"domain", static_cast<int>(result.entity.domain)},
                {"knowledge_type", static_cast<int>(result.entity.knowledge_type)}
            };

            context.relevant_documents.push_back(doc_entry);
            context.relevance_scores.push_back(result.similarity_score);

            // Add to context summary
            context_stream << "[" << (i + 1) << "] " << result.entity.title << ":\n";
            std::string content = result.entity.content.substr(0, 500);
            if (result.entity.content.length() > 500) {
                content += "...";
            }
            context_stream << content << "\n\n";
        }

        context.context_summary = context_stream.str();
        context.total_sources = search_results.size();

        // Cache the context
        cache_context(query_hash, query, context);

        spdlog::info("Retrieved {} knowledge sources for query: {}", context.total_sources, query.substr(0, 50));

    } catch (const std::exception& e) {
        spdlog::error("Exception in retrieve_relevant_context: {}", e.what());
        context.context_summary = "Error retrieving knowledge context.";
    }

    return context;
}

std::string ChatbotService::create_or_get_conversation(const ChatbotRequest& request) {
    try {
        if (request.conversation_id != "new" && !request.conversation_id.empty()) {
            // Validate existing conversation
            if (validate_conversation_access(request.conversation_id, request.user_id)) {
                return request.conversation_id;
            }
        }

        // Create new conversation
        std::string conversation_id = generate_uuid();
        std::string title = generate_conversation_title(request.user_message);

        std::string query = R"(
            INSERT INTO chatbot_conversations
            (conversation_id, user_id, platform, title, metadata)
            VALUES ($1, $2, $3, $4, $5)
        )";

        nlohmann::json metadata = {
            {"model", default_model_},
            {"rag_enabled", rag_enabled_},
            {"max_context_messages", max_context_messages_},
            {"created_via", "api"}
        };

        std::vector<std::string> params = {
            conversation_id,
            request.user_id,
            request.platform,
            title,
            metadata.dump()
        };

        bool success = db_conn_->execute_command(query, params);
        if (success) {
            spdlog::info("Created new conversation {} for user {}", conversation_id, request.user_id);
            return conversation_id;
        }

        spdlog::error("Failed to create conversation for user {}", request.user_id);
        return "";

    } catch (const std::exception& e) {
        spdlog::error("Exception in create_or_get_conversation: {}", e.what());
        return "";
    }
}

std::vector<ChatbotMessage> ChatbotService::get_conversation_history(const std::string& conversation_id, int limit) {
    std::vector<ChatbotMessage> messages;

    try {
        std::string query = R"(
            SELECT role, content, token_count, sources_used, confidence_score
            FROM chatbot_messages
            WHERE conversation_id = $1
            ORDER BY created_at DESC
            LIMIT $2
        )";

        std::vector<std::string> params = {conversation_id, std::to_string(limit)};
        auto result = db_conn_->execute_query(query, params);

        // Reverse to get chronological order (oldest first for GPT context)
        for (auto it = result.rows.rbegin(); it != result.rows.rend(); ++it) {
            const auto& row = *it;

            ChatbotMessage msg;
            msg.role = row.at("role");
            msg.content = row.at("content");

            auto token_count_it = row.find("token_count");
            if (token_count_it != row.end()) {
                msg.token_count = std::stoi(token_count_it->second);
            }

            auto confidence_it = row.find("confidence_score");
            if (confidence_it != row.end()) {
                msg.confidence_score = std::stod(confidence_it->second);
            }

            auto sources_it = row.find("sources_used");
            if (sources_it != row.end() && !sources_it->second.empty()) {
                msg.sources_used = nlohmann::json::parse(sources_it->second);
            }

            messages.push_back(msg);
        }

    } catch (const std::exception& e) {
        spdlog::error("Exception in get_conversation_history: {}", e.what());
    }

    return messages;
}

std::string ChatbotService::build_system_prompt(const KnowledgeContext& context) {
    std::stringstream prompt;

    prompt << "You are a regulatory compliance expert AI assistant. You provide accurate, helpful information about regulatory compliance, laws, and industry standards.\n\n";

    if (!context.context_summary.empty()) {
        prompt << "Use the following regulatory information to inform your responses:\n";
        prompt << context.context_summary;
        prompt << "\n";
    }

    prompt << "Guidelines:\n";
    prompt << "- Be accurate and cite sources when possible\n";
    prompt << "- Explain complex regulatory concepts clearly\n";
    prompt << "- Suggest compliance actions when appropriate\n";
    prompt << "- Admit when you don't have complete information\n";
    prompt << "- Always prioritize user safety and regulatory compliance\n";
    prompt << "- Be professional and helpful\n";

    return prompt.str();
}

ChatbotResponse ChatbotService::generate_gpt4_response(
    const std::vector<ChatbotMessage>& conversation_history,
    const KnowledgeContext& context,
    const ChatbotRequest& request
) {
    ChatbotResponse response;

    try {
        // Build messages for GPT-4 API
    std::vector<OpenAIMessage> messages;

    // Add system prompt
    messages.emplace_back("system", build_system_prompt(context));

    // Add conversation history (limited by context window)
    for (const auto& msg : conversation_history) {
        messages.emplace_back(msg.role, msg.content);
    }

    // Add current user message
    messages.emplace_back("user", request.user_message);

    // Prepare GPT-4 request
    std::string model = request.model_override.value_or(default_model_);

    // Estimate input tokens (rough calculation)
    int estimated_input_tokens = 0;
    for (const auto& msg : messages) {
        auto [input_tokens, _] = estimate_token_count(msg.content);
        estimated_input_tokens += input_tokens;
    }

    // Make GPT-4 API call
    OpenAICompletionRequest gpt_request;
    gpt_request.model = model;
    gpt_request.messages = messages;
    gpt_request.temperature = 0.7;
    gpt_request.max_tokens = 1000;
    gpt_request.presence_penalty = 0.1;
    gpt_request.frequency_penalty = 0.1;

    auto gpt_response = openai_client_->create_chat_completion(gpt_request);

        if (!gpt_response) {
            spdlog::error("GPT-4 API call failed");
            return create_fallback_response("AI service temporarily unavailable");
        }

        // Extract response
        if (gpt_response->choices.empty()) {
            spdlog::error("GPT-4 API returned no choices");
            return create_fallback_response("AI service returned empty response");
        }

        response.response_text = gpt_response->choices[0].message.content;
        response.success = true;
        response.confidence_score = 0.9; // Could be improved with actual confidence scoring

        // Use actual token usage from API response
        response.tokens_used = gpt_response->usage.total_tokens;

        // Calculate cost using actual usage
        int input_tokens = gpt_response->usage.prompt_tokens;
        int output_tokens = gpt_response->usage.completion_tokens;
        response.cost = calculate_message_cost(model, input_tokens, output_tokens);

        // Include sources if available
        if (context.total_sources > 0) {
            nlohmann::json sources = nlohmann::json::array();
            for (size_t i = 0; i < context.relevant_documents.size(); ++i) {
                sources.push_back({
                    {"title", context.relevant_documents[i]["title"]},
                    {"relevance_score", context.relevance_scores[i]},
                    {"doc_id", context.relevant_documents[i]["doc_id"]}
                });
            }
            response.sources_used = sources;
        }

    } catch (const std::exception& e) {
        spdlog::error("Exception in generate_gpt4_response: {}", e.what());
        return create_fallback_response("Error generating AI response");
    }

    return response;
}

void ChatbotService::store_message(
    const std::string& conversation_id,
    const std::string& role,
    const std::string& content,
    int token_count,
    double cost,
    const std::string& model_used,
    double confidence_score,
    const std::optional<nlohmann::json>& sources_used,
    std::chrono::milliseconds processing_time
) {
    try {
        std::string query = R"(
            INSERT INTO chatbot_messages
            (conversation_id, role, content, token_count, model_used, message_cost, confidence_score, sources_used, processing_time_ms)
            VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9)
        )";

        std::vector<std::string> params = {
            conversation_id,
            role,
            content,
            std::to_string(token_count),
            model_used,
            std::to_string(cost),
            std::to_string(confidence_score),
            sources_used ? sources_used->dump() : "",
            std::to_string(processing_time.count())
        };

        db_conn_->execute_command(query, params);

    } catch (const std::exception& e) {
        spdlog::error("Exception in store_message: {}", e.what());
    }
}

void ChatbotService::update_conversation_stats(const std::string& conversation_id, int tokens_used, double cost) {
    try {
        std::string query = R"(
            UPDATE chatbot_conversations
            SET message_count = message_count + 1,
                token_count = token_count + $2,
                total_cost = total_cost + $3,
                last_message_at = NOW(),
                updated_at = NOW()
            WHERE conversation_id = $1
        )";

        std::vector<std::string> params = {
            conversation_id,
            std::to_string(tokens_used),
            std::to_string(cost)
        };

        db_conn_->execute_command(query, params);

    } catch (const std::exception& e) {
        spdlog::error("Exception in update_conversation_stats: {}", e.what());
    }
}

bool ChatbotService::check_rate_limits(const std::string& user_id, const UsageLimits& limits) {
    try {
        // Check hourly request limit
        std::string request_query = R"(
            SELECT COALESCE(SUM(request_count), 0) as total_requests
            FROM chatbot_usage_stats
            WHERE user_id = $1
            AND time_window_start >= NOW() - INTERVAL '1 hour'
        )";

        std::vector<std::string> params = {user_id};
        auto request_result = db_conn_->execute_query(request_query, params);

        if (!request_result.rows.empty()) {
            int total_requests = std::stoi(request_result.rows[0].at("total_requests"));
            if (total_requests >= limits.max_requests_per_hour) {
                return false;
            }
        }

        // Check daily cost limit
        std::string cost_query = R"(
            SELECT COALESCE(SUM(cost_accumulated), 0) as total_cost
            FROM chatbot_usage_stats
            WHERE user_id = $1
            AND time_window_start >= CURRENT_DATE
        )";

        auto cost_result = db_conn_->execute_query(cost_query, params);

        if (!cost_result.rows.empty()) {
            double total_cost = std::stod(cost_result.rows[0].at("total_cost"));
            if (total_cost >= limits.max_cost_per_day) {
                return false;
            }
        }

        return true;

    } catch (const std::exception& e) {
        spdlog::error("Exception in check_rate_limits: {}", e.what());
        return false; // Fail closed for security
    }
}

void ChatbotService::record_usage(const std::string& user_id, int tokens_used, double cost) {
    try {
        // Insert or update usage stats for current hour
        std::string query = R"(
            INSERT INTO chatbot_usage_stats (user_id, request_count, token_count, cost_accumulated, time_window_start, time_window_end)
            VALUES ($1, 1, $2, $3, date_trunc('hour', NOW()), date_trunc('hour', NOW()) + INTERVAL '1 hour')
            ON CONFLICT (user_id, time_window_start, time_window_end)
            DO UPDATE SET
                request_count = chatbot_usage_stats.request_count + 1,
                token_count = chatbot_usage_stats.token_count + $2,
                cost_accumulated = chatbot_usage_stats.cost_accumulated + $3
        )";

        std::vector<std::string> params = {
            user_id,
            std::to_string(tokens_used),
            std::to_string(cost)
        };

        db_conn_->execute_command(query, params);

    } catch (const std::exception& e) {
        spdlog::error("Exception in record_usage: {}", e.what());
    }
}

ChatbotResponse ChatbotService::create_fallback_response(const std::string& error_message) {
    ChatbotResponse response;
    response.success = false;
    response.error_message = error_message;
    response.response_text = get_fallback_response("");
    response.confidence_score = 0.0;
    response.tokens_used = 50; // Estimate
    response.cost = 0.001; // Minimal cost
    response.processing_time = std::chrono::milliseconds(100);
    return response;
}

std::string ChatbotService::get_fallback_response(const std::string& user_message) {
    return "I apologize, but I'm currently experiencing technical difficulties. "
           "Please try again in a few moments, or contact support if the issue persists. "
           "For immediate regulatory compliance questions, I recommend consulting the official documentation.";
}

double ChatbotService::calculate_message_cost(const std::string& model, int input_tokens, int output_tokens) {
    // GPT-4 pricing (as of 2024, per 1K tokens)
    double input_price_per_1k = 0.03;   // $0.03 per 1K input tokens
    double output_price_per_1k = 0.06;  // $0.06 per 1K output tokens

    if (model.find("gpt-3.5") != std::string::npos) {
        input_price_per_1k = 0.0015;   // $0.0015 per 1K input tokens
        output_price_per_1k = 0.002;   // $0.002 per 1K output tokens
    }

    double input_cost = (input_tokens / 1000.0) * input_price_per_1k;
    double output_cost = (output_tokens / 1000.0) * output_price_per_1k;

    return input_cost + output_cost;
}

std::pair<int, int> ChatbotService::estimate_token_count(const std::string& text) {
    // Rough estimation: ~4 characters per token for English text
    // This is a simplification - in production, use proper tokenization
    int estimated_tokens = std::max(1, static_cast<int>(text.length() / 4));
    return {estimated_tokens, estimated_tokens};
}

std::string ChatbotService::generate_conversation_title(const std::string& first_message) {
    // Extract first 50 characters as title, clean it up
    std::string title = first_message.substr(0, 50);
    if (title.length() < first_message.length()) {
        title += "...";
    }

    // Basic cleanup
    std::replace(title.begin(), title.end(), '\n', ' ');
    std::replace(title.begin(), title.end(), '\r', ' ');

    return title.empty() ? "New Conversation" : title;
}

std::string ChatbotService::hash_string(const std::string& input) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, input.c_str(), input.length());
    SHA256_Final(hash, &sha256);

    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }

    return ss.str();
}

std::string ChatbotService::generate_uuid() {
    uuid_t uuid;
    uuid_generate(uuid);
    char uuid_str[37];
    uuid_unparse(uuid, uuid_str);
    return std::string(uuid_str);
}

bool ChatbotService::validate_conversation_access(const std::string& conversation_id, const std::string& user_id) {
    try {
        std::string query = "SELECT COUNT(*) FROM chatbot_conversations WHERE conversation_id = $1 AND user_id = $2";
        std::vector<std::string> params = {conversation_id, user_id};
        auto result = db_conn_->execute_query(query, params);

        return !result.rows.empty() && std::stoi(result.rows[0].at("count")) > 0;

    } catch (const std::exception& e) {
        spdlog::error("Exception in validate_conversation_access: {}", e.what());
        return false;
    }
}

std::optional<KnowledgeContext> ChatbotService::get_cached_context(const std::string& query_hash) {
    try {
        std::string query = R"(
            SELECT retrieved_context, relevance_scores
            FROM chatbot_knowledge_cache
            WHERE query_hash = $1 AND expires_at > NOW()
        )";

        std::vector<std::string> params = {query_hash};
        auto result = db_conn_->execute_query(query, params);

        if (!result.rows.empty()) {
            const auto& row = result.rows[0];

            KnowledgeContext context;
            nlohmann::json docs_json = nlohmann::json::parse(row.at("retrieved_context"));
            context.relevant_documents = docs_json.get<std::vector<nlohmann::json>>();

            nlohmann::json scores_json = nlohmann::json::parse(row.at("relevance_scores"));
            context.relevance_scores = scores_json.get<std::vector<double>>();
            context.total_sources = context.relevant_documents.size();

            // Rebuild context summary
            std::stringstream summary;
            for (size_t i = 0; i < context.relevant_documents.size(); ++i) {
                summary << "[" << (i + 1) << "] " << context.relevant_documents[i]["title"] << ":\n";
                summary << context.relevant_documents[i]["content"] << "\n\n";
            }
            context.context_summary = summary.str();

            return context;
        }

    } catch (const std::exception& e) {
        spdlog::error("Exception in get_cached_context: {}", e.what());
    }

    return std::nullopt;
}

void ChatbotService::cache_context(const std::string& query_hash, const std::string& query, const KnowledgeContext& context) {
    try {
        std::string insert_query = R"(
            INSERT INTO chatbot_knowledge_cache (query_hash, query_text, retrieved_context, relevance_scores)
            VALUES ($1, $2, $3, $4)
            ON CONFLICT (query_hash) DO UPDATE SET
                retrieved_context = EXCLUDED.retrieved_context,
                relevance_scores = EXCLUDED.relevance_scores,
                expires_at = NOW() + INTERVAL '24 hours'
        )";

        nlohmann::json relevance_json = context.relevance_scores;

        nlohmann::json docs_json = context.relevant_documents;

        std::vector<std::string> params = {
            query_hash,
            query,
            docs_json.dump(),
            relevance_json.dump()
        };

        db_conn_->execute_command(insert_query, params);

    } catch (const std::exception& e) {
        spdlog::error("Exception in cache_context: {}", e.what());
    }
}

// Configuration setters
void ChatbotService::set_default_model(const std::string& model) {
    default_model_ = model;
}

void ChatbotService::set_knowledge_retrieval_enabled(bool enabled) {
    rag_enabled_ = enabled;
}

void ChatbotService::set_max_context_length(int max_messages) {
    max_context_messages_ = std::max(1, max_messages);
}

void ChatbotService::set_usage_limits(const UsageLimits& limits) {
    usage_limits_ = limits;
}

} // namespace regulens
