/**
 * Transaction API Handlers - Production-Grade Implementation
 * NO STUBS, NO MOCKS, NO PLACEHOLDERS - Real database operations only
 *
 * Implements comprehensive transaction management:
 * - CRUD operations for transactions
 * - Fraud analysis and risk assessment
 * - Transaction pattern detection
 * - Statistical analysis and metrics
 */

#include "transaction_api_handlers.hpp"
#include <nlohmann/json.hpp>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <random>

using json = nlohmann::json;

namespace regulens {
namespace transactions {

/**
 * GET /api/transactions
 * Get transactions list with filtering and pagination
 * Production: Queries transactions table with advanced filtering
 */
std::string get_transactions(PGconn* db_conn, const std::map<std::string, std::string>& query_params) {
    std::string query = "SELECT transaction_id, customer_id, amount, currency, transaction_type, "
                       "merchant_name, country_code, status, risk_score, transaction_date, "
                       "created_at, updated_at, flagged, approved_by, approved_at "
                       "FROM transactions WHERE 1=1 ";

    std::vector<std::string> params;
    int param_idx = 1;

    // Add filters
    if (query_params.find("customer_id") != query_params.end()) {
        query += " AND customer_id = $" + std::to_string(param_idx++);
        params.push_back(query_params.at("customer_id"));
    }
    if (query_params.find("status") != query_params.end()) {
        query += " AND status = $" + std::to_string(param_idx++);
        params.push_back(query_params.at("status"));
    }
    if (query_params.find("transaction_type") != query_params.end()) {
        query += " AND transaction_type = $" + std::to_string(param_idx++);
        params.push_back(query_params.at("transaction_type"));
    }
    if (query_params.find("country_code") != query_params.end()) {
        query += " AND country_code = $" + std::to_string(param_idx++);
        params.push_back(query_params.at("country_code"));
    }
    if (query_params.find("flagged") != query_params.end()) {
        query += " AND flagged = $" + std::to_string(param_idx++);
        params.push_back(query_params.at("flagged"));
    }
    if (query_params.find("min_amount") != query_params.end()) {
        query += " AND amount >= $" + std::to_string(param_idx++);
        params.push_back(query_params.at("min_amount"));
    }
    if (query_params.find("max_amount") != query_params.end()) {
        query += " AND amount <= $" + std::to_string(param_idx++);
        params.push_back(query_params.at("max_amount"));
    }

    // Add date range filter
    if (query_params.find("start_date") != query_params.end()) {
        query += " AND transaction_date >= $" + std::to_string(param_idx++);
        params.push_back(query_params.at("start_date"));
    }
    if (query_params.find("end_date") != query_params.end()) {
        query += " AND transaction_date <= $" + std::to_string(param_idx++);
        params.push_back(query_params.at("end_date"));
    }

    // Add sorting
    std::string sort_by = "transaction_date";
    std::string sort_order = "DESC";
    
    if (query_params.find("sort_by") != query_params.end()) {
        sort_by = query_params.at("sort_by");
    }
    if (query_params.find("sort_order") != query_params.end()) {
        sort_order = query_params.at("sort_order");
    }
    
    query += " ORDER BY " + sort_by + " " + sort_order;

    // Add pagination
    int limit = 50;
    int offset = 0;
    
    if (query_params.find("limit") != query_params.end()) {
        limit = std::atoi(query_params.at("limit").c_str());
        limit = std::min(limit, 1000); // Max limit
    }
    if (query_params.find("offset") != query_params.end()) {
        offset = std::atoi(query_params.at("offset").c_str());
    }
    
    query += " LIMIT $" + std::to_string(param_idx++) + " OFFSET $" + std::to_string(param_idx++);
    params.push_back(std::to_string(limit));
    params.push_back(std::to_string(offset));

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

    json transactions = json::array();
    int count = PQntuples(result);

    for (int i = 0; i < count; i++) {
        json transaction;
        transaction["id"] = PQgetvalue(result, i, 0);
        transaction["customerId"] = PQgetvalue(result, i, 1);
        transaction["amount"] = std::atof(PQgetvalue(result, i, 2));
        transaction["currency"] = PQgetvalue(result, i, 3);
        transaction["type"] = PQgetvalue(result, i, 4);
        transaction["merchantName"] = PQgetvalue(result, i, 5);
        transaction["countryCode"] = PQgetvalue(result, i, 6);
        transaction["status"] = PQgetvalue(result, i, 7);
        transaction["riskScore"] = std::atof(PQgetvalue(result, i, 8));
        transaction["transactionDate"] = PQgetvalue(result, i, 9);
        transaction["createdAt"] = PQgetvalue(result, i, 10);
        transaction["updatedAt"] = PQgetvalue(result, i, 11);
        transaction["flagged"] = std::string(PQgetvalue(result, i, 12)) == "t";
        
        if (!PQgetisnull(result, i, 13)) {
            transaction["approvedBy"] = PQgetvalue(result, i, 13);
        }
        if (!PQgetisnull(result, i, 14)) {
            transaction["approvedAt"] = PQgetvalue(result, i, 14);
        }

        transactions.push_back(transaction);
    }

    PQclear(result);

    // Get total count for pagination
    std::string count_query = "SELECT COUNT(*) FROM transactions WHERE 1=1 ";
    std::vector<std::string> count_params;
    int count_param_idx = 1;

    // Reapply filters for count
    if (query_params.find("customer_id") != query_params.end()) {
        count_query += " AND customer_id = $" + std::to_string(count_param_idx++);
        count_params.push_back(query_params.at("customer_id"));
    }
    if (query_params.find("status") != query_params.end()) {
        count_query += " AND status = $" + std::to_string(count_param_idx++);
        count_params.push_back(query_params.at("status"));
    }
    if (query_params.find("transaction_type") != query_params.end()) {
        count_query += " AND transaction_type = $" + std::to_string(count_param_idx++);
        count_params.push_back(query_params.at("transaction_type"));
    }
    if (query_params.find("country_code") != query_params.end()) {
        count_query += " AND country_code = $" + std::to_string(count_param_idx++);
        count_params.push_back(query_params.at("country_code"));
    }
    if (query_params.find("flagged") != query_params.end()) {
        count_query += " AND flagged = $" + std::to_string(count_param_idx++);
        count_params.push_back(query_params.at("flagged"));
    }
    if (query_params.find("min_amount") != query_params.end()) {
        count_query += " AND amount >= $" + std::to_string(count_param_idx++);
        count_params.push_back(query_params.at("min_amount"));
    }
    if (query_params.find("max_amount") != query_params.end()) {
        count_query += " AND amount <= $" + std::to_string(count_param_idx++);
        count_params.push_back(query_params.at("max_amount"));
    }
    if (query_params.find("start_date") != query_params.end()) {
        count_query += " AND transaction_date >= $" + std::to_string(count_param_idx++);
        count_params.push_back(query_params.at("start_date"));
    }
    if (query_params.find("end_date") != query_params.end()) {
        count_query += " AND transaction_date <= $" + std::to_string(count_param_idx++);
        count_params.push_back(query_params.at("end_date"));
    }

    std::vector<const char*> countParamValues;
    for (const auto& p : count_params) {
        countParamValues.push_back(p.c_str());
    }

    PGresult* count_result = PQexecParams(db_conn, count_query.c_str(), countParamValues.size(), NULL,
                                         countParamValues.empty() ? NULL : countParamValues.data(), NULL, NULL, 0);

    json response;
    response["transactions"] = transactions;
    response["pagination"] = {
        {"limit", limit},
        {"offset", offset},
        {"total", PQntuples(count_result) > 0 ? std::atoi(PQgetvalue(count_result, 0, 0)) : 0}
    };

    PQclear(count_result);
    return response.dump();
}

/**
 * GET /api/transactions/{id}
 * Get transaction by ID with full details
 * Production: Queries transactions table with related data
 */
std::string get_transaction_by_id(PGconn* db_conn, const std::string& transaction_id) {
    std::string query = "SELECT t.transaction_id, t.customer_id, t.amount, t.currency, "
                       "t.transaction_type, t.merchant_name, t.country_code, t.status, "
                       "t.risk_score, t.transaction_date, t.created_at, t.updated_at, "
                       "t.flagged, t.approved_by, t.approved_at, t.notes, "
                       "c.customer_name, c.customer_email, c.customer_type, "
                       "c.risk_rating as customer_risk_rating "
                       "FROM transactions t "
                       "LEFT JOIN customers c ON t.customer_id = c.customer_id "
                       "WHERE t.transaction_id = $1";

    const char* paramValues[1] = {transaction_id.c_str()};
    PGresult* result = PQexecParams(db_conn, query.c_str(), 1, NULL, paramValues, NULL, NULL, 0);

    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        std::string error = PQerrorMessage(db_conn);
        PQclear(result);
        return "{\"error\":\"Database query failed: " + error + "\"}";
    }

    if (PQntuples(result) == 0) {
        PQclear(result);
        return "{\"error\":\"Transaction not found\",\"transaction_id\":\"" + transaction_id + "\"}";
    }

    json transaction;
    transaction["id"] = PQgetvalue(result, 0, 0);
    transaction["customerId"] = PQgetvalue(result, 0, 1);
    transaction["amount"] = std::atof(PQgetvalue(result, 0, 2));
    transaction["currency"] = PQgetvalue(result, 0, 3);
    transaction["type"] = PQgetvalue(result, 0, 4);
    transaction["merchantName"] = PQgetvalue(result, 0, 5);
    transaction["countryCode"] = PQgetvalue(result, 0, 6);
    transaction["status"] = PQgetvalue(result, 0, 7);
    transaction["riskScore"] = std::atof(PQgetvalue(result, 0, 8));
    transaction["transactionDate"] = PQgetvalue(result, 0, 9);
    transaction["createdAt"] = PQgetvalue(result, 0, 10);
    transaction["updatedAt"] = PQgetvalue(result, 0, 11);
    transaction["flagged"] = std::string(PQgetvalue(result, 0, 12)) == "t";
    
    if (!PQgetisnull(result, 0, 13)) {
        transaction["approvedBy"] = PQgetvalue(result, 0, 13);
    }
    if (!PQgetisnull(result, 0, 14)) {
        transaction["approvedAt"] = PQgetvalue(result, 0, 14);
    }
    if (!PQgetisnull(result, 0, 15)) {
        transaction["notes"] = PQgetvalue(result, 0, 15);
    }

    // Customer information
    if (!PQgetisnull(result, 0, 16)) {
        json customer;
        customer["id"] = transaction["customerId"];
        customer["name"] = PQgetvalue(result, 0, 16);
        customer["email"] = PQgetvalue(result, 0, 17);
        customer["type"] = PQgetvalue(result, 0, 18);
        customer["riskRating"] = PQgetvalue(result, 0, 19);
        transaction["customer"] = customer;
    }

    PQclear(result);
    return transaction.dump();
}

/**
 * POST /api/transactions
 * Create a new transaction
 * Production: Inserts into transactions table with validation
 */
std::string create_transaction(PGconn* db_conn, const std::string& request_body, const std::string& user_id) {
    try {
        json req = json::parse(request_body);

        // Validate required fields
        if (!req.contains("customer_id") || !req.contains("amount") || !req.contains("currency")) {
            return "{\"error\":\"Missing required fields: customer_id, amount, currency\"}";
        }

        std::string customer_id = req["customer_id"];
        double amount = req["amount"];
        std::string currency = req["currency"];
        std::string transaction_type = req.value("transaction_type", "purchase");
        std::string merchant_name = req.value("merchant_name", "");
        std::string country_code = req.value("country_code", "US");
        std::string notes = req.value("notes", "");

        // Validate amount
        if (amount <= 0) {
            return "{\"error\":\"Amount must be positive\"}";
        }

        // Calculate initial risk score
        double risk_score = calculate_transaction_risk_score(db_conn, customer_id);

        // Determine if transaction should be flagged
        bool flagged = is_high_risk_transaction(db_conn, customer_id) || risk_score > 70.0;

        std::string query = "INSERT INTO transactions "
                           "(customer_id, amount, currency, transaction_type, merchant_name, "
                           "country_code, status, risk_score, flagged, notes, created_by) "
                           "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11) "
                           "RETURNING transaction_id, created_at, risk_score, flagged";

        std::string amount_str = std::to_string(amount);
        std::string risk_score_str = std::to_string(risk_score);
        std::string flagged_str = flagged ? "true" : "false";

        const char* paramValues[11] = {
            customer_id.c_str(),
            amount_str.c_str(),
            currency.c_str(),
            transaction_type.c_str(),
            merchant_name.c_str(),
            country_code.c_str(),
            flagged ? "pending_review" : "approved",
            risk_score_str.c_str(),
            flagged_str.c_str(),
            notes.c_str(),
            user_id.c_str()
        };

        PGresult* result = PQexecParams(db_conn, query.c_str(), 11, NULL, paramValues, NULL, NULL, 0);

        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            std::string error = PQerrorMessage(db_conn);
            PQclear(result);
            return "{\"error\":\"Failed to create transaction: " + error + "\"}";
        }

        json response;
        response["id"] = PQgetvalue(result, 0, 0);
        response["customerId"] = customer_id;
        response["amount"] = amount;
        response["currency"] = currency;
        response["type"] = transaction_type;
        response["merchantName"] = merchant_name;
        response["countryCode"] = country_code;
        response["status"] = flagged ? "pending_review" : "approved";
        response["riskScore"] = std::atof(PQgetvalue(result, 0, 2));
        response["flagged"] = std::string(PQgetvalue(result, 0, 3)) == "t";
        response["createdAt"] = PQgetvalue(result, 0, 1);
        response["notes"] = notes;

        PQclear(result);
        return response.dump();

    } catch (const json::exception& e) {
        return "{\"error\":\"Invalid JSON: " + std::string(e.what()) + "\"}";
    }
}

/**
 * PUT /api/transactions/{id}
 * Update an existing transaction
 * Production: Updates transactions table with validation
 */
std::string update_transaction(PGconn* db_conn, const std::string& transaction_id, const std::string& request_body) {
    try {
        json req = json::parse(request_body);

        std::vector<std::string> updates;
        std::vector<std::string> params;
        int param_index = 1;

        if (req.contains("status")) {
            updates.push_back("status = $" + std::to_string(param_index++));
            params.push_back(req["status"]);
        }
        if (req.contains("merchant_name")) {
            updates.push_back("merchant_name = $" + std::to_string(param_index++));
            params.push_back(req["merchant_name"]);
        }
        if (req.contains("notes")) {
            updates.push_back("notes = $" + std::to_string(param_index++));
            params.push_back(req["notes"]);
        }
        if (req.contains("risk_score")) {
            updates.push_back("risk_score = $" + std::to_string(param_index++));
            params.push_back(std::to_string(req["risk_score"].get<double>()));
        }

        if (updates.empty()) {
            return "{\"error\":\"No fields to update\"}";
        }

        updates.push_back("updated_at = CURRENT_TIMESTAMP");

        std::string query = "UPDATE transactions SET " + updates[0];
        for (size_t i = 1; i < updates.size(); i++) {
            query += ", " + updates[i];
        }
        query += " WHERE transaction_id = $" + std::to_string(param_index);
        query += " RETURNING transaction_id, status, updated_at, risk_score";

        params.push_back(transaction_id);

        std::vector<const char*> paramValues;
        for (const auto& p : params) {
            paramValues.push_back(p.c_str());
        }

        PGresult* result = PQexecParams(db_conn, query.c_str(), paramValues.size(), NULL,
                                       paramValues.data(), NULL, NULL, 0);

        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            std::string error = PQerrorMessage(db_conn);
            PQclear(result);
            return "{\"error\":\"Failed to update transaction: " + error + "\"}";
        }

        if (PQntuples(result) == 0) {
            PQclear(result);
            return "{\"error\":\"Transaction not found\",\"transaction_id\":\"" + transaction_id + "\"}";
        }

        json response;
        response["id"] = PQgetvalue(result, 0, 0);
        response["status"] = PQgetvalue(result, 0, 1);
        response["updatedAt"] = PQgetvalue(result, 0, 2);
        response["riskScore"] = std::atof(PQgetvalue(result, 0, 3));
        response["message"] = "Transaction updated successfully";

        PQclear(result);
        return response.dump();

    } catch (const json::exception& e) {
        return "{\"error\":\"Invalid JSON: " + std::string(e.what()) + "\"}";
    }
}

/**
 * DELETE /api/transactions/{id}
 * Delete a transaction (soft delete)
 * Production: Marks transaction as deleted rather than removing it
 */
std::string delete_transaction(PGconn* db_conn, const std::string& transaction_id) {
    std::string query = "UPDATE transactions SET status = 'deleted', updated_at = CURRENT_TIMESTAMP "
                       "WHERE transaction_id = $1 RETURNING transaction_id";

    const char* paramValues[1] = {transaction_id.c_str()};
    PGresult* result = PQexecParams(db_conn, query.c_str(), 1, NULL, paramValues, NULL, NULL, 0);

    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        std::string error = PQerrorMessage(db_conn);
        PQclear(result);
        return "{\"error\":\"Failed to delete transaction: " + error + "\"}";
    }

    if (PQntuples(result) == 0) {
        PQclear(result);
        return "{\"error\":\"Transaction not found\",\"transaction_id\":\"" + transaction_id + "\"}";
    }

    PQclear(result);
    return "{\"success\":true,\"message\":\"Transaction deleted successfully\",\"transaction_id\":\"" + transaction_id + "\"}";
}

/**
 * POST /api/transactions/{id}/analyze
 * Analyze transaction for fraud and risk
 * Production: Runs comprehensive fraud analysis
 */
std::string analyze_transaction(PGconn* db_conn, const std::string& transaction_id, const std::string& request_body) {
    try {
        json req = json::parse(request_body);
        std::string analysis_type = req.value("analysis_type", "comprehensive");

        // Get transaction details
        std::string txn_query = "SELECT customer_id, amount, currency, transaction_type, "
                               "country_code, merchant_name, transaction_date "
                               "FROM transactions WHERE transaction_id = $1";

        const char* txnParams[1] = {transaction_id.c_str()};
        PGresult* txn_result = PQexecParams(db_conn, txn_query.c_str(), 1, NULL, txnParams, NULL, NULL, 0);

        if (PQresultStatus(txn_result) != PGRES_TUPLES_OK || PQntuples(txn_result) == 0) {
            PQclear(txn_result);
            return "{\"error\":\"Transaction not found\"}";
        }

        std::string customer_id = PQgetvalue(txn_result, 0, 0);
        double amount = std::atof(PQgetvalue(txn_result, 0, 1));
        std::string currency = PQgetvalue(txn_result, 0, 2);
        std::string transaction_type = PQgetvalue(txn_result, 0, 3);
        std::string country_code = PQgetvalue(txn_result, 0, 4);
        std::string merchant_name = PQgetvalue(txn_result, 0, 5);
        std::string transaction_date = PQgetvalue(txn_result, 0, 6);

        PQclear(txn_result);

        // Generate analysis
        json analysis = generate_transaction_analysis(db_conn, transaction_id);

        // Store analysis result
        std::string insert_query = "INSERT INTO transaction_fraud_analysis "
                                  "(transaction_id, analysis_type, risk_score, risk_factors, "
                                  "recommendation, analysis_details, created_at) "
                                  "VALUES ($1, $2, $3, $4, $5, $6, CURRENT_TIMESTAMP) "
                                  "RETURNING analysis_id";

        std::string risk_score_str = std::to_string(analysis["riskScore"].get<double>());
        std::string risk_factors_json = analysis["riskFactors"].dump();
        std::string recommendation = analysis["recommendation"];
        std::string details_json = analysis["details"].dump();

        const char* insertParams[6] = {
            transaction_id.c_str(),
            analysis_type.c_str(),
            risk_score_str.c_str(),
            risk_factors_json.c_str(),
            recommendation.c_str(),
            details_json.c_str()
        };

        PGresult* insert_result = PQexecParams(db_conn, insert_query.c_str(), 6, NULL, insertParams, NULL, NULL, 0);

        if (PQresultStatus(insert_result) == PGRES_TUPLES_OK) {
            analysis["analysisId"] = PQgetvalue(insert_result, 0, 0);
        }

        PQclear(insert_result);

        // Update transaction risk score if needed
        double new_risk_score = analysis["riskScore"];
        std::string update_query = "UPDATE transactions SET risk_score = $1, "
                                  "flagged = $2, updated_at = CURRENT_TIMESTAMP "
                                  "WHERE transaction_id = $3";

        std::string new_risk_str = std::to_string(new_risk_score);
        std::string flagged_str = new_risk_score > 70.0 ? "true" : "false";

        const char* updateParams[3] = {
            new_risk_str.c_str(),
            flagged_str.c_str(),
            transaction_id.c_str()
        };

        PQexecParams(db_conn, update_query.c_str(), 3, NULL, updateParams, NULL, NULL, 0);

        return analysis.dump();

    } catch (const json::exception& e) {
        return "{\"error\":\"Invalid JSON: " + std::string(e.what()) + "\"}";
    }
}

/**
 * GET /api/transactions/{id}/fraud-analysis
 * Get fraud analysis for a transaction
 * Production: Returns stored analysis results
 */
std::string get_fraud_analysis(PGconn* db_conn, const std::string& transaction_id) {
    std::string query = "SELECT analysis_id, analysis_type, risk_score, risk_factors, "
                       "recommendation, analysis_details, created_at "
                       "FROM transaction_fraud_analysis "
                       "WHERE transaction_id = $1 "
                       "ORDER BY created_at DESC LIMIT 1";

    const char* paramValues[1] = {transaction_id.c_str()};
    PGresult* result = PQexecParams(db_conn, query.c_str(), 1, NULL, paramValues, NULL, NULL, 0);

    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        std::string error = PQerrorMessage(db_conn);
        PQclear(result);
        return "{\"error\":\"Database query failed: " + error + "\"}";
    }

    if (PQntuples(result) == 0) {
        PQclear(result);
        return "{\"error\":\"No fraud analysis found for transaction\",\"transaction_id\":\"" + transaction_id + "\"}";
    }

    json analysis;
    analysis["analysisId"] = PQgetvalue(result, 0, 0);
    analysis["analysisType"] = PQgetvalue(result, 0, 1);
    analysis["riskScore"] = std::atof(PQgetvalue(result, 0, 2));
    analysis["riskFactors"] = json::parse(PQgetvalue(result, 0, 3));
    analysis["recommendation"] = PQgetvalue(result, 0, 4);
    analysis["details"] = json::parse(PQgetvalue(result, 0, 5));
    analysis["createdAt"] = PQgetvalue(result, 0, 6);

    PQclear(result);
    return analysis.dump();
}

/**
 * POST /api/transactions/{id}/approve
 * Approve a transaction
 * Production: Updates transaction status and logs approval
 */
std::string approve_transaction(PGconn* db_conn, const std::string& transaction_id, const std::string& user_id, const std::string& request_body) {
    try {
        json req = json::parse(request_body);
        std::string notes = req.value("notes", "");

        std::string query = "UPDATE transactions SET status = 'approved', "
                           "approved_by = $1, approved_at = CURRENT_TIMESTAMP, "
                           "notes = $2, updated_at = CURRENT_TIMESTAMP "
                           "WHERE transaction_id = $3 AND status != 'approved' "
                           "RETURNING transaction_id, status, approved_at";

        std::vector<std::string> params = {user_id, notes, transaction_id};
        std::vector<const char*> paramValues;
        for (const auto& p : params) {
            paramValues.push_back(p.c_str());
        }

        PGresult* result = PQexecParams(db_conn, query.c_str(), paramValues.size(), NULL,
                                       paramValues.data(), NULL, NULL, 0);

        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            std::string error = PQerrorMessage(db_conn);
            PQclear(result);
            return "{\"error\":\"Failed to approve transaction: " + error + "\"}";
        }

        if (PQntuples(result) == 0) {
            PQclear(result);
            return "{\"error\":\"Transaction not found or already approved\",\"transaction_id\":\"" + transaction_id + "\"}";
        }

        json response;
        response["id"] = PQgetvalue(result, 0, 0);
        response["status"] = PQgetvalue(result, 0, 1);
        response["approvedAt"] = PQgetvalue(result, 0, 2);
        response["message"] = "Transaction approved successfully";

        PQclear(result);
        return response.dump();

    } catch (const json::exception& e) {
        return "{\"error\":\"Invalid JSON: " + std::string(e.what()) + "\"}";
    }
}

/**
 * POST /api/transactions/{id}/reject
 * Reject a transaction
 * Production: Updates transaction status and logs rejection
 */
std::string reject_transaction(PGconn* db_conn, const std::string& transaction_id, const std::string& user_id, const std::string& request_body) {
    try {
        json req = json::parse(request_body);
        std::string reason = req.value("reason", "");

        std::string query = "UPDATE transactions SET status = 'rejected', "
                           "rejected_by = $1, rejected_at = CURRENT_TIMESTAMP, "
                           "rejection_reason = $2, updated_at = CURRENT_TIMESTAMP "
                           "WHERE transaction_id = $3 AND status != 'rejected' "
                           "RETURNING transaction_id, status, rejected_at";

        std::vector<std::string> params = {user_id, reason, transaction_id};
        std::vector<const char*> paramValues;
        for (const auto& p : params) {
            paramValues.push_back(p.c_str());
        }

        PGresult* result = PQexecParams(db_conn, query.c_str(), paramValues.size(), NULL,
                                       paramValues.data(), NULL, NULL, 0);

        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            std::string error = PQerrorMessage(db_conn);
            PQclear(result);
            return "{\"error\":\"Failed to reject transaction: " + error + "\"}";
        }

        if (PQntuples(result) == 0) {
            PQclear(result);
            return "{\"error\":\"Transaction not found or already rejected\",\"transaction_id\":\"" + transaction_id + "\"}";
        }

        json response;
        response["id"] = PQgetvalue(result, 0, 0);
        response["status"] = PQgetvalue(result, 0, 1);
        response["rejectedAt"] = PQgetvalue(result, 0, 2);
        response["message"] = "Transaction rejected successfully";

        PQclear(result);
        return response.dump();

    } catch (const json::exception& e) {
        return "{\"error\":\"Invalid JSON: " + std::string(e.what()) + "\"}";
    }
}

/**
 * GET /api/transactions/patterns
 * Get transaction patterns for analysis
 * Production: Analyzes transaction patterns and trends
 */
std::string get_transaction_patterns(PGconn* db_conn, const std::map<std::string, std::string>& query_params) {
    std::string time_range = query_params.count("time_range") ? query_params.at("time_range") : "30d";
    std::string pattern_type = query_params.count("pattern_type") ? query_params.at("pattern_type") : "all";

    int days = 30;
    if (time_range.back() == 'd') {
        days = std::atoi(time_range.substr(0, time_range.length()-1).c_str());
    }

    std::string days_str = std::to_string(days);

    json patterns = json::object();

    // Amount distribution pattern
    if (pattern_type == "all" || pattern_type == "amount_distribution") {
        std::string amount_query = 
            "SELECT "
            "COUNT(*) FILTER (WHERE amount < 100) as small_count, "
            "COUNT(*) FILTER (WHERE amount >= 100 AND amount < 1000) as medium_count, "
            "COUNT(*) FILTER (WHERE amount >= 1000 AND amount < 10000) as large_count, "
            "COUNT(*) FILTER (WHERE amount >= 10000) as very_large_count, "
            "AVG(amount) as avg_amount, "
            "PERCENTILE_CONT(0.5) WITHIN GROUP (ORDER BY amount) as median_amount, "
            "PERCENTILE_CONT(0.95) WITHIN GROUP (ORDER BY amount) as p95_amount "
            "FROM transactions "
            "WHERE transaction_date >= CURRENT_TIMESTAMP - INTERVAL '" + days_str + " days'";

        PGresult* amount_result = PQexec(db_conn, amount_query.c_str());
        if (PQresultStatus(amount_result) == PGRES_TUPLES_OK && PQntuples(amount_result) > 0) {
            json amount_dist;
            amount_dist["small"] = std::atoi(PQgetvalue(amount_result, 0, 0));
            amount_dist["medium"] = std::atoi(PQgetvalue(amount_result, 0, 1));
            amount_dist["large"] = std::atoi(PQgetvalue(amount_result, 0, 2));
            amount_dist["veryLarge"] = std::atoi(PQgetvalue(amount_result, 0, 3));
            if (!PQgetisnull(amount_result, 0, 4)) {
                amount_dist["average"] = std::atof(PQgetvalue(amount_result, 0, 4));
            }
            if (!PQgetisnull(amount_result, 0, 5)) {
                amount_dist["median"] = std::atof(PQgetvalue(amount_result, 0, 5));
            }
            if (!PQgetisnull(amount_result, 0, 6)) {
                amount_dist["p95"] = std::atof(PQgetvalue(amount_result, 0, 6));
            }
            patterns["amountDistribution"] = amount_dist;
        }
        PQclear(amount_result);
    }

    // Geographic pattern
    if (pattern_type == "all" || pattern_type == "geographic") {
        std::string geo_query = 
            "SELECT country_code, COUNT(*) as count, SUM(amount) as total_amount "
            "FROM transactions "
            "WHERE transaction_date >= CURRENT_TIMESTAMP - INTERVAL '" + days_str + " days' "
            "GROUP BY country_code "
            "ORDER BY count DESC LIMIT 10";

        PGresult* geo_result = PQexec(db_conn, geo_query.c_str());
        if (PQresultStatus(geo_result) == PGRES_TUPLES_OK) {
            json geo_pattern = json::array();
            int count = PQntuples(geo_result);
            for (int i = 0; i < count; i++) {
                json country;
                country["countryCode"] = PQgetvalue(geo_result, i, 0);
                country["transactionCount"] = std::atoi(PQgetvalue(geo_result, i, 1));
                country["totalAmount"] = std::atof(PQgetvalue(geo_result, i, 2));
                geo_pattern.push_back(country);
            }
            patterns["geographicDistribution"] = geo_pattern;
        }
        PQclear(geo_result);
    }

    // Time-based pattern
    if (pattern_type == "all" || pattern_type == "temporal") {
        std::string time_query = 
            "SELECT "
            "EXTRACT(HOUR FROM transaction_date) as hour, "
            "COUNT(*) as count, "
            "AVG(amount) as avg_amount "
            "FROM transactions "
            "WHERE transaction_date >= CURRENT_TIMESTAMP - INTERVAL '" + days_str + " days' "
            "GROUP BY hour "
            "ORDER BY hour";

        PGresult* time_result = PQexec(db_conn, time_query.c_str());
        if (PQresultStatus(time_result) == PGRES_TUPLES_OK) {
            json time_pattern = json::array();
            int count = PQntuples(time_result);
            for (int i = 0; i < count; i++) {
                json hour_data;
                hour_data["hour"] = std::atoi(PQgetvalue(time_result, i, 0));
                hour_data["transactionCount"] = std::atoi(PQgetvalue(time_result, i, 1));
                if (!PQgetisnull(time_result, i, 2)) {
                    hour_data["averageAmount"] = std::atof(PQgetvalue(time_result, i, 2));
                }
                time_pattern.push_back(hour_data);
            }
            patterns["temporalDistribution"] = time_pattern;
        }
        PQclear(time_result);
    }

    // Risk pattern
    if (pattern_type == "all" || pattern_type == "risk") {
        std::string risk_query = 
            "SELECT "
            "COUNT(*) FILTER (WHERE risk_score < 30) as low_risk, "
            "COUNT(*) FILTER (WHERE risk_score >= 30 AND risk_score < 60) as medium_risk, "
            "COUNT(*) FILTER (WHERE risk_score >= 60 AND risk_score < 80) as high_risk, "
            "COUNT(*) FILTER (WHERE risk_score >= 80) as very_high_risk, "
            "AVG(risk_score) as avg_risk_score "
            "FROM transactions "
            "WHERE transaction_date >= CURRENT_TIMESTAMP - INTERVAL '" + days_str + " days'";

        PGresult* risk_result = PQexec(db_conn, risk_query.c_str());
        if (PQresultStatus(risk_result) == PGRES_TUPLES_OK && PQntuples(risk_result) > 0) {
            json risk_pattern;
            risk_pattern["lowRisk"] = std::atoi(PQgetvalue(risk_result, 0, 0));
            risk_pattern["mediumRisk"] = std::atoi(PQgetvalue(risk_result, 0, 1));
            risk_pattern["highRisk"] = std::atoi(PQgetvalue(risk_result, 0, 2));
            risk_pattern["veryHighRisk"] = std::atoi(PQgetvalue(risk_result, 0, 3));
            if (!PQgetisnull(risk_result, 0, 4)) {
                risk_pattern["averageRiskScore"] = std::atof(PQgetvalue(risk_result, 0, 4));
            }
            patterns["riskDistribution"] = risk_pattern;
        }
        PQclear(risk_result);
    }

    json response;
    response["patterns"] = patterns;
    response["timeRange"] = time_range;
    response["patternType"] = pattern_type;

    return response.dump();
}

/**
 * POST /api/transactions/detect-anomalies
 * Detect anomalies in transactions
 * Production: Runs anomaly detection algorithms
 */
std::string detect_anomalies(PGconn* db_conn, const std::string& request_body) {
    try {
        json req = json::parse(request_body);
        std::string time_range = req.value("time_range", "7d");
        double anomaly_threshold = req.value("threshold", 2.0); // Standard deviations

        int days = 7;
        if (time_range.back() == 'd') {
            days = std::atoi(time_range.substr(0, time_range.length()-1).c_str());
        }

        std::string days_str = std::to_string(days);

        // Detect amount anomalies
        std::string anomaly_query = 
            "WITH transaction_stats AS ("
            "SELECT "
            "AVG(amount) as avg_amount, "
            "STDDEV(amount) as stddev_amount "
            "FROM transactions "
            "WHERE transaction_date >= CURRENT_TIMESTAMP - INTERVAL '" + days_str + " days'"
            "), "
            "anomalous_transactions AS ("
            "SELECT t.transaction_id, t.amount, t.customer_id, t.transaction_date, "
            "s.avg_amount, s.stddev_amount, "
            "ABS(t.amount - s.avg_amount) / NULLIF(s.stddev_amount, 0) as z_score "
            "FROM transactions t, transaction_stats s "
            "WHERE t.transaction_date >= CURRENT_TIMESTAMP - INTERVAL '" + days_str + " days' "
            "AND ABS(t.amount - s.avg_amount) / NULLIF(s.stddev_amount, 0) > " + std::to_string(anomaly_threshold) +
            ") "
            "SELECT transaction_id, amount, customer_id, transaction_date, avg_amount, stddev_amount, z_score "
            "FROM anomalous_transactions "
            "ORDER BY z_score DESC LIMIT 50";

        PGresult* result = PQexec(db_conn, anomaly_query.c_str());

        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            std::string error = PQerrorMessage(db_conn);
            PQclear(result);
            return "{\"error\":\"Failed to detect anomalies: " + error + "\"}";
        }

        json anomalies = json::array();
        int count = PQntuples(result);

        for (int i = 0; i < count; i++) {
            json anomaly;
            anomaly["transactionId"] = PQgetvalue(result, i, 0);
            anomaly["amount"] = std::atof(PQgetvalue(result, i, 1));
            anomaly["customerId"] = PQgetvalue(result, i, 2);
            anomaly["transactionDate"] = PQgetvalue(result, i, 3);
            anomaly["averageAmount"] = std::atof(PQgetvalue(result, i, 4));
            anomaly["standardDeviation"] = std::atof(PQgetvalue(result, i, 5));
            anomaly["zScore"] = std::atof(PQgetvalue(result, i, 6));
            anomaly["anomalyType"] = "amount_outlier";
            anomaly["severity"] = std::atof(PQgetvalue(result, i, 6)) > 3.0 ? "high" : "medium";
            anomalies.push_back(anomaly);
        }

        PQclear(result);

        // Detect frequency anomalies (multiple transactions from same customer in short time)
        std::string freq_query = 
            "WITH customer_transaction_counts AS ("
            "SELECT customer_id, COUNT(*) as transaction_count, "
            "MIN(transaction_date) as first_txn, MAX(transaction_date) as last_txn "
            "FROM transactions "
            "WHERE transaction_date >= CURRENT_TIMESTAMP - INTERVAL '" + days_str + " days' "
            "GROUP BY customer_id "
            "HAVING COUNT(*) > 10"  // More than 10 transactions
            "), "
            "frequency_anomalies AS ("
            "SELECT c.customer_id, c.transaction_count, c.first_txn, c.last_txn, "
            "EXTRACT(EPOCH FROM (c.last_txn - c.first_txn)) / 3600 as hours_span"
            "FROM customer_transaction_counts c "
            "WHERE EXTRACT(EPOCH FROM (c.last_txn - c.first_txn)) / 3600 < 24"  // Within 24 hours
            ") "
            "SELECT fa.customer_id, fa.transaction_count, fa.hours_span, "
            "c.customer_name, c.customer_email "
            "FROM frequency_anomalies fa "
            "LEFT JOIN customers c ON fa.customer_id = c.customer_id "
            "ORDER BY fa.transaction_count DESC LIMIT 20";

        PGresult* freq_result = PQexec(db_conn, freq_query.c_str());

        if (PQresultStatus(freq_result) == PGRES_TUPLES_OK) {
            int freq_count = PQntuples(freq_result);
            for (int i = 0; i < freq_count; i++) {
                json freq_anomaly;
                freq_anomaly["customerId"] = PQgetvalue(freq_result, i, 0);
                freq_anomaly["transactionCount"] = std::atoi(PQgetvalue(freq_result, i, 1));
                freq_anomaly["hoursSpan"] = std::atof(PQgetvalue(freq_result, i, 2));
                if (!PQgetisnull(freq_result, i, 3)) {
                    freq_anomaly["customerName"] = PQgetvalue(freq_result, i, 3);
                }
                if (!PQgetisnull(freq_result, i, 4)) {
                    freq_anomaly["customerEmail"] = PQgetvalue(freq_result, i, 4);
                }
                freq_anomaly["anomalyType"] = "high_frequency";
                freq_anomaly["severity"] = std::atoi(PQgetvalue(freq_result, i, 1)) > 50 ? "high" : "medium";
                anomalies.push_back(freq_anomaly);
            }
        }

        PQclear(freq_result);

        json response;
        response["anomalies"] = anomalies;
        response["timeRange"] = time_range;
        response["threshold"] = anomaly_threshold;
        response["totalAnomalies"] = anomalies.size();

        return response.dump();

    } catch (const json::exception& e) {
        return "{\"error\":\"Invalid JSON: " + std::string(e.what()) + "\"}";
    }
}

/**
 * GET /api/transactions/stats
 * Get transaction statistics
 * Production: Aggregates transaction data for statistics
 */
std::string get_transaction_stats(PGconn* db_conn, const std::map<std::string, std::string>& query_params) {
    std::string time_range = query_params.count("time_range") ? query_params.at("time_range") : "30d";

    int days = 30;
    if (time_range.back() == 'd') {
        days = std::atoi(time_range.substr(0, time_range.length()-1).c_str());
    }

    std::string days_str = std::to_string(days);

    // Basic statistics
    std::string stats_query = 
        "SELECT "
        "COUNT(*) as total_transactions, "
        "SUM(amount) as total_amount, "
        "AVG(amount) as avg_amount, "
        "MIN(amount) as min_amount, "
        "MAX(amount) as max_amount, "
        "COUNT(DISTINCT customer_id) as unique_customers, "
        "COUNT(*) FILTER (WHERE flagged = true) as flagged_transactions, "
        "COUNT(*) FILTER (WHERE status = 'approved') as approved_transactions, "
        "COUNT(*) FILTER (WHERE status = 'rejected') as rejected_transactions, "
        "COUNT(*) FILTER (WHERE status = 'pending_review') as pending_transactions "
        "FROM transactions "
        "WHERE transaction_date >= CURRENT_TIMESTAMP - INTERVAL '" + days_str + " days'";

    PGresult* result = PQexec(db_conn, stats_query.c_str());

    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        std::string error = PQerrorMessage(db_conn);
        PQclear(result);
        return "{\"error\":\"Failed to get statistics: " + error + "\"}";
    }

    json stats;
    if (PQntuples(result) > 0) {
        stats["totalTransactions"] = std::atoi(PQgetvalue(result, 0, 0));
        if (!PQgetisnull(result, 0, 1)) {
            stats["totalAmount"] = std::atof(PQgetvalue(result, 0, 1));
        }
        if (!PQgetisnull(result, 0, 2)) {
            stats["averageAmount"] = std::atof(PQgetvalue(result, 0, 2));
        }
        if (!PQgetisnull(result, 0, 3)) {
            stats["minimumAmount"] = std::atof(PQgetvalue(result, 0, 3));
        }
        if (!PQgetisnull(result, 0, 4)) {
            stats["maximumAmount"] = std::atof(PQgetvalue(result, 0, 4));
        }
        stats["uniqueCustomers"] = std::atoi(PQgetvalue(result, 0, 5));
        stats["flaggedTransactions"] = std::atoi(PQgetvalue(result, 0, 6));
        stats["approvedTransactions"] = std::atoi(PQgetvalue(result, 0, 7));
        stats["rejectedTransactions"] = std::atoi(PQgetvalue(result, 0, 8));
        stats["pendingTransactions"] = std::atoi(PQgetvalue(result, 0, 9));
    }

    PQclear(result);

    // Transaction type breakdown
    std::string type_query = 
        "SELECT transaction_type, COUNT(*) as count, SUM(amount) as total_amount "
        "FROM transactions "
        "WHERE transaction_date >= CURRENT_TIMESTAMP - INTERVAL '" + days_str + " days' "
        "GROUP BY transaction_type "
        "ORDER BY count DESC";

    PGresult* type_result = PQexec(db_conn, type_query.c_str());

    if (PQresultStatus(type_result) == PGRES_TUPLES_OK) {
        json type_breakdown = json::array();
        int count = PQntuples(type_result);
        for (int i = 0; i < count; i++) {
            json type_stat;
            type_stat["type"] = PQgetvalue(type_result, i, 0);
            type_stat["count"] = std::atoi(PQgetvalue(type_result, i, 1));
            if (!PQgetisnull(type_result, i, 2)) {
                type_stat["totalAmount"] = std::atof(PQgetvalue(type_result, i, 2));
            }
            type_breakdown.push_back(type_stat);
        }
        stats["transactionTypeBreakdown"] = type_breakdown;
    }

    PQclear(type_result);

    // Daily transaction trends
    std::string trend_query = 
        "SELECT "
        "DATE(transaction_date) as date, "
        "COUNT(*) as count, "
        "SUM(amount) as total_amount, "
        "AVG(amount) as avg_amount "
        "FROM transactions "
        "WHERE transaction_date >= CURRENT_TIMESTAMP - INTERVAL '" + days_str + " days' "
        "GROUP BY DATE(transaction_date) "
        "ORDER BY date DESC";

    PGresult* trend_result = PQexec(db_conn, trend_query.c_str());

    if (PQresultStatus(trend_result) == PGRES_TUPLES_OK) {
        json trends = json::array();
        int count = PQntuples(trend_result);
        for (int i = 0; i < count; i++) {
            json trend;
            trend["date"] = PQgetvalue(trend_result, i, 0);
            trend["transactionCount"] = std::atoi(PQgetvalue(trend_result, i, 1));
            if (!PQgetisnull(trend_result, i, 2)) {
                trend["totalAmount"] = std::atof(PQgetvalue(trend_result, i, 2));
            }
            if (!PQgetisnull(trend_result, i, 3)) {
                trend["averageAmount"] = std::atof(PQgetvalue(trend_result, i, 3));
            }
            trends.push_back(trend);
        }
        stats["dailyTrends"] = trends;
    }

    PQclear(trend_result);

    stats["timeRange"] = time_range;

    return stats.dump();
}

/**
 * GET /api/transactions/metrics
 * Get detailed transaction metrics
 * Production: Returns comprehensive metrics for dashboard
 */
std::string get_transaction_metrics(PGconn* db_conn, const std::map<std::string, std::string>& query_params) {
    std::string time_range = query_params.count("time_range") ? query_params.at("time_range") : "30d";

    int days = 30;
    if (time_range.back() == 'd') {
        days = std::atoi(time_range.substr(0, time_range.length()-1).c_str());
    }

    std::string days_str = std::to_string(days);

    json metrics;

    // Approval rate metrics
    std::string approval_query = 
        "SELECT "
        "COUNT(*) as total, "
        "COUNT(*) FILTER (WHERE status = 'approved') as approved, "
        "COUNT(*) FILTER (WHERE status = 'rejected') as rejected, "
        "COUNT(*) FILTER (WHERE status = 'pending_review') as pending "
        "FROM transactions "
        "WHERE transaction_date >= CURRENT_TIMESTAMP - INTERVAL '" + days_str + " days'";

    PGresult* approval_result = PQexec(db_conn, approval_query.c_str());

    if (PQresultStatus(approval_result) == PGRES_TUPLES_OK && PQntuples(approval_result) > 0) {
        int total = std::atoi(PQgetvalue(approval_result, 0, 0));
        int approved = std::atoi(PQgetvalue(approval_result, 0, 1));
        int rejected = std::atoi(PQgetvalue(approval_result, 0, 2));
        int pending = std::atoi(PQgetvalue(approval_result, 0, 3));

        json approval_metrics;
        approval_metrics["total"] = total;
        approval_metrics["approved"] = approved;
        approval_metrics["rejected"] = rejected;
        approval_metrics["pending"] = pending;
        approval_metrics["approvalRate"] = total > 0 ? (double)approved / total : 0.0;
        approval_metrics["rejectionRate"] = total > 0 ? (double)rejected / total : 0.0;
        approval_metrics["pendingRate"] = total > 0 ? (double)pending / total : 0.0;

        metrics["approvalMetrics"] = approval_metrics;
    }

    PQclear(approval_result);

    // Risk distribution metrics
    std::string risk_query = 
        "SELECT "
        "AVG(risk_score) as avg_risk, "
        "PERCENTILE_CONT(0.5) WITHIN GROUP (ORDER BY risk_score) as median_risk, "
        "PERCENTILE_CONT(0.95) WITHIN GROUP (ORDER BY risk_score) as p95_risk, "
        "COUNT(*) FILTER (WHERE risk_score > 70) as high_risk_count "
        "FROM transactions "
        "WHERE transaction_date >= CURRENT_TIMESTAMP - INTERVAL '" + days_str + " days'";

    PGresult* risk_result = PQexec(db_conn, risk_query.c_str());

    if (PQresultStatus(risk_result) == PGRES_TUPLES_OK && PQntuples(risk_result) > 0) {
        json risk_metrics;
        if (!PQgetisnull(risk_result, 0, 0)) {
            risk_metrics["averageRiskScore"] = std::atof(PQgetvalue(risk_result, 0, 0));
        }
        if (!PQgetisnull(risk_result, 0, 1)) {
            risk_metrics["medianRiskScore"] = std::atof(PQgetvalue(risk_result, 0, 1));
        }
        if (!PQgetisnull(risk_result, 0, 2)) {
            risk_metrics["p95RiskScore"] = std::atof(PQgetvalue(risk_result, 0, 2));
        }
        risk_metrics["highRiskTransactionCount"] = std::atoi(PQgetvalue(risk_result, 0, 3));

        metrics["riskMetrics"] = risk_metrics;
    }

    PQclear(risk_result);

    // Velocity metrics (transactions per hour/day)
    std::string velocity_query = 
        "SELECT "
        "COUNT(*) / " + std::to_string(days) + " as avg_daily_transactions, "
        "COUNT(*) / (" + std::to_string(days) + " * 24) as avg_hourly_transactions "
        "FROM transactions "
        "WHERE transaction_date >= CURRENT_TIMESTAMP - INTERVAL '" + days_str + " days'";

    PGresult* velocity_result = PQexec(db_conn, velocity_query.c_str());

    if (PQresultStatus(velocity_result) == PGRES_TUPLES_OK && PQntuples(velocity_result) > 0) {
        json velocity_metrics;
        velocity_metrics["averageDailyTransactions"] = std::atof(PQgetvalue(velocity_result, 0, 0));
        velocity_metrics["averageHourlyTransactions"] = std::atof(PQgetvalue(velocity_result, 0, 1));

        metrics["velocityMetrics"] = velocity_metrics;
    }

    PQclear(velocity_result);

    // Top merchants by transaction count
    std::string merchant_query = 
        "SELECT merchant_name, COUNT(*) as count, SUM(amount) as total_amount "
        "FROM transactions "
        "WHERE transaction_date >= CURRENT_TIMESTAMP - INTERVAL '" + days_str + " days' "
        "AND merchant_name IS NOT NULL AND merchant_name != '' "
        "GROUP BY merchant_name "
        "ORDER BY count DESC LIMIT 10";

    PGresult* merchant_result = PQexec(db_conn, merchant_query.c_str());

    if (PQresultStatus(merchant_result) == PGRES_TUPLES_OK) {
        json top_merchants = json::array();
        int count = PQntuples(merchant_result);
        for (int i = 0; i < count; i++) {
            json merchant;
            merchant["name"] = PQgetvalue(merchant_result, i, 0);
            merchant["transactionCount"] = std::atoi(PQgetvalue(merchant_result, i, 1));
            if (!PQgetisnull(merchant_result, i, 2)) {
                merchant["totalAmount"] = std::atof(PQgetvalue(merchant_result, i, 2));
            }
            top_merchants.push_back(merchant);
        }
        metrics["topMerchants"] = top_merchants;
    }

    PQclear(merchant_result);

    metrics["timeRange"] = time_range;

    return metrics.dump();
}

// Helper functions implementation

double calculate_transaction_risk_score(PGconn* db_conn, const std::string& customer_id) {
    // Simple risk calculation based on customer history
    std::string query = "SELECT "
                       "COUNT(*) FILTER (WHERE status = 'rejected') as rejected_count, "
                       "COUNT(*) FILTER (WHERE flagged = true) as flagged_count, "
                       "COUNT(*) as total_count "
                       "FROM transactions "
                       "WHERE customer_id = $1 AND transaction_date >= CURRENT_TIMESTAMP - INTERVAL '90 days'";

    const char* paramValues[1] = {customer_id.c_str()};
    PGresult* result = PQexecParams(db_conn, query.c_str(), 1, NULL, paramValues, NULL, NULL, 0);

    if (PQresultStatus(result) != PGRES_TUPLES_OK || PQntuples(result) == 0) {
        PQclear(result);
        return 30.0; // Default medium risk
    }

    int rejected_count = std::atoi(PQgetvalue(result, 0, 0));
    int flagged_count = std::atoi(PQgetvalue(result, 0, 1));
    int total_count = std::atoi(PQgetvalue(result, 0, 2));

    PQclear(result);

    // Calculate risk score (0-100)
    double risk_score = 30.0; // Base risk

    if (total_count > 0) {
        double rejection_rate = (double)rejected_count / total_count;
        double flag_rate = (double)flagged_count / total_count;

        risk_score += rejection_rate * 40.0; // Up to 40 points for rejections
        risk_score += flag_rate * 30.0;      // Up to 30 points for flags
    }

    return std::min(100.0, std::max(0.0, risk_score));
}

std::string generate_transaction_analysis(PGconn* db_conn, const std::string& transaction_id) {
    json analysis;
    
    // Get transaction details for analysis
    std::string query = "SELECT t.amount, t.currency, t.transaction_type, t.country_code, "
                       "t.customer_id, c.risk_rating as customer_risk, c.customer_type "
                       "FROM transactions t "
                       "LEFT JOIN customers c ON t.customer_id = c.customer_id "
                       "WHERE t.transaction_id = $1";

    const char* paramValues[1] = {transaction_id.c_str()};
    PGresult* result = PQexecParams(db_conn, query.c_str(), 1, NULL, paramValues, NULL, NULL, 0);

    if (PQresultStatus(result) != PGRES_TUPLES_OK || PQntuples(result) == 0) {
        PQclear(result);
        return "{\"error\":\"Transaction not found\"}";
    }

    double amount = std::atof(PQgetvalue(result, 0, 0));
    std::string currency = PQgetvalue(result, 0, 1);
    std::string transaction_type = PQgetvalue(result, 0, 2);
    std::string country_code = PQgetvalue(result, 0, 3);
    std::string customer_id = PQgetvalue(result, 0, 4);
    std::string customer_risk = !PQgetisnull(result, 0, 5) ? PQgetvalue(result, 0, 5) : "medium";
    std::string customer_type = !PQgetisnull(result, 0, 6) ? PQgetvalue(result, 0, 6) : "individual";

    PQclear(result);

    // Calculate risk factors
    json risk_factors = json::array();
    double risk_score = 0.0;

    // Amount-based risk
    if (amount > 10000) {
        risk_factors.push_back(json{{"factor", "high_amount"}, {"description", "High transaction amount"}, {"weight", 0.3}});
        risk_score += 30.0;
    } else if (amount > 1000) {
        risk_factors.push_back(json{{"factor", "medium_amount"}, {"description", "Medium transaction amount"}, {"weight", 0.15}});
        risk_score += 15.0;
    }

    // Geographic risk
    if (country_code != "US") {
        risk_factors.push_back(json{{"factor", "international"}, {"description", "International transaction"}, {"weight", 0.2}});
        risk_score += 20.0;
    }

    // Transaction type risk
    if (transaction_type == "wire_transfer" || transaction_type == "crypto") {
        risk_factors.push_back(json{{"factor", "high_risk_type"}, {"description", "High-risk transaction type"}, {"weight", 0.25}});
        risk_score += 25.0;
    }

    // Customer risk
    if (customer_risk == "high") {
        risk_factors.push_back(json{{"factor", "high_risk_customer"}, {"description", "High-risk customer"}, {"weight", 0.3}});
        risk_score += 30.0;
    } else if (customer_risk == "medium") {
        risk_factors.push_back(json{{"factor", "medium_risk_customer"}, {"description", "Medium-risk customer"}, {"weight", 0.15}});
        risk_score += 15.0;
    }

    // Customer type risk
    if (customer_type == "business") {
        risk_factors.push_back(json{{"factor", "business_entity"}, {"description", "Business entity transaction"}, {"weight", 0.1}});
        risk_score += 10.0;
    }

    analysis["riskScore"] = std::min(100.0, risk_score);
    analysis["riskFactors"] = risk_factors;

    // Generate recommendation
    if (risk_score >= 80) {
        analysis["recommendation"] = "reject";
        analysis["reason"] = "High risk transaction - multiple risk factors detected";
    } else if (risk_score >= 60) {
        analysis["recommendation"] = "manual_review";
        analysis["reason"] = "Medium-high risk - requires manual review";
    } else if (risk_score >= 40) {
        analysis["recommendation"] = "approve_with_monitoring";
        analysis["reason"] = "Medium risk - approve but monitor for follow-up";
    } else {
        analysis["recommendation"] = "approve";
        analysis["reason"] = "Low risk transaction";
    }

    // Add detailed analysis
    json details;
    details["amountAnalysis"] = {
        {"amount", amount},
        {"currency", currency},
        {"riskLevel", amount > 10000 ? "high" : amount > 1000 ? "medium" : "low"}
    };
    details["geographicAnalysis"] = {
        {"countryCode", country_code},
        {"isInternational", country_code != "US"}
    };
    details["customerAnalysis"] = {
        {"customerId", customer_id},
        {"riskRating", customer_risk},
        {"customerType", customer_type}
    };

    analysis["details"] = details;

    return analysis.dump();
}

bool is_high_risk_transaction(PGconn* db_conn, const std::string& customer_id) {
    // Check if customer has high risk indicators
    std::string query = "SELECT "
                       "COUNT(*) FILTER (WHERE status = 'rejected') as rejected_count, "
                       "COUNT(*) FILTER (WHERE flagged = true) as flagged_count "
                       "FROM transactions "
                       "WHERE customer_id = $1 AND transaction_date >= CURRENT_TIMESTAMP - INTERVAL '30 days'";

    const char* paramValues[1] = {customer_id.c_str()};
    PGresult* result = PQexecParams(db_conn, query.c_str(), 1, NULL, paramValues, NULL, NULL, 0);

    if (PQresultStatus(result) != PGRES_TUPLES_OK || PQntuples(result) == 0) {
        PQclear(result);
        return false;
    }

    int rejected_count = std::atoi(PQgetvalue(result, 0, 0));
    int flagged_count = std::atoi(PQgetvalue(result, 0, 1));

    PQclear(result);

    // High risk if more than 2 rejections or more than 5 flags in last 30 days
    return rejected_count > 2 || flagged_count > 5;
}

} // namespace transactions
} // namespace regulens
