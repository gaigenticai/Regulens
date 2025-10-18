/**
 * AsyncJobManager - Production-Grade Async Job Processing
 * 
 * Enterprise-grade job queue management with:
 * - Priority-based job scheduling
 * - Multiple execution modes (SYNC, ASYNC, BATCH, STREAMING)
 * - Worker thread pool management
 * - Progress tracking and monitoring
 * - Batch execution support
 * - Fault tolerance and retry logic
 * - Cloud-deployable configuration
 */

#pragma once

#include <string>
#include <vector>
#include <queue>
#include <map>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <optional>
#include <chrono>
#include <nlohmann/json.hpp>
#include <libpq-fe.h>
#include "../database/postgresql_connection.hpp"
#include "../logging/structured_logger.hpp"
#include "../config/configuration_manager.hpp"
#include "../error_handler.hpp"

using json = nlohmann::json;

namespace regulens {
namespace async_jobs {

/**
 * ExecutionMode enum for different job processing strategies
 */
enum class ExecutionMode {
    SYNCHRONOUS,    // Immediate execution, caller waits
    ASYNCHRONOUS,   // Background execution, caller gets job ID
    BATCH,          // Multiple items processed in batches
    STREAMING       // Real-time streaming of results
};

/**
 * JobStatus enum for job lifecycle tracking
 */
enum class JobStatus {
    PENDING,        // Waiting for execution
    RUNNING,        // Currently executing
    COMPLETED,      // Successfully finished
    FAILED,         // Execution failed
    CANCELLED       // User cancelled
};

/**
 * JobPriority enum for job scheduling
 */
enum class JobPriority {
    LOW = 0,
    MEDIUM = 1,
    HIGH = 2,
    CRITICAL = 3
};

/**
 * Job structure representing a single async job
 */
struct AsyncJob {
    std::string job_id;
    std::string job_type;
    std::string user_id;
    ExecutionMode execution_mode;
    JobStatus status;
    JobPriority priority;
    json request_payload;
    json result_payload;
    std::string error_message;
    int progress_percentage;
    int estimated_completion_time; // seconds
    std::chrono::system_clock::time_point started_at;
    std::chrono::system_clock::time_point completed_at;
    std::chrono::system_clock::time_point created_at;
    json metadata;
};

/**
 * JobResult represents individual result item from a job
 */
struct JobResult {
    std::string result_id;
    std::string job_id;
    std::string batch_id;
    int item_index;
    bool success;
    json output_data;
    json error_details;
    int execution_time_ms;
};

/**
 * Worker thread for processing async jobs
 */
class JobWorker {
public:
    JobWorker(const std::string& worker_id, 
              std::shared_ptr<PostgreSQLConnection> db_conn,
              std::shared_ptr<StructuredLogger> logger);
    ~JobWorker();

    void start();
    void stop();
    bool is_running() const { return running_; }
    size_t jobs_processed() const { return jobs_processed_; }

private:
    std::string worker_id_;
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::shared_ptr<StructuredLogger> logger_;
    std::atomic<bool> running_;
    std::thread worker_thread_;
    std::atomic<size_t> jobs_processed_;

    void worker_loop();
    std::optional<AsyncJob> claim_next_job();
    void process_job(const AsyncJob& job);
    void update_job_progress(const std::string& job_id, int progress, const std::string& status);
    void finalize_job(const std::string& job_id, bool success, const json& result = json::object(), const std::string& error = "");
    bool should_retry(int attempt, const std::string& error);
};

/**
 * Main AsyncJobManager for managing job queue and worker threads
 */
class AsyncJobManager {
public:
    AsyncJobManager(
        std::shared_ptr<PostgreSQLConnection> db_conn,
        std::shared_ptr<StructuredLogger> logger,
        std::shared_ptr<ConfigurationManager> config,
        std::shared_ptr<ErrorHandler> error_handler
    );
    
    ~AsyncJobManager();

    /**
     * Initialize the job manager with worker threads
     */
    bool initialize();

    /**
     * Shutdown the job manager gracefully
     */
    void shutdown();

    /**
     * Submit a new job to the queue
     * @param job_type Type of job (e.g., "rule_evaluation", "policy_generation")
     * @param user_id User who submitted the job
     * @param execution_mode How to execute the job
     * @param request_payload Job parameters
     * @param priority Job priority
     * @return Job ID if successful, empty string on error
     */
    std::string submit_job(
        const std::string& job_type,
        const std::string& user_id,
        ExecutionMode execution_mode,
        const json& request_payload,
        JobPriority priority = JobPriority::MEDIUM
    );

    /**
     * Get job status and details
     */
    std::optional<AsyncJob> get_job(const std::string& job_id);

    /**
     * Get jobs for a specific user
     */
    std::vector<AsyncJob> get_user_jobs(const std::string& user_id, const std::string& status_filter = "");

    /**
     * Get queue statistics
     */
    json get_queue_stats();

    /**
     * Cancel a job
     */
    bool cancel_job(const std::string& job_id);

    /**
     * Get job results
     */
    std::vector<JobResult> get_job_results(const std::string& job_id);

    /**
     * Submit a batch job with multiple items
     */
    std::string submit_batch_job(
        const std::string& job_type,
        const std::string& user_id,
        const json& items,
        size_t batch_size = 100,
        JobPriority priority = JobPriority::MEDIUM
    );

    /**
     * Get batch execution details
     */
    json get_batch_details(const std::string& batch_id);

    /**
     * Register a job handler function
     */
    using JobHandler = std::function<json(const json& payload)>;
    void register_job_handler(const std::string& job_type, JobHandler handler);

    /**
     * Health check for async job system
     */
    json get_health_status();

    /**
     * Get worker statistics
     */
    json get_worker_statistics();

    /**
     * Get system metrics
     */
    json get_system_metrics();

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::shared_ptr<StructuredLogger> logger_;
    std::shared_ptr<ConfigurationManager> config_;
    std::shared_ptr<ErrorHandler> error_handler_;

    std::vector<std::shared_ptr<JobWorker>> workers_;
    std::mutex workers_mutex_;
    std::atomic<bool> running_;

    std::map<std::string, JobHandler> job_handlers_;
    std::mutex handlers_mutex_;

    // Statistics
    std::atomic<size_t> total_jobs_submitted_;
    std::atomic<size_t> total_jobs_completed_;
    std::atomic<size_t> total_jobs_failed_;
    std::atomic<size_t> active_jobs_;

    // Configuration
    size_t worker_thread_count_;
    int job_timeout_seconds_;
    int max_retries_;
    int retry_backoff_seconds_;

    // Database operations
    AsyncJob load_job_from_db(const std::string& job_id);
    std::vector<AsyncJob> load_jobs_from_db(const std::string& query, const std::vector<std::string>& params);
    bool persist_job_to_db(const AsyncJob& job);
    bool update_job_status(const std::string& job_id, JobStatus status, const json& result = json::object());
    bool persist_job_result(const JobResult& result);

    // Utility
    std::string generate_job_id();
    std::string generate_result_id();
    std::string generate_batch_id();
    ExecutionMode parse_execution_mode(const std::string& mode);
    std::string execution_mode_to_string(ExecutionMode mode);
    JobStatus string_to_job_status(const std::string& status);
    std::string job_status_to_string(JobStatus status);
};

} // namespace async_jobs
} // namespace regulens

#endif // REGULENS_ASYNC_JOB_MANAGER_HPP
