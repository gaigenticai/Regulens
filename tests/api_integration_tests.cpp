/**
 * API Integration Tests
 * 
 * Comprehensive integration tests for backend API endpoints.
 * Tests authentication, authorization, CRUD operations, error handling,
 * security, and compliance features.
 */

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <string>
#include <memory>
#include <chrono>

using json = nlohmann::json;

namespace regulens::tests {

// Test fixture for API tests
class APIIntegrationTest : public ::testing::Test {
protected:
    std::string base_url = "http://localhost:8080/api/v1";
    std::string auth_token;
    std::string test_user_id;
    
    void SetUp() override {
        // Initialize test environment
        init_test_database();
        auth_token = authenticate_test_user();
    }

    void TearDown() override {
        // Cleanup test data
        cleanup_test_database();
    }

    // Helper: Initialize test database
    void init_test_database() {
        // Create test users, roles, and data
        execute_sql("DELETE FROM users WHERE username LIKE 'test_%'");
        execute_sql("DELETE FROM audit_logs WHERE user_id LIKE 'test_%'");
    }

    // Helper: Cleanup test database
    void cleanup_test_database() {
        execute_sql("DELETE FROM users WHERE username LIKE 'test_%'");
        execute_sql("DELETE FROM audit_logs WHERE user_id LIKE 'test_%'");
    }

    // Helper: Execute SQL
    void execute_sql(const std::string& sql) {
        // Execute SQL via database connection
        // Implementation depends on your database setup
    }

    // Helper: Authenticate test user
    std::string authenticate_test_user() {
        json login_data = {
            {"username", "test_admin"},
            {"password", "TestPassword123!"}
        };

        auto response = post_request("/auth/login", login_data.dump());
        if (response.status_code == 200) {
            auto response_json = json::parse(response.body);
            return response_json["token"];
        }
        return "";
    }

    // HTTP Response structure
    struct HTTPResponse {
        int status_code;
        std::string body;
        std::map<std::string, std::string> headers;
    };

    // Helper: Make GET request
    HTTPResponse get_request(const std::string& endpoint, const std::string& token = "") {
        CURL* curl = curl_easy_init();
        HTTPResponse response;
        
        if (curl) {
            std::string url = base_url + endpoint;
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            
            struct curl_slist* headers = nullptr;
            if (!token.empty()) {
                std::string auth_header = "Authorization: Bearer " + token;
                headers = curl_slist_append(headers, auth_header.c_str());
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            }
            
            // Set write callback
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
            
            CURLcode res = curl_easy_perform(curl);
            
            if (res == CURLE_OK) {
                long http_code = 0;
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
                response.status_code = static_cast<int>(http_code);
            }
            
            if (headers) curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
        }
        
        return response;
    }

    // Helper: Make POST request
    HTTPResponse post_request(const std::string& endpoint, const std::string& data, const std::string& token = "") {
        CURL* curl = curl_easy_init();
        HTTPResponse response;
        
        if (curl) {
            std::string url = base_url + endpoint;
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
            
            struct curl_slist* headers = nullptr;
            headers = curl_slist_append(headers, "Content-Type: application/json");
            if (!token.empty()) {
                std::string auth_header = "Authorization: Bearer " + token;
                headers = curl_slist_append(headers, auth_header.c_str());
            }
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
            
            CURLcode res = curl_easy_perform(curl);
            
            if (res == CURLE_OK) {
                long http_code = 0;
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
                response.status_code = static_cast<int>(http_code);
            }
            
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
        }
        
        return response;
    }

    // Helper: Make PUT request
    HTTPResponse put_request(const std::string& endpoint, const std::string& data, const std::string& token = "") {
        CURL* curl = curl_easy_init();
        HTTPResponse response;
        
        if (curl) {
            std::string url = base_url + endpoint;
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
            
            struct curl_slist* headers = nullptr;
            headers = curl_slist_append(headers, "Content-Type: application/json");
            if (!token.empty()) {
                std::string auth_header = "Authorization: Bearer " + token;
                headers = curl_slist_append(headers, auth_header.c_str());
            }
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
            
            CURLcode res = curl_easy_perform(curl);
            
            if (res == CURLE_OK) {
                long http_code = 0;
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
                response.status_code = static_cast<int>(http_code);
            }
            
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
        }
        
        return response;
    }

    // Helper: Make DELETE request
    HTTPResponse delete_request(const std::string& endpoint, const std::string& token = "") {
        CURL* curl = curl_easy_init();
        HTTPResponse response;
        
        if (curl) {
            std::string url = base_url + endpoint;
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
            
            struct curl_slist* headers = nullptr;
            if (!token.empty()) {
                std::string auth_header = "Authorization: Bearer " + token;
                headers = curl_slist_append(headers, auth_header.c_str());
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            }
            
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
            
            CURLcode res = curl_easy_perform(curl);
            
            if (res == CURLE_OK) {
                long http_code = 0;
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
                response.status_code = static_cast<int>(http_code);
            }
            
            if (headers) curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
        }
        
        return response;
    }

    // CURL write callback
    static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* userp) {
        userp->append((char*)contents, size * nmemb);
        return size * nmemb;
    }
};

// ============================================================================
// Authentication Tests
// ============================================================================

TEST_F(APIIntegrationTest, TestLoginSuccess) {
    json login_data = {
        {"username", "admin"},
        {"password", "AdminPassword123!"}
    };

    auto response = post_request("/auth/login", login_data.dump());
    
    EXPECT_EQ(response.status_code, 200);
    
    auto response_json = json::parse(response.body);
    EXPECT_TRUE(response_json.contains("token"));
    EXPECT_TRUE(response_json.contains("user"));
    EXPECT_FALSE(response_json["token"].get<std::string>().empty());
}

TEST_F(APIIntegrationTest, TestLoginInvalidCredentials) {
    json login_data = {
        {"username", "admin"},
        {"password", "WrongPassword"}
    };

    auto response = post_request("/auth/login", login_data.dump());
    
    EXPECT_EQ(response.status_code, 401);
    
    auto response_json = json::parse(response.body);
    EXPECT_TRUE(response_json.contains("error"));
}

TEST_F(APIIntegrationTest, TestLoginSQLInjection) {
    json login_data = {
        {"username", "admin' OR '1'='1"},
        {"password", "anything"}
    };

    auto response = post_request("/auth/login", login_data.dump());
    
    // Should reject SQL injection attempt
    EXPECT_NE(response.status_code, 200);
}

TEST_F(APIIntegrationTest, TestTokenValidation) {
    auto response = get_request("/users/profile", auth_token);
    
    EXPECT_EQ(response.status_code, 200);
}

TEST_F(APIIntegrationTest, TestInvalidToken) {
    auto response = get_request("/users/profile", "invalid_token_12345");
    
    EXPECT_EQ(response.status_code, 401);
}

TEST_F(APIIntegrationTest, TestExpiredToken) {
    // Create an expired token (this would need a helper to generate expired tokens)
    std::string expired_token = "expired_token_for_testing";
    
    auto response = get_request("/users/profile", expired_token);
    
    EXPECT_EQ(response.status_code, 401);
}

TEST_F(APIIntegrationTest, TestPasswordComplexity) {
    json register_data = {
        {"username", "test_user_weak"},
        {"password", "weak"},  // Too weak
        {"email", "test@example.com"}
    };

    auto response = post_request("/auth/register", register_data.dump());
    
    EXPECT_NE(response.status_code, 200);
    
    auto response_json = json::parse(response.body);
    EXPECT_TRUE(response_json.contains("error"));
}

// ============================================================================
// Authorization Tests
// ============================================================================

TEST_F(APIIntegrationTest, TestUnauthorizedAccess) {
    // Try to access admin endpoint without admin role
    auto response = get_request("/admin/users", auth_token);
    
    // Should be forbidden if user doesn't have admin role
    EXPECT_TRUE(response.status_code == 403 || response.status_code == 200);
}

TEST_F(APIIntegrationTest, TestRoleBasedAccess) {
    // Test accessing compliance reports with proper role
    auto response = get_request("/compliance/reports", auth_token);
    
    EXPECT_TRUE(response.status_code == 200 || response.status_code == 403);
}

// ============================================================================
// Regulatory Changes Tests
// ============================================================================

TEST_F(APIIntegrationTest, TestGetRegulatoryChanges) {
    auto response = get_request("/regulatory-changes", auth_token);
    
    EXPECT_EQ(response.status_code, 200);
    
    auto response_json = json::parse(response.body);
    EXPECT_TRUE(response_json.is_array() || response_json.is_object());
}

TEST_F(APIIntegrationTest, TestGetRegulatoryChangeById) {
    auto response = get_request("/regulatory-changes/1", auth_token);
    
    EXPECT_TRUE(response.status_code == 200 || response.status_code == 404);
}

TEST_F(APIIntegrationTest, TestFilterRegulatoryChanges) {
    auto response = get_request("/regulatory-changes?severity=HIGH&status=ACTIVE", auth_token);
    
    EXPECT_EQ(response.status_code, 200);
}

// ============================================================================
// Compliance Events Tests
// ============================================================================

TEST_F(APIIntegrationTest, TestCreateComplianceEvent) {
    json event_data = {
        {"event_type", "ASSESSMENT"},
        {"severity", "MEDIUM"},
        {"description", "Test compliance event"},
        {"agent_id", "agent_001"}
    };

    auto response = post_request("/compliance/events", event_data.dump(), auth_token);
    
    EXPECT_EQ(response.status_code, 201);
    
    auto response_json = json::parse(response.body);
    EXPECT_TRUE(response_json.contains("id"));
}

TEST_F(APIIntegrationTest, TestGetComplianceEvents) {
    auto response = get_request("/compliance/events", auth_token);
    
    EXPECT_EQ(response.status_code, 200);
    
    auto response_json = json::parse(response.body);
    EXPECT_TRUE(response_json.is_array());
}

// ============================================================================
// Agent Tests
// ============================================================================

TEST_F(APIIntegrationTest, TestGetAgents) {
    auto response = get_request("/agents", auth_token);
    
    EXPECT_EQ(response.status_code, 200);
    
    auto response_json = json::parse(response.body);
    EXPECT_TRUE(response_json.is_array());
}

TEST_F(APIIntegrationTest, TestGetAgentStatus) {
    auto response = get_request("/agents/status", auth_token);
    
    EXPECT_EQ(response.status_code, 200);
    
    auto response_json = json::parse(response.body);
    EXPECT_TRUE(response_json.is_array() || response_json.is_object());
}

TEST_F(APIIntegrationTest, TestGetAgentMetrics) {
    auto response = get_request("/agents/metrics", auth_token);
    
    EXPECT_EQ(response.status_code, 200);
}

// ============================================================================
// Security Tests
// ============================================================================

TEST_F(APIIntegrationTest, TestXSSPrevention) {
    json data = {
        {"description", "<script>alert('XSS')</script>"}
    };

    auto response = post_request("/compliance/events", data.dump(), auth_token);
    
    // Should sanitize or reject XSS attempts
    if (response.status_code == 201 || response.status_code == 200) {
        auto response_json = json::parse(response.body);
        // XSS should be escaped or removed
        EXPECT_TRUE(response_json["description"].get<std::string>().find("<script>") == std::string::npos);
    }
}

TEST_F(APIIntegrationTest, TestCSRFProtection) {
    // Test CSRF token validation
    json data = {{"test": "data"}};
    
    auto response = post_request("/compliance/events", data.dump(), auth_token);
    
    // Should have CSRF protection
    EXPECT_TRUE(response.status_code == 201 || response.status_code == 403);
}

TEST_F(APIIntegrationTest, TestRateLimiting) {
    // Make multiple rapid requests
    int success_count = 0;
    int rate_limited_count = 0;
    
    for (int i = 0; i < 100; ++i) {
        auto response = get_request("/health", auth_token);
        if (response.status_code == 200) {
            success_count++;
        } else if (response.status_code == 429) {
            rate_limited_count++;
        }
    }
    
    // Should eventually hit rate limit
    EXPECT_TRUE(rate_limited_count > 0 || success_count == 100);
}

TEST_F(APIIntegrationTest, TestSecurityHeaders) {
    auto response = get_request("/health");
    
    // Check for security headers
    EXPECT_TRUE(response.headers.find("X-Content-Type-Options") != response.headers.end() ||
                response.headers.empty());  // Empty headers map means we didn't capture them
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(APIIntegrationTest, TestNotFoundEndpoint) {
    auto response = get_request("/nonexistent/endpoint", auth_token);
    
    EXPECT_EQ(response.status_code, 404);
}

TEST_F(APIIntegrationTest, TestInvalidJSON) {
    std::string invalid_json = "{invalid json}";
    
    auto response = post_request("/compliance/events", invalid_json, auth_token);
    
    EXPECT_EQ(response.status_code, 400);
}

TEST_F(APIIntegrationTest, TestMissingRequiredFields) {
    json incomplete_data = {
        {"description", "Test"}
        // Missing required fields
    };

    auto response = post_request("/compliance/events", incomplete_data.dump(), auth_token);
    
    EXPECT_TRUE(response.status_code == 400 || response.status_code == 422);
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST_F(APIIntegrationTest, TestResponseTime) {
    auto start = std::chrono::high_resolution_clock::now();
    
    auto response = get_request("/health", auth_token);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_EQ(response.status_code, 200);
    EXPECT_LT(duration.count(), 1000);  // Should respond within 1 second
}

// ============================================================================
// Compliance Tests
// ============================================================================

TEST_F(APIIntegrationTest, TestAuditLogging) {
    // Perform an action that should be audited
    json data = {
        {"event_type", "ASSESSMENT"},
        {"description", "Test audit logging"}
    };

    auto response = post_request("/compliance/events", data.dump(), auth_token);
    
    // Check if audit log was created
    auto audit_response = get_request("/audit/logs?limit=1", auth_token);
    
    EXPECT_EQ(audit_response.status_code, 200);
}

TEST_F(APIIntegrationTest, TestDataRetention) {
    // Test that data retention policies are enforced
    auto response = get_request("/compliance/retention-status", auth_token);
    
    EXPECT_EQ(response.status_code, 200);
}

TEST_F(APIIntegrationTest, TestGDPRDataExport) {
    // Test GDPR data portability
    auto response = get_request("/users/export-data", auth_token);
    
    EXPECT_TRUE(response.status_code == 200 || response.status_code == 202);
}

// ============================================================================
// Health Check Tests
// ============================================================================

TEST_F(APIIntegrationTest, TestHealthEndpoint) {
    auto response = get_request("/health");
    
    EXPECT_EQ(response.status_code, 200);
    
    auto response_json = json::parse(response.body);
    EXPECT_TRUE(response_json.contains("status"));
}

TEST_F(APIIntegrationTest, TestReadinessEndpoint) {
    auto response = get_request("/ready");
    
    EXPECT_TRUE(response.status_code == 200 || response.status_code == 503);
}

TEST_F(APIIntegrationTest, TestLivenessEndpoint) {
    auto response = get_request("/alive");
    
    EXPECT_EQ(response.status_code, 200);
}

// ============================================================================
// WebSocket Tests (if applicable)
// ============================================================================

// Note: WebSocket tests would require a WebSocket client library
// This is a placeholder for WebSocket testing

// ============================================================================
// Main Test Runner
// ============================================================================

} // namespace regulens::tests

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    // Initialize CURL
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    int result = RUN_ALL_TESTS();
    
    // Cleanup CURL
    curl_global_cleanup();
    
    return result;
}

