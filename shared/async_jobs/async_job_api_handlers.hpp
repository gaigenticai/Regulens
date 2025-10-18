/**
 * Async Jobs API Handlers - Production-Grade REST Endpoints
 * 
 * Comprehensive REST API for async job management:
 * - Job submission with priority control
 * - Job status polling
 * - Batch job processing
 * - Job cancellation and cleanup
 * - Queue statistics and monitoring
 * - Job history and results retrieval
 */

#pragma once

#include <string>
#include <memory>
#include <map>
#include <nlohmann/json.hpp>
#include "async_job_manager.hpp"
#include "../database/postgresql_connection.hpp"
#include "../logging/structured_logger.hpp"
#include "../error_handler.hpp"

using json = nlohmann::json;

namespace regulens {
namespace async_jobs {

/**
 * HTTP Response structure for consistency with system
 */
struct HTTPResponse {
    int status_code;
    std::string status_message;
    std::string body;
    std::string content_type;
};

/**
 * API Handlers for async job management
 */
class AsyncJobAPIHandlers {
public:
    AsyncJobAPIHandlers(
        std::shared_ptr<AsyncJobManager> job_manager,
        std::shared_ptr<StructuredLogger> logger,
        std::shared_ptr<ErrorHandler> error_handler
    );

    ~AsyncJobAPIHandlers();

    /**
     * POST /api/jobs - Submit a new job
     * Body: {
     *   "job_type": "rule_evaluation",
     *   "execution_mode": "ASYNCHRONOUS",
     *   "priority": "HIGH",
     *   "payload": {...}
     * }
     */
    HTTPResponse handle_submit_job(const std::string& request_body, const std::string& user_id);

    /**
     * GET /api/jobs/{job_id} - Get job status and details
     */
    HTTPResponse handle_get_job(const std::string& job_id);

    /**
     * GET /api/jobs - List user's jobs
     * Query params: status=PENDING|RUNNING|COMPLETED|FAILED&limit=100&offset=0
     */
    HTTPResponse handle_list_jobs(const std::map<std::string, std::string>& query_params, const std::string& user_id);

    /**
     * POST /api/jobs/{job_id}/cancel - Cancel a job
     */
    HTTPResponse handle_cancel_job(const std::string& job_id, const std::string& user_id);

    /**
     * GET /api/jobs/{job_id}/results - Get job results
     * Query params: limit=50&offset=0
     */
    HTTPResponse handle_get_job_results(const std::string& job_id, const std::map<std::string, std::string>& query_params);

    /**
     * POST /api/jobs/batch - Submit batch job
     * Body: {
     *   "job_type": "bulk_evaluation",
     *   "items": [...],
     *   "batch_size": 100,
     *   "priority": "MEDIUM"
     * }
     */
    HTTPResponse handle_submit_batch_job(const std::string& request_body, const std::string& user_id);

    /**
     * GET /api/jobs/batch/{batch_id} - Get batch execution details
     */
    HTTPResponse handle_get_batch_details(const std::string& batch_id);

    /**
     * GET /api/jobs/queue/stats - Get queue statistics
     * Query params: interval=1h|24h|7d
     */
    HTTPResponse handle_queue_statistics(const std::map<std::string, std::string>& query_params);

    /**
     * GET /api/jobs/health - Health check for async job system
     */
    HTTPResponse handle_health_check();

    /**
     * GET /api/jobs/metrics - Get system metrics
     */
    HTTPResponse handle_system_metrics();

    /**
     * POST /api/jobs/handler/register - Register custom job handler (admin only)
     * Body: {
     *   "job_type": "custom_processor",
     *   "handler_endpoint": "/api/custom/process"
     * }
     */
    HTTPResponse handle_register_job_handler(const std::string& request_body);

private:
    std::shared_ptr<AsyncJobManager> job_manager_;
    std::shared_ptr<StructuredLogger> logger_;
    std::shared_ptr<ErrorHandler> error_handler_;

    // Private helper methods
    HTTPResponse create_error_response(int status_code, const std::string& message);
    HTTPResponse create_success_response(const json& data, int status_code = 200);
    
    std::string parse_job_type(const json& request);
    ExecutionMode parse_execution_mode(const json& request);
    JobPriority parse_priority(const json& request);
    std::string execution_mode_to_api_string(ExecutionMode mode);
    std::string job_status_to_api_string(JobStatus status);
    std::string job_priority_to_string(JobPriority priority);
};

} // namespace async_jobs
} // namespace regulens

#endif // REGULENS_ASYNC_JOB_API_HANDLERS_HPP
