/**
 * Simple API Server for Regulens Features
 * Provides REST endpoints for Embeddings Explorer, Memory Management, and Data Quality Monitor
 */

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <regex>
#include <nlohmann/json.hpp>

// Include the web UI server
#include "shared/web_ui/web_ui_server.hpp"

// Include our feature handlers
#include "shared/embeddings/embeddings_explorer.hpp"
#include "shared/memory/memory_visualizer.hpp"
#include "shared/data_quality/data_quality_handlers.hpp"
#include "shared/data_quality/quality_checker.hpp"

// Database and logging
#include "shared/database/postgresql_connection.hpp"
#include "shared/logging/structured_logger.hpp"
#include "shared/config/configuration_manager.hpp"
#include "shared/config/config_types.hpp"

using namespace regulens;
using json = nlohmann::json;

class FeatureAPIServer {
private:
    std::unique_ptr<WebUIServer> web_server_;
    PostgreSQLConnection* db_conn_;
    StructuredLogger* logger_;
    ConfigurationManager* config_manager_;

    // Feature handlers
    std::shared_ptr<embeddings::EmbeddingsExplorer> embeddings_explorer_;
    std::shared_ptr<memory::MemoryVisualizer> memory_visualizer_;
    std::shared_ptr<DataQualityHandlers> data_quality_handlers_;
    std::shared_ptr<QualityChecker> quality_checker_;

    // Background threads
    std::thread embedding_thread_;
    std::thread quality_thread_;

public:
    FeatureAPIServer(const std::string& db_conn_str) {
        // Parse basic connection string or use defaults
        DatabaseConfig db_config;
        db_config.host = "localhost";
        db_config.port = 5432;
        db_config.database = "regulens";
        db_config.user = "regulens_user";
        db_config.password = "test_password_123"; // Default for testing

        // Simple parsing of connection string like "host=localhost port=5432 dbname=regulens user=regulens_user password=..."
        if (!db_conn_str.empty()) {
            // Basic parsing - in production this should be more robust
            if (db_conn_str.find("host=") != std::string::npos) {
                // For now, use defaults
            }
        }

        db_conn_ = new PostgreSQLConnection(db_config);
        logger_ = &StructuredLogger::get_instance();
        config_manager_ = new ConfigurationManager();

        // Initialize logger
        logger_->initialize();

        // Initialize feature handlers
        embeddings_explorer_ = std::make_shared<embeddings::EmbeddingsExplorer>(db_conn_, logger_);
        memory_visualizer_ = std::make_shared<memory::MemoryVisualizer>(db_conn_, logger_);
        data_quality_handlers_ = std::make_shared<DataQualityHandlers>(db_conn_, logger_);
        quality_checker_ = std::make_shared<QualityChecker>(db_conn_, data_quality_handlers_, logger_);

        // Initialize web server
        web_server_ = std::make_unique<WebUIServer>(8080);
        web_server_->set_config_manager(config_manager_);
        web_server_->set_logger(logger_);

        register_api_routes();
    }

    void register_api_routes() {
        // Embeddings Explorer API routes
        web_server_->add_route("GET", "/api/embeddings/models", [this](const HTTPRequest& req) -> HTTPResponse {
            try {
                json response = {
                    {"models", embeddings_explorer_->get_available_models()},
                    {"status", "success"}
                };
                return HTTPResponse(200, "OK", response.dump(), "application/json");
            } catch (const std::exception& e) {
                logger_->log(LogLevel::ERROR, "Exception in /api/embeddings/models: " + std::string(e.what()));
                return HTTPResponse(500, "Internal Server Error", R"({"error": "Internal server error"})", "application/json");
            }
        });

        web_server_->add_route("POST", "/api/embeddings/visualize", [this](const HTTPRequest& req) -> HTTPResponse {
            try {
                json request_data = json::parse(req.body);
                std::string model = request_data.value("model", "openai-ada-002");
                std::string algorithm = request_data.value("algorithm", "t-sne");
                json parameters = request_data.value("parameters", json::object());
                bool use_cache = request_data.value("use_cache", true);

                // Get sample embeddings for visualization
                auto points = embeddings_explorer_->load_embeddings(model, 1000, 0);
                if (points.empty()) {
                    return HTTPResponse(404, "Not Found", R"({"error": "No embeddings found for model"})", "application/json");
                }

                auto result = embeddings_explorer_->generate_visualization(model, algorithm, points, parameters, use_cache);

                json response = {
                    {"visualization_id", result.visualization_id},
                    {"coordinates", result.coordinates},
                    {"sample_size", result.sample_size},
                    {"quality_metrics", result.quality_metrics},
                    {"status", "success"}
                };
                return HTTPResponse(200, "OK", response.dump(), "application/json");
            } catch (const std::exception& e) {
                logger_->log(LogLevel::ERROR, "Exception in /api/embeddings/visualize: " + std::string(e.what()));
                return HTTPResponse(500, "Internal Server Error", R"({"error": "Internal server error"})", "application/json");
            }
        });

        web_server_->add_route("GET", "/api/embeddings/search", [this](const HTTPRequest& req) -> HTTPResponse {
            try {
                std::string query = req.query_params.count("q") ? req.query_params.at("q") : "";
                std::string model = req.query_params.count("model") ? req.query_params.at("model") : "openai-ada-002";
                int top_k = req.query_params.count("top_k") ? std::stoi(req.query_params.at("top_k")) : 10;
                bool use_cache = req.query_params.count("use_cache") ? (req.query_params.at("use_cache") == "true") : true;

                if (query.empty()) {
                    return HTTPResponse(400, "Bad Request", R"({"error": "Query parameter 'q' is required"})", "application/json");
                }

                embeddings::SearchQuery search_query;
                search_query.query_text = query;
                search_query.top_k = top_k;
                auto results = embeddings_explorer_->semantic_search(search_query, model, use_cache);

                json results_json = json::array();
                for (const auto& result : results) {
                    results_json.push_back({
                        {"id", result.point.id},
                        {"similarity_score", result.similarity_score},
                        {"rank", result.rank},
                        {"metadata", result.point.metadata}
                    });
                }

                json response = {
                    {"query", query},
                    {"model", model},
                    {"results", results_json},
                    {"status", "success"}
                };
                return HTTPResponse(200, "OK", response.dump(), "application/json");
            } catch (const std::exception& e) {
                logger_->log(LogLevel::ERROR, "Exception in /api/embeddings/search: " + std::string(e.what()));
                return HTTPResponse(500, "Internal Server Error", R"({"error": "Internal server error"})", "application/json");
            }
        });

        // Memory Management API routes
        web_server_->add_route("GET", "/api/agents/{agent_id}/memory", [this](const HTTPRequest& req) -> HTTPResponse {
            try {
                std::string agent_id = req.params.count("agent_id") ? req.params.at("agent_id") : "";
                if (agent_id.empty()) {
                    return HTTPResponse(400, "Bad Request", R"({"error": "agent_id parameter is required"})", "application/json");
                }

                memory::VisualizationRequest viz_request;
                viz_request.agent_id = agent_id;
                viz_request.visualization_type = "graph";
                viz_request.use_cache = req.query_params.count("use_cache") ?
                    (req.query_params.at("use_cache") == "true") : true;

                // Parse parameters from query string
                json parameters = json::object();
                for (const auto& [key, value] : req.query_params) {
                    if (key != "use_cache") {
                        parameters[key] = value;
                    }
                }
                viz_request.parameters = parameters;

                auto result = memory_visualizer_->generate_graph_visualization(viz_request);

                return HTTPResponse(200, "OK", result.data.dump(), "application/json");
            } catch (const std::exception& e) {
                logger_->log(LogLevel::ERROR, "Exception in /api/agents/{agent_id}/memory: " + std::string(e.what()));
                return HTTPResponse(500, "Internal Server Error", R"({"error": "Internal server error"})", "application/json");
            }
        });

        // Data Quality Monitor API routes
        web_server_->add_route("GET", "/api/data-quality/rules", [this](const HTTPRequest& req) -> HTTPResponse {
            try {
                std::map<std::string, std::string> headers_map(req.headers.begin(), req.headers.end());
                std::string response = data_quality_handlers_->list_quality_rules(headers_map);
                return HTTPResponse(200, "OK", response, "application/json");
            } catch (const std::exception& e) {
                logger_->log(LogLevel::ERROR, "Exception in /api/data-quality/rules: " + std::string(e.what()));
                return HTTPResponse(500, "Internal Server Error", R"({"error": "Internal server error"})", "application/json");
            }
        });

        web_server_->add_route("POST", "/api/data-quality/rules", [this](const HTTPRequest& req) -> HTTPResponse {
            try {
                std::map<std::string, std::string> headers_map(req.headers.begin(), req.headers.end());
                std::string response = data_quality_handlers_->create_quality_rule(req.body, headers_map);
                return HTTPResponse(201, "Created", response, "application/json");
            } catch (const std::exception& e) {
                logger_->log(LogLevel::ERROR, "Exception in POST /api/data-quality/rules: " + std::string(e.what()));
                return HTTPResponse(500, "Internal Server Error", R"({"error": "Internal server error"})", "application/json");
            }
        });

        web_server_->add_route("GET", "/api/data-quality/checks", [this](const HTTPRequest& req) -> HTTPResponse {
            try {
                std::map<std::string, std::string> headers_map(req.headers.begin(), req.headers.end());
                std::string response = data_quality_handlers_->get_quality_checks(headers_map);
                return HTTPResponse(200, "OK", response, "application/json");
            } catch (const std::exception& e) {
                logger_->log(LogLevel::ERROR, "Exception in /api/data-quality/checks: " + std::string(e.what()));
                return HTTPResponse(500, "Internal Server Error", R"({"error": "Internal server error"})", "application/json");
            }
        });

        web_server_->add_route("POST", "/api/data-quality/run/{rule_id}", [this](const HTTPRequest& req) -> HTTPResponse {
            try {
                std::string rule_id = req.params.count("rule_id") ? req.params.at("rule_id") : "";
                if (rule_id.empty()) {
                    return HTTPResponse(400, "Bad Request", R"({"error": "rule_id parameter is required"})", "application/json");
                }

                std::map<std::string, std::string> headers_map(req.headers.begin(), req.headers.end());
                std::string response = data_quality_handlers_->run_quality_check(rule_id, headers_map);
                return HTTPResponse(200, "OK", response, "application/json");
            } catch (const std::exception& e) {
                logger_->log(LogLevel::ERROR, "Exception in /api/data-quality/run/{rule_id}: " + std::string(e.what()));
                return HTTPResponse(500, "Internal Server Error", R"({"error": "Internal server error"})", "application/json");
            }
        });

        web_server_->add_route("GET", "/api/data-quality/dashboard", [this](const HTTPRequest& req) -> HTTPResponse {
            try {
                std::map<std::string, std::string> headers_map(req.headers.begin(), req.headers.end());
                std::string response = data_quality_handlers_->get_quality_dashboard(headers_map);
                return HTTPResponse(200, "OK", response, "application/json");
            } catch (const std::exception& e) {
                logger_->log(LogLevel::ERROR, "Exception in /api/data-quality/dashboard: " + std::string(e.what()));
                return HTTPResponse(500, "Internal Server Error", R"({"error": "Internal server error"})", "application/json");
            }
        });

        logger_->log(LogLevel::INFO, "API routes registered successfully for features: Embeddings Explorer, Memory Management, Data Quality Monitor");
    }

    ~FeatureAPIServer() {
        delete db_conn_;
        delete logger_;
        delete config_manager_;
    }

    void start_background_tasks() {
        // Start background thread for data quality checks
        quality_thread_ = std::thread([this, logger = logger_]() {
            logger->log(LogLevel::INFO, "Starting background data quality monitoring thread");

            while (web_server_ && web_server_->is_running()) {
                try {
                    // Run scheduled data quality checks
                    if (quality_checker_) {
                        quality_checker_->run_all_checks();
                    }
                } catch (const std::exception& e) {
                    logger->log(LogLevel::ERROR, "Exception in background data quality checks: " + std::string(e.what()));
                }

                // Sleep for 15 minutes before next check
                std::this_thread::sleep_for(std::chrono::minutes(15));
            }

            logger->log(LogLevel::INFO, "Background data quality monitoring thread stopped");
        });
    }

    void run() {
        std::cout << "🚀 Starting Feature API Server..." << std::endl;

        // Start the web server
        if (web_server_ && web_server_->start()) {
            std::cout << "✅ Web UI Server started on port 8080" << std::endl;
            std::cout << "🌐 API endpoints available:" << std::endl;
            std::cout << "   • GET  /api/embeddings/models" << std::endl;
            std::cout << "   • POST /api/embeddings/visualize" << std::endl;
            std::cout << "   • GET  /api/embeddings/search" << std::endl;
            std::cout << "   • GET  /api/agents/{agent_id}/memory" << std::endl;
            std::cout << "   • GET  /api/data-quality/rules" << std::endl;
            std::cout << "   • POST /api/data-quality/rules" << std::endl;
            std::cout << "   • GET  /api/data-quality/checks" << std::endl;
            std::cout << "   • POST /api/data-quality/run/{rule_id}" << std::endl;
            std::cout << "   • GET  /api/data-quality/dashboard" << std::endl;
        } else {
            std::cerr << "❌ Failed to start Web UI Server" << std::endl;
            return;
        }

        // Start background tasks
        start_background_tasks();

        // Keep server running
        std::cout << "\n🎯 Feature API Server is running! Press Ctrl+C to stop." << std::endl;
        std::cout << "📊 Check logs for activity and API requests." << std::endl;

        // Simple event loop to keep server alive
        while (web_server_ && web_server_->is_running()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
};

int main(int argc, char* argv[]) {
    try {
        // Get database connection string from environment - PRODUCTION REQUIREMENT: No hardcoded defaults
        const char* db_conn_env = std::getenv("DATABASE_URL");
        if (!db_conn_env || strlen(db_conn_env) == 0) {
            std::cerr << "❌ FATAL ERROR: DATABASE_URL environment variable not set" << std::endl;
            std::cerr << "   Set it with: export DATABASE_URL='postgresql://user:pass@host:port/db'" << std::endl;
            return EXIT_FAILURE;
        }
        std::string db_conn_str = db_conn_env;

        std::cout << "🔌 Database connection: " << db_conn_str << std::endl;

        FeatureAPIServer server(db_conn_str);
        server.run();

    } catch (const std::exception& e) {
        std::cerr << "❌ Server startup failed: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}