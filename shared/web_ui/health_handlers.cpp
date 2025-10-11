/**
 * Health Check Handlers Implementation
 *
 * Production-grade health check implementation for Kubernetes readiness, liveness,
 * and startup probes with comprehensive service health monitoring and metrics.
 */

#include "health_handlers.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <thread>
#include <regex>
#include <unistd.h>
#include <sys/statvfs.h>
#include "../database/postgresql_connection.hpp"
#include "../cache/redis_client.hpp"
#include "../network/http_client.hpp"
#include "../event_system/event_bus.hpp"
#include "../../core/agent/agent_orchestrator.hpp"

#ifdef __APPLE__
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/vm_statistics.h>
#include <mach/mach_types.h>
#include <mach/message.h>
#include <mach/kern_return.h>
#else
#include <sys/sysinfo.h>
#endif

namespace regulens {

// HealthCheckHandler Implementation

HealthCheckHandler::HealthCheckHandler(std::shared_ptr<ConfigurationManager> config,
                                     std::shared_ptr<StructuredLogger> logger,
                                     std::shared_ptr<ErrorHandler> error_handler,
                                     std::shared_ptr<PrometheusMetricsCollector> metrics)
    : config_(config), logger_(logger), error_handler_(error_handler), metrics_(metrics) {}

bool HealthCheckHandler::initialize() {
    service_start_time_ = std::chrono::system_clock::now();

    // Register basic service health checks
    register_health_check("service_startup",
                         [this]() -> HealthCheckResult {
                             auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
                                 std::chrono::system_clock::now() - service_start_time_).count();

                             return HealthCheckResult{
                                 true, "healthy",
                                 "Service has been running for " + std::to_string(uptime) + " seconds",
                                 {{"uptime_seconds", uptime}}
                             };
                         },
                         true,  // critical
                         {HealthProbeType::READINESS, HealthProbeType::LIVENESS, HealthProbeType::STARTUP});

    // Register system resource checks
    register_health_check("system_resources",
                         []() -> HealthCheckResult {
#ifdef __APPLE__
                             // macOS-specific system resource check
                             vm_size_t page_size;
                             vm_statistics64_data_t vm_stats;
                             mach_port_t mach_port = mach_host_self();
                             mach_msg_type_number_t count = sizeof(vm_stats) / sizeof(natural_t);
                             host_page_size(mach_port, &page_size);
                             
                             if (host_statistics64(mach_port, HOST_VM_INFO, (host_info64_t)&vm_stats, &count) != KERN_SUCCESS) {
                                 return HealthCheckResult{
                                     false, "unhealthy",
                                     "Failed to get system information on macOS",
                                     {{"error", "host_statistics_failed"}}
                                 };
                             }
                             
                             uint64_t total_mem = (vm_stats.wire_count + vm_stats.active_count + 
                                                   vm_stats.inactive_count + vm_stats.free_count) * page_size;
                             uint64_t used_mem = (vm_stats.wire_count + vm_stats.active_count) * page_size;
                             double memory_usage = total_mem > 0 ? (100.0 * used_mem / total_mem) : 0.0;
                             
                             // Get load averages
                             double loadavg[3];
                             if (getloadavg(loadavg, 3) == -1) {
                                 loadavg[0] = loadavg[1] = loadavg[2] = 0.0;
                             }
#else
                             // Linux-specific system resource check
                             struct sysinfo sys_info;
                             if (sysinfo(&sys_info) != 0) {
                                 return HealthCheckResult{
                                     false, "unhealthy",
                                     "Failed to get system information",
                                     {{"error", "sysinfo_failed"}}
                                 };
                             }

                             double memory_usage = 100.0 * (1.0 - (double)sys_info.freeram / sys_info.totalram);
                             double loadavg[3] = {
                                 sys_info.loads[0] / 65536.0,
                                 sys_info.loads[1] / 65536.0,
                                 sys_info.loads[2] / 65536.0
                             };
#endif

                             if (memory_usage > 95.0) {
                                 return HealthCheckResult{
                                     false, "unhealthy",
                                     "High memory usage: " + std::to_string(memory_usage) + "%",
                                     {{"memory_usage_percent", memory_usage}}
                                 };
                             }

                             return HealthCheckResult{
                                 true, "healthy",
                                 "System resources within acceptable limits",
                                 {
                                     {"memory_usage_percent", memory_usage},
                                     {"load_average_1min", loadavg[0]},
                                     {"load_average_5min", loadavg[1]},
                                     {"load_average_15min", loadavg[2]}
                                 }
                             };
                         },
                         false,  // not critical
                         {HealthProbeType::READINESS, HealthProbeType::LIVENESS});

    if (logger_) {
        logger_->info("Health check handler initialized with basic checks",
                     "HealthCheckHandler", "initialize");
    }

    return true;
}

void HealthCheckHandler::register_health_check(const std::string& name,
                                             HealthCheckFunction check_function,
                                             bool critical,
                                             const std::vector<HealthProbeType>& probe_types) {
    std::lock_guard<std::mutex> lock(health_checks_mutex_);

    // Use emplace to avoid copying atomic members
    health_checks_.emplace(std::piecewise_construct,
                          std::forward_as_tuple(name),
                          std::forward_as_tuple());
    
    auto& info = health_checks_[name];
    info.name = name;
    info.function = check_function;
    info.critical = critical;
    info.probe_types = probe_types;

    if (logger_) {
        logger_->debug("Registered health check",
                      "HealthCheckHandler", "register_health_check",
                      {{"check_name", name}, 
                       {"critical", critical ? "true" : "false"}, 
                       {"probe_types", std::to_string(probe_types.size())}});
    }
}

void HealthCheckHandler::unregister_health_check(const std::string& name) {
    std::lock_guard<std::mutex> lock(health_checks_mutex_);

    auto it = health_checks_.find(name);
    if (it != health_checks_.end()) {
        health_checks_.erase(it);

        if (logger_) {
            logger_->debug("Unregistered health check",
                          "HealthCheckHandler", "unregister_health_check",
                          {{"check_name", name}});
        }
    }
}

std::pair<int, std::string> HealthCheckHandler::readiness_probe() {
    total_probes_++;

    auto results = execute_health_checks(HealthProbeType::READINESS);
    auto aggregated = aggregate_results(results, true); // All critical checks must pass for readiness

    update_metrics(HealthProbeType::READINESS, aggregated.healthy);

    if (!aggregated.healthy) {
        failed_probes_++;
    }

    return format_response(aggregated, false); // Lightweight response for health probes
}

std::pair<int, std::string> HealthCheckHandler::liveness_probe() {
    total_probes_++;

    auto results = execute_health_checks(HealthProbeType::LIVENESS);
    auto aggregated = aggregate_results(results, false); // Any critical check failure means not live

    update_metrics(HealthProbeType::LIVENESS, aggregated.healthy);

    if (!aggregated.healthy) {
        failed_probes_++;
    }

    return format_response(aggregated, false); // Lightweight response for health probes
}

std::pair<int, std::string> HealthCheckHandler::startup_probe() {
    total_probes_++;

    // For startup probe, check if service has completed initialization
    if (!service_started_) {
        // Check startup-specific health checks
        auto results = execute_health_checks(HealthProbeType::STARTUP);
        auto aggregated = aggregate_results(results, true);

        if (aggregated.healthy) {
            mark_service_started();
        }

        update_metrics(HealthProbeType::STARTUP, aggregated.healthy);
        return format_response(aggregated, false);
    }

    // Service is started, return healthy
    HealthCheckResult result{true, "healthy", "Service startup completed"};
    update_metrics(HealthProbeType::STARTUP, true);
    return format_response(result, false);
}

nlohmann::json HealthCheckHandler::get_detailed_health() {
    std::lock_guard<std::mutex> lock(health_checks_mutex_);

    nlohmann::json health = {
        {"service", {
            {"name", "regulens"},
            {"version", "1.0.0"},
            {"status", "running"},
            {"uptime_seconds", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now() - service_start_time_).count()}
        }},
        {"checks", nlohmann::json::object()},
        {"summary", {
            {"total_checks", health_checks_.size()},
            {"healthy_checks", 0},
            {"unhealthy_checks", 0},
            {"overall_status", "healthy"}
        }}
    };

    size_t healthy_count = 0;
    size_t unhealthy_count = 0;

    for (const auto& [name, info] : health_checks_) {
        try {
            auto result = info.function();
            health["checks"][name] = result.to_json();

            if (result.healthy) {
                healthy_count++;
            } else {
                unhealthy_count++;
                health["summary"]["overall_status"] = "unhealthy";
            }
        } catch (const std::exception& e) {
            unhealthy_count++;
            health["summary"]["overall_status"] = "unhealthy";
            health["checks"][name] = {
                {"healthy", false},
                {"status", "error"},
                {"message", "Health check threw exception: " + std::string(e.what())},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            };
        }
    }

    health["summary"]["healthy_checks"] = healthy_count;
    health["summary"]["unhealthy_checks"] = unhealthy_count;

    return health;
}

nlohmann::json HealthCheckHandler::get_health_metrics() {
    std::lock_guard<std::mutex> lock(health_checks_mutex_);

    nlohmann::json metrics = {
        {"total_probes", total_probes_.load()},
        {"failed_probes", failed_probes_.load()},
        {"registered_checks", health_checks_.size()},
        {"service_started", service_started_.load()}
    };

    // Individual check metrics
    nlohmann::json check_metrics = nlohmann::json::object();
    for (const auto& [name, info] : health_checks_) {
        check_metrics[name] = {
            {"executions", info.executions.load()},
            {"failures", info.failures.load()},
            {"last_execution", std::chrono::duration_cast<std::chrono::milliseconds>(
                info.last_execution.time_since_epoch()).count()},
            {"last_failure", std::chrono::duration_cast<std::chrono::milliseconds>(
                info.last_failure.time_since_epoch()).count()}
        };
    }
    metrics["checks"] = check_metrics;

    return metrics;
}

std::vector<HealthCheckResult> HealthCheckHandler::execute_health_checks(HealthProbeType probe_type) {
    std::lock_guard<std::mutex> lock(health_checks_mutex_);
    std::vector<HealthCheckResult> results;

    for (auto& [name, info] : health_checks_) {
        // Check if this health check applies to the requested probe type
        if (std::find(info.probe_types.begin(), info.probe_types.end(), probe_type) == info.probe_types.end()) {
            continue;
        }

        try {
            auto result = info.function();
            results.push_back(result);

            info.executions++;
            info.last_execution = std::chrono::system_clock::now();

            if (!result.healthy) {
                info.failures++;
                info.last_failure = std::chrono::system_clock::now();

                if (logger_) {
                logger_->warn("Health check failed",
                              "HealthCheckHandler", "execute_health_checks",
                              {{"check_name", name}, 
                               {"probe_type", std::to_string(static_cast<int>(probe_type))},
                               {"message", result.message}});
                }
            }
        } catch (const std::exception& e) {
            info.executions++;
            info.failures++;
            info.last_execution = std::chrono::system_clock::now();
            info.last_failure = std::chrono::system_clock::now();

            results.push_back(HealthCheckResult{
                false, "error",
                "Health check threw exception: " + std::string(e.what())
            });

            if (logger_) {
                logger_->error("Health check exception",
                               "HealthCheckHandler", "execute_health_checks",
                               {{"check_name", name}, 
                                {"probe_type", std::to_string(static_cast<int>(probe_type))},
                                {"exception", e.what()}});
            }
        }
    }

    return results;
}

HealthCheckResult HealthCheckHandler::aggregate_results(const std::vector<HealthCheckResult>& results,
                                                      bool require_all_critical) {
    if (results.empty()) {
        return HealthCheckResult{true, "healthy", "No health checks configured"};
    }

    size_t healthy_count = 0;
    size_t unhealthy_count = 0;
    std::vector<std::string> failure_messages;

    nlohmann::json details = nlohmann::json::object();

    for (const auto& result : results) {
        details["checks"].push_back(result.to_json());

        if (result.healthy) {
            healthy_count++;
        } else {
            unhealthy_count++;
            failure_messages.push_back(result.message);
        }
    }

    bool overall_healthy = unhealthy_count == 0;

    if (!overall_healthy) {
        std::string message = "Health check failures: " +
                             std::to_string(unhealthy_count) + "/" +
                             std::to_string(results.size()) + " checks failed";

        if (!failure_messages.empty()) {
            message += " - " + failure_messages[0]; // Include first failure message
        }

        return HealthCheckResult{false, "unhealthy", message, details};
    }

    return HealthCheckResult{
        true, "healthy",
        "All health checks passed: " + std::to_string(healthy_count) + "/" + std::to_string(results.size()),
        details
    };
}

std::pair<int, std::string> HealthCheckHandler::format_response(const HealthCheckResult& result, bool detailed) {
    int status_code = result.healthy ? 200 : 503;

    if (detailed) {
        nlohmann::json response = result.to_json();
        return {status_code, response.dump(2)};
    } else {
        // Lightweight health probe response for orchestrators
        std::string response = result.healthy ? "OK" : "NOT_OK";
        return {status_code, response};
    }
}

void HealthCheckHandler::update_metrics(HealthProbeType probe_type, bool success) {
    // Production-grade metrics tracking with Prometheus and database persistence
    try {
        // Update Prometheus metrics if available
        if (metrics_) {
            std::string probe_name;
            switch (probe_type) {
                case HealthProbeType::LIVENESS:
                    probe_name = "liveness";
                    break;
                case HealthProbeType::READINESS:
                    probe_name = "readiness";
                    break;
                case HealthProbeType::STARTUP:
                    probe_name = "startup";
                    break;
            }
            
            // Update Prometheus counter
            metrics_->increment_counter("health_check_total", 
                                       {{"probe_type", probe_name}, 
                                        {"status", success ? "success" : "failure"}});
            
            // Update gauge for current health status
            metrics_->set_gauge("health_check_status", 
                               success ? 1.0 : 0.0,
                               {{"probe_type", probe_name}});
            
            // Update histogram for response times if applicable
            auto now = std::chrono::system_clock::now();
            if (last_probe_time_.count(probe_type) > 0) {
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - last_probe_time_[probe_type]
                );
                metrics_->observe_histogram("health_check_duration_ms",
                                           duration.count(),
                                           {{"probe_type", probe_name}});
            }
            last_probe_time_[probe_type] = now;
        }
        
        // Persist metrics to database for long-term trending and analysis
        if (db_connection_) {
            std::string insert_query = R"(
                INSERT INTO health_metrics 
                (probe_type, success, timestamp, response_time_ms, metadata)
                VALUES ($1, $2, NOW(), $3, $4)
            )";
            
            int response_time = 0;
            if (last_probe_time_.count(probe_type) > 0) {
                auto now = std::chrono::system_clock::now();
                response_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - last_probe_time_[probe_type]
                ).count();
            }
            
            nlohmann::json metadata = {
                {"service_name", "regulens"},
                {"instance_id", instance_id_},
                {"environment", config_->get_string("ENVIRONMENT").value_or("production")}
            };
            
            db_connection_->execute_query(insert_query, {
                std::to_string(static_cast<int>(probe_type)),
                success ? "true" : "false",
                std::to_string(response_time),
                metadata.dump()
            });
        }
    }
    catch (const std::exception& e) {
        if (logger_) {
            logger_->warn("Failed to update health metrics: " + std::string(e.what()),
                         "HealthCheckHandler", "update_metrics");
        }
    }
}

void HealthCheckHandler::mark_service_started() {
    service_started_ = true;

    if (logger_) {
        logger_->info("Service marked as started for startup probe",
                     "HealthCheckHandler", "mark_service_started");
    }
}

// Factory function

std::shared_ptr<HealthCheckHandler> create_health_check_handler(
    std::shared_ptr<ConfigurationManager> config,
    std::shared_ptr<StructuredLogger> logger,
    std::shared_ptr<ErrorHandler> error_handler,
    std::shared_ptr<PrometheusMetricsCollector> metrics) {

    auto handler = std::make_shared<HealthCheckHandler>(config, logger, error_handler, metrics);
    if (handler->initialize()) {
        return handler;
    }
    return nullptr;
}

// Standard health check functions

namespace health_checks {

HealthCheckFunction database_health_check(std::shared_ptr<PostgreSQLConnection> db_conn) {
    return [db_conn]() -> HealthCheckResult {
        // Production database connectivity check
        if (!db_conn) {
            return HealthCheckResult{
                false, "unhealthy",
                "Database connection not initialized",
                {{"error", "null_connection"}}
            };
        }

        try {
            // Check database connectivity using available API
            bool connected = db_conn->is_connected();
            
            if (!connected) {
                return HealthCheckResult{
                    false, "unhealthy",
                    "Database not connected",
                    {{"error", "not_connected"}}
                };
            }
            
            // Execute health check query to verify database responsiveness
            auto start_time = std::chrono::high_resolution_clock::now();
            auto result = db_conn->execute_query_single("SELECT 1", {});
            auto end_time = std::chrono::high_resolution_clock::now();
            
            auto response_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_time - start_time).count();

            if (!result.has_value()) {
                return HealthCheckResult{
                    false, "unhealthy",
                    "Database query failed",
                    {{"error", "query_failed"}, {"response_time_ms", response_time_ms}}
                };
            }

            return HealthCheckResult{
                true, "healthy",
                "Database connection successful",
                {{"connected", connected}, {"response_time_ms", response_time_ms}}
            };
        } catch (const std::exception& e) {
            return HealthCheckResult{
                false, "unhealthy",
                "Database connection failed: " + std::string(e.what()),
                {{"error", e.what()}}
            };
        }
    };
}

HealthCheckFunction redis_health_check(std::shared_ptr<RedisClient> redis_client) {
    return [redis_client]() -> HealthCheckResult {
        // Production Redis connectivity check
        if (!redis_client) {
            return HealthCheckResult{
                false, "unhealthy",
                "Redis connection not initialized",
                {{"error", "null_connection"}}
            };
        }

        try {
            // Production-grade Redis health check with PING, stats, and memory monitoring
            auto start_time = std::chrono::high_resolution_clock::now();
            
            // Check 1: Connection status
            if (!redis_client->is_connected()) {
                return HealthCheckResult{
                    false, "unhealthy",
                    "Redis is not connected",
                    {{"error", "not_connected"}}
                };
            }
            
            // Check 2: Execute PING command to verify responsiveness
            bool ping_success = false;
            try {
                // Use RedisClient API to execute PING
                ping_success = redis_client->ping();
            } catch (const std::exception& e) {
                return HealthCheckResult{
                    false, "unhealthy",
                    "Redis PING failed: " + std::string(e.what()),
                    {{"error", "ping_failed"}, {"exception", e.what()}}
                };
            }
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto response_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_time - start_time).count();
            
            if (!ping_success) {
                return HealthCheckResult{
                    false, "unhealthy",
                    "Redis PING failed",
                    {
                        {"error", "ping_failed"},
                        {"response_time_ms", response_time_ms}
                    }
                };
            }
            
            // Check 3: Get Redis statistics and server info
            nlohmann::json additional_info;
            additional_info["response_time_ms"] = response_time_ms;
            additional_info["ping_success"] = true;
            
            try {
                // Get Redis server information
                auto info = redis_client->get_info();
                // Extract relevant Redis server information
                if (info.contains("redis_version")) {
                    additional_info["redis_version"] = info["redis_version"];
                }
                if (info.contains("used_memory")) {
                    additional_info["used_memory"] = info["used_memory"];
                }
                if (info.contains("used_memory_human")) {
                    additional_info["used_memory_human"] = info["used_memory_human"];
                }
                if (info.contains("connected_clients")) {
                    int clients = info["connected_clients"];
                    additional_info["connected_clients"] = clients;
                    
                    // Warn if too many clients
                    if (clients > 1000) {
                        additional_info["warning"] = "high_client_count";
                    }
                }
                if (info.contains("total_commands_processed")) {
                    additional_info["total_commands_processed"] = info["total_commands_processed"];
                }
                if (info.contains("uptime_in_seconds")) {
                    additional_info["uptime_in_seconds"] = info["uptime_in_seconds"];
                }
                if (info.contains("evicted_keys")) {
                    long long evicted = info["evicted_keys"];
                    additional_info["evicted_keys"] = evicted;
                    
                    // Warn if memory pressure causing evictions
                    if (evicted > 1000) {
                        additional_info["warning"] = "memory_pressure_evictions";
                        return HealthCheckResult{
                            true, "degraded",
                            "Redis experiencing memory pressure",
                            additional_info
                        };
                    }
                }
            } catch (...) {
                // Statistics gathering failed, but connection is ok
                additional_info["stats_error"] = "failed_to_get_statistics";
            }
            
            // Check 4: Response time threshold
            if (response_time_ms > 500) {
                additional_info["warning"] = "high_latency";
                return HealthCheckResult{
                    true, "degraded",
                    "Redis response time is high",
                    additional_info
                };
            }
            
            return HealthCheckResult{
                true, "healthy",
                "Redis is fully operational",
                additional_info
            };
        } catch (const std::exception& e) {
            return HealthCheckResult{
                false, "unhealthy",
                "Redis connection failed: " + std::string(e.what()),
                {{"error", e.what()}}
            };
        }
    };
}

HealthCheckFunction api_health_check(const std::string& service_name, const std::string& endpoint,
                                    std::shared_ptr<HttpClient> http_client,
                                    int timeout_ms) {
    return [service_name, endpoint, http_client, timeout_ms]() -> HealthCheckResult {
        // Production external API health check with real HTTP request
        if (!http_client) {
            return HealthCheckResult{
                false, "unhealthy",
                "HTTP client not initialized for " + service_name,
                {{"endpoint", endpoint}, {"error", "null_http_client"}}
            };
        }

        try {
            auto start_time = std::chrono::high_resolution_clock::now();
            
            // Set timeout for health check
            http_client->set_timeout(timeout_ms);
            
            // Make actual HTTP GET request to the health endpoint
            HttpResponse response = http_client->get(endpoint);
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto response_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_time - start_time).count();

            // Check HTTP status code
            if (response.status_code >= 200 && response.status_code < 300) {
                return HealthCheckResult{
                    true, "healthy",
                    service_name + " API is responding",
                    {
                        {"endpoint", endpoint},
                        {"response_time_ms", response_time_ms},
                        {"status_code", response.status_code},
                        {"content_length", response.body.length()}
                    }
                };
            } else if (response.status_code >= 500) {
                return HealthCheckResult{
                    false, "unhealthy",
                    service_name + " API returned server error",
                    {
                        {"endpoint", endpoint},
                        {"response_time_ms", response_time_ms},
                        {"status_code", response.status_code},
                        {"error", "server_error"}
                    }
                };
            } else {
                return HealthCheckResult{
                    true, "degraded",
                    service_name + " API returned non-success status",
                    {
                        {"endpoint", endpoint},
                        {"response_time_ms", response_time_ms},
                        {"status_code", response.status_code},
                        {"warning", "unexpected_status"}
                    }
                };
            }
        } catch (const std::exception& e) {
            return HealthCheckResult{
                false, "unhealthy",
                service_name + " API check failed: " + std::string(e.what()),
                {{"endpoint", endpoint}, {"error", e.what()}}
            };
        }
    };
}

HealthCheckFunction filesystem_health_check(const std::vector<std::string>& paths) {
    return [paths]() -> HealthCheckResult {
        try {
            for (const auto& path : paths) {
                // Check if path exists and is accessible
                if (access(path.c_str(), R_OK) != 0) {
                    return HealthCheckResult{
                        false, "unhealthy",
                        "Path not accessible: " + path,
                        {{"path", path}, {"error", "access_denied"}}
                    };
                }

                // Check if it's a directory and get disk usage
                struct statvfs stat;
                if (statvfs(path.c_str(), &stat) == 0) {
                    double free_percent = 100.0 * stat.f_bavail / stat.f_blocks;
                    if (free_percent < 5.0) {
                        return HealthCheckResult{
                            false, "unhealthy",
                            "Low disk space on path: " + path + " (" + std::to_string(free_percent) + "% free)",
                            {{"path", path}, {"free_percent", free_percent}}
                        };
                    }
                }
            }

            return HealthCheckResult{
                true, "healthy",
                "All filesystem paths are accessible",
                {{"checked_paths", paths.size()}}
            };
        } catch (const std::exception& e) {
            return HealthCheckResult{
                false, "unhealthy",
                "Filesystem check failed: " + std::string(e.what()),
                {{"error", e.what()}}
            };
        }
    };
}

HealthCheckFunction memory_health_check(double max_memory_percent) {
    return [max_memory_percent]() -> HealthCheckResult {
        try {
            double memory_usage = 0.0;
            
#ifdef __APPLE__
            // macOS-specific memory check
            vm_size_t page_size;
            vm_statistics64_data_t vm_stats;
            mach_port_t mach_port = mach_host_self();
            mach_msg_type_number_t count = sizeof(vm_stats) / sizeof(natural_t);
            host_page_size(mach_port, &page_size);
            
            if (host_statistics64(mach_port, HOST_VM_INFO, (host_info64_t)&vm_stats, &count) != KERN_SUCCESS) {
                return HealthCheckResult{
                    false, "unhealthy",
                    "Failed to get memory information on macOS",
                    {{"error", "host_statistics_failed"}}
                };
            }
            
            uint64_t total_mem = (vm_stats.wire_count + vm_stats.active_count + 
                                  vm_stats.inactive_count + vm_stats.free_count) * page_size;
            uint64_t used_mem = (vm_stats.wire_count + vm_stats.active_count) * page_size;
            memory_usage = total_mem > 0 ? (100.0 * used_mem / total_mem) : 0.0;
#else
            // Linux-specific memory check
            struct sysinfo sys_info;
            if (sysinfo(&sys_info) != 0) {
                return HealthCheckResult{
                    false, "unhealthy",
                    "Failed to get memory information",
                    {{"error", "sysinfo_failed"}}
                };
            }
            memory_usage = 100.0 * (1.0 - (double)sys_info.freeram / sys_info.totalram);
#endif

            if (memory_usage > max_memory_percent) {
                return HealthCheckResult{
                    false, "unhealthy",
                    "High memory usage: " + std::to_string(memory_usage) + "% (max: " + std::to_string(max_memory_percent) + "%)",
                    {{"memory_usage_percent", memory_usage}, {"max_allowed_percent", max_memory_percent}}
                };
            }

            return HealthCheckResult{
                true, "healthy",
                "Memory usage within limits: " + std::to_string(memory_usage) + "%",
                {{"memory_usage_percent", memory_usage}, {"max_allowed_percent", max_memory_percent}}
            };
        } catch (const std::exception& e) {
            return HealthCheckResult{
                false, "unhealthy",
                "Memory check failed: " + std::string(e.what()),
                {{"error", e.what()}}
            };
        }
    };
}

HealthCheckFunction cpu_health_check(double max_cpu_percent) {
    return [max_cpu_percent]() -> HealthCheckResult {
        try {
            static std::chrono::steady_clock::time_point last_check = std::chrono::steady_clock::now();
            static uint64_t last_total = 0;
            static uint64_t last_idle = 0;
            static double last_cpu_usage = 0.0;

            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_check).count();

            // Only update CPU usage every 1 second minimum to avoid excessive calculations
            if (elapsed < 1000 && last_total > 0) {
                // Use cached value
                double cpu_usage = last_cpu_usage;
                if (cpu_usage > max_cpu_percent) {
                    return HealthCheckResult{
                        false, "unhealthy",
                        "High CPU usage: " + std::to_string(cpu_usage) + "% (max: " + std::to_string(max_cpu_percent) + "%)",
                        {{"cpu_usage_percent", cpu_usage}, {"max_allowed_percent", max_cpu_percent}}
                    };
                }
                return HealthCheckResult{
                    true, "healthy",
                    "CPU usage within limits: " + std::to_string(cpu_usage) + "%",
                    {{"cpu_usage_percent", cpu_usage}, {"max_allowed_percent", max_cpu_percent}}
                };
            }

            double cpu_usage = 0.0;

#ifdef __APPLE__
            // macOS implementation using host_statistics
            mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
            host_cpu_load_info_data_t cpu_load;
            kern_return_t kr = host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, (host_info_t)&cpu_load, &count);
            if (kr != KERN_SUCCESS) {
                return HealthCheckResult{
                    false, "unhealthy",
                    "Failed to get CPU statistics from macOS",
                    {{"error", "host_statistics_failed"}}
                };
            }

            uint64_t total_ticks = 0;
            for (int i = 0; i < CPU_STATE_MAX; i++) {
                total_ticks += cpu_load.cpu_ticks[i];
            }
            uint64_t idle_ticks = cpu_load.cpu_ticks[CPU_STATE_IDLE];

            if (last_total > 0) {
                uint64_t total_diff = total_ticks - last_total;
                uint64_t idle_diff = idle_ticks - last_idle;

                if (total_diff > 0) {
                    cpu_usage = 100.0 * (1.0 - (double)idle_diff / total_diff);
                }
            }

            last_total = total_ticks;
            last_idle = idle_ticks;
#else
            // Linux implementation using /proc/stat
            std::ifstream stat_file("/proc/stat");
            if (!stat_file.is_open()) {
                return HealthCheckResult{
                    false, "unhealthy",
                    "Failed to open /proc/stat for CPU monitoring",
                    {{"error", "proc_stat_unavailable"}}
                };
            }

            std::string line;
            std::getline(stat_file, line);
            stat_file.close();

            // Parse CPU line: cpu user nice system idle iowait irq softirq steal guest guest_nice
            std::istringstream iss(line);
            std::string cpu_label;
            uint64_t user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice;
            iss >> cpu_label >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal >> guest >> guest_nice;

            uint64_t total = user + nice + system + idle + iowait + irq + softirq + steal + guest + guest_nice;

            if (last_total > 0) {
                uint64_t total_diff = total - last_total;
                uint64_t idle_diff = idle - last_idle;

                if (total_diff > 0) {
                    cpu_usage = 100.0 * (1.0 - (double)idle_diff / total_diff);
                }
            }

            last_total = total;
            last_idle = idle;
#endif

            last_check = now;
            last_cpu_usage = cpu_usage;

            if (cpu_usage > max_cpu_percent) {
                return HealthCheckResult{
                    false, "unhealthy",
                    "High CPU usage: " + std::to_string(cpu_usage) + "% (max: " + std::to_string(max_cpu_percent) + "%)",
                    {{"cpu_usage_percent", cpu_usage}, {"max_allowed_percent", max_cpu_percent}}
                };
            }

            return HealthCheckResult{
                true, "healthy",
                "CPU usage within limits: " + std::to_string(cpu_usage) + "%",
                {{"cpu_usage_percent", cpu_usage}, {"max_allowed_percent", max_cpu_percent}}
            };
        } catch (const std::exception& e) {
            return HealthCheckResult{
                false, "unhealthy",
                "CPU check failed: " + std::string(e.what()),
                {{"error", e.what()}}
            };
        }
    };
}

HealthCheckFunction disk_space_health_check(double min_free_percent) {
    return [min_free_percent]() -> HealthCheckResult {
        try {
            struct statvfs stat;
            if (statvfs("/", &stat) != 0) {
                return HealthCheckResult{
                    false, "unhealthy",
                    "Failed to get disk space information",
                    {{"error", "statvfs_failed"}}
                };
            }

            double free_percent = 100.0 * stat.f_bavail / stat.f_blocks;

            if (free_percent < min_free_percent) {
                return HealthCheckResult{
                    false, "unhealthy",
                    "Low disk space: " + std::to_string(free_percent) + "% free (min: " + std::to_string(min_free_percent) + "%)",
                    {{"free_percent", free_percent}, {"min_required_percent", min_free_percent}}
                };
            }

            return HealthCheckResult{
                true, "healthy",
                "Disk space sufficient: " + std::to_string(free_percent) + "% free",
                {{"free_percent", free_percent}, {"min_required_percent", min_free_percent}}
            };
        } catch (const std::exception& e) {
            return HealthCheckResult{
                false, "unhealthy",
                "Disk space check failed: " + std::string(e.what()),
                {{"error", e.what()}}
            };
        }
    };
}

HealthCheckFunction dependency_health_check(const std::unordered_map<std::string, std::string>& dependencies,
                                           std::shared_ptr<HttpClient> http_client) {
    return [dependencies, http_client]() -> HealthCheckResult {
        // Production dependency health checks with real HTTP requests
        if (!http_client) {
            return HealthCheckResult{
                false, "unhealthy",
                "HTTP client not initialized for dependency checks",
                {{"error", "null_http_client"}}
            };
        }

        try {
            nlohmann::json dependency_status = nlohmann::json::object();
            bool all_healthy = true;
            std::string unhealthy_services;

            for (const auto& [service, endpoint] : dependencies) {
                try {
                    auto start_time = std::chrono::high_resolution_clock::now();
                    
                    // Set timeout for dependency check (shorter than main health check)
                    http_client->set_timeout(2000); // 2 second timeout
                    
                    HttpResponse response = http_client->get(endpoint);
                    
                    auto end_time = std::chrono::high_resolution_clock::now();
                    auto response_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        end_time - start_time).count();

                    bool is_healthy = (response.status_code >= 200 && response.status_code < 300);
                    
                    dependency_status[service] = {
                        {"healthy", is_healthy},
                        {"endpoint", endpoint},
                        {"response_time_ms", response_time_ms},
                        {"status_code", response.status_code}
                    };

                    if (!is_healthy) {
                        all_healthy = false;
                        if (!unhealthy_services.empty()) unhealthy_services += ", ";
                        unhealthy_services += service;
                    }
                } catch (const std::exception& e) {
                    dependency_status[service] = {
                        {"healthy", false},
                        {"endpoint", endpoint},
                        {"error", e.what()}
                    };
                    all_healthy = false;
                    if (!unhealthy_services.empty()) unhealthy_services += ", ";
                    unhealthy_services += service;
                }
            }

            if (!all_healthy) {
                return HealthCheckResult{
                    false, "degraded",
                    "Some dependencies are unhealthy: " + unhealthy_services,
                    {{"dependencies", dependency_status}, {"unhealthy_services", unhealthy_services}}
                };
            }

            return HealthCheckResult{
                true, "healthy",
                "All service dependencies are healthy",
                {{"dependencies", dependency_status}}
            };
        } catch (const std::exception& e) {
            return HealthCheckResult{
                false, "unhealthy",
                "Dependency check failed: " + std::string(e.what()),
                {{"error", e.what()}}
            };
        }
    };
}

HealthCheckFunction queue_depth_health_check(std::shared_ptr<EventBus> event_bus, size_t max_queue_depth) {
    return [event_bus, max_queue_depth]() -> HealthCheckResult {
        // Production queue depth monitoring from EventBus
        if (!event_bus) {
            return HealthCheckResult{
                false, "unhealthy",
                "Event bus not initialized",
                {{"error", "null_event_bus"}}
            };
        }

        try {
            // Get actual queue depths from event bus
            size_t pending_events = event_bus->get_pending_event_count();
            size_t processing_events = event_bus->get_processing_event_count();
            size_t failed_events = event_bus->get_failed_event_count();
            size_t total_depth = pending_events + processing_events;
            
            // Get queue capacity and utilization
            size_t queue_capacity = event_bus->get_queue_capacity();
            double utilization_pct = (static_cast<double>(total_depth) / queue_capacity) * 100.0;

            if (total_depth > max_queue_depth) {
                return HealthCheckResult{
                    false, "unhealthy",
                    "Queue depth too high: " + std::to_string(total_depth) + " (max: " + std::to_string(max_queue_depth) + ")",
                    {
                        {"current_depth", total_depth},
                        {"max_depth", max_queue_depth},
                        {"pending_events", pending_events},
                        {"processing_events", processing_events},
                        {"failed_events", failed_events},
                        {"utilization_percent", utilization_pct}
                    }
                };
            }

            // Warn if queue is getting full
            if (utilization_pct > 80.0) {
                return HealthCheckResult{
                    true, "degraded",
                    "Queue utilization high: " + std::to_string(utilization_pct) + "%",
                    {
                        {"current_depth", total_depth},
                        {"max_depth", max_queue_depth},
                        {"pending_events", pending_events},
                        {"processing_events", processing_events},
                        {"failed_events", failed_events},
                        {"utilization_percent", utilization_pct},
                        {"warning", "high_utilization"}
                    }
                };
            }

            return HealthCheckResult{
                true, "healthy",
                "Queue depth within limits: " + std::to_string(total_depth),
                {
                    {"current_depth", total_depth},
                    {"max_depth", max_queue_depth},
                    {"pending_events", pending_events},
                    {"processing_events", processing_events},
                    {"failed_events", failed_events},
                    {"utilization_percent", utilization_pct}
                }
            };
        } catch (const std::exception& e) {
            return HealthCheckResult{
                false, "unhealthy",
                "Queue depth check failed: " + std::string(e.what()),
                {{"error", e.what()}}
            };
        }
    };
}

HealthCheckFunction thread_pool_health_check(std::shared_ptr<AgentOrchestrator> orchestrator, size_t min_available_threads) {
    return [orchestrator, min_available_threads]() -> HealthCheckResult {
        // Production thread pool monitoring from AgentOrchestrator
        if (!orchestrator) {
            return HealthCheckResult{
                false, "unhealthy",
                "Agent orchestrator not initialized",
                {{"error", "null_orchestrator"}}
            };
        }

        try {
            // Get actual thread pool statistics from orchestrator
            auto pool_stats = orchestrator->get_thread_pool_stats();
            
            size_t total_threads = pool_stats.value("total_threads", 0);
            size_t active_threads = pool_stats.value("active_threads", 0);
            size_t idle_threads = pool_stats.value("idle_threads", 0);
            size_t queued_tasks = pool_stats.value("queued_tasks", 0);
            size_t completed_tasks = pool_stats.value("completed_tasks", 0);
            
            // Calculate available threads (idle threads that can accept work)
            size_t available_threads = idle_threads;
            double utilization_pct = total_threads > 0 ? 
                (static_cast<double>(active_threads) / total_threads) * 100.0 : 0.0;

            if (available_threads < min_available_threads) {
                return HealthCheckResult{
                    false, "unhealthy",
                    "Insufficient available threads: " + std::to_string(available_threads) + " (min: " + std::to_string(min_available_threads) + ")",
                    {
                        {"total_threads", total_threads},
                        {"active_threads", active_threads},
                        {"idle_threads", idle_threads},
                        {"available_threads", available_threads},
                        {"min_required", min_available_threads},
                        {"queued_tasks", queued_tasks},
                        {"utilization_percent", utilization_pct}
                    }
                };
            }

            // Warn if thread pool is under pressure
            if (utilization_pct > 85.0 || queued_tasks > 100) {
                return HealthCheckResult{
                    true, "degraded",
                    "Thread pool under pressure: " + std::to_string(utilization_pct) + "% utilized, " + 
                    std::to_string(queued_tasks) + " tasks queued",
                    {
                        {"total_threads", total_threads},
                        {"active_threads", active_threads},
                        {"idle_threads", idle_threads},
                        {"available_threads", available_threads},
                        {"min_required", min_available_threads},
                        {"queued_tasks", queued_tasks},
                        {"completed_tasks", completed_tasks},
                        {"utilization_percent", utilization_pct},
                        {"warning", "high_utilization"}
                    }
                };
            }

            return HealthCheckResult{
                true, "healthy",
                "Thread pool healthy: " + std::to_string(available_threads) + " threads available",
                {
                    {"total_threads", total_threads},
                    {"active_threads", active_threads},
                    {"idle_threads", idle_threads},
                    {"available_threads", available_threads},
                    {"min_required", min_available_threads},
                    {"queued_tasks", queued_tasks},
                    {"completed_tasks", completed_tasks},
                    {"utilization_percent", utilization_pct}
                }
            };
        } catch (const std::exception& e) {
            return HealthCheckResult{
                false, "unhealthy",
                "Thread pool check failed: " + std::string(e.what()),
                {{"error", e.what()}}
            };
        }
    };
}

} // namespace health_checks

} // namespace regulens
