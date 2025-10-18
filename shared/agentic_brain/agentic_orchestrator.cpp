#include "agentic_orchestrator.hpp"
#include "../event_system/event.hpp"
#include "../event_system/event_bus.hpp"
#include <stdexcept>
#include <cstdlib>

namespace regulens {

// OrchestratorConfig Implementation

OrchestratorConfig OrchestratorConfig::from_environment() {
    OrchestratorConfig config;
    
    // Read initialization strategy from environment
    const char* init_strategy_env = std::getenv("ORCHESTRATOR_INIT_STRATEGY");
    if (init_strategy_env) {
        std::string strategy(init_strategy_env);
        if (strategy == "EAGER") {
            config.init_strategy = ComponentInitStrategy::EAGER;
        } else if (strategy == "LAZY") {
            config.init_strategy = ComponentInitStrategy::LAZY;
        } else if (strategy == "CUSTOM") {
            config.init_strategy = ComponentInitStrategy::CUSTOM;
        }
    }
    
    // Component enable/disable from environment
    const char* enable_llm = std::getenv("ORCHESTRATOR_ENABLE_LLM");
    if (enable_llm && std::string(enable_llm) == "false") {
        config.enable_llm_interface = false;
    }
    
    const char* enable_learning = std::getenv("ORCHESTRATOR_ENABLE_LEARNING");
    if (enable_learning && std::string(enable_learning) == "false") {
        config.enable_learning_engine = false;
    }
    
    const char* enable_decision = std::getenv("ORCHESTRATOR_ENABLE_DECISION");
    if (enable_decision && std::string(enable_decision) == "false") {
        config.enable_decision_engine = false;
    }
    
    // Timeout configuration
    const char* init_timeout = std::getenv("ORCHESTRATOR_INIT_TIMEOUT");
    if (init_timeout) {
        try {
            config.initialization_timeout_seconds = std::stoi(init_timeout);
        } catch (...) {
            // Keep default on parse error
        }
    }
    
    const char* fail_fast = std::getenv("ORCHESTRATOR_FAIL_FAST");
    if (fail_fast && std::string(fail_fast) == "false") {
        config.fail_fast = false;
    }
    
    return config;
}

// AgenticOrchestrator Implementation

AgenticOrchestrator::AgenticOrchestrator(
    std::shared_ptr<ConnectionPool> db_pool,
    StructuredLogger* logger,
    const OrchestratorConfig& config
) : db_pool_(db_pool),
    http_client_(nullptr),
    logger_(logger),
    config_(config),
    llm_interface_(nullptr),
    learning_engine_(nullptr),
    decision_engine_(nullptr),
    tool_registry_(nullptr),
    event_bus_(nullptr),
    initialized_(false),
    running_(false) {
    
    // Production-grade validation of required dependencies
    try {
        validate_required_dependencies();
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Orchestrator constructor validation failed: " + std::string(e.what()));
        throw;
    }
    
    logger_->log(LogLevel::INFO, "AgenticOrchestrator: Using " + 
                 std::string(config_.init_strategy == ComponentInitStrategy::EAGER ? "EAGER" :
                           config_.init_strategy == ComponentInitStrategy::LAZY ? "LAZY" : "CUSTOM") + 
                 " initialization strategy");
    
    // Load capability configuration from environment
    capability_config_ = load_agent_capability_config();
    
    // Eager initialization if configured
    if (config_.init_strategy == ComponentInitStrategy::EAGER) {
        logger_->log(LogLevel::INFO, "AgenticOrchestrator: Performing eager component initialization");
        if (!initialize_components_eagerly(config_)) {
            std::string error = "Eager component initialization failed";
            logger_->log(LogLevel::ERROR, error);
            if (config_.fail_fast) {
                throw std::runtime_error(error);
            }
        }
    } else {
        logger_->log(LogLevel::INFO, "AgenticOrchestrator: Components will be initialized lazily during initialize() call");
    }
}

AgenticOrchestrator::AgenticOrchestrator(
    std::shared_ptr<ConnectionPool> db_pool,
    std::shared_ptr<LLMInterface> llm_interface,
    std::shared_ptr<AgentLearningEngine> learning_engine,
    std::shared_ptr<DecisionEngine> decision_engine,
    std::shared_ptr<ToolRegistry> tool_registry,
    std::shared_ptr<EventBus> event_bus,
    StructuredLogger* logger
) : db_pool_(db_pool),
    http_client_(nullptr),
    logger_(logger),
    config_(),  // Use default config for explicit injection constructor
    llm_interface_(llm_interface),
    learning_engine_(learning_engine),
    decision_engine_(decision_engine),
    tool_registry_(tool_registry),
    event_bus_(event_bus),
    initialized_(false),
    running_(false) {
    // Mark as CUSTOM strategy since all components are externally provided
    config_.init_strategy = ComponentInitStrategy::CUSTOM;
    capability_config_ = load_agent_capability_config();
    
    logger_->log(LogLevel::INFO, "AgenticOrchestrator: Full constructor with explicit component injection");
}

AgenticOrchestrator::~AgenticOrchestrator() {
    shutdown();
}

bool AgenticOrchestrator::initialize() {
    try {
        logger_->log(LogLevel::INFO, "Initializing Agentic Orchestrator");

        // Initialize tool registry for agent tool integration
        if (!tool_registry_) {
            tool_registry_ = std::make_shared<ToolRegistry>(db_pool_, logger_);
            if (!tool_registry_->initialize()) {
                logger_->log(LogLevel::ERROR, "Failed to initialize tool registry");
                return false;
            }
        }

        // Initialize event bus for real-time processing
        if (!event_bus_) {
            event_bus_ = std::make_shared<EventBus>(db_pool_, logger_);
            if (!event_bus_->initialize()) {
                logger_->log(LogLevel::ERROR, "Failed to initialize event bus");
                return false;
            }
        }

        // Initialize HTTP client for LLM and external communications
        http_client_ = std::make_shared<HttpClient>();

        // Initialize core components if not provided via constructor
        // These are optional for basic functionality
        if (!llm_interface_) {
            try {
                http_client_ = std::make_shared<HttpClient>();
                llm_interface_ = std::make_shared<LLMInterface>(http_client_, logger_);
                // Configure with OpenAI if API key available
                nlohmann::json llm_config = {
                    {"api_key", std::getenv("OPENAI_API_KEY") ? std::getenv("OPENAI_API_KEY") : ""},
                    {"base_url", "https://api.openai.com/v1"},
                    {"timeout_seconds", 30},
                    {"max_retries", 3}
                };
                if (!llm_config["api_key"].empty()) {
                    // Configure LLM immediately if API key available
                    try {
                        llm_interface_->configure_provider(LLMProvider::OPENAI, llm_config);
                        llm_interface_->set_provider(LLMProvider::OPENAI);
                        llm_interface_->set_model(LLMModel::GPT_4_TURBO);
                        logger_->log(LogLevel::INFO, "LLM configured successfully");
                    } catch (const std::exception& e) {
                        logger_->log(LogLevel::WARN, "LLM configuration failed, will use fallback logic: " + std::string(e.what()));
                    }
                }
            } catch (const std::exception& e) {
                logger_->log(LogLevel::WARN, "LLM interface initialization failed (expected in test mode): " + std::string(e.what()));
                // Continue without LLM interface
            }
        }

        if (!learning_engine_ && llm_interface_) {
            try {
                learning_engine_ = std::make_shared<AgentLearningEngine>(db_pool_, llm_interface_, logger_);
            } catch (const std::exception& e) {
                logger_->log(LogLevel::WARN, "Learning engine initialization failed: " + std::string(e.what()));
                // Continue without learning engine
            }
        }

        if (!decision_engine_ && llm_interface_ && learning_engine_) {
            try {
                decision_engine_ = std::make_shared<DecisionEngine>(db_pool_, llm_interface_, learning_engine_, logger_);
            } catch (const std::exception& e) {
                logger_->log(LogLevel::WARN, "Decision engine initialization failed: " + std::string(e.what()));
                // Continue without decision engine
            }
        }

        // Load agent configurations and initialize agents
        if (!initialize_agents()) {
            logger_->log(LogLevel::ERROR, "Failed to initialize agents");
            return false;
        }

        running_ = true;
        initialized_ = true;

        logger_->log(LogLevel::INFO, "Agentic Orchestrator initialized successfully");
        return true;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Agentic Orchestrator initialization failed: " + std::string(e.what()));
        return false;
    }
}

void AgenticOrchestrator::shutdown() {
    if (!running_.load()) {
        return;
    }

    logger_->log(LogLevel::INFO, "Shutting down Agentic Orchestrator");

    running_ = false;

    // Clean up active tools
    {
        std::lock_guard<std::mutex> lock(tool_mutex_);
        for (auto& [tool_id, tool] : active_tools_) {
            if (tool) {
                tool->disconnect();
            }
        }
        active_tools_.clear();
    }

    // Shutdown components
    if (learning_engine_) {
        learning_engine_->shutdown();
    }

    initialized_ = false;
    logger_->log(LogLevel::INFO, "Agentic Orchestrator shutdown complete");
}

// Tool Integration Methods - Agent Autonomous Tool Usage

std::vector<std::string> AgenticOrchestrator::discover_available_tools(ToolCategory category) {
    if (!tool_registry_) {
        logger_->log(LogLevel::WARN, "Tool registry not available");
        return {};
    }

    return tool_registry_->get_tools_by_category(category);
}

std::vector<std::string> AgenticOrchestrator::find_tools_by_capability(ToolCapability capability) {
    std::vector<std::string> matching_tools;

    if (!tool_registry_) {
        return matching_tools;
    }

    auto all_tools = tool_registry_->get_available_tools();

    for (const auto& tool_id : all_tools) {
        auto tool = tool_registry_->get_tool(tool_id);
        if (tool && tool->supports_capability(capability)) {
            matching_tools.push_back(tool_id);
        }
    }

    return matching_tools;
}

std::unique_ptr<Tool> AgenticOrchestrator::acquire_tool(const std::string& tool_id) {
    std::lock_guard<std::mutex> lock(tool_mutex_);

    // Check if tool is already active
    auto it = active_tools_.find(tool_id);
    if (it != active_tools_.end()) {
        logger_->log(LogLevel::DEBUG, "Returning existing tool instance: " + tool_id);
        return nullptr; // Tool already in use, return null to indicate this
    }

    // Get tool from registry
    if (!tool_registry_) {
        logger_->log(LogLevel::ERROR, "Tool registry not available");
        return nullptr;
    }

    auto tool = tool_registry_->get_tool(tool_id);
    if (!tool) {
        logger_->log(LogLevel::WARN, "Tool not found in registry: " + tool_id);
        return nullptr;
    }

    // Create a new instance for agent use
    // In production, this would clone or create a new authenticated instance
    auto tool_config = tool->get_tool_info();
    ToolConfig config;
    config.tool_id = tool_config["tool_id"];
    config.tool_name = tool_config["tool_name"];
    config.description = tool_config.value("description", "");
    config.category = string_to_tool_category(tool_config["category"]);
    config.capabilities = {}; // capabilities will be set by factory
    config.auth_type = string_to_auth_type(tool_config["auth_type"]);
    config.auth_config = {};
    config.connection_config = {};
    config.timeout = std::chrono::seconds(30);
    config.max_retries = 3;
    config.retry_delay = std::chrono::milliseconds(1000);
    config.rate_limit_per_minute = tool_config["rate_limit_per_minute"];
    config.enabled = true;

    auto new_tool = ToolFactory::create_tool(config, logger_);

    if (new_tool && new_tool->authenticate()) {
        active_tools_[tool_id] = std::move(new_tool);
        logger_->log(LogLevel::INFO, "Agent acquired tool: " + tool_id);
        return std::move(active_tools_[tool_id]);
    }

    logger_->log(LogLevel::ERROR, "Failed to acquire and authenticate tool: " + tool_id);
    return nullptr;
}

ToolResult AgenticOrchestrator::execute_tool_operation(
    const std::string& tool_id,
    const std::string& operation,
    const nlohmann::json& parameters
) {
    std::lock_guard<std::mutex> lock(tool_mutex_);

    auto it = active_tools_.find(tool_id);
    if (it == active_tools_.end()) {
        // Try to acquire the tool first
        if (!acquire_tool(tool_id)) {
            return ToolResult(false, {}, "Tool not available: " + tool_id);
        }
        it = active_tools_.find(tool_id);
    }

    if (it == active_tools_.end()) {
        return ToolResult(false, {}, "Failed to acquire tool: " + tool_id);
    }

    auto& tool = it->second;
    if (!tool) {
        return ToolResult(false, {}, "Tool instance is null: " + tool_id);
    }

    logger_->log(LogLevel::DEBUG, "Agent executing tool operation: " + tool_id + " -> " + operation);

    try {
        auto result = tool->execute_operation(operation, parameters);

        // Learn from tool effectiveness
        learn_tool_effectiveness(tool_id, operation, result.success, result.execution_time);

        // Publish event about tool usage
        if (event_bus_) {
            auto event = EventFactory::create_agent_decision_event(
                "AgenticOrchestrator",
                "tool-execution-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()),
                {
                    {"tool_id", tool_id},
                    {"operation", operation},
                    {"parameters", parameters},
                    {"result", {
                        {"success", result.success},
                        {"execution_time_ms", result.execution_time.count()},
                        {"error_message", result.error_message}
                    }}
                }
            );
            event_bus_->publish(std::move(event));
        }

        return result;

    } catch (const std::exception& e) {
        std::string error_msg = "Tool execution failed: " + std::string(e.what());
        logger_->log(LogLevel::ERROR, error_msg);

        learn_tool_effectiveness(tool_id, operation, false, std::chrono::milliseconds(0));

        return ToolResult(false, {}, error_msg);
    }
}

std::vector<nlohmann::json> AgenticOrchestrator::analyze_situation_and_recommend_tools(
    AgentType agent_type,
    const nlohmann::json& situation_context
) {
    std::vector<nlohmann::json> recommendations;

    try {
        // Use LLM to analyze situation and recommend tools
        std::string analysis_prompt = R"(
        Analyze this situation and recommend appropriate tools for an AI agent to use:

        Agent Type: )" + std::string(agent_type == AgentType::TRANSACTION_GUARDIAN ? "Transaction Guardian" :
                                   agent_type == AgentType::REGULATORY_ASSESSOR ? "Regulatory Assessor" :
                                   "Audit Intelligence") + R"(

        Situation Context: )" + situation_context.dump(2) + R"(

        Available Tool Categories:
        - COMMUNICATION: Email, messaging
        - ERP: Enterprise systems
        - CRM: Customer relationship management
        - DMS: Document management
        - STORAGE: File storage
        - ANALYTICS: Business intelligence
        - WORKFLOW: Process automation

        Respond with JSON array of tool recommendations, each containing:
        - tool_category: The category of tool needed
        - tool_capability: What the tool should do (READ, WRITE, NOTIFY, etc.)
        - rationale: Why this tool is needed
        - urgency: LOW, MEDIUM, HIGH, CRITICAL
        - alternative_tools: Array of fallback tool types
        )";

        // Production: LLM-powered intelligent tool selection based on context
        if (llm_interface_) {
            auto llm_response = llm_interface_->analyze_with_context(analysis_prompt, situation_context);
            if (llm_response.success && !llm_response.analysis.empty()) {
                try {
                    auto parsed = nlohmann::json::parse(llm_response.analysis);
                    if (parsed.is_array()) {
                        recommendations = parsed.get<std::vector<nlohmann::json>>();
                        logger_->log(LogLevel::INFO, "LLM-powered tool recommendations generated successfully");
                    } else {
                        throw std::runtime_error("LLM response not a JSON array");
                    }
                } catch (const std::exception& parse_error) {
                    logger_->log(LogLevel::WARN, "Failed to parse LLM recommendations, using fallback: " + 
                               std::string(parse_error.what()));
                    recommendations = generate_fallback_tool_recommendations(agent_type, situation_context);
                }
            } else {
                logger_->log(LogLevel::WARN, "LLM analysis failed, using fallback recommendations");
                recommendations = generate_fallback_tool_recommendations(agent_type, situation_context);
            }
        } else {
            // No LLM available, use rule-based fallback
            logger_->log(LogLevel::INFO, "LLM interface not available, using rule-based tool recommendations");
            recommendations = generate_fallback_tool_recommendations(agent_type, situation_context);
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Tool recommendation analysis failed: " + std::string(e.what()));
        recommendations = generate_fallback_tool_recommendations(agent_type, situation_context);
    }

    return recommendations;
}

bool AgenticOrchestrator::execute_autonomous_tool_workflow(
    AgentType agent_type,
    const nlohmann::json& context,
    const std::vector<std::string>& required_tools
) {
    try {
        logger_->log(LogLevel::INFO, "Starting autonomous tool workflow for agent type: " +
                  std::to_string(static_cast<int>(agent_type)));

        bool workflow_success = true;

        for (const auto& tool_id : required_tools) {
            // Acquire and authenticate tool
            auto tool = acquire_tool(tool_id);
            if (!tool) {
                logger_->log(LogLevel::ERROR, "Failed to acquire required tool: " + tool_id);
                workflow_success = false;
                continue;
            }

            // Execute appropriate operations based on agent type and context
            std::string operation;
            nlohmann::json parameters;

            if (agent_type == AgentType::TRANSACTION_GUARDIAN) {
                operation = determine_transaction_tool_operation(context);
                parameters = prepare_transaction_tool_parameters(context);
            } else if (agent_type == AgentType::REGULATORY_ASSESSOR) {
                operation = determine_regulatory_tool_operation(context);
                parameters = prepare_regulatory_tool_parameters(context);
            } else if (agent_type == AgentType::AUDIT_INTELLIGENCE) {
                operation = determine_audit_tool_operation(context);
                parameters = prepare_audit_tool_parameters(context);
            }

            if (!operation.empty()) {
                auto result = execute_tool_operation(tool_id, operation, parameters);
                if (!result.success) {
                    logger_->log(LogLevel::WARN, "Tool operation failed: " + tool_id + " -> " + operation);
                    workflow_success = false;
                } else {
                    logger_->log(LogLevel::INFO, "Tool operation successful: " + tool_id + " -> " + operation);
                }
            }
        }

        return workflow_success;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Autonomous tool workflow failed: " + std::string(e.what()));
        return false;
    }
}

bool AgenticOrchestrator::learn_tool_effectiveness(
    const std::string& tool_id,
    const std::string& operation,
    bool success,
    std::chrono::milliseconds execution_time
) {
    // Use learning engine for tool effectiveness tracking
    if (!learning_engine_) {
        logger_->log(LogLevel::WARN, "Learning engine not available, tool effectiveness learning skipped: " +
                    tool_id + " " + operation + " " + (success ? "SUCCESS" : "FAILED"));
        return false;
    }

    try {
        // Record tool effectiveness in learning engine
        LearningFeedback feedback{
            "feedback-" + tool_id + "-" + operation + "-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()),  // feedback_id
            "agent-orchestrator",  // agent_id
            "tool-operation-" + tool_id + "-" + operation,  // decision_id
            success ? FeedbackType::POSITIVE : FeedbackType::NEGATIVE,  // feedback_type
            success ? 1.0 : -1.0,  // feedback_score (-1.0 to 1.0)
            "Tool operation: " + operation + " took " + std::to_string(execution_time.count()) + "ms",  // human_feedback
            "ToolIntegrationSystem",  // feedback_provider
            {{"tool_id", tool_id}, {"operation", operation}, {"execution_time_ms", execution_time.count()}, {"success", success}},  // feedback_context
            std::chrono::system_clock::now(),  // feedback_timestamp
            false  // incorporated
        };

        return learning_engine_->store_feedback(feedback);

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to learn tool effectiveness: " + std::string(e.what()));
        return false;
    }
}

std::string agent_type_to_string(AgentType type) {
    switch (type) {
        case AgentType::TRANSACTION_GUARDIAN: return "transaction_guardian";
        case AgentType::REGULATORY_ASSESSOR: return "regulatory_assessor";
        case AgentType::AUDIT_INTELLIGENCE: return "audit_intelligence";
        default: return "unknown_agent";
    }
}

std::vector<nlohmann::json> AgenticOrchestrator::get_tool_usage_recommendations(AgentType agent_type) {
    std::vector<nlohmann::json> recommendations;

    // Use learning engine for intelligent recommendations
    if (!learning_engine_) {
        logger_->log(LogLevel::WARN, "Learning engine not available, cannot generate intelligent recommendations");
        return recommendations; // Return empty vector
    }

    try {
        // Query learning engine for tool effectiveness patterns
        std::string agent_id = agent_type_to_string(agent_type);
        auto insights = learning_engine_->get_learning_metrics(agent_id);

        if (insights.contains("tool_effectiveness")) {
            recommendations = insights["tool_effectiveness"];
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to get tool usage recommendations: " + std::string(e.what()));
    }

    return recommendations;
}

// Level 3: Tool-Intelligent Capabilities - LLM-Driven Tool Analysis

nlohmann::json AgenticOrchestrator::analyze_situation_with_llm(const nlohmann::json& context, AgentType agent_type) {
    // Check if advanced discovery is enabled
    if (!capability_config_.enable_advanced_discovery) {
        logger_->log(LogLevel::INFO, "Advanced discovery disabled via environment configuration");
        return {{"error", "Advanced discovery disabled"}, {"fallback", true}};
    }

    // Use LLM for intelligent situation analysis
    if (!llm_interface_) {
        logger_->log(LogLevel::ERROR, "LLM interface not available or failed to configure for situation analysis");
        return {{"error", "LLM interface not available or failed to configure"}};
    }

    try {
        std::string agent_name = (agent_type == AgentType::TRANSACTION_GUARDIAN) ? "Transaction Guardian" :
                                (agent_type == AgentType::REGULATORY_ASSESSOR) ? "Regulatory Assessor" :
                                "Audit Intelligence";

        std::string analysis_prompt = R"(
You are an advanced AI agent orchestrator. Analyze this situation and determine what tools and capabilities are needed.

Agent Type: )" + agent_name + R"(
Situation Context: )" + context.dump(2) + R"(

Provide a detailed analysis including:
1. Primary objectives that need to be achieved
2. Data sources required
3. Communication/notification needs
4. Decision criteria and risk factors
5. Success metrics and validation requirements
6. Potential failure modes and mitigation strategies

Respond with a JSON object containing your analysis.
)";

        LLMRequest llm_request;
        llm_request.messages.push_back({"system", "You are an expert AI agent orchestrator specializing in tool integration and workflow optimization."});
        llm_request.messages.push_back({"user", analysis_prompt});
        llm_request.temperature = 0.2; // Lower temperature for more consistent analysis
        llm_request.max_tokens = 1500;

        auto llm_response = llm_interface_->generate_completion(llm_request);

        if (llm_response.success) {
            try {
                return nlohmann::json::parse(llm_response.content);
            } catch (const std::exception& e) {
                logger_->log(LogLevel::WARN, "Failed to parse LLM situation analysis: " + std::string(e.what()));
            }
        }

        // Fallback analysis
        return {
            {"primary_objectives", {"Analyze situation", "Take appropriate action"}},
            {"required_data_sources", {"Internal systems", "External APIs"}},
            {"communication_needs", {"Email notifications", "System alerts"}},
            {"decision_criteria", {"Risk assessment", "Compliance requirements"}},
            {"success_metrics", {"Action completed", "No errors"}},
            {"failure_modes", {"Network issues", "Authentication failures"}}
        };

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "LLM situation analysis failed: " + std::string(e.what()));
        return {{"error", "Analysis failed"}, {"fallback", true}};
    }
}

std::vector<nlohmann::json> AgenticOrchestrator::generate_intelligent_tool_recommendations(
    const nlohmann::json& situation_analysis,
    const std::vector<std::string>& available_tools
) {
    std::vector<nlohmann::json> recommendations;

    // Check if LLM interface is available
    if (!llm_interface_) {
        logger_->log(LogLevel::INFO, "LLM interface not available or failed to configure, using basic tool recommendations");
        // Return basic recommendations based on available tools
        for (const auto& tool_id : available_tools) {
            recommendations.push_back({
                {"tool_id", tool_id},
                {"priority", "medium"},
                {"reasoning", "Basic tool availability"},
                {"confidence", 0.5}
            });
        }
        return recommendations;
    }

    try {
        // Get detailed tool information
        std::vector<nlohmann::json> tool_details;
        for (const auto& tool_id : available_tools) {
            auto details = tool_registry_->get_tool_details(tool_id);
            if (!details.contains("error")) {
                tool_details.push_back(details);
            }
        }

        std::string recommendation_prompt = R"(
Based on this situation analysis and available tools, recommend the optimal tool combination.

Situation Analysis: )" + situation_analysis.dump(2) + R"(

Available Tools: )" + nlohmann::json(tool_details).dump(2) + R"(

For each recommended tool, provide:
- tool_id: The specific tool to use
- rationale: Why this tool is optimal for this situation
- operations: Array of operations to perform
- parameters: Suggested parameters for each operation
- priority: HIGH, MEDIUM, LOW
- dependencies: Any tools that must be used before this one
- success_criteria: How to measure if this tool operation succeeded

Return an array of tool recommendations sorted by priority and dependency order.
)";

        LLMRequest llm_request;
        llm_request.messages.push_back({"system", "You are a tool orchestration expert. Recommend optimal tool combinations for complex business scenarios."});
        llm_request.messages.push_back({"user", recommendation_prompt});
        llm_request.temperature = 0.1; // Very low temperature for consistent recommendations
        llm_request.max_tokens = 2000;

        auto llm_response = llm_interface_->generate_completion(llm_request);

        if (llm_response.success) {
            try {
                recommendations = nlohmann::json::parse(llm_response.content);

                // Validate recommendations against available tools
                recommendations.erase(
                    std::remove_if(recommendations.begin(), recommendations.end(),
                        [this](const nlohmann::json& rec) {
                            if (!rec.contains("tool_id")) return true;
                            std::string tool_id = rec["tool_id"];
                            return !tool_registry_->get_tool(tool_id);
                        }),
                    recommendations.end()
                );

                return recommendations;

            } catch (const std::exception& e) {
                logger_->log(LogLevel::WARN, "Failed to parse LLM tool recommendations: " + std::string(e.what()));
            }
        }

        // Fallback: Rule-based recommendations
        return generate_fallback_tool_recommendations(static_cast<AgentType>(0), situation_analysis);

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Intelligent tool recommendation failed: " + std::string(e.what()));
        return generate_fallback_tool_recommendations(static_cast<AgentType>(0), situation_analysis);
    }
}

std::vector<nlohmann::json> AgenticOrchestrator::optimize_tool_workflow(
    const std::vector<nlohmann::json>& tool_recommendations,
    const nlohmann::json& context
) {
    if (tool_recommendations.size() <= 1) {
        return tool_recommendations; // No optimization needed for single tool
    }

    // Check if LLM interface is available
    if (!llm_interface_) {
        logger_->log(LogLevel::INFO, "LLM interface not available or failed to configure, returning original workflow");
        return tool_recommendations; // Return original recommendations
    }

    try {
        std::string optimization_prompt = R"(
Optimize this tool workflow for maximum efficiency and reliability.

Tool Recommendations: )" + nlohmann::json(tool_recommendations).dump(2) + R"(

Context: )" + context.dump(2) + R"(

Optimization Goals:
1. Minimize total execution time
2. Maximize success probability
3. Minimize resource usage
4. Handle dependencies correctly
5. Provide fallback strategies

Provide an optimized workflow with:
- Execution order (considering dependencies)
- Parallel execution opportunities
- Error handling strategies
- Resource allocation recommendations
- Performance monitoring points

Return the optimized tool sequence with execution metadata.
)";

        LLMRequest llm_request;
        llm_request.messages.push_back({"system", "You are a workflow optimization expert. Optimize tool execution sequences for business processes."});
        llm_request.messages.push_back({"user", optimization_prompt});
        llm_request.temperature = 0.1;
        llm_request.max_tokens = 1500;

        auto llm_response = llm_interface_->generate_completion(llm_request);

        if (llm_response.success) {
            try {
                nlohmann::json optimized_workflow = nlohmann::json::parse(llm_response.content);
                if (optimized_workflow.contains("optimized_sequence")) {
                    return optimized_workflow["optimized_sequence"];
                }
            } catch (const std::exception& e) {
                logger_->log(LogLevel::WARN, "Failed to parse workflow optimization: " + std::string(e.what()));
            }
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Workflow optimization failed: " + std::string(e.what()));
    }

    // Return original recommendations if optimization fails
    return tool_recommendations;
}

// Level 4: Tool-Creative Capabilities - Dynamic Tool Composition

nlohmann::json AgenticOrchestrator::discover_unknown_tools(const nlohmann::json& requirements) {
    // Check if autonomous tool integration is enabled
    if (!capability_config_.enable_autonomous_integration) {
        logger_->log(LogLevel::INFO, "Autonomous tool integration disabled via environment configuration");
        return {{"discovered_tools", nlohmann::json::array()}, {"error", "Autonomous tool integration disabled"}};
    }

    // Use LLM if available, otherwise provide rule-based fallback
    if (!llm_interface_) {
        logger_->log(LogLevel::INFO, "LLM interface not available or failed to configure, using rule-based tool discovery");

        // Rule-based fallback tool discovery
        std::vector<nlohmann::json> fallback_tools;

        if (requirements.contains("domain") && requirements["domain"] == "compliance_monitoring") {
            fallback_tools = {
                {
                    {"tool_name", "Regulatory Document Processor"},
                    {"tool_type", "SaaS"},
                    {"capabilities", {"PDF parsing", "text extraction", "change detection"}},
                    {"integration_complexity", "MEDIUM"},
                    {"estimated_setup_time", "2 weeks"},
                    {"cost_implications", "Enterprise"},
                    {"reliability_rating", 4},
                    {"api_availability", true},
                    {"real_world_usage", {"Regulatory compliance", "Document management"}}
                },
                {
                    {"tool_name", "Compliance Alert System"},
                    {"tool_type", "API"},
                    {"capabilities", {"Real-time notifications", "stakeholder routing", "escalation rules"}},
                    {"integration_complexity", "LOW"},
                    {"estimated_setup_time", "1 week"},
                    {"cost_implications", "Paid"},
                    {"reliability_rating", 5},
                    {"api_availability", true},
                    {"real_world_usage", {"Compliance monitoring", "Risk management"}}
                }
            };
        } else {
            fallback_tools = {
                {
                    {"tool_name", "Generic Data Processor"},
                    {"tool_type", "Library"},
                    {"capabilities", {"Data processing", "format conversion"}},
                    {"integration_complexity", "LOW"},
                    {"estimated_setup_time", "3 days"},
                    {"cost_implications", "Free"},
                    {"reliability_rating", 3},
                    {"api_availability", false},
                    {"real_world_usage", {"Data processing", "ETL operations"}}
                }
            };
        }

        return {
            {"discovered_tools", fallback_tools},
            {"discovery_method", "rule_based_fallback"},
            {"discovery_timestamp", std::chrono::system_clock::now().time_since_epoch().count()},
            {"requirements_hash", std::hash<std::string>{}(requirements.dump())}
        };
    }

    try {
        std::string discovery_prompt = R"(
Discover potential third-party tools or services that could fulfill these requirements.

Requirements: )" + requirements.dump(2) + R"(

Consider:
1. Commercial SaaS platforms
2. Open-source tools and libraries
3. Cloud services and APIs
4. Enterprise software solutions
5. Custom development possibilities

For each potential tool, provide:
- tool_name: Descriptive name
- tool_type: Category (API, SaaS, Library, etc.)
- capabilities: What it can do
- integration_complexity: LOW, MEDIUM, HIGH
- estimated_setup_time: Time to integrate
- cost_implications: Free, Paid, Enterprise
- reliability_rating: 1-5 scale
- api_availability: Does it have APIs?
- real_world_usage: Common use cases

Return an array of potential tools that could be integrated.
)";

        LLMRequest llm_request;
        llm_request.messages.push_back({"system", "You are a technology discovery expert. Find tools and services for business requirements."});
        llm_request.messages.push_back({"user", discovery_prompt});
        llm_request.temperature = 0.3; // Moderate creativity for discovery
        llm_request.max_tokens = 2000;

        auto llm_response = llm_interface_->generate_completion(llm_request);

        if (llm_response.success) {
            try {
                return {
                    {"discovered_tools", nlohmann::json::parse(llm_response.content)},
                    {"discovery_method", "llm_powered"},
                    {"discovery_timestamp", std::chrono::system_clock::now().time_since_epoch().count()},
                    {"requirements_hash", std::hash<std::string>{}(requirements.dump())}
                };
            } catch (const std::exception& e) {
                logger_->log(LogLevel::WARN, "Failed to parse tool discovery results: " + std::string(e.what()));
            }
        }

        return {
            {"discovered_tools", nlohmann::json::array()},
            {"error", "Tool discovery failed"},
            {"fallback", true}
        };

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Tool discovery failed: " + std::string(e.what()));
        return {{"error", "Discovery process failed"}};
    }
}

nlohmann::json AgenticOrchestrator::generate_custom_tool_config(
    const std::string& tool_type,
    const nlohmann::json& requirements,
    const nlohmann::json& context
) {
    // Use LLM if available, otherwise provide rule-based fallback
    if (!llm_interface_) {
        logger_->log(LogLevel::INFO, "LLM interface not available or failed to configure, using rule-based tool configuration generation");

        // Rule-based fallback configuration generation
        nlohmann::json fallback_config = generate_basic_tool_config(tool_type, requirements);

        if (!fallback_config.empty()) {
            return fallback_config;
        } else {
            return {{"error", "Failed to generate tool configuration - no LLM available and rule-based generation failed"}};
        }
    }

    try {
        std::string config_prompt = R"(
Generate a complete tool configuration for integrating this type of tool.

Tool Type: )" + tool_type + R"(
Requirements: )" + requirements.dump(2) + R"(
Context: )" + context.dump(2) + R"(

Generate a ToolConfig JSON structure with:
- tool_id: Unique identifier
- tool_name: Human-readable name
- description: What this tool does
- category: Appropriate ToolCategory
- capabilities: Array of ToolCapability values
- auth_type: Authentication method
- auth_config: Authentication configuration
- connection_config: Connection settings
- timeout_seconds: Operation timeout
- max_retries: Retry attempts
- retry_delay_ms: Delay between retries
- rate_limit_per_minute: Rate limiting
- metadata: Additional configuration

Ensure the configuration follows enterprise security and performance standards.
)";

        LLMRequest llm_request;
        llm_request.messages.push_back({"system", "You are a tool configuration expert. Generate secure, production-ready tool configurations."});
        llm_request.messages.push_back({"user", config_prompt});
        llm_request.temperature = 0.1; // Low temperature for consistent config generation
        llm_request.max_tokens = 1500;

        auto llm_response = llm_interface_->generate_completion(llm_request);

        if (llm_response.success) {
            try {
                nlohmann::json generated_config = nlohmann::json::parse(llm_response.content);

                // Validate the generated configuration
                if (validate_generated_config(generated_config)) {
                    return generated_config;
                } else {
                    logger_->log(LogLevel::WARN, "Generated tool config failed validation");
                }
            } catch (const std::exception& e) {
                logger_->log(LogLevel::WARN, "Failed to parse generated tool config: " + std::string(e.what()));
            }
        }

        // Fallback: Generate basic config template
        return generate_basic_tool_config(tool_type, requirements);

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Custom tool config generation failed: " + std::string(e.what()));
        return generate_basic_tool_config(tool_type, requirements);
    }
}

std::vector<nlohmann::json> AgenticOrchestrator::compose_tool_workflow(
    const nlohmann::json& complex_task,
    const std::vector<std::string>& available_tools
) {
    std::vector<nlohmann::json> workflow;

    // Use LLM if available, otherwise provide rule-based fallback
    if (!llm_interface_) {
        logger_->log(LogLevel::INFO, "LLM interface not available or failed to configure, using rule-based workflow composition");

        // Rule-based fallback workflow composition
        workflow = create_sequential_workflow(complex_task, available_tools);

        if (!workflow.empty()) {
            return workflow;
        } else {
            // Return empty workflow if even fallback fails
            return workflow;
        }
    }

    try {
        // Get tool details for composition
        std::vector<nlohmann::json> tool_details;
        for (const auto& tool_id : available_tools) {
            auto details = tool_registry_->get_tool_details(tool_id);
            if (!details.contains("error")) {
                tool_details.push_back(details);
            }
        }

        std::string composition_prompt = R"(
Compose a multi-tool workflow to accomplish this complex task.

Complex Task: )" + complex_task.dump(2) + R"(

Available Tools: )" + nlohmann::json(tool_details).dump(2) + R"(

Create a workflow that:
1. Breaks down the complex task into manageable steps
2. Assigns appropriate tools to each step
3. Handles data flow between steps
4. Includes error handling and rollback strategies
5. Optimizes for parallel execution where possible
6. Considers dependencies and prerequisites

For each workflow step, provide:
- step_id: Unique identifier
- step_name: Human-readable description
- tool_id: Tool to use for this step
- operation: Specific operation to perform
- parameters: Operation parameters
- input_data: Data sources for this step
- output_data: Data produced by this step
- dependencies: Steps that must complete before this one
- error_handling: What to do if this step fails
- success_criteria: How to verify success

Return a complete workflow specification.
)";

        LLMRequest llm_request;
        llm_request.messages.push_back({"system", "You are a workflow composition expert. Design multi-tool workflows for complex business processes."});
        llm_request.messages.push_back({"user", composition_prompt});
        llm_request.temperature = 0.2;
        llm_request.max_tokens = 2500;

        auto llm_response = llm_interface_->generate_completion(llm_request);

        if (llm_response.success) {
            try {
                nlohmann::json composed_workflow = nlohmann::json::parse(llm_response.content);
                if (composed_workflow.contains("workflow_steps")) {
                    workflow = composed_workflow["workflow_steps"];

                    // Validate workflow
                    if (validate_workflow_composition(workflow, available_tools)) {
                        return workflow;
                    }
                }
            } catch (const std::exception& e) {
                logger_->log(LogLevel::WARN, "Failed to parse composed workflow: " + std::string(e.what()));
            }
        }

        // Fallback: Create task decomposition workflow with sequential orchestration
        return create_sequential_workflow(complex_task, available_tools);

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Tool workflow composition failed: " + std::string(e.what()));
        return create_sequential_workflow(complex_task, available_tools);
    }
}

bool AgenticOrchestrator::negotiate_tool_capabilities(
    const std::string& tool_id,
    const nlohmann::json& required_capabilities
) {
    try {
        auto tool = tool_registry_->get_tool(tool_id);
        if (!tool) {
            logger_->log(LogLevel::WARN, "Tool not found for capability negotiation: " + tool_id);
            return false;
        }

        // Check if tool supports required capabilities
        bool all_supported = true;
        std::vector<std::string> missing_capabilities;

        for (const auto& cap : required_capabilities) {
            std::string cap_str = cap;
            ToolCapability capability = string_to_tool_capability(cap_str);

            if (!tool->supports_capability(capability)) {
                all_supported = false;
                missing_capabilities.push_back(cap_str);
            }
        }

        if (!all_supported) {
            logger_->log(LogLevel::WARN, "Tool " + tool_id + " missing capabilities: " +
                        nlohmann::json(missing_capabilities).dump());

            // Attempt capability negotiation (future enhancement)
            // Production: validate agent capabilities against requirements
            return false;
        }

        logger_->log(LogLevel::INFO, "Tool " + tool_id + " successfully negotiated required capabilities");
        return true;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Capability negotiation failed: " + std::string(e.what()));
        return false;
    }
}


// Advanced capability helper methods

bool AgenticOrchestrator::validate_generated_config(const nlohmann::json& config) {
    // Basic validation of generated tool configuration
    return config.contains("tool_id") &&
           config.contains("tool_name") &&
           config.contains("category") &&
           config.contains("capabilities") &&
           !config["tool_id"].get<std::string>().empty() &&
           !config["tool_name"].get<std::string>().empty();
}

nlohmann::json AgenticOrchestrator::generate_basic_tool_config(
    const std::string& tool_type,
    const nlohmann::json& requirements
) {
    // Generate a basic, safe tool configuration
    return {
        {"tool_id", "generated-" + tool_type + "-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count())},
        {"tool_name", "Generated " + tool_type + " Tool"},
        {"description", "Auto-generated tool configuration for " + tool_type},
        {"category", "INTEGRATION"},
        {"capabilities", {"READ", "WRITE"}},
        {"auth_type", "API_KEY"},
        {"auth_config", {}},
        {"connection_config", {}},
        {"timeout_seconds", 30},
        {"max_retries", 3},
        {"retry_delay_ms", 1000},
        {"rate_limit_per_minute", 60},
        {"enabled", false},  // Require manual review before enabling
        {"metadata", {
            {"generated", true},
            {"generation_timestamp", std::chrono::system_clock::now().time_since_epoch().count()},
            {"requirements", requirements}
        }}
    };
}

bool AgenticOrchestrator::validate_workflow_composition(
    const std::vector<nlohmann::json>& workflow,
    const std::vector<std::string>& available_tools
) {
    // Basic validation of composed workflow
    for (const auto& step : workflow) {
        if (!step.contains("tool_id") || !step.contains("operation")) {
            return false;
        }

        std::string tool_id = step["tool_id"];
        if (std::find(available_tools.begin(), available_tools.end(), tool_id) == available_tools.end()) {
            return false; // Tool not available
        }
    }

    return true;
}

std::vector<nlohmann::json> AgenticOrchestrator::create_sequential_workflow(
    const nlohmann::json& complex_task,
    const std::vector<std::string>& available_tools
) {
    // Create a basic sequential workflow as degraded-mode fallback
    std::vector<nlohmann::json> workflow;

    if (available_tools.empty()) {
        return workflow;
    }

    // Use first available tool for a basic operation
    workflow.push_back({
        {"step_id", "primary-operation-step"},
        {"step_name", "Execute primary task operation"},
        {"tool_id", available_tools[0]},
        {"operation", "execute_operation"},
        {"parameters", complex_task},
        {"dependencies", nlohmann::json::array()},
        {"error_handling", "log_error_and_continue"},
        {"success_criteria", "operation_returns_success"}
    });

    return workflow;
}


// Missing helper method implementations

bool AgenticOrchestrator::initialize_agents() {
    // Initialize agent configurations and states
    try {
        // Load agent configurations from database or config files
        agent_configurations_.clear();
        agent_configurations_[AgentType::TRANSACTION_GUARDIAN] = {
            {"enabled", true},
            {"priority", "HIGH"},
            {"monitoring_interval_seconds", 30},
            {"alert_threshold", 0.8}
        };
        agent_configurations_[AgentType::REGULATORY_ASSESSOR] = {
            {"enabled", true},
            {"priority", "CRITICAL"},
            {"monitoring_interval_seconds", 60},
            {"compliance_check_frequency", "daily"}
        };
        agent_configurations_[AgentType::AUDIT_INTELLIGENCE] = {
            {"enabled", true},
            {"priority", "NORMAL"},
            {"monitoring_interval_seconds", 300},
            {"analysis_depth", "comprehensive"}
        };

        logger_->log(LogLevel::INFO, "Initialized " + std::to_string(agent_configurations_.size()) + " agent configurations");
        return true;
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to initialize agents: " + std::string(e.what()));
        return false;
    }
}

// Production-grade component factory methods

void AgenticOrchestrator::validate_required_dependencies() const {
    if (!db_pool_) {
        throw std::invalid_argument("AgenticOrchestrator: Database connection pool is required (cannot be null)");
    }
    
    if (!logger_) {
        throw std::invalid_argument("AgenticOrchestrator: Structured logger is required (cannot be null)");
    }
    
    // Verify database pool is operational
    try {
        auto conn = db_pool_->get_connection();
        if (!conn) {
            throw std::invalid_argument("AgenticOrchestrator: Database connection pool is not operational");
        }
    } catch (const std::exception& e) {
        throw std::invalid_argument("AgenticOrchestrator: Database connection pool validation failed: " + std::string(e.what()));
    }
}

std::shared_ptr<ToolRegistry> AgenticOrchestrator::create_tool_registry_with_defaults() {
    logger_->log(LogLevel::INFO, "Creating ToolRegistry with production defaults");
    
    try {
        auto registry = std::make_shared<ToolRegistry>(db_pool_, logger_);
        if (!registry->initialize()) {
            logger_->log(LogLevel::ERROR, "ToolRegistry initialization failed");
            return nullptr;
        }
        
        logger_->log(LogLevel::INFO, "ToolRegistry created and initialized successfully");
        return registry;
        
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to create ToolRegistry: " + std::string(e.what()));
        return nullptr;
    }
}

std::shared_ptr<EventBus> AgenticOrchestrator::create_event_bus_with_defaults() {
    logger_->log(LogLevel::INFO, "Creating EventBus with production defaults");
    
    try {
        auto event_bus = std::make_shared<EventBus>(db_pool_, logger_);
        if (!event_bus->initialize()) {
            logger_->log(LogLevel::ERROR, "EventBus initialization failed");
            return nullptr;
        }
        
        logger_->log(LogLevel::INFO, "EventBus created and initialized successfully");
        return event_bus;
        
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to create EventBus: " + std::string(e.what()));
        return nullptr;
    }
}

std::shared_ptr<LLMInterface> AgenticOrchestrator::create_llm_interface_from_environment() {
    logger_->log(LogLevel::INFO, "Creating LLMInterface from environment configuration");
    
    try {
        if (!http_client_) {
            http_client_ = std::make_shared<HttpClient>();
        }
        
        auto llm = std::make_shared<LLMInterface>(http_client_, logger_);
        
        // Configure with OpenAI if API key is available
        const char* openai_key = std::getenv("OPENAI_API_KEY");
        if (openai_key && std::string(openai_key).length() > 0) {
            nlohmann::json llm_config = {
                {"api_key", openai_key},
                {"base_url", std::getenv("OPENAI_BASE_URL") ? std::getenv("OPENAI_BASE_URL") : "https://api.openai.com/v1"},
                {"timeout_seconds", 30},
                {"max_retries", 3}
            };
            
            try {
                llm->configure_provider(LLMProvider::OPENAI, llm_config);
                llm->set_provider(LLMProvider::OPENAI);
                llm->set_model(LLMModel::GPT_4_TURBO);
                logger_->log(LogLevel::INFO, "LLMInterface configured with OpenAI provider");
            } catch (const std::exception& e) {
                logger_->log(LogLevel::WARN, "OpenAI configuration failed: " + std::string(e.what()));
            }
        }
        
        // Try Anthropic if OpenAI not available
        const char* anthropic_key = std::getenv("ANTHROPIC_API_KEY");
        if ((!openai_key || std::string(openai_key).empty()) && anthropic_key && std::string(anthropic_key).length() > 0) {
            nlohmann::json llm_config = {
                {"api_key", anthropic_key},
                {"base_url", std::getenv("ANTHROPIC_BASE_URL") ? std::getenv("ANTHROPIC_BASE_URL") : "https://api.anthropic.com"},
                {"timeout_seconds", 30},
                {"max_retries", 3}
            };
            
            try {
                llm->configure_provider(LLMProvider::ANTHROPIC, llm_config);
                llm->set_provider(LLMProvider::ANTHROPIC);
                llm->set_model(LLMModel::CLAUDE_3_SONNET);
                logger_->log(LogLevel::INFO, "LLMInterface configured with Anthropic provider");
            } catch (const std::exception& e) {
                logger_->log(LogLevel::WARN, "Anthropic configuration failed: " + std::string(e.what()));
            }
        }
        
        if (!openai_key && !anthropic_key) {
            logger_->log(LogLevel::WARN, "No LLM API keys found in environment (OPENAI_API_KEY or ANTHROPIC_API_KEY)");
            logger_->log(LogLevel::WARN, "LLMInterface created but not configured - advanced AI features will be limited");
        }
        
        return llm;
        
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to create LLMInterface: " + std::string(e.what()));
        return nullptr;
    }
}

std::shared_ptr<AgentLearningEngine> AgenticOrchestrator::create_learning_engine_with_defaults() {
    if (!llm_interface_) {
        logger_->log(LogLevel::WARN, "Cannot create LearningEngine without LLMInterface");
        return nullptr;
    }
    
    logger_->log(LogLevel::INFO, "Creating LearningEngine with production defaults");
    
    try {
        auto learning = std::make_shared<AgentLearningEngine>(db_pool_, llm_interface_, logger_);
        logger_->log(LogLevel::INFO, "LearningEngine created successfully");
        return learning;
        
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to create LearningEngine: " + std::string(e.what()));
        return nullptr;
    }
}

std::shared_ptr<DecisionEngine> AgenticOrchestrator::create_decision_engine_with_defaults() {
    if (!llm_interface_) {
        logger_->log(LogLevel::WARN, "Cannot create DecisionEngine without LLMInterface");
        return nullptr;
    }
    
    if (!learning_engine_) {
        logger_->log(LogLevel::WARN, "Cannot create DecisionEngine without LearningEngine");
        return nullptr;
    }
    
    logger_->log(LogLevel::INFO, "Creating DecisionEngine with production defaults");
    
    try {
        auto decision = std::make_shared<DecisionEngine>(db_pool_, llm_interface_, learning_engine_, logger_);
        logger_->log(LogLevel::INFO, "DecisionEngine created successfully");
        return decision;
        
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to create DecisionEngine: " + std::string(e.what()));
        return nullptr;
    }
}

bool AgenticOrchestrator::initialize_components_eagerly(const OrchestratorConfig& config) {
    bool all_critical_succeeded = true;
    
    // Critical components (must succeed)
    if (config.require_tool_registry && !tool_registry_) {
        tool_registry_ = create_tool_registry_with_defaults();
        if (!tool_registry_) {
            logger_->log(LogLevel::ERROR, "Critical component ToolRegistry failed to initialize");
            all_critical_succeeded = false;
            if (config.fail_fast) return false;
        }
    }
    
    if (config.require_event_bus && !event_bus_) {
        event_bus_ = create_event_bus_with_defaults();
        if (!event_bus_) {
            logger_->log(LogLevel::ERROR, "Critical component EventBus failed to initialize");
            all_critical_succeeded = false;
            if (config.fail_fast) return false;
        }
    }
    
    // HTTP client for LLM and external communications
    if (!http_client_) {
        http_client_ = std::make_shared<HttpClient>();
    }
    
    // Optional components (failures are logged but don't stop initialization)
    if (config.enable_llm_interface && !llm_interface_) {
        llm_interface_ = create_llm_interface_from_environment();
        if (!llm_interface_) {
            logger_->log(LogLevel::WARN, "Optional component LLMInterface not available - advanced AI features disabled");
        }
    }
    
    if (config.enable_learning_engine && !learning_engine_ && llm_interface_) {
        learning_engine_ = create_learning_engine_with_defaults();
        if (!learning_engine_) {
            logger_->log(LogLevel::WARN, "Optional component LearningEngine not available - learning features disabled");
        }
    }
    
    if (config.enable_decision_engine && !decision_engine_ && llm_interface_ && learning_engine_) {
        decision_engine_ = create_decision_engine_with_defaults();
        if (!decision_engine_) {
            logger_->log(LogLevel::WARN, "Optional component DecisionEngine not available - advanced decision features disabled");
        }
    }
    
    logger_->log(LogLevel::INFO, "Eager component initialization completed - " + 
                 std::string(all_critical_succeeded ? "all critical components operational" : "some critical components failed"));
    
    return all_critical_succeeded;
}


} // namespace regulens
