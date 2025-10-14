/**
 * Regulatory Chatbot Service Implementation
 * Production-grade regulatory compliance chatbot with full audit trail
 */

#include "regulatory_chatbot_service.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <uuid/uuid.h>
#include <spdlog/spdlog.h>
#include <regex>

namespace regulens {
namespace chatbot {

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

        // Store messages with audit trail
        std::optional<nlohmann::json> citations;
        if (citations_required_ && response.sources_used) {
            citations = cite_sources("temp_" + generate_uuid(), response.sources_used.value());
        }

        store_regulatory_message(
            session_id, "user", request.user_message, request.query_context,
            response.tokens_used / 2, 0.0, 1.0, std::nullopt, std::nullopt, response.processing_time
        );

        store_regulatory_message(
            session_id, "assistant", response.response_text, request.query_context,
            response.tokens_used / 2, response.cost, response.confidence_score,
            response.sources_used, citations, response.processing_time
        );

        // Store audit trail
        if (audit_trail_enabled_) {
            store_audit_trail(session_id, "temp_" + generate_uuid(), request, response);
        }

        logger_->log(LogLevel::INFO,
            "Regulatory chatbot response generated for user {} in session {} ({} tokens, confidence: {:.2f})",
            request.user_id, session_id, response.tokens_used, response.confidence_score);

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

        // Add regulatory domain filtering
        nlohmann::json metadata_filter = {
            {"regulatory_domain", context.regulatory_domain},
            {"jurisdiction", context.jurisdiction}
        };
        semantic_query.metadata_filter = metadata_filter;

        // Search knowledge base
        auto search_results = knowledge_base_->semantic_search(semantic_query);

        // Build context from regulatory results
        std::stringstream context_stream;
        context_stream << "Regulatory Context (" << context.regulatory_domain << " - " << context.jurisdiction << "):\n\n";

        for (size_t i = 0; i < search_results.size(); ++i) {
            const auto& result = search_results[i];

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
        knowledge_context.total_sources = search_results.size();

        // Calculate regulatory confidence
        knowledge_context.relevance_scores.push_back(
            calculate_regulatory_confidence(knowledge_context.relevant_documents, context)
        );

        logger_->log(LogLevel::INFO,
            "Retrieved {} regulatory knowledge sources for domain: {} jurisdiction: {}",
            knowledge_context.total_sources, context.regulatory_domain, context.jurisdiction);

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
        std::vector<nlohmann::json> messages;

        // System message
        messages.push_back({
            {"role", "system"},
            {"content", system_prompt}
        });

        // Add conversation history (limited)
        int history_limit = std::min(static_cast<int>(conversation_history.size()), request.max_context_messages - 1);
        for (int i = conversation_history.size() - history_limit; i < conversation_history.size(); ++i) {
            messages.push_back({
                {"role", conversation_history[i].role},
                {"content", conversation_history[i].content}
            });
        }

        // Add current user message
        messages.push_back({
            {"role", "user"},
            {"content", request.user_message}
        });

        // Prepare OpenAI request
        nlohmann::json openai_request = {
            {"model", request.model_override.value_or(default_model_)},
            {"messages", messages},
            {"temperature", 0.1}, // Lower temperature for regulatory accuracy
            {"max_tokens", 2000},
            {"presence_penalty", 0.0},
            {"frequency_penalty", 0.0}
        };

        // Call OpenAI API
        auto openai_response = openai_client_->chat_completion(openai_request);

        if (openai_response.contains("error")) {
            response.error_message = openai_response["error"]["message"];
            response.success = false;
            return response;
        }

        // Extract response
        response.response_text = openai_response["choices"][0]["message"]["content"];
        response.success = true;

        // Extract usage information
        if (openai_response.contains("usage")) {
            response.tokens_used = openai_response["usage"]["total_tokens"];
            response.cost = calculate_message_cost(
                request.model_override.value_or(default_model_),
                openai_response["usage"]["prompt_tokens"],
                openai_response["usage"]["completion_tokens"]
            );
        }

        // Set sources and confidence
        response.sources_used = nlohmann::json(knowledge_context.relevant_documents);
        response.confidence_score = calculate_regulatory_confidence(
            knowledge_context.relevant_documents,
            request.query_context
        );

        // Generate citations if required
        if (citations_required_ && !knowledge_context.relevant_documents.empty()) {
            response.citations = cite_sources("temp_" + generate_uuid(), knowledge_context.relevant_documents);
        }

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

std::vector<nlohmann::json> RegulatoryChatbotService::cite_sources(
    const std::string& message_id,
    const std::vector<nlohmann::json>& sources
) {
    std::vector<nlohmann::json> citations;

    try {
        auto conn = db_conn_->get_connection();
        if (!conn) {
            logger_->log(LogLevel::ERROR, "Database connection failed in cite_sources");
            return citations;
        }

        for (const auto& source : sources) {
            std::string citation_id = generate_uuid();

            const char* citation_params[8] = {
                citation_id.c_str(),
                message_id.c_str(),
                source.value("doc_id", "unknown").c_str(),
                source.value("title", "Untitled Document").c_str(),
                source.value("source_type", "knowledge_base").c_str(),
                std::to_string(source.value("relevance_score", 0.0)).c_str(),
                std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()).c_str(),
                source.dump().c_str()
            };

            PGresult* result = PQexecParams(
                conn,
                "INSERT INTO chatbot_knowledge_citations "
                "(citation_id, message_id, knowledge_base_id, document_title, document_source, "
                "relevance_score, cited_at, citation_metadata) "
                "VALUES ($1, $2, $3, $4, $5, $6::decimal, to_timestamp($7::bigint), $8::jsonb)",
                8, nullptr, citation_params, nullptr, nullptr, 0
            );

            if (PQresultStatus(result) == PGRES_COMMAND_OK) {
                nlohmann::json citation = {
                    {"citation_id", citation_id},
                    {"document_title", source.value("title", "Untitled Document")},
                    {"document_source", source.value("source_type", "knowledge_base")},
                    {"relevance_score", source.value("relevance_score", 0.0)},
                    {"cited_at", std::chrono::system_clock::now().time_since_epoch().count()}
                };
                citations.push_back(citation);
            } else {
                logger_->log(LogLevel::ERROR, "Failed to store citation: " + std::string(PQresultErrorMessage(result)));
            }

            PQclear(result);
        }

        logger_->log(LogLevel::INFO, "Stored {} citations for message {}", citations.size(), message_id);

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
            logger_->log(LogLevel::WARN, "High-risk regulatory query processed",
                {
                    {"session_id", session_id},
                    {"user_id", request.user_id},
                    {"query_type", request.query_context.query_type},
                    {"regulatory_domain", request.query_context.regulatory_domain},
                    {"risk_level", request.query_context.risk_level},
                    {"confidence_score", response.confidence_score}
                });
        }

        return true;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in store_audit_trail: " + std::string(e.what()));
        return false;
    }
}

std::string RegulatoryChatbotService::create_session(const std::string& user_id, const RegulatoryQueryContext& context) {
    try {
        auto conn = db_conn_->get_connection();
        if (!conn) {
            logger_->log(LogLevel::ERROR, "Database connection failed in create_session");
            return "";
        }

        std::string session_id = generate_uuid();
        std::string title = "Regulatory Consultation: " + context.regulatory_domain + " (" + context.jurisdiction + ")";

        nlohmann::json metadata = {
            {"regulatory_domain", context.regulatory_domain},
            {"jurisdiction", context.jurisdiction},
            {"query_type", context.query_type},
            {"risk_level", context.risk_level},
            {"audit_mode", audit_trail_enabled_},
            {"model", default_model_}
        };

        const char* session_params[6] = {
            session_id.c_str(),
            user_id.c_str(),
            title.c_str(),
            context.regulatory_domain.c_str(),
            context.jurisdiction.c_str(),
            metadata.dump().c_str()
        };

        PGresult* result = PQexecParams(
            conn,
            "INSERT INTO chatbot_sessions "
            "(session_id, user_id, session_title, regulatory_domain, jurisdiction, session_metadata) "
            "VALUES ($1, $2, $3, $4, $5, $6::jsonb)",
            6, nullptr, session_params, nullptr, nullptr, 0
        );

        if (PQresultStatus(result) == PGRES_COMMAND_OK) {
            logger_->log(LogLevel::INFO, "Created regulatory chatbot session {} for user {}", session_id, user_id);
            PQclear(result);
            return session_id;
        } else {
            logger_->log(LogLevel::ERROR, "Failed to create session: " + std::string(PQresultErrorMessage(result)));
            PQclear(result);
            return "";
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in create_session: " + std::string(e.what()));
        return "";
    }
}

void RegulatoryChatbotService::store_regulatory_message(
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
        auto conn = db_conn_->get_connection();
        if (!conn) {
            logger_->log(LogLevel::ERROR, "Database connection failed in store_regulatory_message");
            return;
        }

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

        std::string sources_str = sources ? sources->dump() : "null";
        std::string citations_str = citations ? citations->dump() : "null";

        const char* message_params[8] = {
            message_id.c_str(),
            session_id.c_str(),
            role.c_str(),
            content.c_str(),
            std::to_string(confidence_score).c_str(),
            sources_str.c_str(),
            citations_str.c_str(),
            message_metadata.dump().c_str()
        };

        PGresult* result = PQexecParams(
            conn,
            "INSERT INTO chatbot_messages "
            "(message_id, session_id, role, content, confidence_score, sources, citations, message_metadata) "
            "VALUES ($1, $2, $3, $4, $5::decimal, $6::jsonb, $7::jsonb, $8::jsonb)",
            8, nullptr, message_params, nullptr, nullptr, 0
        );

        if (PQresultStatus(result) != PGRES_COMMAND_OK) {
            logger_->log(LogLevel::ERROR, "Failed to store message: " + std::string(PQresultErrorMessage(result)));
        }

        PQclear(result);

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in store_regulatory_message: " + std::string(e.what()));
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
    // Implementation for logging regulation access - could be extended for detailed audit trails
    logger_->log(LogLevel::INFO, "Regulatory domain accessed in session",
        {{"session_id", session_id}, {"regulation_code", regulation_code}});
    return true;
}

double RegulatoryChatbotService::calculate_message_cost(
    const std::string& model,
    int input_tokens,
    int output_tokens
) {
    // Simplified cost calculation - should be updated with actual pricing
    double input_cost_per_token = 0.0000015;  // Approximate for GPT-4
    double output_cost_per_token = 0.000002;

    return (input_tokens * input_cost_per_token) + (output_tokens * output_cost_per_token);
}

// Placeholder implementations for remaining methods
std::optional<RegulatoryChatbotSession> RegulatoryChatbotService::get_session(const std::string& session_id) {
    // Implementation would query database for session details
    return std::nullopt;
}

std::vector<RegulatoryChatbotSession> RegulatoryChatbotService::get_user_sessions(const std::string& user_id, int limit) {
    // Implementation would query database for user sessions
    return {};
}

bool RegulatoryChatbotService::archive_session(const std::string& session_id) {
    // Implementation would update session status in database
    return true;
}

bool RegulatoryChatbotService::update_session_activity(const std::string& session_id) {
    // Implementation would update last_activity_at in database
    return true;
}

std::vector<RegulatoryChatbotMessage> RegulatoryChatbotService::get_session_messages(
    const std::string& session_id,
    int limit,
    int offset
) {
    // Implementation would query database for session messages
    return {};
}

bool RegulatoryChatbotService::submit_feedback(
    const std::string& message_id,
    const std::string& feedback_type,
    const std::optional<std::string>& comments
) {
    // Implementation would update message feedback in database
    return true;
}

} // namespace chatbot
} // namespace regulens
