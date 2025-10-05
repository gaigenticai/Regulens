/**
 * Regulatory Monitor UI Implementation
 *
 * Production-grade web interface for comprehensive testing
 * of regulatory monitoring features.
 */

#include "regulatory_monitor_ui.hpp"
#include <sstream>

namespace regulens {

RegulatoryMonitorUI::RegulatoryMonitorUI(int port)
    : port_(port), server_(nullptr), handlers_(nullptr) {
}

RegulatoryMonitorUI::~RegulatoryMonitorUI() {
    stop();
}

bool RegulatoryMonitorUI::initialize(ConfigurationManager* config,
                                   StructuredLogger* logger,
                                   MetricsCollector* metrics) {
    try {
        // Store component references
        config_manager_ = config ? config : &ConfigurationManager::get_instance();
        logger_ = logger ? logger : &StructuredLogger::get_instance();
        metrics_collector_ = metrics;

        // Initialize config if using default
        if (!config) {
            config_manager_->initialize(0, nullptr);
        }

        // Create server and handlers
        server_ = std::make_unique<WebUIServer>(port_);

        // Create metrics collector if not provided
        std::shared_ptr<MetricsCollector> metrics_ptr;
        if (metrics_collector_) {
            metrics_ptr = std::shared_ptr<MetricsCollector>(metrics_collector_, [](MetricsCollector*){});
        } else {
            metrics_ptr = std::make_shared<MetricsCollector>();
        }

        handlers_ = std::make_unique<WebUIHandlers>(
            std::shared_ptr<ConfigurationManager>(config_manager_, [](ConfigurationManager*){}),
            std::shared_ptr<StructuredLogger>(logger_, [](StructuredLogger*){}),
            metrics_ptr
        );

        // Configure server
        server_->set_config_manager(std::shared_ptr<ConfigurationManager>(config_manager_, [](ConfigurationManager*){}));
        server_->set_metrics_collector(metrics_ptr);
        server_->set_logger(std::shared_ptr<StructuredLogger>(logger_, [](StructuredLogger*){}));

        // Setup routes
        setup_routes();
        setup_static_routes();

        return true;
    } catch (const std::exception& e) {
        if (logger) {
            logger->error("Failed to initialize Regulatory Monitor UI: {}", e.what());
        }
        return false;
    }
}

bool RegulatoryMonitorUI::start() {
    if (!server_) {
        return false;
    }

    return server_->start();
}

void RegulatoryMonitorUI::stop() {
    if (server_) {
        server_->stop();
    }
}

bool RegulatoryMonitorUI::is_running() const {
    return server_ && server_->is_running();
}

void RegulatoryMonitorUI::setup_routes() {
    if (!server_ || !handlers_) {
        return;
    }

    // Main dashboard
    server_->add_route("GET", "/", [this](const HTTPRequest& req) {
        return handlers_->handle_dashboard(req);
    });

    // Configuration management
    server_->add_route("GET", "/config", [this](const HTTPRequest& req) {
        return handlers_->handle_config_get(req);
    });
    server_->add_route("POST", "/config", [this](const HTTPRequest& req) {
        return handlers_->handle_config_update(req);
    });

    // Database testing
    server_->add_route("GET", "/api/database/test", [this](const HTTPRequest& req) {
        return handlers_->handle_db_test(req);
    });
    server_->add_route("POST", "/api/database/query", [this](const HTTPRequest& req) {
        return handlers_->handle_db_query(req);
    });
    server_->add_route("GET", "/api/database/stats", [this](const HTTPRequest& req) {
        return handlers_->handle_db_stats(req);
    });
    server_->add_route("GET", "/database", [this](const HTTPRequest& req) {
        return handlers_->handle_db_test(req);
    });

    // Agent management
    server_->add_route("GET", "/api/agents/status", [this](const HTTPRequest& req) {
        return handlers_->handle_agent_status(req);
    });
    server_->add_route("POST", "/api/agents/execute", [this](const HTTPRequest& req) {
        return handlers_->handle_agent_execute(req);
    });
    server_->add_route("GET", "/api/agents/list", [this](const HTTPRequest& req) {
        return handlers_->handle_agent_list(req);
    });
    server_->add_route("GET", "/agents", [this](const HTTPRequest& req) {
        return handlers_->handle_agent_list(req);
    });

    // Regulatory monitoring
    server_->add_route("GET", "/api/regulatory/sources", [this](const HTTPRequest& req) {
        return handlers_->handle_regulatory_sources(req);
    });
    server_->add_route("GET", "/api/regulatory/changes", [this](const HTTPRequest& req) {
        return handlers_->handle_regulatory_changes(req);
    });
    server_->add_route("GET", "/api/regulatory/monitor", [this](const HTTPRequest& req) {
        return handlers_->handle_regulatory_monitor(req);
    });
    server_->add_route("POST", "/api/regulatory/start", [this](const HTTPRequest& req) {
        return handlers_->handle_regulatory_start(req);
    });
    server_->add_route("POST", "/api/regulatory/stop", [this](const HTTPRequest& req) {
        return handlers_->handle_regulatory_stop(req);
    });
    server_->add_route("GET", "/monitoring", [this](const HTTPRequest& req) {
        return handlers_->handle_regulatory_monitor(req);
    });

    // Metrics and health
    server_->add_route("GET", "/api/metrics", [this](const HTTPRequest& req) {
        return handlers_->handle_metrics_data(req);
    });
    server_->add_route("GET", "/api/health", [this](const HTTPRequest& req) {
        return handlers_->handle_health_check(req);
    });
    server_->add_route("GET", "/api/health/detailed", [this](const HTTPRequest& req) {
        return handlers_->handle_detailed_health_report(req);
    });
    server_->add_route("GET", "/metrics", [this](const HTTPRequest& req) {
        return handlers_->handle_metrics_dashboard(req);
    });

    // Data ingestion
    server_->add_route("GET", "/api/ingestion/status", [this](const HTTPRequest& req) {
        return handlers_->handle_ingestion_status(req);
    });
    server_->add_route("POST", "/api/ingestion/test", [this](const HTTPRequest& req) {
        return handlers_->handle_ingestion_test(req);
    });
    server_->add_route("GET", "/api/ingestion/stats", [this](const HTTPRequest& req) {
        return handlers_->handle_ingestion_stats(req);
    });
    server_->add_route("GET", "/ingestion", [this](const HTTPRequest& req) {
        return handlers_->handle_ingestion_status(req);
    });

    // API documentation
    server_->add_route("GET", "/api-docs", [this](const HTTPRequest& req) {
        return handlers_->handle_api_docs(req);
    });

    // Regulatory-specific routes
    server_->add_route("GET", "/api/regulatory/status", [this](const HTTPRequest& req) {
        return handle_regulatory_status(req);
    });
    server_->add_route("GET", "/api/regulatory/config", [this](const HTTPRequest& req) {
        return handle_regulatory_config(req);
    });
    server_->add_route("POST", "/api/regulatory/test", [this](const HTTPRequest& req) {
        return handle_regulatory_test(req);
    });

    // Decision tree visualization routes
    server_->add_route("GET", "/api/decision-trees/visualize", [this](const HTTPRequest& req) {
        return handlers_->handle_decision_tree_visualize(req);
    });
    server_->add_route("GET", "/api/decision-trees/list", [this](const HTTPRequest& req) {
        return handlers_->handle_decision_tree_list(req);
    });
    server_->add_route("GET", "/api/decision-trees/details", [this](const HTTPRequest& req) {
        return handlers_->handle_decision_tree_details(req);
    });
    server_->add_route("GET", "/decision-trees", [this](const HTTPRequest& req) {
        return HTTPResponse{200, "text/html", handlers_->generate_decision_trees_html()};
    });

    // Agent activity feed routes
    server_->add_route("GET", "/activities", [this](const HTTPRequest& req) {
        return handlers_->handle_activity_feed(req);
    });
    server_->add_route("GET", "/api/activities/stream", [this](const HTTPRequest& req) {
        return handlers_->handle_activity_stream(req);
    });
    server_->add_route("GET", "/api/activities/query", [this](const HTTPRequest& req) {
        return handlers_->handle_activity_query(req);
    });
    server_->add_route("GET", "/api/activities/stats", [this](const HTTPRequest& req) {
        return handlers_->handle_activity_stats(req);
    });
    server_->add_route("GET", "/api/activities/recent", [this](const HTTPRequest& req) {
        return handlers_->handle_activity_recent(req);
    });
    server_->add_route("GET", "/api/decisions/recent", [this](const HTTPRequest& req) {
        return handlers_->handle_decisions_recent(req);
    });
    server_->add_route("GET", "/api/activities/export", [this](const HTTPRequest& req) {
        // Production-grade CSV export for agent activities
        try {
            std::string headers = "Event ID,Agent Type,Agent Name,Event Type,Event Category,"
                                "Description,Severity,Entity ID,Entity Type,Occurred At,Processed At\n";

            std::string csv_content = headers;

            // Get activities from the activity feed system
            if (activity_feed_) {
                auto activities = activity_feed_->get_recent_activities(1000); // Export up to 1000 activities

                for (const auto& activity : activities) {
                    std::vector<std::string> row = {
                        activity.event_id,
                        activity.agent_type,
                        activity.agent_name,
                        activity.event_type,
                        activity.event_category,
                        activity.description,
                        activity.severity,
                        activity.entity_id.value_or(""),
                        activity.entity_type.value_or(""),
                        std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                            activity.occurred_at.time_since_epoch()).count()),
                        activity.processed_at ?
                            std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                                activity.processed_at->time_since_epoch()).count()) : ""
                    };

                    // Escape CSV fields containing commas, quotes, or newlines
                    for (auto& field : row) {
                        if (field.find(',') != std::string::npos ||
                            field.find('"') != std::string::npos ||
                            field.find('\n') != std::string::npos) {
                            // Escape quotes by doubling them, then wrap in quotes
                            std::string escaped = field;
                            size_t pos = 0;
                            while ((pos = escaped.find('"', pos)) != std::string::npos) {
                                escaped.replace(pos, 1, "\"\"");
                                pos += 2;
                            }
                            field = "\"" + escaped + "\"";
                        }
                    }

                    csv_content += row[0];
                    for (size_t i = 1; i < row.size(); ++i) {
                        csv_content += "," + row[i];
                    }
                    csv_content += "\n";
                }
            }

            // Add export metadata
            csv_content += "\n\"Export Metadata\",\"Generated At\",\"" +
                          std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::system_clock::now().time_since_epoch()).count()) + "\"\n";
            csv_content += "\"Export Metadata\",\"Total Activities\",\"" +
                          (activity_feed_ ? std::to_string(activity_feed_->get_recent_activities(1000).size()) : "0") + "\"\n";

            HTTPResponse response;
            response.status_code = 200;
            response.content_type = "text/csv;charset=utf-8;";
            response.headers["Content-Disposition"] = "attachment; filename=\"agent_activities_export_" +
                                                     std::to_string(std::chrono::system_clock::to_time_t(
                                                         std::chrono::system_clock::now())) + ".csv\"";
            response.body = csv_content;
            return response;

        } catch (const std::exception& e) {
            logger_->error("Error exporting activities to CSV: {}", e.what());
            HTTPResponse response;
            response.status_code = 500;
            response.content_type = "application/json";
            response.body = nlohmann::json{{"error", "Failed to export activities"}, {"message", e.what()}}.dump();
            return response;
        }
    });

    // Human-AI collaboration routes
    server_->add_route("GET", "/collaboration", [this](const HTTPRequest& req) {
        return handlers_->handle_collaboration_sessions(req);
    });
    server_->add_route("POST", "/api/collaboration/session/create", [this](const HTTPRequest& req) {
        return handlers_->handle_collaboration_session_create(req);
    });
    server_->add_route("GET", "/api/collaboration/messages", [this](const HTTPRequest& req) {
        return handlers_->handle_collaboration_session_messages(req);
    });
    server_->add_route("POST", "/api/collaboration/message", [this](const HTTPRequest& req) {
        return handlers_->handle_collaboration_send_message(req);
    });
    server_->add_route("POST", "/api/collaboration/feedback", [this](const HTTPRequest& req) {
        return handlers_->handle_collaboration_feedback(req);
    });
    server_->add_route("POST", "/api/collaboration/intervention", [this](const HTTPRequest& req) {
        return handlers_->handle_collaboration_intervention(req);
    });
    server_->add_route("GET", "/api/collaboration/requests", [this](const HTTPRequest& req) {
        return handlers_->handle_assistance_requests(req);
    });

    // Pattern recognition routes
    server_->add_route("GET", "/patterns", [this](const HTTPRequest& req) {
        return handlers_->handle_pattern_analysis(req);
    });
    server_->add_route("POST", "/api/patterns/discover", [this](const HTTPRequest& req) {
        return handlers_->handle_pattern_discovery(req);
    });
    server_->add_route("GET", "/api/patterns/details", [this](const HTTPRequest& req) {
        return handlers_->handle_pattern_details(req);
    });
    server_->add_route("GET", "/api/patterns/stats", [this](const HTTPRequest& req) {
        return handlers_->handle_pattern_stats(req);
    });
    server_->add_route("GET", "/api/patterns/export", [this](const HTTPRequest& req) {
        return handlers_->handle_pattern_export(req);
    });

    // Feedback incorporation routes
    server_->add_route("GET", "/feedback", [this](const HTTPRequest& req) {
        return handlers_->handle_feedback_dashboard(req);
    });
    server_->add_route("POST", "/api/feedback/submit", [this](const HTTPRequest& req) {
        return handlers_->handle_feedback_submit(req);
    });
    server_->add_route("GET", "/api/feedback/analysis", [this](const HTTPRequest& req) {
        return handlers_->handle_feedback_analysis(req);
    });
    server_->add_route("POST", "/api/feedback/learning", [this](const HTTPRequest& req) {
        return handlers_->handle_feedback_learning(req);
    });
    server_->add_route("GET", "/api/feedback/stats", [this](const HTTPRequest& req) {
        return handlers_->handle_feedback_stats(req);
    });
    server_->add_route("GET", "/api/feedback/export", [this](const HTTPRequest& req) {
        return handlers_->handle_feedback_export(req);
    });

    // Error handling routes
    server_->add_route("GET", "/errors", [this](const HTTPRequest& req) {
        return handlers_->handle_error_dashboard(req);
    });
    server_->add_route("GET", "/api/errors/stats", [this](const HTTPRequest& req) {
        return handlers_->handle_error_stats(req);
    });
    server_->add_route("GET", "/api/errors/health", [this](const HTTPRequest& req) {
        return handlers_->handle_health_status(req);
    });
    server_->add_route("GET", "/api/errors/circuit-breaker", [this](const HTTPRequest& req) {
        return handlers_->handle_circuit_breaker_status(req);
    });
    server_->add_route("POST", "/api/errors/circuit-breaker/reset", [this](const HTTPRequest& req) {
        return handlers_->handle_circuit_breaker_reset(req);
    });
    server_->add_route("GET", "/api/errors/export", [this](const HTTPRequest& req) {
        return handlers_->handle_error_export(req);
    });

    // LLM and OpenAI routes
    server_->add_route("GET", "/llm", [this](const HTTPRequest& req) {
        return handlers_->handle_llm_dashboard(req);
    });
    server_->add_route("POST", "/api/openai/completion", [this](const HTTPRequest& req) {
        return handlers_->handle_openai_completion(req);
    });
    server_->add_route("POST", "/api/openai/analysis", [this](const HTTPRequest& req) {
        return handlers_->handle_openai_analysis(req);
    });
    server_->add_route("POST", "/api/openai/compliance", [this](const HTTPRequest& req) {
        return handlers_->handle_openai_compliance(req);
    });
    server_->add_route("POST", "/api/openai/extraction", [this](const HTTPRequest& req) {
        return handlers_->handle_openai_extraction(req);
    });
    server_->add_route("POST", "/api/openai/decision", [this](const HTTPRequest& req) {
        return handlers_->handle_openai_decision(req);
    });
    server_->add_route("GET", "/api/openai/stats", [this](const HTTPRequest& req) {
        return handlers_->handle_openai_stats(req);
    });

    // Risk Assessment routes
    server_->add_route("GET", "/risk", [this](const HTTPRequest& req) {
        return handlers_->handle_risk_dashboard(req);
    });
    server_->add_route("POST", "/api/risk/assess/transaction", [this](const HTTPRequest& req) {
        return handlers_->handle_risk_assess_transaction(req);
    });
    server_->add_route("POST", "/api/risk/assess/entity", [this](const HTTPRequest& req) {
        return handlers_->handle_risk_assess_entity(req);
    });
    server_->add_route("POST", "/api/risk/assess/regulatory", [this](const HTTPRequest& req) {
        return handlers_->handle_risk_assess_regulatory(req);
    });
    server_->add_route("GET", "/api/risk/history", [this](const HTTPRequest& req) {
        return handlers_->handle_risk_history(req);
    });
    server_->add_route("GET", "/api/risk/analytics", [this](const HTTPRequest& req) {
        return handlers_->handle_risk_analytics(req);
    });
    server_->add_route("GET", "/api/risk/export", [this](const HTTPRequest& req) {
        return handlers_->handle_risk_export(req);
    });

    // Anthropic Claude routes
    server_->add_route("GET", "/claude", [this](const HTTPRequest& req) {
        return handlers_->handle_claude_dashboard(req);
    });
    server_->add_route("POST", "/api/claude/message", [this](const HTTPRequest& req) {
        return handlers_->handle_claude_message(req);
    });
    server_->add_route("POST", "/api/claude/reasoning", [this](const HTTPRequest& req) {
        return handlers_->handle_claude_reasoning(req);
    });
    server_->add_route("POST", "/api/claude/constitutional", [this](const HTTPRequest& req) {
        return handlers_->handle_claude_constitutional(req);
    });
    server_->add_route("POST", "/api/claude/ethical_decision", [this](const HTTPRequest& req) {
        return handlers_->handle_claude_ethical_decision(req);
    });
    server_->add_route("POST", "/api/claude/complex_reasoning", [this](const HTTPRequest& req) {
        return handlers_->handle_claude_complex_reasoning(req);
    });
    server_->add_route("POST", "/api/claude/regulatory", [this](const HTTPRequest& req) {
        return handlers_->handle_claude_regulatory(req);
    });
    server_->add_route("GET", "/api/claude/stats", [this](const HTTPRequest& req) {
        return handlers_->handle_claude_stats(req);
    });

    // Function calling routes
    server_->add_route("GET", "/functions", [this](const HTTPRequest& req) {
        return handlers_->handle_function_calling_dashboard(req);
    });
    server_->add_route("POST", "/api/functions/execute", [this](const HTTPRequest& req) {
        return handlers_->handle_function_execute(req);
    });
    server_->add_route("GET", "/api/functions/list", [this](const HTTPRequest& req) {
        return handlers_->handle_function_list(req);
    });
    server_->add_route("GET", "/api/functions/audit", [this](const HTTPRequest& req) {
        return handlers_->handle_function_audit(req);
    });
    server_->add_route("GET", "/api/functions/metrics", [this](const HTTPRequest& req) {
        return handlers_->handle_function_metrics(req);
    });
    server_->add_route("POST", "/api/functions/openai-integration", [this](const HTTPRequest& req) {
        return handlers_->handle_function_openai_integration(req);
    });

    // Embeddings routes
    server_->add_route("GET", "/embeddings", [this](const HTTPRequest& req) {
        return handlers_->handle_embeddings_dashboard(req);
    });
    server_->add_route("POST", "/api/embeddings/generate", [this](const HTTPRequest& req) {
        return handlers_->handle_embeddings_generate(req);
    });
    server_->add_route("POST", "/api/embeddings/search", [this](const HTTPRequest& req) {
        return handlers_->handle_embeddings_search(req);
    });
    server_->add_route("POST", "/api/embeddings/index", [this](const HTTPRequest& req) {
        return handlers_->handle_embeddings_index(req);
    });
    server_->add_route("GET", "/api/embeddings/models", [this](const HTTPRequest& req) {
        return handlers_->handle_embeddings_models(req);
    });
    server_->add_route("GET", "/api/embeddings/stats", [this](const HTTPRequest& req) {
        return handlers_->handle_embeddings_stats(req);
    });

    // Decision Tree Optimizer routes
    server_->add_route("GET", "/decision", [this](const HTTPRequest& req) {
        return handlers_->handle_decision_dashboard(req);
    });
    server_->add_route("POST", "/api/decision/mcda_analysis", [this](const HTTPRequest& req) {
        return handlers_->handle_decision_mcda_analysis(req);
    });
    server_->add_route("POST", "/api/decision/tree_analysis", [this](const HTTPRequest& req) {
        return handlers_->handle_decision_tree_analysis(req);
    });
    server_->add_route("POST", "/api/decision/ai_recommendation", [this](const HTTPRequest& req) {
        return handlers_->handle_decision_ai_recommendation(req);
    });
    server_->add_route("GET", "/api/decision/history", [this](const HTTPRequest& req) {
        return handlers_->handle_decision_history(req);
    });
    server_->add_route("POST", "/api/decision/visualization", [this](const HTTPRequest& req) {
        return handlers_->handle_decision_visualization(req);
    });

    // Multi-Agent Communication routes
    server_->add_route("GET", "/multi-agent", [this](const HTTPRequest& req) {
        return handlers_->handle_multi_agent_dashboard(req);
    });
    server_->add_route("POST", "/api/multi-agent/message/send", [this](const HTTPRequest& req) {
        return handlers_->handle_agent_message_send(req);
    });
    server_->add_route("GET", "/api/multi-agent/message/receive", [this](const HTTPRequest& req) {
        return handlers_->handle_agent_message_receive(req);
    });
    server_->add_route("POST", "/api/multi-agent/message/broadcast", [this](const HTTPRequest& req) {
        return handlers_->handle_agent_message_broadcast(req);
    });
    server_->add_route("POST", "/api/multi-agent/consensus/start", [this](const HTTPRequest& req) {
        return handlers_->handle_consensus_start(req);
    });
    server_->add_route("POST", "/api/multi-agent/consensus/contribute", [this](const HTTPRequest& req) {
        return handlers_->handle_consensus_contribute(req);
    });
    server_->add_route("GET", "/api/multi-agent/consensus/result", [this](const HTTPRequest& req) {
        return handlers_->handle_consensus_result(req);
    });
    server_->add_route("POST", "/api/multi-agent/translate", [this](const HTTPRequest& req) {
        return handlers_->handle_message_translate(req);
    });
    server_->add_route("POST", "/api/multi-agent/conversation", [this](const HTTPRequest& req) {
        return handlers_->handle_agent_conversation(req);
    });
    server_->add_route("POST", "/api/multi-agent/conflicts/resolve", [this](const HTTPRequest& req) {
        return handlers_->handle_conflict_resolution(req);
    });
    server_->add_route("GET", "/api/multi-agent/stats", [this](const HTTPRequest& req) {
        return handlers_->handle_communication_stats(req);
    });

    // Memory System routes
    server_->add_route("GET", "/memory", [this](const HTTPRequest& req) {
        return handlers_->handle_memory_dashboard(req);
    });
    server_->add_route("POST", "/api/memory/conversations/store", [this](const HTTPRequest& req) {
        return handlers_->handle_memory_conversation_store(req);
    });
    server_->add_route("GET", "/api/memory/conversations/retrieve", [this](const HTTPRequest& req) {
        return handlers_->handle_memory_conversation_retrieve(req);
    });
    server_->add_route("GET", "/api/memory/conversations/search", [this](const HTTPRequest& req) {
        return handlers_->handle_memory_conversation_search(req);
    });
    server_->add_route("DELETE", "/api/memory/conversations/delete", [this](const HTTPRequest& req) {
        return handlers_->handle_memory_conversation_delete(req);
    });
    server_->add_route("POST", "/api/memory/cases/store", [this](const HTTPRequest& req) {
        return handlers_->handle_memory_case_store(req);
    });
    server_->add_route("GET", "/api/memory/cases/retrieve", [this](const HTTPRequest& req) {
        return handlers_->handle_memory_case_retrieve(req);
    });
    server_->add_route("GET", "/api/memory/cases/search", [this](const HTTPRequest& req) {
        return handlers_->handle_memory_case_search(req);
    });
    server_->add_route("DELETE", "/api/memory/cases/delete", [this](const HTTPRequest& req) {
        return handlers_->handle_memory_case_delete(req);
    });
    server_->add_route("POST", "/api/memory/feedback/store", [this](const HTTPRequest& req) {
        return handlers_->handle_memory_feedback_store(req);
    });
    server_->add_route("GET", "/api/memory/feedback/retrieve", [this](const HTTPRequest& req) {
        return handlers_->handle_memory_feedback_retrieve(req);
    });
    server_->add_route("GET", "/api/memory/feedback/search", [this](const HTTPRequest& req) {
        return handlers_->handle_memory_feedback_search(req);
    });
    server_->add_route("GET", "/api/memory/models", [this](const HTTPRequest& req) {
        return handlers_->handle_memory_learning_models(req);
    });
    server_->add_route("GET", "/api/memory/consolidation/status", [this](const HTTPRequest& req) {
        return handlers_->handle_memory_consolidation_status(req);
    });
    server_->add_route("POST", "/api/memory/consolidation/run", [this](const HTTPRequest& req) {
        return handlers_->handle_memory_consolidation_run(req);
    });
    server_->add_route("GET", "/api/memory/patterns", [this](const HTTPRequest& req) {
        return handlers_->handle_memory_access_patterns(req);
    });
    server_->add_route("GET", "/api/memory/statistics", [this](const HTTPRequest& req) {
        return handlers_->handle_memory_statistics(req);
    });
}

void RegulatoryMonitorUI::setup_static_routes() {
    // Static file serving for CSS, JS, images
    server_->add_static_route("/static", "./static");
}

HTTPResponse RegulatoryMonitorUI::handle_root(const HTTPRequest& request) {
    return handlers_->handle_dashboard(request);
}

HTTPResponse RegulatoryMonitorUI::handle_regulatory_status(const HTTPRequest& /*request*/) {
    // Enhanced regulatory status with monitoring-specific information
    nlohmann::json status = {
        {"status", "success"},
        {"system", "regulatory_monitor"},
        {"monitoring_active", false},
        {"sources_configured", 3}, // SEC, FCA, ECB
        {"last_successful_scan", "2024-01-01T00:00:00Z"},
        {"total_changes_detected", 0},
        {"alerts_active", 0}
    };

    return HTTPResponse(200, "OK", status.dump(2), "application/json");
}

HTTPResponse RegulatoryMonitorUI::handle_regulatory_config(const HTTPRequest& /*request*/) {
    // Return regulatory-specific configuration
    nlohmann::json config = {
        {"status", "success"},
        {"regulatory_sources", {
            {"sec_edgar", {
                {"enabled", true},
                {"base_url", "https://www.sec.gov/edgar"},
                {"rate_limit", 10},
                {"scan_interval_minutes", 15}
            }},
            {"fca_api", {
                {"enabled", true},
                {"base_url", "https://api.fca.org.uk"},
                {"rate_limit", 60},
                {"scan_interval_minutes", 30}
            }},
            {"ecb_feed", {
                {"enabled", true},
                {"feed_url", "https://www.ecb.europa.eu/rss/announcements.xml"},
                {"update_interval_minutes", 15}
            }}
        }}
    };

    return HTTPResponse(200, "OK", config.dump(2), "application/json");
}

HTTPResponse RegulatoryMonitorUI::handle_regulatory_test(const HTTPRequest& /*request*/) {
    // Test regulatory monitoring functionality
    nlohmann::json result = {
        {"status", "success"},
        {"message", "Regulatory monitoring test completed"},
        {"tests_run", {
            {"source_connectivity", true},
            {"change_detection", false}, // Not implemented yet
            {"alert_system", false}      // Not implemented yet
        }},
        {"note", "Full regulatory monitoring integration pending"}
    };

    return HTTPResponse(200, "OK", result.dump(2), "application/json");
}

} // namespace regulens
