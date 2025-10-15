/**
 * Regulatory Chatbot Service Implementation
 * Production-grade regulatory compliance chatbot with full audit trail
 */

#include "regulatory_chatbot_service.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <unordered_map>
#include <unordered_set>
#include <cctype>
#include <uuid/uuid.h>
#include <spdlog/spdlog.h>
#include <regex>
#include <ctime>

namespace regulens {
namespace chatbot {

namespace {

std::string format_double(double value, int precision = 6) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << value;
    return oss.str();
}

std::vector<nlohmann::json> json_array_to_vector(const nlohmann::json& value) {
    std::vector<nlohmann::json> result;
    if (!value.is_array()) {
        return result;
    }
    result.reserve(value.size());
    for (const auto& item : value) {
        result.push_back(item);
    }
    return result;
}

nlohmann::json safe_parse_json(const std::string& raw, const nlohmann::json& fallback = nlohmann::json::object()) {
    if (raw.empty()) {
        return fallback;
    }
    try {
        return nlohmann::json::parse(raw);
    } catch (...) {
        return fallback;
    }
}

std::string to_lower_copy(const std::string& input) {
    std::string lower = input;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return lower;
}

} // namespace

RegulatoryChatbotService::RegulatoryChatbotService(
    std::shared_ptr<PostgreSQLConnection> db_conn,
    std::shared_ptr<VectorKnowledgeBase> knowledge_base,
    std::shared_ptr<OpenAIClient> openai_client,
    std::shared_ptr<StructuredLogger> logger
) : db_conn_(db_conn), knowledge_base_(knowledge_base), openai_client_(openai_client), logger_(logger) {

    if (!db_conn_) {
        throw std::runtime_error("Database connection is required for RegulatoryChatbotService");
    }
    if (!knowledge_base_) {
        throw std::runtime_error("Knowledge base is required for RegulatoryChatbotService");
    }
    if (!openai_client_) {
        throw std::runtime_error("OpenAI client is required for RegulatoryChatbotService");
    }
    if (!logger_) {
        throw std::runtime_error("Logger is required for RegulatoryChatbotService");
    }

    logger_->log(LogLevel::INFO, "RegulatoryChatbotService initialized with audit trail enabled");
}

RegulatoryChatbotService::~RegulatoryChatbotService() {
    logger_->log(LogLevel::INFO, "RegulatoryChatbotService shutting down");
}

RegulatoryChatbotResponse RegulatoryChatbotService::handle_regulatory_query(const RegulatoryChatbotRequest& request) {
    auto start_time = std::chrono::high_resolution_clock::now();

    try {
        // Validate request
        if (request.user_message.empty()) {
            return create_error_response("Empty message received");
        }

        if (request.user_id.empty()) {
            return create_error_response("User ID is required");
        }

        // Create or get session
        std::string session_id = request.session_id;
        if (session_id == "new" || session_id.empty()) {
            session_id = create_session(request.user_id, request.query_context);
            if (session_id.empty()) {
                return create_error_response("Failed to create regulatory session");
            }
        } else {
            // Validate existing session
            auto session = get_session(session_id);
            if (!session || session->user_id != request.user_id) {
                return create_error_response("Invalid session access");
            }
            update_session_activity(session_id);
        }

        // Get conversation history
        std::vector<RegulatoryChatbotMessage> session_messages = get_session_messages(session_id, max_session_messages_);

        // Convert to base ChatbotMessage format for compatibility
        std::vector<ChatbotMessage> conversation_history;
        for (const auto& msg : session_messages) {
            conversation_history.push_back({
                msg.role,
                msg.content,
                0, // token_count will be estimated
                msg.sources,
                msg.confidence_score
            });
        }

        // Search regulatory knowledge with context filtering
        KnowledgeContext knowledge_context = search_regulatory_knowledge(
            request.user_message,
            request.query_context
        );

        // Generate regulatory response
        RegulatoryChatbotResponse response = generate_regulatory_response(
            conversation_history,
            knowledge_context,
            request
        );
        response.session_id = session_id;

        // Calculate processing time
        auto end_time = std::chrono::high_resolution_clock::now();
        response.processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        // Validate response compliance
        response.regulatory_warnings = validate_response_compliance(response.response_text, request.query_context);

        // Generate compliance recommendations
        response.compliance_recommendations = generate_compliance_recommendations(
            request.query_context,
            knowledge_context.relevant_documents
        );

        // Prepare citation preview for response payload
        std::optional<nlohmann::json> citation_payload;
        std::vector<nlohmann::json> citation_sources;
        if (citations_required_ && response.sources_used && response.sources_used->is_array()) {
            citation_sources = json_array_to_vector(*response.sources_used);
            auto previews = build_citation_previews(citation_sources);
            if (!previews.empty()) {
                citation_payload = previews;
                response.citations = previews;
            }
        }

        // Persist user message
        store_regulatory_message(
            session_id,
            "user",
            request.user_message,
            request.query_context,
            response.tokens_used / 2,
            0.0,
            1.0,
            std::nullopt,
            std::nullopt,
            response.processing_time
        );

        // Persist assistant response and citations
        std::string assistant_message_id = store_regulatory_message(
            session_id,
            "assistant",
            response.response_text,
            request.query_context,
            response.tokens_used / 2,
            response.cost,
            response.confidence_score,
            response.sources_used,
            citation_payload,
            response.processing_time
        );

        // Store audit trail
        if (audit_trail_enabled_ && !assistant_message_id.empty()) {
            store_audit_trail(session_id, assistant_message_id, request, response);
        }

        logger_->log(
            LogLevel::INFO,
            "Regulatory chatbot response generated",
            "RegulatoryChatbotService",
            "handle_regulatory_query",
            {
                {"user_id", request.user_id},
                {"session_id", session_id},
                {"tokens_used", std::to_string(response.tokens_used)},
                {"confidence", format_double(response.confidence_score, 4)}
            }
        );

        return response;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_regulatory_query: " + std::string(e.what()));

        auto end_time = std::chrono::high_resolution_clock::now();
        auto processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        RegulatoryChatbotResponse error_response = create_error_response("An error occurred while processing your regulatory query");
        error_response.processing_time = processing_time;
        return error_response;
    }
}

KnowledgeContext RegulatoryChatbotService::search_regulatory_knowledge(
    const std::string& query,
    const RegulatoryQueryContext& context,
    int max_results
) {
    KnowledgeContext knowledge_context;

    try {
        // Create regulatory-focused semantic query
        SemanticQuery semantic_query;
        semantic_query.query_text = query + " " + context.regulatory_domain + " " + context.jurisdiction;
        semantic_query.max_results = max_results;
        semantic_query.similarity_threshold = 0.75f;
        semantic_query.domain_filter = KnowledgeDomain::REGULATORY_COMPLIANCE;

        // Search knowledge base
        auto search_results = knowledge_base_->semantic_search(semantic_query);

        std::vector<regulens::QueryResult> filtered_results;
        filtered_results.reserve(search_results.size());

        for (const auto& result : search_results) {
            const auto& metadata = result.entity.metadata;
            std::string entity_domain = metadata.value("regulatory_domain", "");
            std::string entity_jurisdiction = metadata.value("jurisdiction", "");

            bool domain_match = context.regulatory_domain.empty() || entity_domain.empty() ||
                                to_lower_copy(entity_domain) == to_lower_copy(context.regulatory_domain);
            bool jurisdiction_match = context.jurisdiction.empty() || entity_jurisdiction.empty() ||
                                      to_lower_copy(entity_jurisdiction) == to_lower_copy(context.jurisdiction);

            if (domain_match && jurisdiction_match) {
                filtered_results.push_back(result);
            }
        }

        if (filtered_results.empty()) {
            filtered_results = search_results;
        }

        // Build context from regulatory results
        std::stringstream context_stream;
        context_stream << "Regulatory Context (" << context.regulatory_domain << " - " << context.jurisdiction << "):\n\n";

        knowledge_context.relevance_scores.reserve(filtered_results.size());
        for (size_t i = 0; i < filtered_results.size(); ++i) {
            const auto& result = filtered_results[i];

            // Enhanced document entry with regulatory metadata
            nlohmann::json doc_entry = {
                {"title", result.entity.title},
                {"content", result.entity.content},
                {"relevance_score", result.similarity_score},
                {"doc_id", result.entity.entity_id},
                {"domain", static_cast<int>(result.entity.domain)},
                {"regulatory_domain", context.regulatory_domain},
                {"jurisdiction", context.jurisdiction},
                {"source_type", "knowledge_base"},
                {"citation_required", context.requires_citation}
            };

            knowledge_context.relevant_documents.push_back(doc_entry);
            knowledge_context.relevance_scores.push_back(result.similarity_score);

            // Add to context summary with regulatory focus
            context_stream << "[" << (i + 1) << "] " << result.entity.title << ":\n";
            std::string content = result.entity.content.substr(0, 750); // Longer excerpts for regulatory content
            if (result.entity.content.length() > 750) {
                content += "... [Citation: " + result.entity.entity_id + "]";
            }
            context_stream << content << "\n\n";
        }

        knowledge_context.context_summary = context_stream.str();
        knowledge_context.total_sources = static_cast<int>(filtered_results.size());

        // Calculate regulatory confidence
        knowledge_context.relevance_scores.push_back(
            calculate_regulatory_confidence(knowledge_context.relevant_documents, context)
        );

        logger_->log(
            LogLevel::INFO,
            "Regulatory knowledge retrieval completed",
            "RegulatoryChatbotService",
            "search_regulatory_knowledge",
            {
                {"domain", context.regulatory_domain},
                {"jurisdiction", context.jurisdiction},
                {"sources", std::to_string(knowledge_context.total_sources)}
            }
        );

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in search_regulatory_knowledge: " + std::string(e.what()));
        knowledge_context.context_summary = "Error retrieving regulatory knowledge context.";
    }

    return knowledge_context;
}

RegulatoryChatbotResponse RegulatoryChatbotService::generate_regulatory_response(
    const std::vector<ChatbotMessage>& conversation_history,
    const KnowledgeContext& knowledge_context,
    const RegulatoryChatbotRequest& request
) {
    RegulatoryChatbotResponse response;

    try {
        // Build regulatory system prompt
        std::string system_prompt = build_regulatory_system_prompt(request.query_context, knowledge_context);

        // Prepare messages for OpenAI API
        std::vector<OpenAIMessage> messages;
        messages.emplace_back("system", system_prompt);

        int history_limit = std::min(static_cast<int>(conversation_history.size()), request.max_context_messages - 1);
        size_t start_index = conversation_history.size() > static_cast<size_t>(history_limit)
            ? conversation_history.size() - static_cast<size_t>(history_limit)
            : 0;
        for (size_t i = start_index; i < conversation_history.size(); ++i) {
            messages.emplace_back(conversation_history[i].role, conversation_history[i].content);
        }

        messages.emplace_back("user", request.user_message);

        OpenAICompletionRequest completion_request;
        completion_request.model = request.model_override.value_or(default_model_);
        completion_request.messages = messages;
        completion_request.temperature = 0.1;
        completion_request.max_tokens = 2000;
        completion_request.presence_penalty = 0.0;
        completion_request.frequency_penalty = 0.0;
        completion_request.user = request.user_id;

        auto openai_result = openai_client_->create_chat_completion(completion_request);
        if (!openai_result) {
            response.error_message = "OpenAI API request failed";
            response.success = false;
            return response;
        }

        const auto& openai_response = *openai_result;
        if (openai_response.choices.empty()) {
            response.error_message = "OpenAI returned no completion choices";
            response.success = false;
            return response;
        }

        response.response_text = openai_response.choices.front().message.content;
        response.success = true;
        response.tokens_used = openai_response.usage.total_tokens;
        response.cost = calculate_message_cost(
            completion_request.model,
            openai_response.usage.prompt_tokens,
            openai_response.usage.completion_tokens
        );

        response.sources_used = nlohmann::json(knowledge_context.relevant_documents);
        response.confidence_score = calculate_regulatory_confidence(
            knowledge_context.relevant_documents,
            request.query_context
        );

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in generate_regulatory_response: " + std::string(e.what()));
        response.error_message = "Failed to generate regulatory response";
        response.success = false;
    }

    return response;
}

std::string RegulatoryChatbotService::build_regulatory_system_prompt(
    const RegulatoryQueryContext& context,
    const KnowledgeContext& knowledge_context
) {
    std::stringstream prompt;

    prompt << "You are an expert regulatory compliance assistant specializing in " << context.regulatory_domain;
    prompt << " regulations in " << context.jurisdiction << " jurisdiction.\n\n";

    prompt << "CRITICAL REQUIREMENTS:\n";
    prompt << "1. Always cite specific regulations, laws, or guidelines when providing advice\n";
    prompt << "2. Clearly distinguish between definitive requirements and best practices\n";
    prompt << "3. Include appropriate disclaimers for legal advice\n";
    prompt << "4. Maintain objectivity and accuracy above all else\n";
    prompt << "5. If uncertain about any regulatory requirement, explicitly state this\n\n";

    prompt << "REGULATORY CONTEXT:\n";
    prompt << context.regulatory_domain << " - " << context.jurisdiction << "\n";
    prompt << "Risk Level: " << context.risk_level << "\n";
    prompt << "Query Type: " << context.query_type << "\n\n";

    if (!context.relevant_regulations.empty()) {
        prompt << "RELEVANT REGULATIONS TO CONSIDER:\n";
        for (const auto& reg : context.relevant_regulations) {
            prompt << "- " << reg << "\n";
        }
        prompt << "\n";
    }

    prompt << "AVAILABLE KNOWLEDGE BASE:\n";
    prompt << knowledge_context.context_summary << "\n";

    prompt << "RESPONSE GUIDELINES:\n";
    prompt << "- Provide specific, actionable regulatory guidance\n";
    prompt << "- Include concrete compliance steps when applicable\n";
    prompt << "- Reference specific regulatory citations\n";
    prompt << "- Suggest risk mitigation strategies\n";
    prompt << "- Recommend documentation and record-keeping\n\n";

    prompt << "DISCLAIMER: This is not legal advice. Consult with qualified legal counsel for your specific situation.\n";

    return prompt.str();
}

std::vector<nlohmann::json> RegulatoryChatbotService::build_citation_previews(const std::vector<nlohmann::json>& sources) const {
    std::vector<nlohmann::json> previews;
    previews.reserve(sources.size());

    for (const auto& source : sources) {
        nlohmann::json citation = {
            {"knowledge_base_id", source.value("doc_id", source.value("entity_id", "unknown"))},
            {"document_title", source.value("title", "Untitled Document")},
            {"document_source", source.value("source_type", "knowledge_base")},
            {"relevance_score", source.value("relevance_score", 0.0)},
            {"metadata", source}
        };

        if (source.contains("jurisdiction")) {
            citation["jurisdiction"] = source["jurisdiction"];
        }
        if (source.contains("regulatory_domain")) {
            citation["regulatory_domain"] = source["regulatory_domain"];
        }

        previews.push_back(std::move(citation));
    }

    return previews;
}

std::vector<nlohmann::json> RegulatoryChatbotService::cite_sources(
    const std::string& message_id,
    const std::vector<nlohmann::json>& sources
) {
    std::vector<nlohmann::json> citations = build_citation_previews(sources);

    try {
        for (auto& citation : citations) {
            std::string citation_id = generate_uuid();
            nlohmann::json citation_metadata = citation.value("metadata", nlohmann::json::object());

            bool success = db_conn_->execute_command(
                "INSERT INTO chatbot_knowledge_citations "
                "(citation_id, message_id, knowledge_base_id, document_title, document_source, relevance_score, citation_metadata) "
                "VALUES ($1, $2, $3, $4, $5, $6::decimal, $7::jsonb)",
                {
                    citation_id,
                    message_id,
                    citation.value("knowledge_base_id", "unknown"),
                    citation.value("document_title", "Untitled Document"),
                    citation.value("document_source", "knowledge_base"),
                    format_double(citation.value("relevance_score", 0.0), 4),
                    citation_metadata.dump()
                }
            );

            if (success) {
                citation["citation_id"] = citation_id;
            } else {
                logger_->log(
                    LogLevel::WARN,
                    "Failed to insert citation",
                    "RegulatoryChatbotService",
                    "cite_sources",
                    {
                        {"message_id", message_id},
                        {"knowledge_base_id", citation.value("knowledge_base_id", "unknown")}
                    }
                );
            }
        }

        if (!citations.empty()) {
            log_citation_usage(message_id, citations);
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in cite_sources: " + std::string(e.what()));
    }

    return citations;
}

bool RegulatoryChatbotService::store_audit_trail(
    const std::string& session_id,
    const std::string& message_id,
    const RegulatoryChatbotRequest& request,
    const RegulatoryChatbotResponse& response
) {
    try {
        // Log session activity
        log_access_to_regulation(session_id, request.query_context.regulatory_domain);

        // Log high-risk queries
        if (request.query_context.risk_level == "high" || request.query_context.risk_level == "critical") {
            logger_->log(
                LogLevel::WARN,
                "High-risk regulatory query processed",
                "RegulatoryChatbotService",
                "store_audit_trail",
                {
                    {"session_id", session_id},
                    {"user_id", request.user_id},
                    {"query_type", request.query_context.query_type},
                    {"regulatory_domain", request.query_context.regulatory_domain},
                    {"risk_level", request.query_context.risk_level},
                    {"confidence_score", format_double(response.confidence_score, 4)}
                }
            );
        }

        return true;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in store_audit_trail: " + std::string(e.what()));
        return false;
    }
}

std::string RegulatoryChatbotService::create_session(const std::string& user_id, const RegulatoryQueryContext& context) {
    try {
        std::string session_id = generate_uuid();
        std::string title = generate_session_title("Regulatory consultation", context);

        nlohmann::json metadata = {
            {"regulatory_domain", context.regulatory_domain},
            {"jurisdiction", context.jurisdiction},
            {"query_type", context.query_type},
            {"risk_level", context.risk_level},
            {"audit_mode", audit_trail_enabled_},
            {"model", default_model_}
        };

        bool success = db_conn_->execute_command(
            "INSERT INTO chatbot_sessions (session_id, user_id, session_title, session_metadata) "
            "VALUES ($1, $2, $3, $4::jsonb)",
            {session_id, user_id, title, metadata.dump()}
        );

        if (success) {
            logger_->log(
                LogLevel::INFO,
                "Created regulatory chatbot session",
                "RegulatoryChatbotService",
                "create_session",
                {
                    {"session_id", session_id},
                    {"user_id", user_id},
                    {"regulatory_domain", context.regulatory_domain},
                    {"jurisdiction", context.jurisdiction}
                }
            );
            return session_id;
        }

        logger_->log(
            LogLevel::ERROR,
            "Failed to create regulatory chatbot session",
            "RegulatoryChatbotService",
            "create_session",
            {
                {"user_id", user_id},
                {"regulatory_domain", context.regulatory_domain},
                {"jurisdiction", context.jurisdiction}
            }
        );
        return "";

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in create_session: " + std::string(e.what()));
        return "";
    }
}

std::string RegulatoryChatbotService::store_regulatory_message(
    const std::string& session_id,
    const std::string& role,
    const std::string& content,
    const RegulatoryQueryContext& context,
    int token_count,
    double cost,
    double confidence_score,
    const std::optional<nlohmann::json>& sources,
    const std::optional<nlohmann::json>& citations,
    std::chrono::milliseconds processing_time
) {
    try {
        std::string message_id = generate_uuid();
        nlohmann::json message_metadata = {
            {"regulatory_domain", context.regulatory_domain},
            {"jurisdiction", context.jurisdiction},
            {"query_type", context.query_type},
            {"risk_level", context.risk_level},
            {"token_count", token_count},
            {"cost", cost},
            {"processing_time_ms", processing_time.count()},
            {"model_used", default_model_}
        };
        std::string sources_payload = (sources && !sources->is_null()) ? sources->dump() : "null";

        bool success = db_conn_->execute_command(
            "INSERT INTO chatbot_messages (message_id, session_id, role, content, confidence_score, sources, message_metadata) "
            "VALUES ($1, $2, $3, $4, $5::decimal, $6::jsonb, $7::jsonb)",
            {
                message_id,
                session_id,
                role,
                content,
                format_double(confidence_score, 4),
                sources_payload,
                message_metadata.dump()
            }
        );

        if (!success) {
            logger_->log(
                LogLevel::ERROR,
                "Failed to store chatbot message",
                "RegulatoryChatbotService",
                "store_regulatory_message",
                {
                    {"session_id", session_id},
                    {"role", role}
                }
            );
            return "";
        }

        if (citations && citations->is_array() && !citations->empty()) {
            auto citation_vector = json_array_to_vector(*citations);
            auto stored_citations = cite_sources(message_id, citation_vector);

            if (!stored_citations.empty()) {
                nlohmann::json citation_ids = nlohmann::json::array();
                for (const auto& citation : stored_citations) {
                    if (citation.contains("citation_id")) {
                        citation_ids.push_back(citation["citation_id"]);
                    }
                }

                if (!citation_ids.empty()) {
                    db_conn_->execute_command(
                        "UPDATE chatbot_messages SET message_metadata = message_metadata || $2::jsonb WHERE message_id = $1",
                        {message_id, nlohmann::json{{"citation_ids", citation_ids}}.dump()}
                    );
                }
            }
        }

        return message_id;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in store_regulatory_message: " + std::string(e.what()));
        return "";
    }
}

std::vector<std::string> RegulatoryChatbotService::validate_response_compliance(
    const std::string& response_text,
    const RegulatoryQueryContext& context
) {
    std::vector<std::string> warnings;

    // Check for required disclaimer language
    if (!contains_disclaimer_language(response_text)) {
        warnings.push_back("Response should include legal disclaimer");
    }

    // Check for regulatory warnings based on context
    auto context_warnings = check_regulatory_warnings(response_text, context);
    warnings.insert(warnings.end(), context_warnings.begin(), context_warnings.end());

    // Check confidence threshold
    if (context.risk_level == "high" || context.risk_level == "critical") {
        warnings.push_back("High-risk regulatory query - recommend human review");
    }

    return warnings;
}

std::vector<std::string> RegulatoryChatbotService::generate_compliance_recommendations(
    const RegulatoryQueryContext& context,
    const std::vector<nlohmann::json>& relevant_sources
) {
    std::vector<std::string> recommendations;

    // Base recommendations by regulatory domain
    if (context.regulatory_domain == "aml") {
        recommendations.push_back("Implement robust customer due diligence procedures");
        recommendations.push_back("Establish comprehensive transaction monitoring systems");
        recommendations.push_back("Maintain detailed records of suspicious activity investigations");
    } else if (context.regulatory_domain == "kyc") {
        recommendations.push_back("Verify customer identity using multiple reliable sources");
        recommendations.push_back("Regularly update customer information");
        recommendations.push_back("Implement risk-based enhanced due diligence for high-risk customers");
    } else if (context.regulatory_domain == "fraud") {
        recommendations.push_back("Deploy multi-layered fraud detection systems");
        recommendations.push_back("Implement real-time transaction monitoring");
        recommendations.push_back("Establish clear incident response procedures");
    }

    // Jurisdiction-specific recommendations
    if (context.jurisdiction == "eu") {
        recommendations.push_back("Ensure GDPR compliance for data processing activities");
    } else if (context.jurisdiction == "us") {
        recommendations.push_back("Comply with applicable state and federal regulations");
    }

    // Risk-based recommendations
    if (context.risk_level == "high" || context.risk_level == "critical") {
        recommendations.push_back("Consider engaging external compliance experts");
        recommendations.push_back("Implement additional monitoring and controls");
        recommendations.push_back("Prepare detailed documentation for regulatory examinations");
    }

    return recommendations;
}

double RegulatoryChatbotService::calculate_regulatory_confidence(
    const std::vector<nlohmann::json>& sources,
    const RegulatoryQueryContext& context
) {
    if (sources.empty()) {
        return 0.0;
    }

    double total_relevance = 0.0;
    int high_relevance_count = 0;

    for (const auto& source : sources) {
        double relevance = source.value("relevance_score", 0.0);
        total_relevance += relevance;

        if (relevance >= 0.8) {
            high_relevance_count++;
        }
    }

    double avg_relevance = total_relevance / sources.size();
    double high_relevance_ratio = static_cast<double>(high_relevance_count) / sources.size();

    // Boost confidence for high-risk queries with good sources
    double confidence = avg_relevance * 0.7 + high_relevance_ratio * 0.3;

    if (context.risk_level == "high" || context.risk_level == "critical") {
        confidence = std::max(confidence, 0.85); // Minimum confidence for high-risk
    }

    return std::min(confidence, 1.0);
}

std::string RegulatoryChatbotService::generate_uuid() {
    uuid_t uuid;
    uuid_generate(uuid);

    char uuid_str[37];
    uuid_unparse_lower(uuid, uuid_str);

    return std::string(uuid_str);
}

RegulatoryChatbotResponse RegulatoryChatbotService::create_error_response(const std::string& error_message) {
    RegulatoryChatbotResponse response;
    response.success = false;
    response.error_message = error_message;
    response.confidence_score = 0.0;
    response.processing_time = std::chrono::milliseconds(0);
    return response;
}

bool RegulatoryChatbotService::contains_disclaimer_language(const std::string& response) {
    std::vector<std::string> disclaimer_keywords = {
        "not legal advice",
        "consult legal counsel",
        "consult attorney",
        "professional advice",
        "disclaimer"
    };

    std::string lower_response = response;
    std::transform(lower_response.begin(), lower_response.end(), lower_response.begin(), ::tolower);

    for (const auto& keyword : disclaimer_keywords) {
        if (lower_response.find(keyword) != std::string::npos) {
            return true;
        }
    }

    return false;
}

std::vector<std::string> RegulatoryChatbotService::check_regulatory_warnings(
    const std::string& response,
    const RegulatoryQueryContext& context
) {
    std::vector<std::string> warnings;

    // Check for definitive language in uncertain contexts
    if (context.query_type == "regulatory_interpretation") {
        std::vector<std::string> definitive_words = {"always", "never", "must", "required"};
        std::string lower_response = response;
        std::transform(lower_response.begin(), lower_response.end(), lower_response.begin(), ::tolower);

        for (const auto& word : definitive_words) {
            if (lower_response.find(word) != std::string::npos) {
                warnings.push_back("Use of definitive language in regulatory interpretation - consider qualifying statements");
                break;
            }
        }
    }

    return warnings;
}

bool RegulatoryChatbotService::log_access_to_regulation(const std::string& session_id, const std::string& regulation_code) {
    logger_->log(
        LogLevel::INFO,
        "Regulatory domain accessed",
        "RegulatoryChatbotService",
        "log_access_to_regulation",
        {
            {"session_id", session_id},
            {"regulation_code", regulation_code}
        }
    );
    return true;
}

std::string RegulatoryChatbotService::generate_session_title(const std::string& seed, const RegulatoryQueryContext& context) {
    std::string normalized_seed = seed;
    if (normalized_seed.size() > 60) {
        normalized_seed = normalized_seed.substr(0, 57) + "...";
    }

    std::ostringstream oss;
    oss << "Regulatory Consultation - " << context.regulatory_domain;
    if (!context.jurisdiction.empty()) {
        oss << " (" << context.jurisdiction << ")";
    }
    if (!normalized_seed.empty()) {
        oss << " - " << normalized_seed;
    }
    return oss.str();
}

KnowledgeContext RegulatoryChatbotService::filter_regulatory_context(
    const KnowledgeContext& context,
    const RegulatoryQueryContext& query_context
) {
    KnowledgeContext filtered;
    filtered.context_summary = context.context_summary;

    for (size_t i = 0; i < context.relevant_documents.size(); ++i) {
        const auto& doc = context.relevant_documents[i];
        std::string domain = doc.value("regulatory_domain", "");
        std::string jurisdiction = doc.value("jurisdiction", "");

        bool domain_match = query_context.regulatory_domain.empty() || domain.empty() ||
                            to_lower_copy(domain) == to_lower_copy(query_context.regulatory_domain);
        bool jurisdiction_match = query_context.jurisdiction.empty() || jurisdiction.empty() ||
                                  to_lower_copy(jurisdiction) == to_lower_copy(query_context.jurisdiction);

        if (domain_match && jurisdiction_match) {
            filtered.relevant_documents.push_back(doc);
            if (i < context.relevance_scores.size()) {
                filtered.relevance_scores.push_back(context.relevance_scores[i]);
            }
        }
    }

    filtered.total_sources = static_cast<int>(filtered.relevant_documents.size());
    return filtered;
}

bool RegulatoryChatbotService::log_citation_usage(const std::string& message_id, const std::vector<nlohmann::json>& citations) {
    logger_->log(
        LogLevel::INFO,
        "Stored regulator citations",
        "RegulatoryChatbotService",
        "log_citation_usage",
        {
            {"message_id", message_id},
            {"citation_count", std::to_string(citations.size())}
        }
    );
    return true;
}

std::string RegulatoryChatbotService::format_citations_for_display(const std::vector<nlohmann::json>& citations) {
    if (citations.empty()) {
        return "";
    }

    std::ostringstream oss;
    for (size_t i = 0; i < citations.size(); ++i) {
        const auto& citation = citations[i];
        oss << "[" << (i + 1) << "] "
            << citation.value("document_title", "Untitled Document")
            << " (" << citation.value("document_source", "knowledge_base") << ")";
        if (i + 1 < citations.size()) {
            oss << "; ";
        }
    }
    return oss.str();
}

double RegulatoryChatbotService::calculate_message_cost(
    const std::string& model,
    int input_tokens,
    int output_tokens
) {
    double input_cost_per_token = 0.0000015;  // Approximate for GPT-4
    double output_cost_per_token = 0.000002;

    return (input_tokens * input_cost_per_token) + (output_tokens * output_cost_per_token);
}

std::optional<RegulatoryChatbotSession> RegulatoryChatbotService::get_session(const std::string& session_id) {
    try {
        auto row = db_conn_->execute_query_single(
            "SELECT session_id, user_id, session_title, started_at, last_activity_at, is_active, session_metadata "
            "FROM chatbot_sessions WHERE session_id = $1",
            {session_id}
        );

        if (!row) {
            return std::nullopt;
        }

        RegulatoryChatbotSession session;
        session.session_id = row->value("session_id", "");
        session.user_id = row->value("user_id", "");
        session.title = row->value("session_title", "");
        session.is_active = row->value("is_active", "t") == "t";
        session.started_at = parse_timestamp(row->value("started_at", ""));
        session.last_activity_at = parse_timestamp(row->value("last_activity_at", ""));

        auto metadata = safe_parse_json(row->value("session_metadata", ""));
        session.session_metadata = metadata;
        session.regulatory_domain = metadata.value("regulatory_domain", "");
        session.jurisdiction = metadata.value("jurisdiction", "");
        session.audit_mode = metadata.value("audit_mode", audit_trail_enabled_);

        if (metadata.contains("accessed_regulations") && metadata["accessed_regulations"].is_array()) {
            for (const auto& item : metadata["accessed_regulations"]) {
                if (item.is_string()) {
                    session.accessed_regulations.push_back(item.get<std::string>());
                }
            }
        }

        return session;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in get_session: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::vector<RegulatoryChatbotSession> RegulatoryChatbotService::get_user_sessions(const std::string& user_id, int limit) {
    std::vector<RegulatoryChatbotSession> sessions;

    try {
        limit = std::clamp(limit, 1, 200);
        auto rows = db_conn_->execute_query_multi(
            "SELECT session_id, user_id, session_title, started_at, last_activity_at, is_active, session_metadata "
            "FROM chatbot_sessions WHERE user_id = $1 ORDER BY last_activity_at DESC LIMIT $2::int",
            {user_id, std::to_string(limit)}
        );

        sessions.reserve(rows.size());
        for (const auto& row : rows) {
            RegulatoryChatbotSession session;
            session.session_id = row.value("session_id", "");
            session.user_id = row.value("user_id", "");
            session.title = row.value("session_title", "");
            session.is_active = row.value("is_active", "t") == "t";
            session.started_at = parse_timestamp(row.value("started_at", ""));
            session.last_activity_at = parse_timestamp(row.value("last_activity_at", ""));

            auto metadata = safe_parse_json(row.value("session_metadata", ""));
            session.session_metadata = metadata;
            session.regulatory_domain = metadata.value("regulatory_domain", "");
            session.jurisdiction = metadata.value("jurisdiction", "");
            sessions.push_back(std::move(session));
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in get_user_sessions: " + std::string(e.what()));
    }

    return sessions;
}

bool RegulatoryChatbotService::archive_session(const std::string& session_id) {
    try {
        bool success = db_conn_->execute_command(
            "UPDATE chatbot_sessions SET is_active = false WHERE session_id = $1",
            {session_id}
        );
        return success;
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in archive_session: " + std::string(e.what()));
        return false;
    }
}

bool RegulatoryChatbotService::update_session_activity(const std::string& session_id) {
    try {
        return db_conn_->execute_command(
            "UPDATE chatbot_sessions SET last_activity_at = NOW() WHERE session_id = $1",
            {session_id}
        );
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in update_session_activity: " + std::string(e.what()));
        return false;
    }
}

std::vector<nlohmann::json> RegulatoryChatbotService::fetch_message_citations(const std::string& message_id) {
    std::vector<nlohmann::json> result;

    try {
        auto rows = db_conn_->execute_query_multi(
            "SELECT citation_id, knowledge_base_id, document_title, document_source, relevance_score, cited_at, citation_metadata "
            "FROM chatbot_knowledge_citations WHERE message_id = $1 ORDER BY cited_at DESC",
            {message_id}
        );

        result.reserve(rows.size());
        for (const auto& row : rows) {
            nlohmann::json citation = {
                {"citation_id", row.value("citation_id", "")},
                {"knowledge_base_id", row.value("knowledge_base_id", "")},
                {"document_title", row.value("document_title", "")},
                {"document_source", row.value("document_source", "")},
                {"relevance_score", row.contains("relevance_score") ? std::stod(row.value("relevance_score", "0")) : 0.0},
                {"cited_at", row.value("cited_at", "")}
            };

            citation["metadata"] = safe_parse_json(row.value("citation_metadata", ""));
            result.push_back(std::move(citation));
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in fetch_message_citations: " + std::string(e.what()));
    }

    return result;
}

std::vector<RegulatoryChatbotMessage> RegulatoryChatbotService::get_session_messages(
    const std::string& session_id,
    int limit,
    int offset
) {
    std::vector<RegulatoryChatbotMessage> messages;

    try {
        limit = std::clamp(limit, 1, 200);
        offset = std::max(0, offset);

        auto rows = db_conn_->execute_query_multi(
            "SELECT message_id, session_id, role, content, timestamp, sources, confidence_score, feedback, message_metadata "
            "FROM chatbot_messages WHERE session_id = $1 ORDER BY timestamp DESC LIMIT $2::int OFFSET $3::int",
            {session_id, std::to_string(limit), std::to_string(offset)}
        );

        messages.reserve(rows.size());
        for (const auto& row : rows) {
            RegulatoryChatbotMessage message;
            message.message_id = row.value("message_id", "");
            message.session_id = row.value("session_id", session_id);
            message.role = row.value("role", "assistant");
            message.content = row.value("content", "");
            message.timestamp = parse_timestamp(row.value("timestamp", ""));
            message.feedback = row.value("feedback", "");

            try {
                message.confidence_score = std::stod(row.value("confidence_score", "0"));
            } catch (...) {
                message.confidence_score = 0.0;
            }

            auto sources_json = safe_parse_json(row.value("sources", ""), nlohmann::json::array());
            if (!sources_json.is_null() && !sources_json.empty()) {
                message.sources = sources_json;
            }

            auto metadata = safe_parse_json(row.value("message_metadata", ""));
            message.context.regulatory_domain = metadata.value("regulatory_domain", "");
            message.context.jurisdiction = metadata.value("jurisdiction", "");
            message.context.query_type = metadata.value("query_type", "");
            message.context.risk_level = metadata.value("risk_level", "");
            message.context.requires_citation = metadata.value("requires_citation", true);
            message.context.audit_trail_required = metadata.value("audit_mode", audit_trail_enabled_);

            auto citations_vec = fetch_message_citations(message.message_id);
            if (!citations_vec.empty()) {
                message.citations = citations_vec;
            }
            messages.push_back(std::move(message));
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in get_session_messages: " + std::string(e.what()));
    }

    return messages;
}

bool RegulatoryChatbotService::submit_feedback(
    const std::string& message_id,
    const std::string& feedback_type,
    const std::optional<std::string>& comments
) {
    try {
        bool success = db_conn_->execute_command(
            "UPDATE chatbot_messages SET feedback = $2 WHERE message_id = $1",
            {message_id, feedback_type}
        );

        if (success && comments && !comments->empty()) {
            db_conn_->execute_command(
                "UPDATE chatbot_messages SET message_metadata = message_metadata || $2::jsonb WHERE message_id = $1",
                {message_id, nlohmann::json{{"feedback_comment", *comments}}.dump()}
            );
        }

        return success;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in submit_feedback: " + std::string(e.what()));
        return false;
    }
}

std::chrono::system_clock::time_point RegulatoryChatbotService::parse_timestamp(const std::string& timestamp) const {
    if (timestamp.empty()) {
        return std::chrono::system_clock::now();
    }

    std::tm tm{};
    std::istringstream ss(timestamp);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (ss.fail()) {
        ss.clear();
        ss.str(timestamp);
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    }

#if defined(_WIN32)
    time_t time_value = _mkgmtime(&tm);
#else
    time_t time_value = timegm(&tm);
#endif
    if (time_value == -1) {
        return std::chrono::system_clock::now();
    }
    return std::chrono::system_clock::from_time_t(time_value);
}

std::vector<std::string> RegulatoryChatbotService::extract_regulatory_entities(const std::string& text) {
    static const std::vector<std::string> entities = {
        "aml", "kyc", "gdpr", "ccpa", "basel", "sox", "hipaa", "finra", "sec", "fca"
    };

    std::vector<std::string> matches;
    std::string lower = to_lower_copy(text);
    for (const auto& entity : entities) {
        if (lower.find(entity) != std::string::npos) {
            matches.push_back(entity);
        }
    }
    return matches;
}

std::vector<std::string> RegulatoryChatbotService::identify_risk_indicators(const std::string& query) {
    static const std::vector<std::string> indicators = {
        "penalty", "violation", "non-compliance", "fine", "sanction", "risk", "investigation", "audit"
    };

    std::vector<std::string> detected;
    std::string lower = to_lower_copy(query);
    for (const auto& indicator : indicators) {
        if (lower.find(indicator) != std::string::npos) {
            detected.push_back(indicator);
        }
    }
    return detected;
}

void RegulatoryChatbotService::set_regulatory_focus_domains(const std::vector<std::string>& domains) {
    regulatory_focus_domains_ = domains;
}

void RegulatoryChatbotService::set_audit_trail_enabled(bool enabled) {
    audit_trail_enabled_ = enabled;
}

void RegulatoryChatbotService::set_minimum_confidence_threshold(double threshold) {
    min_confidence_threshold_ = std::clamp(threshold, 0.0, 1.0);
}

void RegulatoryChatbotService::set_citation_required(bool required) {
    citations_required_ = required;
}

} // namespace chatbot
} // namespace regulens
