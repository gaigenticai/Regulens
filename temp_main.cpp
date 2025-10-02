// Main function
int main() {
    std::cout << "🔍 Regulens Agentic AI Compliance System - Standalone UI Demo" << std::endl;
    std::cout << "Production-grade web interface for comprehensive feature testing" << std::endl;
    std::cout << std::endl;

    try {
        // Create demo components
        auto knowledge_base = std::make_shared<SimpleKnowledgeBase>();
        auto monitor = std::make_shared<SimpleRegulatoryMonitor>();
        auto orchestrator = std::make_shared<SimulatedAgentOrchestrator>();

        // Initialize components
        monitor->set_knowledge_base(knowledge_base);

        // Add real regulatory sources - production-grade compliance monitoring
        auto config_manager = std::make_shared<ConfigurationManager>();
        config_manager->initialize(0, nullptr);
        auto logger = std::make_shared<StructuredLogger>();

        auto sec_source = std::make_shared<SecEdgarSource>(config_manager, logger);
        auto fca_source = std::make_shared<FcaRegulatorySource>(config_manager, logger);
        monitor->add_source(sec_source);
        monitor->add_source(fca_source);

        // Start monitoring
        monitor->start_monitoring();
        orchestrator->start_orchestration();

        // Create and start web server
        RegulatoryMonitorHTTPServer server(monitor, knowledge_base, orchestrator);
        if (!server.start(8080)) {
            std::cerr << "Failed to start web server" << std::endl;
            return 1;
        }

        std::cout << "🌐 Web UI available at: http://localhost:8080" << std::endl;
        std::cout << "📊 Open your browser and navigate to the URL above" << std::endl;
        std::cout << "🔄 The system will run until interrupted (Ctrl+C)" << std::endl;
        std::cout << std::endl;

        // Wait for shutdown signal
        std::signal(SIGINT, [](int) { /* Handle shutdown */ });

        // Keep running until interrupted
        while (server.is_running()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Critical error: " << e.what() << std::endl;
        return 1;
    }
}

} // namespace regulens
