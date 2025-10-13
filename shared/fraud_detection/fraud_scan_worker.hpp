/**
 * Fraud Scan Worker - Production-grade batch fraud detection processing
 * NO STUBS, NO MOCKS - Real background processing with job queue
 */

#ifndef REGULENS_FRAUD_SCAN_WORKER_HPP
#define REGULENS_FRAUD_SCAN_WORKER_HPP

#include <string>
#include <thread>
#include <atomic>
#include <optional>
#include <libpq-fe.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace regulens {
namespace fraud {

/**
 * ScanJob structure representing a fraud scan job
 */
struct ScanJob {
    std::string job_id;
    json filters;
    std::string created_by;
};

/**
 * FraudScanWorker - Background worker for processing fraud scan jobs
 * Production-grade implementation with atomic job claiming and progress tracking
 */
class FraudScanWorker {
public:
    FraudScanWorker(PGconn* db_conn, const std::string& worker_id);
    ~FraudScanWorker();

    void start();
    void stop();
    bool is_running() const { return running_; }

private:
    PGconn* db_conn_;
    std::string worker_id_;
    std::atomic<bool> running_;
    std::thread worker_thread_;

    std::optional<ScanJob> claim_next_job();
    void process_job(const ScanJob& job);
    void update_job_progress(const std::string& job_id, int processed, int flagged);
    void finalize_job(const std::string& job_id, bool success, const std::string& error = "");
    bool apply_fraud_rules(const std::string& txn_id, double amount, const std::string& currency,
                          const std::string& from_account, const std::string& to_account,
                          const std::string& txn_type);
    bool evaluate_fraud_rule(const std::string& rule_definition, const std::string& rule_type,
                           double amount, const std::string& currency,
                           const std::string& from_account, const std::string& to_account,
                           const std::string& txn_type);
};

} // namespace fraud
} // namespace regulens

#endif // REGULENS_FRAUD_SCAN_WORKER_HPP
