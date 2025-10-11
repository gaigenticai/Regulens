/**
 * Database Connectivity Integration Test
 * Tests PostgreSQL connectivity from frontend through backend
 */

#include <gtest/gtest.h>
#include "../../shared/database/postgresql_connection.hpp"
#include "../../shared/config/configuration_manager.hpp"
#include "../../shared/logging/structured_logger.hpp"
#include <memory>
#include <string>

namespace regulens {
namespace test {

class DatabaseConnectivityTest : public ::testing::Test {
protected:
    std::shared_ptr<ConfigurationManager> config_;
    std::shared_ptr<PostgreSQLConnection> db_;
    std::shared_ptr<StructuredLogger> logger_;

    void SetUp() override {
        config_ = std::make_shared<ConfigurationManager>();
        config_->initialize(0, nullptr);
        
        logger_ = std::make_shared<StructuredLogger>(
            config_->get_string("logging.level").value_or("info"),
            config_->get_string("logging.output").value_or("console")
        );

        // Get database configuration
        std::string db_host = config_->get_string("database.host").value_or("localhost");
        int db_port = config_->get_int("database.port").value_or(5432);
        std::string db_name = config_->get_string("database.name").value_or("regulens_compliance");
        std::string db_user = config_->get_string("database.user").value_or("regulens_user");
        std::string db_password = config_->get_string("database.password").value_or("");

        db_ = std::make_shared<PostgreSQLConnection>(
            db_host, db_port, db_name, db_user, db_password
        );
    }

    void TearDown() override {
        if (db_ && db_->is_connected()) {
            db_->disconnect();
        }
    }
};

// Test 1: Basic database connection
TEST_F(DatabaseConnectivityTest, BasicConnectionTest) {
    ASSERT_TRUE(db_->connect()) << "Failed to connect to PostgreSQL database";
    EXPECT_TRUE(db_->is_connected()) << "Database should be connected";
}

// Test 2: Critical tables exist
TEST_F(DatabaseConnectivityTest, CriticalTablesExist) {
    ASSERT_TRUE(db_->connect());

    std::vector<std::string> critical_tables = {
        "case_base",
        "learning_feedback",
        "conversation_memory",
        "memory_consolidation_log",
        "agent_decisions",
        "regulatory_changes",
        "audit_log",
        "transactions",
        "compliance_events",
        "knowledge_base"
    };

    for (const auto& table : critical_tables) {
        std::string query = "SELECT EXISTS (SELECT FROM information_schema.tables WHERE table_schema = 'public' AND table_name = $1)";
        auto result = db_->execute_params(query, {table});
        
        ASSERT_TRUE(result) << "Failed to check table: " << table;
        ASSERT_GT(PQntuples(result.get()), 0) << "No result for table check: " << table;
        
        std::string exists = PQgetvalue(result.get(), 0, 0);
        EXPECT_EQ(exists, "t") << "Table does not exist: " << table;
    }
}

// Test 3: Case-based reasoning table operations
TEST_F(DatabaseConnectivityTest, CaseBaseOperations) {
    ASSERT_TRUE(db_->connect());

    // Test INSERT
    std::string insert_query = R"(
        INSERT INTO case_base (case_id, domain, case_type, problem_description, solution_description, 
                              context_factors, outcome_metrics, confidence_score, usage_count, created_at)
        VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, NOW())
        ON CONFLICT (case_id) DO NOTHING
    )";
    
    auto insert_result = db_->execute_params(insert_query, {
        "test_case_001",
        "compliance",
        "regulatory_check",
        "Test compliance scenario",
        "Test solution approach",
        "{}",
        "{}",
        "0.95",
        "0"
    });
    EXPECT_TRUE(insert_result) << "Failed to insert test case";

    // Test SELECT
    std::string select_query = "SELECT case_id, domain, confidence_score FROM case_base WHERE case_id = $1";
    auto select_result = db_->execute_params(select_query, {"test_case_001"});
    
    ASSERT_TRUE(select_result) << "Failed to select test case";
    EXPECT_GT(PQntuples(select_result.get()), 0) << "Test case not found";

    if (PQntuples(select_result.get()) > 0) {
        std::string case_id = PQgetvalue(select_result.get(), 0, 0);
        std::string domain = PQgetvalue(select_result.get(), 0, 1);
        
        EXPECT_EQ(case_id, "test_case_001");
        EXPECT_EQ(domain, "compliance");
    }

    // Cleanup
    std::string delete_query = "DELETE FROM case_base WHERE case_id = $1";
    auto delete_result = db_->execute_params(delete_query, {"test_case_001"});
    EXPECT_TRUE(delete_result) << "Failed to delete test case";
}

// Test 4: Learning feedback table operations
TEST_F(DatabaseConnectivityTest, LearningFeedbackOperations) {
    ASSERT_TRUE(db_->connect());

    // Test INSERT
    std::string insert_query = R"(
        INSERT INTO learning_feedback (feedback_id, agent_type, agent_name, feedback_type, 
                                      feedback_score, feedback_text, learning_applied, feedback_timestamp)
        VALUES ($1, $2, $3, $4, $5, $6, $7, NOW())
        ON CONFLICT (feedback_id) DO NOTHING
    )";
    
    auto insert_result = db_->execute_params(insert_query, {
        "test_feedback_001",
        "compliance_agent",
        "agent_001",
        "POSITIVE",
        "0.9",
        "Test feedback",
        "false"
    });
    EXPECT_TRUE(insert_result) << "Failed to insert test feedback";

    // Test SELECT with aggregation
    std::string select_query = R"(
        SELECT agent_type, COUNT(*) as count, AVG(feedback_score) as avg_score 
        FROM learning_feedback 
        WHERE feedback_id = $1
        GROUP BY agent_type
    )";
    auto select_result = db_->execute_params(select_query, {"test_feedback_001"});
    
    ASSERT_TRUE(select_result) << "Failed to select test feedback";
    
    if (PQntuples(select_result.get()) > 0) {
        std::string agent_type = PQgetvalue(select_result.get(), 0, 0);
        EXPECT_EQ(agent_type, "compliance_agent");
    }

    // Cleanup
    std::string delete_query = "DELETE FROM learning_feedback WHERE feedback_id = $1";
    auto delete_result = db_->execute_params(delete_query, {"test_feedback_001"});
    EXPECT_TRUE(delete_result) << "Failed to delete test feedback";
}

// Test 5: Conversation memory table operations
TEST_F(DatabaseConnectivityTest, ConversationMemoryOperations) {
    ASSERT_TRUE(db_->connect());

    // Test INSERT
    std::string insert_query = R"(
        INSERT INTO conversation_memory (memory_id, conversation_id, agent_id, agent_type, 
                                        memory_type, importance_level, content, created_at)
        VALUES ($1, $2, $3, $4, $5, $6, $7, NOW())
        ON CONFLICT (memory_id) DO NOTHING
    )";
    
    auto insert_result = db_->execute_params(insert_query, {
        "test_memory_001",
        "conv_001",
        "agent_001",
        "compliance",
        "episodic",
        "5",
        "{\"test\": \"data\"}"
    });
    EXPECT_TRUE(insert_result) << "Failed to insert test memory";

    // Test SELECT with full-text search capability
    std::string select_query = "SELECT memory_id, conversation_id FROM conversation_memory WHERE memory_id = $1";
    auto select_result = db_->execute_params(select_query, {"test_memory_001"});
    
    ASSERT_TRUE(select_result) << "Failed to select test memory";
    EXPECT_GT(PQntuples(select_result.get()), 0) << "Test memory not found";

    // Cleanup
    std::string delete_query = "DELETE FROM conversation_memory WHERE memory_id = $1";
    auto delete_result = db_->execute_params(delete_query, {"test_memory_001"});
    EXPECT_TRUE(delete_result) << "Failed to delete test memory";
}

// Test 6: Transaction support
TEST_F(DatabaseConnectivityTest, TransactionSupport) {
    ASSERT_TRUE(db_->connect());

    // Begin transaction
    ASSERT_TRUE(db_->begin_transaction()) << "Failed to begin transaction";

    // Insert test data
    std::string insert_query = R"(
        INSERT INTO case_base (case_id, domain, case_type, problem_description, solution_description, 
                              context_factors, outcome_metrics, confidence_score, usage_count, created_at)
        VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, NOW())
    )";
    
    auto insert_result = db_->execute_params(insert_query, {
        "test_transaction_case",
        "test_domain",
        "test_type",
        "Test problem",
        "Test solution",
        "{}",
        "{}",
        "0.8",
        "0"
    });
    EXPECT_TRUE(insert_result) << "Failed to insert in transaction";

    // Rollback transaction
    ASSERT_TRUE(db_->rollback_transaction()) << "Failed to rollback transaction";

    // Verify data was rolled back
    std::string select_query = "SELECT case_id FROM case_base WHERE case_id = $1";
    auto select_result = db_->execute_params(select_query, {"test_transaction_case"});
    
    ASSERT_TRUE(select_result) << "Failed to check rollback";
    EXPECT_EQ(PQntuples(select_result.get()), 0) << "Transaction rollback failed - data still exists";
}

// Test 7: Parameterized queries (SQL injection protection)
TEST_F(DatabaseConnectivityTest, ParameterizedQueries) {
    ASSERT_TRUE(db_->connect());

    // Test with potentially malicious input
    std::string malicious_input = "'; DROP TABLE case_base; --";
    
    std::string query = "SELECT case_id FROM case_base WHERE case_id = $1";
    auto result = db_->execute_params(query, {malicious_input});
    
    ASSERT_TRUE(result) << "Parameterized query failed";
    // Should return no results but not execute the DROP TABLE
    EXPECT_EQ(PQntuples(result.get()), 0) << "Unexpected result for malicious input";

    // Verify table still exists
    std::string verify_query = "SELECT EXISTS (SELECT FROM information_schema.tables WHERE table_name = 'case_base')";
    auto verify_result = db_->execute(verify_query);
    
    ASSERT_TRUE(verify_result) << "Failed to verify table existence";
    std::string exists = PQgetvalue(verify_result.get(), 0, 0);
    EXPECT_EQ(exists, "t") << "case_base table was compromised!";
}

// Test 8: Connection pooling and reconnection
TEST_F(DatabaseConnectivityTest, ReconnectionTest) {
    ASSERT_TRUE(db_->connect());
    EXPECT_TRUE(db_->is_connected());

    // Disconnect
    db_->disconnect();
    EXPECT_FALSE(db_->is_connected());

    // Reconnect
    ASSERT_TRUE(db_->connect());
    EXPECT_TRUE(db_->is_connected());

    // Verify can still execute queries
    std::string query = "SELECT 1 AS test";
    auto result = db_->execute(query);
    ASSERT_TRUE(result);
    EXPECT_GT(PQntuples(result.get()), 0);
}

// Test 9: Error handling
TEST_F(DatabaseConnectivityTest, ErrorHandling) {
    ASSERT_TRUE(db_->connect());

    // Test invalid query
    std::string invalid_query = "SELECT * FROM nonexistent_table_xyz";
    auto result = db_->execute(invalid_query);
    
    // Should return null result but not crash
    EXPECT_FALSE(result) << "Invalid query should return false/null";

    // Connection should still be valid
    EXPECT_TRUE(db_->is_connected()) << "Connection should survive query error";

    // Should be able to execute valid query after error
    std::string valid_query = "SELECT 1";
    auto valid_result = db_->execute(valid_query);
    EXPECT_TRUE(valid_result) << "Should be able to execute query after error";
}

// Test 10: Full data flow simulation (Frontend → API → Database)
TEST_F(DatabaseConnectivityTest, FullDataFlowSimulation) {
    ASSERT_TRUE(db_->connect());

    // Simulate frontend request to store case
    std::string store_query = R"(
        INSERT INTO case_base (case_id, domain, case_type, problem_description, solution_description, 
                              context_factors, outcome_metrics, confidence_score, usage_count, created_at)
        VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, NOW())
        RETURNING case_id, domain, confidence_score
    )";
    
    auto store_result = db_->execute_params(store_query, {
        "flow_test_case",
        "compliance",
        "full_flow_test",
        "End-to-end test scenario",
        "Complete solution",
        "{\"context\": \"test\"}",
        "{\"success\": true}",
        "0.92",
        "0"
    });
    
    ASSERT_TRUE(store_result) << "Failed to store case (simulating API POST)";
    EXPECT_GT(PQntuples(store_result.get()), 0) << "No data returned from INSERT";

    // Simulate frontend request to retrieve case
    std::string retrieve_query = R"(
        SELECT case_id, domain, case_type, problem_description, solution_description, 
               confidence_score, usage_count
        FROM case_base 
        WHERE case_id = $1
    )";
    
    auto retrieve_result = db_->execute_params(retrieve_query, {"flow_test_case"});
    ASSERT_TRUE(retrieve_result) << "Failed to retrieve case (simulating API GET)";
    ASSERT_GT(PQntuples(retrieve_result.get()), 0) << "Case not found";

    // Verify data integrity
    std::string case_id = PQgetvalue(retrieve_result.get(), 0, 0);
    std::string domain = PQgetvalue(retrieve_result.get(), 0, 1);
    std::string confidence = PQgetvalue(retrieve_result.get(), 0, 5);
    
    EXPECT_EQ(case_id, "flow_test_case");
    EXPECT_EQ(domain, "compliance");
    EXPECT_NEAR(std::stod(confidence), 0.92, 0.01);

    // Simulate frontend request to update case
    std::string update_query = "UPDATE case_base SET usage_count = usage_count + 1 WHERE case_id = $1";
    auto update_result = db_->execute_params(update_query, {"flow_test_case"});
    EXPECT_TRUE(update_result) << "Failed to update case (simulating API PUT)";

    // Simulate frontend request to delete case
    std::string delete_query = "DELETE FROM case_base WHERE case_id = $1";
    auto delete_result = db_->execute_params(delete_query, {"flow_test_case"});
    EXPECT_TRUE(delete_result) << "Failed to delete case (simulating API DELETE)";

    // Verify deletion
    auto verify_result = db_->execute_params(retrieve_query, {"flow_test_case"});
    ASSERT_TRUE(verify_result);
    EXPECT_EQ(PQntuples(verify_result.get()), 0) << "Case should be deleted";
}

} // namespace test
} // namespace regulens

// Main function for standalone execution
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

