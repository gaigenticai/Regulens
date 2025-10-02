#include "regulatory_monitor.hpp"
#include "change_detector.hpp"
#include "regulatory_source.hpp"
#include "../shared/regulatory_knowledge_base.hpp"

namespace regulens {

RegulatoryMonitor::RegulatoryMonitor(std::shared_ptr<ConfigurationManager> config,
                                   std::shared_ptr<StructuredLogger> logger,
                                   std::shared_ptr<RegulatoryKnowledgeBase> knowledge_base)
    : config_(config), logger_(logger), knowledge_base_(knowledge_base),
      status_(MonitoringStatus::INITIALIZING), should_stop_(false),
      last_successful_check_(std::chrono::system_clock::now()),
      check_interval_(std::chrono::seconds(30)) {}

RegulatoryMonitor::~RegulatoryMonitor() {
    stop_monitoring();
}

bool RegulatoryMonitor::initialize() {
    logger_->info("Initializing Regulatory Monitor");

    // Initialize change detector and document parser
    change_detector_ = std::make_shared<ChangeDetector>(config_, logger_);
    if (!change_detector_->initialize()) {
        logger_->error("Failed to initialize change detector");
        return false;
    }

    // Document parser would be initialized here
    // For demo purposes, we'll skip complex initialization

    status_ = MonitoringStatus::INITIALIZING;
    logger_->info("Regulatory Monitor initialized successfully");
    return true;
}

bool RegulatoryMonitor::start_monitoring() {
    if (status_ == MonitoringStatus::ACTIVE) {
        return true;
    }

    status_ = MonitoringStatus::ACTIVE;
    should_stop_ = false;

    // Start monitoring thread
    monitoring_thread_ = std::thread(&RegulatoryMonitor::monitoring_loop, this);

    logger_->info("Regulatory monitoring started");
    return true;
}

void RegulatoryMonitor::stop_monitoring() {
    if (status_ == MonitoringStatus::SHUTDOWN) {
        return;
    }

    should_stop_ = true;
    status_ = MonitoringStatus::SHUTDOWN;

    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }

    logger_->info("Regulatory monitoring stopped");
}

MonitoringStatus RegulatoryMonitor::get_status() const {
    return status_.load();
}

nlohmann::json RegulatoryMonitor::get_monitoring_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return {
        {"active_sources", active_sources_.size()},
        {"total_checks", sources_checked_.size()},
        {"changes_detected", changes_detected_.size()},
        {"errors_encountered", errors_encountered_.size()},
        {"last_check", std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now() - last_successful_check_).count()}
    };
}

bool RegulatoryMonitor::force_check_all_sources() {
    std::lock_guard<std::mutex> lock(sources_mutex_);

    for (auto& [source_id, source] : active_sources_) {
        if (source->is_active()) {
            check_source(source);
        }
    }

    return true;
}

bool RegulatoryMonitor::add_custom_source(const nlohmann::json& source_config) {
    try {
        std::string source_id = "custom_" + std::to_string(active_sources_.size());
        std::string name = source_config.value("name", "Custom Source");

        // Use RegulatorySourceFactory to create appropriate source type
        auto source = RegulatorySourceFactory::create_custom_source(
            source_id, name, source_config, config_, logger_);

        if (!source) {
            logger_->error("Failed to create custom regulatory source: {}", name);
            return false;
        }

        source->set_active(true);

        std::lock_guard<std::mutex> lock(sources_mutex_);
        active_sources_[source_id] = source;

        logger_->info("Added custom regulatory source: {}", name);
        return true;

    } catch (const std::exception& e) {
        logger_->error("Failed to add custom source: {}", e.what());
        return false;
    }
}

bool RegulatoryMonitor::remove_source(const std::string& source_id) {
    std::lock_guard<std::mutex> lock(sources_mutex_);
    auto it = active_sources_.find(source_id);
    if (it != active_sources_.end()) {
        active_sources_.erase(it);
        logger_->info("Removed regulatory source: {}", source_id);
        return true;
    }
    return false;
}

std::vector<std::string> RegulatoryMonitor::get_active_sources() const {
    std::lock_guard<std::mutex> lock(sources_mutex_);
    std::vector<std::string> sources;
    for (const auto& [id, _] : active_sources_) {
        sources.push_back(id);
    }
    return sources;
}

void RegulatoryMonitor::monitoring_loop() {
    logger_->info("Regulatory monitoring loop started");

    while (!should_stop_) {
        try {
            force_check_all_sources();
            std::this_thread::sleep_for(check_interval_);
        } catch (const std::exception& e) {
            logger_->error("Exception in monitoring loop: {}", e.what());
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }

    logger_->info("Regulatory monitoring loop ended");
}

void RegulatoryMonitor::check_source(std::shared_ptr<RegulatorySource> source) {
    try {
        auto changes = source->check_for_changes();

        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            sources_checked_[source->get_source_id()]++;
        }

        if (!changes.empty()) {
            process_regulatory_changes(convert_to_change_events(changes));
            last_successful_check_ = std::chrono::system_clock::now();
        }

    } catch (const std::exception& e) {
        handle_monitoring_error(source->get_source_id(), e.what());
    }
}

void RegulatoryMonitor::process_regulatory_changes(const std::vector<RegulatoryChangeEvent>& changes) {
    for (const auto& change : changes) {
        // Store in knowledge base
        update_knowledge_base(change);

        // Notify agents
        notify_agents(change);

        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            changes_detected_[change.source_id]++;
        }

        logger_->info("Processed regulatory change: {}", change.document_title);
    }
}

void RegulatoryMonitor::notify_agents(const RegulatoryChangeEvent& change) {
    // Create compliance event for agents
    EventSource source{"regulatory_monitor", change.source_id, "system"};
    ComplianceEvent event(EventType::REGULATORY_CHANGE_DETECTED,
                         EventSeverity::HIGH,
                         "Regulatory change detected: " + change.document_title,
                         source);

    // Add metadata
    event.set_metadata_value("source_id", change.source_id);
    event.set_metadata_value("document_title", change.document_title);
    event.set_metadata_value("impact_level", static_cast<int>(change.impact_level));

    // In real implementation, this would notify the agent orchestrator
    logger_->info("Notified agents about regulatory change: {}", change.document_title);
}

void RegulatoryMonitor::update_knowledge_base(const RegulatoryChangeEvent& change) {
    // Convert to RegulatoryChange object and store
    RegulatoryChangeMetadata metadata;
    metadata.regulatory_body = change.source_id;
    metadata.document_type = "Regulatory Change";
    metadata.keywords = {"compliance", "regulation", change.document_title};

    RegulatoryChange reg_change(change.source_id, change.document_title,
                              change.document_url, metadata);

    knowledge_base_->store_regulatory_change(reg_change);
}

void RegulatoryMonitor::handle_monitoring_error(const std::string& source_id, const std::string& error_description) {
    logger_->error("Monitoring error for source {}: {}", source_id, error_description);

    std::lock_guard<std::mutex> lock(stats_mutex_);
    errors_encountered_[source_id]++;
}

std::vector<RegulatoryChangeEvent> RegulatoryMonitor::convert_to_change_events(const std::vector<RegulatoryChange>& changes) {
    std::vector<RegulatoryChangeEvent> events;
    for (const auto& change : changes) {
        RegulatoryChangeEvent event{
            change.get_source_id(),
            change.get_title(),
            "Regulatory change detected with potential compliance impact",
            std::chrono::system_clock::now(),
            RegulatoryImpact::MEDIUM,
            {"compliance", "regulation"},
            change.get_content_url()
        };
        events.push_back(event);
    }
    return events;
}

} // namespace regulens

