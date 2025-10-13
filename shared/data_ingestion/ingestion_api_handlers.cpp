/**
 * Data Ingestion Monitoring API Handlers
 * Production-Grade Implementation - NO STUBS
 * Real database operations for pipeline monitoring and quality checks
 */

#include <string>
#include <sstream>
#include <vector>
#include <libpq-fe.h>
#include <nlohmann/json.hpp>
#include <ctime>

using json = nlohmann::json;

namespace regulens {
namespace ingestion {

/**
 * GET /ingestion/metrics
 * Retrieve data ingestion pipeline metrics
 * Production: Queries data_ingestion_metrics table with aggregations
 */
std::string get_ingestion_metrics(PGconn* db_conn, const std::map<std::string, std::string>& query_params) {
    // Parse filters
    std::string source_name;
    std::string time_range = "24h";
    int limit = 100;

    if (query_params.find("source") != query_params.end()) {
        source_name = query_params.at("source");
    }
    if (query_params.find("time_range") != query_params.end()) {
        time_range = query_params.at("time_range");
    }
    if (query_params.find("limit") != query_params.end()) {
        limit = std::atoi(query_params.at("limit").c_str());
    }

    // Parse time range to hours
    int hours = 24;
    if (time_range.back() == 'h') {
        hours = std::atoi(time_range.substr(0, time_range.length()-1).c_str());
    } else if (time_range.back() == 'd') {
        hours = std::atoi(time_range.substr(0, time_range.length()-1).c_str()) * 24;
    }

    // Build query
    std::string query =
        "SELECT metric_id, source_name, source_type, pipeline_name, ingestion_timestamp, "
        "records_ingested, records_failed, records_skipped, records_updated, records_deleted, "
        "bytes_processed, duration_ms, throughput_records_per_sec, throughput_mb_per_sec, "
        "error_count, error_messages, warning_count, status, lag_seconds, batch_id, "
        "execution_host, memory_used_mb, cpu_usage_percent "
        "FROM data_ingestion_metrics "
        "WHERE ingestion_timestamp >= CURRENT_TIMESTAMP - INTERVAL '" + std::to_string(hours) + " hours' ";

    std::vector<std::string> params;
    if (!source_name.empty()) {
        query += "AND source_name = $1 ";
        params.push_back(source_name);
    }

    query += "ORDER BY ingestion_timestamp DESC LIMIT " + std::to_string(limit);

    // Execute query
    PGresult* result;
    if (params.empty()) {
        result = PQexec(db_conn, query.c_str());
    } else {
        const char* paramValues[1] = {params[0].c_str()};
        result = PQexecParams(db_conn, query.c_str(), 1, NULL, paramValues, NULL, NULL, 0);
    }

    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        std::string error = PQerrorMessage(db_conn);
        PQclear(result);
        return "{\"error\":\"Database query failed: " + error + "\"}";
    }

    json metrics_array = json::array();
    int count = PQntuples(result);

    for (int i = 0; i < count; i++) {
        json metric;
        metric["id"] = PQgetvalue(result, i, 0);
        metric["sourceName"] = PQgetvalue(result, i, 1);
        metric["sourceType"] = PQgetvalue(result, i, 2);
        if (!PQgetisnull(result, i, 3)) metric["pipelineName"] = PQgetvalue(result, i, 3);
        metric["timestamp"] = PQgetvalue(result, i, 4);
        metric["recordsIngested"] = std::atoi(PQgetvalue(result, i, 5));
        metric["recordsFailed"] = std::atoi(PQgetvalue(result, i, 6));
        metric["recordsSkipped"] = std::atoi(PQgetvalue(result, i, 7));
        metric["recordsUpdated"] = std::atoi(PQgetvalue(result, i, 8));
        metric["recordsDeleted"] = std::atoi(PQgetvalue(result, i, 9));
        metric["bytesProcessed"] = std::atoll(PQgetvalue(result, i, 10));
        if (!PQgetisnull(result, i, 11)) metric["durationMs"] = std::atoi(PQgetvalue(result, i, 11));
        if (!PQgetisnull(result, i, 12)) metric["throughputRecordsPerSec"] = std::atof(PQgetvalue(result, i, 12));
        if (!PQgetisnull(result, i, 13)) metric["throughputMbPerSec"] = std::atof(PQgetvalue(result, i, 13));
        metric["errorCount"] = std::atoi(PQgetvalue(result, i, 14));

        if (!PQgetisnull(result, i, 15)) {
            try {
                metric["errorMessages"] = json::parse(PQgetvalue(result, i, 15));
            } catch(...) {
                metric["errorMessages"] = json::array();
            }
        }

        metric["warningCount"] = std::atoi(PQgetvalue(result, i, 16));
        metric["status"] = PQgetvalue(result, i, 17);
        if (!PQgetisnull(result, i, 18)) metric["lagSeconds"] = std::atoi(PQgetvalue(result, i, 18));
        if (!PQgetisnull(result, i, 19)) metric["batchId"] = PQgetvalue(result, i, 19);
        if (!PQgetisnull(result, i, 20)) metric["executionHost"] = PQgetvalue(result, i, 20);
        if (!PQgetisnull(result, i, 21)) metric["memoryUsedMb"] = std::atoi(PQgetvalue(result, i, 21));
        if (!PQgetisnull(result, i, 22)) metric["cpuUsagePercent"] = std::atof(PQgetvalue(result, i, 22));

        metrics_array.push_back(metric);
    }

    PQclear(result);

    // Get aggregated statistics
    std::string stats_query =
        "SELECT "
        "COUNT(*) as total_runs, "
        "SUM(records_ingested) as total_records, "
        "SUM(records_failed) as total_failed, "
        "SUM(bytes_processed) as total_bytes, "
        "AVG(duration_ms) as avg_duration, "
        "AVG(throughput_records_per_sec) as avg_throughput, "
        "COUNT(*) FILTER (WHERE status = 'success') as successful_runs, "
        "COUNT(*) FILTER (WHERE status = 'failed') as failed_runs, "
        "AVG(lag_seconds) as avg_lag "
        "FROM data_ingestion_metrics "
        "WHERE ingestion_timestamp >= CURRENT_TIMESTAMP - INTERVAL '" + std::to_string(hours) + " hours' ";

    if (!source_name.empty()) {
        stats_query += "AND source_name = '" + source_name + "' ";
    }

    PGresult* stats_result = PQexec(db_conn, stats_query.c_str());

    json summary;
    if (PQresultStatus(stats_result) == PGRES_TUPLES_OK && PQntuples(stats_result) > 0) {
        summary["totalRuns"] = std::atoi(PQgetvalue(stats_result, 0, 0));
        summary["totalRecords"] = std::atoll(PQgetvalue(stats_result, 0, 1));
        summary["totalFailed"] = std::atoll(PQgetvalue(stats_result, 0, 2));
        summary["totalBytes"] = std::atoll(PQgetvalue(stats_result, 0, 3));
        if (!PQgetisnull(stats_result, 0, 4)) summary["avgDurationMs"] = std::atof(PQgetvalue(stats_result, 0, 4));
        if (!PQgetisnull(stats_result, 0, 5)) summary["avgThroughput"] = std::atof(PQgetvalue(stats_result, 0, 5));
        summary["successfulRuns"] = std::atoi(PQgetvalue(stats_result, 0, 6));
        summary["failedRuns"] = std::atoi(PQgetvalue(stats_result, 0, 7));
        if (!PQgetisnull(stats_result, 0, 8)) summary["avgLagSeconds"] = std::atof(PQgetvalue(stats_result, 0, 8));

        // Calculate success rate
        int total = summary["totalRuns"];
        int successful = summary["successfulRuns"];
        if (total > 0) {
            summary["successRate"] = (double)successful / total;
        } else {
            summary["successRate"] = 0.0;
        }
    }

    PQclear(stats_result);

    // Get per-source breakdown
    std::string sources_query =
        "SELECT source_name, source_type, COUNT(*) as run_count, "
        "SUM(records_ingested) as total_records, "
        "AVG(throughput_records_per_sec) as avg_throughput, "
        "MAX(ingestion_timestamp) as last_run "
        "FROM data_ingestion_metrics "
        "WHERE ingestion_timestamp >= CURRENT_TIMESTAMP - INTERVAL '" + std::to_string(hours) + " hours' "
        "GROUP BY source_name, source_type "
        "ORDER BY total_records DESC LIMIT 20";

    PGresult* sources_result = PQexec(db_conn, sources_query.c_str());

    json sources_array = json::array();
    if (PQresultStatus(sources_result) == PGRES_TUPLES_OK) {
        int source_count = PQntuples(sources_result);
        for (int i = 0; i < source_count; i++) {
            json source;
            source["name"] = PQgetvalue(sources_result, i, 0);
            source["type"] = PQgetvalue(sources_result, i, 1);
            source["runCount"] = std::atoi(PQgetvalue(sources_result, i, 2));
            source["totalRecords"] = std::atoll(PQgetvalue(sources_result, i, 3));
            if (!PQgetisnull(sources_result, i, 4)) source["avgThroughput"] = std::atof(PQgetvalue(sources_result, i, 4));
            source["lastRun"] = PQgetvalue(sources_result, i, 5);
            sources_array.push_back(source);
        }
    }

    PQclear(sources_result);

    // Build response
    json response;
    response["metrics"] = metrics_array;
    response["summary"] = summary;
    response["sources"] = sources_array;
    response["timeRange"] = time_range;
    response["count"] = count;

    return response.dump();
}

/**
 * GET /ingestion/quality-checks
 * Retrieve data quality check results
 * Production: Queries data_quality_checks and data_quality_summary tables
 */
std::string get_quality_checks(PGconn* db_conn, const std::map<std::string, std::string>& query_params) {
    // Parse filters
    std::string source_name;
    std::string table_name;
    std::string check_type;
    std::string severity;
    bool failed_only = false;
    int limit = 100;

    if (query_params.find("source") != query_params.end()) {
        source_name = query_params.at("source");
    }
    if (query_params.find("table") != query_params.end()) {
        table_name = query_params.at("table");
    }
    if (query_params.find("type") != query_params.end()) {
        check_type = query_params.at("type");
    }
    if (query_params.find("severity") != query_params.end()) {
        severity = query_params.at("severity");
    }
    if (query_params.find("failed_only") != query_params.end()) {
        failed_only = query_params.at("failed_only") == "true";
    }
    if (query_params.find("limit") != query_params.end()) {
        limit = std::atoi(query_params.at("limit").c_str());
    }

    // Build query
    std::string query =
        "SELECT check_id, source_name, table_name, column_name, check_type, check_name, "
        "check_description, executed_at, passed, quality_score, records_checked, "
        "records_passed, records_failed, null_count, failure_rate, failure_examples, "
        "severity, threshold_min, threshold_max, measured_value, expected_value, "
        "deviation, recommendation, remediation_action, remediation_status "
        "FROM data_quality_checks "
        "WHERE executed_at >= CURRENT_TIMESTAMP - INTERVAL '7 days' ";

    std::vector<std::string> params;
    int param_idx = 1;

    if (!source_name.empty()) {
        query += "AND source_name = $" + std::to_string(param_idx++) + " ";
        params.push_back(source_name);
    }
    if (!table_name.empty()) {
        query += "AND table_name = $" + std::to_string(param_idx++) + " ";
        params.push_back(table_name);
    }
    if (!check_type.empty()) {
        query += "AND check_type = $" + std::to_string(param_idx++) + " ";
        params.push_back(check_type);
    }
    if (!severity.empty()) {
        query += "AND severity = $" + std::to_string(param_idx++) + " ";
        params.push_back(severity);
    }
    if (failed_only) {
        query += "AND passed = false ";
    }

    query += "ORDER BY executed_at DESC, severity DESC LIMIT " + std::to_string(limit);

    // Execute query
    std::vector<const char*> paramValues;
    for (const auto& p : params) {
        paramValues.push_back(p.c_str());
    }

    PGresult* result = PQexecParams(db_conn, query.c_str(), paramValues.size(), NULL,
                                   paramValues.empty() ? NULL : paramValues.data(), NULL, NULL, 0);

    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        std::string error = PQerrorMessage(db_conn);
        PQclear(result);
        return "{\"error\":\"Database query failed: " + error + "\"}";
    }

    json checks_array = json::array();
    int count = PQntuples(result);

    for (int i = 0; i < count; i++) {
        json check;
        check["id"] = PQgetvalue(result, i, 0);
        if (!PQgetisnull(result, i, 1)) check["sourceName"] = PQgetvalue(result, i, 1);
        check["tableName"] = PQgetvalue(result, i, 2);
        if (!PQgetisnull(result, i, 3)) check["columnName"] = PQgetvalue(result, i, 3);
        check["checkType"] = PQgetvalue(result, i, 4);
        check["checkName"] = PQgetvalue(result, i, 5);
        if (!PQgetisnull(result, i, 6)) check["description"] = PQgetvalue(result, i, 6);
        check["executedAt"] = PQgetvalue(result, i, 7);
        check["passed"] = std::string(PQgetvalue(result, i, 8)) == "t";
        if (!PQgetisnull(result, i, 9)) check["qualityScore"] = std::atof(PQgetvalue(result, i, 9));
        if (!PQgetisnull(result, i, 10)) check["recordsChecked"] = std::atoi(PQgetvalue(result, i, 10));
        if (!PQgetisnull(result, i, 11)) check["recordsPassed"] = std::atoi(PQgetvalue(result, i, 11));
        if (!PQgetisnull(result, i, 12)) check["recordsFailed"] = std::atoi(PQgetvalue(result, i, 12));
        if (!PQgetisnull(result, i, 13)) check["nullCount"] = std::atoi(PQgetvalue(result, i, 13));
        if (!PQgetisnull(result, i, 14)) check["failureRate"] = std::atof(PQgetvalue(result, i, 14));

        if (!PQgetisnull(result, i, 15)) {
            try {
                check["failureExamples"] = json::parse(PQgetvalue(result, i, 15));
            } catch(...) {
                check["failureExamples"] = json::array();
            }
        }

        check["severity"] = PQgetvalue(result, i, 16);
        if (!PQgetisnull(result, i, 17)) check["thresholdMin"] = std::atof(PQgetvalue(result, i, 17));
        if (!PQgetisnull(result, i, 18)) check["thresholdMax"] = std::atof(PQgetvalue(result, i, 18));
        if (!PQgetisnull(result, i, 19)) check["measuredValue"] = std::atof(PQgetvalue(result, i, 19));
        if (!PQgetisnull(result, i, 20)) check["expectedValue"] = std::atof(PQgetvalue(result, i, 20));
        if (!PQgetisnull(result, i, 21)) check["deviation"] = std::atof(PQgetvalue(result, i, 21));
        if (!PQgetisnull(result, i, 22)) check["recommendation"] = PQgetvalue(result, i, 22);
        if (!PQgetisnull(result, i, 23)) check["remediationAction"] = PQgetvalue(result, i, 23);
        if (!PQgetisnull(result, i, 24)) check["remediationStatus"] = PQgetvalue(result, i, 24);

        checks_array.push_back(check);
    }

    PQclear(result);

    // Get quality summary statistics
    std::string summary_query =
        "SELECT "
        "COUNT(*) as total_checks, "
        "COUNT(*) FILTER (WHERE passed = true) as passed_checks, "
        "COUNT(*) FILTER (WHERE passed = false) as failed_checks, "
        "COUNT(*) FILTER (WHERE severity = 'critical') as critical_issues, "
        "COUNT(*) FILTER (WHERE severity = 'high') as high_issues, "
        "AVG(quality_score) as avg_quality_score "
        "FROM data_quality_checks "
        "WHERE executed_at >= CURRENT_TIMESTAMP - INTERVAL '7 days' ";

    if (!source_name.empty()) {
        summary_query += "AND source_name = '" + source_name + "' ";
    }
    if (!table_name.empty()) {
        summary_query += "AND table_name = '" + table_name + "' ";
    }

    PGresult* summary_result = PQexec(db_conn, summary_query.c_str());

    json summary;
    if (PQresultStatus(summary_result) == PGRES_TUPLES_OK && PQntuples(summary_result) > 0) {
        summary["totalChecks"] = std::atoi(PQgetvalue(summary_result, 0, 0));
        summary["passedChecks"] = std::atoi(PQgetvalue(summary_result, 0, 1));
        summary["failedChecks"] = std::atoi(PQgetvalue(summary_result, 0, 2));
        summary["criticalIssues"] = std::atoi(PQgetvalue(summary_result, 0, 3));
        summary["highIssues"] = std::atoi(PQgetvalue(summary_result, 0, 4));
        if (!PQgetisnull(summary_result, 0, 5)) summary["avgQualityScore"] = std::atof(PQgetvalue(summary_result, 0, 5));

        // Calculate pass rate
        int total = summary["totalChecks"];
        int passed = summary["passedChecks"];
        if (total > 0) {
            summary["passRate"] = (double)passed / total;
        } else {
            summary["passRate"] = 0.0;
        }
    }

    PQclear(summary_result);

    // Get latest quality scores by table
    std::string tables_query =
        "SELECT DISTINCT ON (table_name) "
        "table_name, source_name, overall_quality_score, completeness_score, "
        "accuracy_score, validity_score, total_records, quality_issues_count, "
        "critical_issues_count, snapshot_date "
        "FROM data_quality_summary "
        "ORDER BY table_name, snapshot_date DESC "
        "LIMIT 20";

    PGresult* tables_result = PQexec(db_conn, tables_query.c_str());

    json tables_array = json::array();
    if (PQresultStatus(tables_result) == PGRES_TUPLES_OK) {
        int table_count = PQntuples(tables_result);
        for (int i = 0; i < table_count; i++) {
            json table;
            table["tableName"] = PQgetvalue(tables_result, i, 0);
            if (!PQgetisnull(tables_result, i, 1)) table["sourceName"] = PQgetvalue(tables_result, i, 1);
            if (!PQgetisnull(tables_result, i, 2)) table["overallScore"] = std::atof(PQgetvalue(tables_result, i, 2));
            if (!PQgetisnull(tables_result, i, 3)) table["completeness"] = std::atof(PQgetvalue(tables_result, i, 3));
            if (!PQgetisnull(tables_result, i, 4)) table["accuracy"] = std::atof(PQgetvalue(tables_result, i, 4));
            if (!PQgetisnull(tables_result, i, 5)) table["validity"] = std::atof(PQgetvalue(tables_result, i, 5));
            if (!PQgetisnull(tables_result, i, 6)) table["totalRecords"] = std::atoll(PQgetvalue(tables_result, i, 6));
            if (!PQgetisnull(tables_result, i, 7)) table["issuesCount"] = std::atoi(PQgetvalue(tables_result, i, 7));
            if (!PQgetisnull(tables_result, i, 8)) table["criticalIssues"] = std::atoi(PQgetvalue(tables_result, i, 8));
            table["lastChecked"] = PQgetvalue(tables_result, i, 9);
            tables_array.push_back(table);
        }
    }

    PQclear(tables_result);

    // Build response
    json response;
    response["checks"] = checks_array;
    response["summary"] = summary;
    response["tables"] = tables_array;
    response["count"] = count;

    return response.dump();
}

} // namespace ingestion
} // namespace regulens
