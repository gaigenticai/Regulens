#ifndef REGULENS_TRAINING_API_HANDLERS_HPP
#define REGULENS_TRAINING_API_HANDLERS_HPP

#include <string>
#include <map>
#include <memory>
#include <libpq-fe.h>
#include <nlohmann/json.hpp>
#include "../database/postgresql_connection.hpp"
#include "../logging/structured_logger.hpp"

namespace regulens {
namespace training {

class TrainingAPIHandlers {
public:
    TrainingAPIHandlers(
        std::shared_ptr<PostgreSQLConnection> db_conn,
        StructuredLogger& logger
    );

    // Course management
    std::string handle_get_courses(const std::map<std::string, std::string>& query_params);
    std::string handle_get_course_by_id(const std::string& course_id);
    std::string handle_create_course(const std::string& request_body, const std::string& user_id);
    std::string handle_update_course(const std::string& course_id, const std::string& request_body);

    // Enrollment management
    std::string handle_enroll_user(const std::string& course_id, const std::string& request_body, const std::string& user_id);
    std::string handle_get_user_progress(const std::string& user_id, const std::map<std::string, std::string>& query_params);
    std::string handle_update_progress(const std::string& enrollment_id, const std::string& request_body);
    std::string handle_mark_complete(const std::string& course_id, const std::string& user_id);

    // Quiz management
    std::string handle_submit_quiz(const std::string& quiz_id, const std::string& request_body, const std::string& user_id);
    std::string handle_get_quiz_results(const std::string& enrollment_id);

    // Certifications
    std::string handle_get_certifications(const std::string& user_id);
    std::string handle_issue_certificate(const std::string& enrollment_id);
    std::string handle_verify_certificate(const std::string& verification_code);

    // Analytics
    std::string handle_get_leaderboard(const std::map<std::string, std::string>& query_params);
    std::string handle_get_training_stats(const std::string& user_id);

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    StructuredLogger& logger_;

    // Helper methods
    double calculate_quiz_score(const nlohmann::json& user_answers, const nlohmann::json& correct_answers);
    std::string generate_certificate_url(const std::string& user_id, const std::string& course_id);
    std::string generate_verification_code();
    std::string generate_certificate_hash(const std::string& user_id, const std::string& course_id, const std::string& issued_date);
    bool check_prerequisites(const std::string& user_id, const nlohmann::json& prerequisites);
    nlohmann::json serialize_course(PGresult* result, int row);
    nlohmann::json serialize_enrollment(PGresult* result, int row);
    nlohmann::json serialize_certification(PGresult* result, int row);
    nlohmann::json serialize_quiz_submission(PGresult* result, int row);
    std::string extract_user_id_from_jwt(const std::map<std::string, std::string>& headers);
    bool validate_json_schema(const nlohmann::json& data, const std::string& schema_type);
};

} // namespace training
} // namespace regulens

#endif // REGULENS_TRAINING_API_HANDLERS_HPP