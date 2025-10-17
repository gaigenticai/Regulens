#include "training_api_handlers.hpp"
#include <nlohmann/json.hpp>
#include <random>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <algorithm>

using json = nlohmann::json;

namespace regulens {
namespace training {

TrainingAPIHandlers::TrainingAPIHandlers(
    std::shared_ptr<PostgreSQLConnection> db_conn,
    std::shared_ptr<StructuredLogger> logger
) : db_conn_(db_conn), logger_(logger) {}

std::string TrainingAPIHandlers::handle_get_courses(const std::map<std::string, std::string>& query_params) {
    try {
        auto conn = db_conn_->get_connection();
        if (!conn) {
            return R"({"error": "Database connection failed"})";
        }

        std::string course_type = query_params.count("type") ? query_params.at("type") : "";
        std::string difficulty = query_params.count("difficulty") ? query_params.at("difficulty") : "";
        int limit = query_params.count("limit") ? std::stoi(query_params.at("limit")) : 50;
        int offset = query_params.count("offset") ? std::stoi(query_params.at("offset")) : 0;

        std::stringstream query;
        query << "SELECT course_id, title, description, course_type, difficulty_level, "
              << "duration_minutes, pass_threshold, tags, is_active, created_at, updated_at, created_by "
              << "FROM training_courses WHERE is_active = true";

        std::vector<std::string> params;
        int param_count = 0;

        if (!course_type.empty()) {
            query << " AND course_type = $" << (++param_count);
            params.push_back(course_type);
        }

        if (!difficulty.empty()) {
            query << " AND difficulty_level = $" << (++param_count);
            params.push_back(difficulty);
        }

        query << " ORDER BY created_at DESC LIMIT $" << (++param_count) << " OFFSET $" << (++param_count);
        params.push_back(std::to_string(limit));
        params.push_back(std::to_string(offset));

        std::vector<const char*> param_values;
        for (const auto& p : params) {
            param_values.push_back(p.c_str());
        }

        PGresult* result = PQexecParams(
            conn,
            query.str().c_str(),
            param_count,
            nullptr,
            param_values.data(),
            nullptr,
            nullptr,
            0
        );

        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            std::string error = PQerrorMessage(conn);
            PQclear(result);
            logger_->log(LogLevel::ERROR, "Failed to fetch training courses: " + error);
            return R"({"error": "Failed to fetch courses"})";
        }

        json courses = json::array();
        int num_rows = PQntuples(result);

        for (int i = 0; i < num_rows; i++) {
            courses.push_back(serialize_course(result, i));
        }

        PQclear(result);

        // Get total count for pagination
        std::stringstream count_query;
        count_query << "SELECT COUNT(*) FROM training_courses WHERE is_active = true";
        
        std::vector<std::string> count_params;
        int count_param_count = 0;
        
        if (!course_type.empty()) {
            count_query << " AND course_type = $" << (++count_param_count);
            count_params.push_back(course_type);
        }
        
        if (!difficulty.empty()) {
            count_query << " AND difficulty_level = $" << (++count_param_count);
            count_params.push_back(difficulty);
        }

        std::vector<const char*> count_param_values;
        for (const auto& p : count_params) {
            count_param_values.push_back(p.c_str());
        }

        PGresult* count_result = PQexecParams(
            conn,
            count_query.str().c_str(),
            count_param_count,
            nullptr,
            count_param_values.data(),
            nullptr,
            nullptr,
            0
        );

        int total_count = 0;
        if (PQresultStatus(count_result) == PGRES_TUPLES_OK && PQntuples(count_result) > 0) {
            total_count = std::stoi(PQgetvalue(count_result, 0, 0));
        }
        PQclear(count_result);

        json response = {
            {"courses", courses},
            {"pagination", {
                {"total", total_count},
                {"limit", limit},
                {"offset", offset},
                {"hasMore", (offset + num_rows) < total_count}
            }}
        };

        return response.dump();

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_get_courses: " + std::string(e.what()));
        return R"({"error": "Internal server error"})";
    }
}

std::string TrainingAPIHandlers::handle_get_course_by_id(const std::string& course_id) {
    try {
        auto conn = db_conn_->get_connection();
        if (!conn) {
            return R"({"error": "Database connection failed"})";
        }

        const char* params[1] = {course_id.c_str()};
        PGresult* result = PQexecParams(
            conn,
            "SELECT course_id, title, description, course_type, difficulty_level, "
            "duration_minutes, pass_threshold, course_content, prerequisites, tags, "
            "is_active, created_at, updated_at, created_by "
            "FROM training_courses WHERE course_id = $1 AND is_active = true",
            1, nullptr, params, nullptr, nullptr, 0
        );

        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            std::string error = PQerrorMessage(conn);
            PQclear(result);
            logger_->log(LogLevel::ERROR, "Failed to fetch course: " + error);
            return R"({"error": "Failed to fetch course"})";
        }

        if (PQntuples(result) == 0) {
            PQclear(result);
            return R"({"error": "Course not found"})";
        }

        json course = serialize_course(result, 0);
        
        // Parse JSON fields
        if (!PQgetisnull(result, 0, 7)) {
            course["content"] = json::parse(PQgetvalue(result, 0, 7));
        }
        if (!PQgetisnull(result, 0, 8)) {
            course["prerequisites"] = json::parse(PQgetvalue(result, 0, 8));
        }

        PQclear(result);
        return course.dump();

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_get_course_by_id: " + std::string(e.what()));
        return R"({"error": "Internal server error"})";
    }
}

std::string TrainingAPIHandlers::handle_create_course(const std::string& request_body, const std::string& user_id) {
    try {
        json request = json::parse(request_body);

        // Validate required fields
        if (!request.contains("title") || !request.contains("course_type") || 
            !request.contains("difficulty_level") || !request.contains("duration_minutes")) {
            return R"({"error": "Missing required fields: title, course_type, difficulty_level, duration_minutes"})";
        }

        auto conn = db_conn_->get_connection();
        if (!conn) {
            return R"({"error": "Database connection failed"})";
        }

        std::string title = request["title"];
        std::string description = request.value("description", "");
        std::string course_type = request["course_type"];
        std::string difficulty_level = request["difficulty_level"];
        int duration_minutes = request["duration_minutes"];
        double pass_threshold = request.value("pass_threshold", 80.0);
        json course_content = request.value("content", json::object());
        json prerequisites = request.value("prerequisites", json::array());
        json tags = request.value("tags", json::array());

        std::string duration_str = std::to_string(duration_minutes);
        std::string threshold_str = std::to_string(pass_threshold);
        std::string course_content_str = course_content.dump();
        std::string prerequisites_str = prerequisites.dump();
        std::string tags_str = tags.dump();

        const char* params[10] = {
            title.c_str(),
            description.c_str(),
            course_type.c_str(),
            difficulty_level.c_str(),
            duration_str.c_str(),
            threshold_str.c_str(),
            course_content_str.c_str(),
            prerequisites_str.c_str(),
            tags_str.c_str(),
            user_id.c_str()
        };

        PGresult* result = PQexecParams(
            conn,
            "INSERT INTO training_courses (title, description, course_type, difficulty_level, "
            "duration_minutes, pass_threshold, course_content, prerequisites, tags, created_by) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7::jsonb, $8::jsonb, $9, $10) "
            "RETURNING course_id, created_at",
            10, nullptr, params, nullptr, nullptr, 0
        );

        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            std::string error = PQerrorMessage(conn);
            PQclear(result);
            logger_->log(LogLevel::ERROR, "Failed to create course: " + error);
            return R"({"error": "Failed to create course"})";
        }

        json response = {
            {"course_id", PQgetvalue(result, 0, 0)},
            {"title", title},
            {"description", description},
            {"course_type", course_type},
            {"difficulty_level", difficulty_level},
            {"duration_minutes", duration_minutes},
            {"pass_threshold", pass_threshold},
            {"content", course_content},
            {"prerequisites", prerequisites},
            {"tags", tags},
            {"created_at", PQgetvalue(result, 0, 1)},
            {"created_by", user_id}
        };

        PQclear(result);
        return response.dump();

    } catch (const json::exception& e) {
        logger_->log(LogLevel::ERROR, "JSON parsing error in handle_create_course: " + std::string(e.what()));
        return R"({"error": "Invalid JSON format"})";
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_create_course: " + std::string(e.what()));
        return R"({"error": "Internal server error"})";
    }
}

std::string TrainingAPIHandlers::handle_enroll_user(const std::string& course_id, [[maybe_unused]] const std::string& request_body, const std::string& user_id) {
    try {
        auto conn = db_conn_->get_connection();
        if (!conn) {
            return R"({"error": "Database connection failed"})";
        }

        // Check if course exists and is active
        const char* course_params[1] = {course_id.c_str()};
        PGresult* course_result = PQexecParams(
            conn,
            "SELECT course_id, title, course_content, prerequisites FROM training_courses "
            "WHERE course_id = $1 AND is_active = true",
            1, nullptr, course_params, nullptr, nullptr, 0
        );

        if (PQresultStatus(course_result) != PGRES_TUPLES_OK || PQntuples(course_result) == 0) {
            PQclear(course_result);
            return R"({"error": "Course not found or inactive"})";
        }

        // Check prerequisites
        if (!PQgetisnull(course_result, 0, 3)) {
            json prerequisites = json::parse(PQgetvalue(course_result, 0, 3));
            if (!prerequisites.empty() && !check_prerequisites(user_id, prerequisites)) {
                PQclear(course_result);
                return R"({"error": "Prerequisites not met"})";
            }
        }

        // Check if already enrolled
        const char* enrollment_check_params[2] = {user_id.c_str(), course_id.c_str()};
        PGresult* enrollment_check = PQexecParams(
            conn,
            "SELECT enrollment_id FROM training_enrollments WHERE user_id = $1 AND course_id = $2",
            2, nullptr, enrollment_check_params, nullptr, nullptr, 0
        );

        if (PQntuples(enrollment_check) > 0) {
            PQclear(enrollment_check);
            PQclear(course_result);
            return R"({"error": "Already enrolled in this course"})";
        }
        PQclear(enrollment_check);

        // Create enrollment
        const char* enrollment_params[2] = {user_id.c_str(), course_id.c_str()};
        PGresult* enrollment_result = PQexecParams(
            conn,
            "INSERT INTO training_enrollments (user_id, course_id) VALUES ($1, $2) "
            "RETURNING enrollment_id, enrollment_date",
            2, nullptr, enrollment_params, nullptr, nullptr, 0
        );

        if (PQresultStatus(enrollment_result) != PGRES_TUPLES_OK) {
            std::string error = PQerrorMessage(conn);
            PQclear(enrollment_result);
            PQclear(course_result);
            logger_->log(LogLevel::ERROR, "Failed to enroll user: " + error);
            return R"({"error": "Failed to enroll in course"})";
        }

        json response = {
            {"enrollment_id", PQgetvalue(enrollment_result, 0, 0)},
            {"user_id", user_id},
            {"course_id", course_id},
            {"course_title", PQgetvalue(course_result, 0, 1)},
            {"enrollment_date", PQgetvalue(enrollment_result, 0, 1)},
            {"status", "enrolled"},
            {"message", "Successfully enrolled in course"}
        };

        PQclear(enrollment_result);
        PQclear(course_result);

        return response.dump();

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_enroll_user: " + std::string(e.what()));
        return R"({"error": "Internal server error"})";
    }
}

std::string TrainingAPIHandlers::handle_get_user_progress(const std::string& user_id, const std::map<std::string, std::string>& query_params) {
    try {
        auto conn = db_conn_->get_connection();
        if (!conn) {
            return R"({"error": "Database connection failed"})";
        }

        int limit = query_params.count("limit") ? std::stoi(query_params.at("limit")) : 50;
        int offset = query_params.count("offset") ? std::stoi(query_params.at("offset")) : 0;

        std::string limit_str = std::to_string(limit);
        std::string offset_str = std::to_string(offset);

        const char* params[3] = {
            user_id.c_str(),
            limit_str.c_str(),
            offset_str.c_str()
        };

        PGresult* result = PQexecParams(
            conn,
            "SELECT e.enrollment_id, e.course_id, e.enrollment_date, e.progress, "
            "e.current_module, e.status, e.quiz_attempts, e.quiz_score, e.completed_at, "
            "e.certificate_issued, e.last_accessed, c.title, c.course_type, c.difficulty_level "
            "FROM training_enrollments e "
            "JOIN training_courses c ON e.course_id = c.course_id "
            "WHERE e.user_id = $1 "
            "ORDER BY e.enrollment_date DESC "
            "LIMIT $2 OFFSET $3",
            3, nullptr, params, nullptr, nullptr, 0
        );

        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            std::string error = PQerrorMessage(conn);
            PQclear(result);
            logger_->log(LogLevel::ERROR, "Failed to fetch user progress: " + error);
            return R"({"error": "Failed to fetch progress"})";
        }

        json enrollments = json::array();
        int num_rows = PQntuples(result);

        for (int i = 0; i < num_rows; i++) {
            json enrollment = serialize_enrollment(result, i);
            enrollment["course_title"] = PQgetvalue(result, i, 11);
            enrollment["course_type"] = PQgetvalue(result, i, 12);
            enrollment["difficulty_level"] = PQgetvalue(result, i, 13);
            enrollments.push_back(enrollment);
        }

        PQclear(result);

        // Get total count
        const char* count_params[1] = {user_id.c_str()};
        PGresult* count_result = PQexecParams(
            conn,
            "SELECT COUNT(*) FROM training_enrollments WHERE user_id = $1",
            1, nullptr, count_params, nullptr, nullptr, 0
        );

        int total_count = 0;
        if (PQresultStatus(count_result) == PGRES_TUPLES_OK && PQntuples(count_result) > 0) {
            total_count = std::stoi(PQgetvalue(count_result, 0, 0));
        }
        PQclear(count_result);

        // Get overall stats
        const char* stats_params[1] = {user_id.c_str()};
        PGresult* stats_result = PQexecParams(
            conn,
            "SELECT "
            "COUNT(*) as total_enrollments, "
            "COUNT(*) FILTER (WHERE status = 'completed') as completed_courses, "
            "AVG(progress) as avg_progress, "
            "AVG(quiz_score) as avg_quiz_score, "
            "SUM(duration_minutes) as total_learning_time "
            "FROM training_enrollments e "
            "JOIN training_courses c ON e.course_id = c.course_id "
            "WHERE e.user_id = $1",
            1, nullptr, stats_params, nullptr, nullptr, 0
        );

        json stats = json::object();
        if (PQresultStatus(stats_result) == PGRES_TUPLES_OK && PQntuples(stats_result) > 0) {
            stats["total_enrollments"] = std::stoi(PQgetvalue(stats_result, 0, 0));
            stats["completed_courses"] = std::stoi(PQgetvalue(stats_result, 0, 1));
            
            if (!PQgetisnull(stats_result, 0, 2)) {
                stats["average_progress"] = std::stod(PQgetvalue(stats_result, 0, 2));
            }
            if (!PQgetisnull(stats_result, 0, 3)) {
                stats["average_quiz_score"] = std::stod(PQgetvalue(stats_result, 0, 3));
            }
            if (!PQgetisnull(stats_result, 0, 4)) {
                stats["total_learning_time"] = std::stoi(PQgetvalue(stats_result, 0, 4));
            }
        }
        PQclear(stats_result);

        json response = {
            {"enrollments", enrollments},
            {"pagination", {
                {"total", total_count},
                {"limit", limit},
                {"offset", offset},
                {"hasMore", (offset + num_rows) < total_count}
            }},
            {"stats", stats}
        };

        return response.dump();

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_get_user_progress: " + std::string(e.what()));
        return R"({"error": "Internal server error"})";
    }
}

std::string TrainingAPIHandlers::handle_submit_quiz(const std::string& quiz_id, const std::string& request_body, const std::string& user_id) {
    try {
        json request = json::parse(request_body);
        std::string course_id = request["courseId"];
        json user_answers = request["answers"];
        int time_taken = request.value("timeTaken", 0);

        auto conn = db_conn_->get_connection();
        if (!conn) {
            return R"({"error": "Database connection failed"})";
        }

        // Get enrollment
        const char* enrollment_params[2] = {user_id.c_str(), course_id.c_str()};
        PGresult* enrollment_result = PQexecParams(
            conn,
            "SELECT enrollment_id FROM training_enrollments WHERE user_id = $1 AND course_id = $2",
            2, nullptr, enrollment_params, nullptr, nullptr, 0
        );

        if (PQntuples(enrollment_result) == 0) {
            PQclear(enrollment_result);
            return R"({"error": "Enrollment not found"})";
        }

        std::string enrollment_id = PQgetvalue(enrollment_result, 0, 0);
        PQclear(enrollment_result);

        // Get course with correct answers
        const char* course_params[1] = {course_id.c_str()};
        PGresult* course_result = PQexecParams(
            conn,
            "SELECT course_content, pass_threshold FROM training_courses WHERE course_id = $1",
            1, nullptr, course_params, nullptr, nullptr, 0
        );

        if (PQntuples(course_result) == 0) {
            PQclear(course_result);
            return R"({"error": "Course not found"})";
        }

        json course_content = json::parse(PQgetvalue(course_result, 0, 0));
        double pass_threshold = std::stod(PQgetvalue(course_result, 0, 1));
        PQclear(course_result);

        // Calculate score
        json correct_answers = course_content["quizzes"][quiz_id]["answers"];
        double score = calculate_quiz_score(user_answers, correct_answers);
        bool passed = score >= pass_threshold;

        // Build feedback
        json feedback = json::array();
        for (auto& [question_id, user_answer] : user_answers.items()) {
            bool correct = false;
            if (correct_answers.contains(question_id)) {
                correct = (user_answer == correct_answers[question_id]);
            }
            feedback.push_back({
                {"questionId", question_id},
                {"userAnswer", user_answer},
                {"correctAnswer", correct_answers.value(question_id, "")},
                {"isCorrect", correct}
            });
        }

        // Store submission
        std::string feedback_str = feedback.dump();
        std::string score_str = std::to_string(score);
        std::string time_taken_str = std::to_string(time_taken);
        std::string user_answers_str = user_answers.dump();
        const char* submission_params[7] = {
            enrollment_id.c_str(),
            quiz_id.c_str(),
            user_answers_str.c_str(),
            score_str.c_str(),
            passed ? "true" : "false",
            time_taken_str.c_str(),
            feedback_str.c_str()
        };

        PGresult* insert_result = PQexecParams(
            conn,
            "INSERT INTO training_quiz_submissions (enrollment_id, quiz_id, user_answers, score, passed, time_taken_seconds, feedback) "
            "VALUES ($1, $2, $3::jsonb, $4, $5, $6, $7::jsonb) RETURNING submission_id",
            7, nullptr, submission_params, nullptr, nullptr, 0
        );

        if (PQresultStatus(insert_result) != PGRES_TUPLES_OK) {
            std::string error = "Database operation failed";
            PQclear(insert_result);
            return R"({"error": "Failed to save quiz submission"})";
        }

        std::string submission_id = PQgetvalue(insert_result, 0, 0);
        PQclear(insert_result);

        // Update enrollment
        std::string score_str2 = std::to_string(score);
        const char* update_params[3] = {
            score_str2.c_str(),
            enrollment_id.c_str(),
            score_str2.c_str()
        };

        PGresult* update_result = PQexecParams(
            conn,
            "UPDATE training_enrollments SET quiz_score = $1, quiz_attempts = quiz_attempts + 1, "
            "status = CASE WHEN $3 >= (SELECT pass_threshold FROM training_courses WHERE course_id = (SELECT course_id FROM training_enrollments WHERE enrollment_id = $2)) THEN 'completed' ELSE status END, "
            "last_accessed = CURRENT_TIMESTAMP "
            "WHERE enrollment_id = $2",
            3, nullptr, update_params, nullptr, nullptr, 0
        );

        if (PQresultStatus(update_result) != PGRES_COMMAND_OK) {
            logger_->log(LogLevel::ERROR, "Failed to update enrollment status");
        }
        PQclear(update_result);

        // Issue certificate if passed
        std::string certificate_url = "";
        if (passed) {
            certificate_url = generate_certificate_url(user_id, course_id);
            std::string cert_hash = generate_certificate_hash(user_id, course_id, std::to_string(std::time(nullptr)));

            const char* cert_params[4] = {
                user_id.c_str(),
                course_id.c_str(),
                certificate_url.c_str(),
                cert_hash.c_str()
            };

            PGresult* cert_result = PQexecParams(
                conn,
                "INSERT INTO training_certifications (user_id, course_id, certification_name, certificate_url, certificate_hash, verification_code) "
                "VALUES ($1, $2, (SELECT title || ' Certification' FROM training_courses WHERE course_id = $2), $3, $4, $5) "
                "RETURNING certification_id",
                4, nullptr, cert_params, nullptr, nullptr, 0
            );
            
            if (PQresultStatus(cert_result) == PGRES_TUPLES_OK && PQntuples(cert_result) > 0) {
                // Update enrollment to mark certificate as issued
                const char* cert_update_params[2] = {PQgetvalue(cert_result, 0, 0), enrollment_id.c_str()};
                PQexecParams(
                    conn,
                    "UPDATE training_enrollments SET certificate_issued = true, certificate_url = $1 WHERE enrollment_id = $2",
                    2, nullptr, cert_update_params, nullptr, nullptr, 0
                );
            }
            PQclear(cert_result);
        }

        json response = {
            {"submission_id", submission_id},
            {"score", score},
            {"passed", passed},
            {"feedback", feedback},
            {"pass_threshold", pass_threshold},
            {"certificate_url", certificate_url},
            {"message", passed ? "Quiz passed successfully!" : "Quiz not passed. Please review and try again."}
        };

        return response.dump();

    } catch (const json::exception& e) {
        logger_->log(LogLevel::ERROR, "JSON parsing error in handle_submit_quiz: " + std::string(e.what()));
        return R"({"error": "Invalid JSON format"})";
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_submit_quiz: " + std::string(e.what()));
        return R"({"error": "Internal server error"})";
    }
}

std::string TrainingAPIHandlers::handle_get_certifications(const std::string& user_id) {
    try {
        if (!db_conn_->is_connected()) {
            return R"({"error": "Database connection failed"})";
        }

        auto conn = db_conn_->get_connection();
        if (!conn) {
            return R"({"error": "Database connection failed"})";
        }

        const char* params[1] = {user_id.c_str()};
        PGresult* result = PQexecParams(
            conn,
            "SELECT c.certification_id, c.course_id, c.certification_name, c.issued_date, "
            "c.expiry_date, c.certificate_url, c.verification_code, c.is_valid, "
            "t.title as course_title, t.course_type "
            "FROM training_certifications c "
            "JOIN training_courses t ON c.course_id = t.course_id "
            "WHERE c.user_id = $1 AND c.is_valid = true "
            "ORDER BY c.issued_date DESC",
            1, nullptr, params, nullptr, nullptr, 0
        );

        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            std::string error = "Database operation failed";
            PQclear(result);
            logger_->log(LogLevel::ERROR, "Failed to fetch certifications: " + error);
            return R"({"error": "Failed to fetch certifications"})";
        }

        json certifications = json::array();
        int num_rows = PQntuples(result);

        for (int i = 0; i < num_rows; i++) {
            certifications.push_back(serialize_certification(result, i));
        }

        PQclear(result);

        json response = {
            {"certifications", certifications},
            {"count", num_rows}
        };

        return response.dump();

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_get_certifications: " + std::string(e.what()));
        return R"({"error": "Internal server error"})";
    }
}

std::string TrainingAPIHandlers::handle_get_leaderboard(const std::map<std::string, std::string>& query_params) {
    try {
        if (!db_conn_->is_connected()) {
            return R"({"error": "Database connection failed"})";
        }

        std::string time_range = query_params.count("timeRange") ? query_params.at("timeRange") : "all";
        int limit = query_params.count("limit") ? std::stoi(query_params.at("limit")) : 20;

        std::string limit_str = std::to_string(limit);

        auto conn = db_conn_->get_connection();
        if (!conn) {
            return R"({"error": "Database connection failed"})";
        }

        std::stringstream query;
        query << "SELECT e.user_id, u.name as user_name, u.email, COUNT(*) as courses_completed, "
              << "AVG(e.quiz_score) as avg_score, SUM(c.duration_minutes) as total_time, "
              << "MAX(e.completed_at) as last_completion "
              << "FROM training_enrollments e "
              << "JOIN training_courses c ON e.course_id = c.course_id "
              << "JOIN users u ON e.user_id = u.id "
              << "WHERE e.status = 'completed'";

        if (time_range == "30d") {
            query << " AND e.completed_at >= CURRENT_TIMESTAMP - INTERVAL '30 days'";
        } else if (time_range == "90d") {
            query << " AND e.completed_at >= CURRENT_TIMESTAMP - INTERVAL '90 days'";
        } else if (time_range == "1y") {
            query << " AND e.completed_at >= CURRENT_TIMESTAMP - INTERVAL '1 year'";
        }

        int param_count = 0;
        query << " GROUP BY e.user_id, u.name, u.email "
              << "ORDER BY avg_score DESC, courses_completed DESC, total_time ASC "
              << "LIMIT $" << (++param_count);

        const char* params[1] = {limit_str.c_str()};

        PGresult* result = PQexecParams(
            conn,
            query.str().c_str(),
            1, nullptr, params, nullptr, nullptr, 0
        );

        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            std::string error = PQerrorMessage(conn);
            PQclear(result);
            logger_->log(LogLevel::ERROR, "Failed to fetch leaderboard: " + error);
            return R"({"error": "Failed to fetch leaderboard"})";
        }

        json leaderboard = json::array();
        int num_rows = PQntuples(result);

        for (int i = 0; i < num_rows; i++) {
            json entry = {
                {"rank", i + 1},
                {"user_id", PQgetvalue(result, i, 0)},
                {"user_name", PQgetvalue(result, i, 1)},
                {"user_email", PQgetvalue(result, i, 2)},
                {"courses_completed", std::stoi(PQgetvalue(result, i, 3))},
                {"average_score", std::stod(PQgetvalue(result, i, 4))},
                {"total_learning_time", std::stoi(PQgetvalue(result, i, 5))},
                {"last_completion", PQgetvalue(result, i, 6)}
            };
            leaderboard.push_back(entry);
        }

        PQclear(result);

        json response = {
            {"leaderboard", leaderboard},
            {"time_range", time_range},
            {"count", num_rows}
        };

        return response.dump();

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_get_leaderboard: " + std::string(e.what()));
        return R"({"error": "Internal server error"})";
    }
}

// Helper methods implementation
double TrainingAPIHandlers::calculate_quiz_score(const json& user_answers, const json& correct_answers) {
    int total_questions = correct_answers.size();
    if (total_questions == 0) return 0.0;

    int correct_count = 0;
    for (auto& [question_id, correct_answer] : correct_answers.items()) {
        if (user_answers.contains(question_id) && user_answers[question_id] == correct_answer) {
            correct_count++;
        }
    }

    return (static_cast<double>(correct_count) / total_questions) * 100.0;
}

std::string TrainingAPIHandlers::generate_certificate_url([[maybe_unused]] const std::string& user_id, [[maybe_unused]] const std::string& course_id) {
    // In production, this would be a real URL to a certificate service
    // For now, generate a placeholder URL
    return "https://certificates.regulens.com/cert/" + generate_verification_code();
}

std::string TrainingAPIHandlers::generate_verification_code() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 35);

    const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::stringstream ss;

    for (int i = 0; i < 12; i++) {
        if (i > 0 && i % 4 == 0) ss << "-";
        ss << chars[dis(gen)];
    }

    return ss.str();
}

std::string TrainingAPIHandlers::generate_certificate_hash(const std::string& user_id, const std::string& course_id, const std::string& issued_date) {
    std::string input = user_id + course_id + issued_date;
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;

    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(mdctx, input.c_str(), input.length());
    EVP_DigestFinal_ex(mdctx, hash, &hash_len);
    EVP_MD_CTX_free(mdctx);

    std::stringstream ss;
    for(unsigned int i = 0; i < hash_len; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }

    return ss.str();
}

bool TrainingAPIHandlers::check_prerequisites(const std::string& user_id, const json& prerequisites) {
    if (!prerequisites.is_array() || prerequisites.empty()) {
        return true;
    }

    auto conn = db_conn_->get_connection();
    if (!conn) {
        return false;
    }

    for (const auto& prereq : prerequisites) {
        std::string prereq_course_id = prereq;
        
        // Check if user has completed this prerequisite course
        const char* params[2] = {user_id.c_str(), prereq_course_id.c_str()};
        PGresult* result = PQexecParams(
            conn,
            "SELECT COUNT(*) FROM training_enrollments "
            "WHERE user_id = $1 AND course_id = $2 AND status = 'completed'",
            2, nullptr, params, nullptr, nullptr, 0
        );

        bool completed = false;
        if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) > 0) {
            completed = std::stoi(PQgetvalue(result, 0, 0)) > 0;
        }
        PQclear(result);

        if (!completed) {
            return false;
        }
    }

    return true;
}

json TrainingAPIHandlers::serialize_course(PGresult* result, int row) {
    return {
        {"course_id", PQgetvalue(result, row, 0)},
        {"title", PQgetvalue(result, row, 1)},
        {"description", PQgetvalue(result, row, 2)},
        {"course_type", PQgetvalue(result, row, 3)},
        {"difficulty_level", PQgetvalue(result, row, 4)},
        {"duration_minutes", std::stoi(PQgetvalue(result, row, 5))},
        {"pass_threshold", std::stod(PQgetvalue(result, row, 6))},
        {"tags", json::parse(PQgetvalue(result, row, 8))},
        {"is_active", std::string(PQgetvalue(result, row, 9)) == "t"},
        {"created_at", PQgetvalue(result, row, 10)},
        {"updated_at", PQgetvalue(result, row, 11)},
        {"created_by", PQgetvalue(result, row, 12)}
    };
}

json TrainingAPIHandlers::serialize_enrollment(PGresult* result, int row) {
    json enrollment = {
        {"enrollment_id", PQgetvalue(result, row, 0)},
        {"course_id", PQgetvalue(result, row, 1)},
        {"enrollment_date", PQgetvalue(result, row, 2)},
        {"progress", std::stod(PQgetvalue(result, row, 3))},
        {"current_module", std::stoi(PQgetvalue(result, row, 4))},
        {"status", PQgetvalue(result, row, 5)},
        {"quiz_attempts", std::stoi(PQgetvalue(result, row, 6))},
        {"last_accessed", PQgetvalue(result, row, 9)},
        {"certificate_issued", std::string(PQgetvalue(result, row, 10)) == "t"}
    };

    if (!PQgetisnull(result, row, 7)) {
        enrollment["quiz_score"] = std::stod(PQgetvalue(result, row, 7));
    }
    if (!PQgetisnull(result, row, 8)) {
        enrollment["completed_at"] = PQgetvalue(result, row, 8);
    }

    return enrollment;
}

json TrainingAPIHandlers::serialize_certification(PGresult* result, int row) {
    json certification = {
        {"certification_id", PQgetvalue(result, row, 0)},
        {"course_id", PQgetvalue(result, row, 1)},
        {"certification_name", PQgetvalue(result, row, 2)},
        {"issued_date", PQgetvalue(result, row, 3)},
        {"certificate_url", PQgetvalue(result, row, 5)},
        {"verification_code", PQgetvalue(result, row, 6)},
        {"is_valid", std::string(PQgetvalue(result, row, 7)) == "t"},
        {"course_title", PQgetvalue(result, row, 8)},
        {"course_type", PQgetvalue(result, row, 9)}
    };

    if (!PQgetisnull(result, row, 4)) {
        certification["expiry_date"] = PQgetvalue(result, row, 4);
    }

    return certification;
}

json TrainingAPIHandlers::serialize_quiz_submission(PGresult* result, int row) {
    json submission = {
        {"submission_id", PQgetvalue(result, row, 0)},
        {"enrollment_id", PQgetvalue(result, row, 1)},
        {"quiz_id", PQgetvalue(result, row, 2)},
        {"user_answers", json::parse(PQgetvalue(result, row, 3))},
        {"score", std::stod(PQgetvalue(result, row, 4))},
        {"passed", std::string(PQgetvalue(result, row, 5)) == "t"},
        {"submitted_at", PQgetvalue(result, row, 6)},
        {"feedback", json::parse(PQgetvalue(result, row, 8))}
    };

    if (!PQgetisnull(result, row, 7)) {
        submission["time_taken_seconds"] = std::stoi(PQgetvalue(result, row, 7));
    }

    return submission;
}

std::string TrainingAPIHandlers::handle_get_training_stats(const std::string& user_id) {
    try {
        auto conn = db_conn_->get_connection();
        if (!conn) {
            return R"({"error": "Database connection failed"})";
        }

        const char* params[1] = {user_id.c_str()};
        PGresult* result = PQexecParams(
            conn,
            "SELECT "
            "COUNT(*) as total_enrollments, "
            "COUNT(*) FILTER (WHERE status = 'completed') as completed_courses, "
            "AVG(progress) as avg_progress, "
            "AVG(quiz_score) as avg_quiz_score, "
            "SUM(CASE WHEN quiz_score >= 80 THEN 1 ELSE 0 END) as passed_quizzes, "
            "SUM(quiz_attempts) as total_quiz_attempts, "
            "SUM(c.duration_minutes) as total_learning_time "
            "FROM training_enrollments e "
            "JOIN training_courses c ON e.course_id = c.course_id "
            "WHERE e.user_id = $1",
            1, nullptr, params, nullptr, nullptr, 0
        );

        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            std::string error = PQerrorMessage(conn);
            PQclear(result);
            logger_->log(LogLevel::ERROR, "Failed to fetch training stats: " + error);
            return R"({"error": "Failed to fetch training stats"})";
        }

        json stats;
        if (PQntuples(result) > 0) {
            stats["total_enrollments"] = std::stoi(PQgetvalue(result, 0, 0));
            stats["completed_courses"] = std::stoi(PQgetvalue(result, 0, 1));
            
            if (!PQgetisnull(result, 0, 2)) {
                stats["average_progress"] = std::stod(PQgetvalue(result, 0, 2));
            }
            if (!PQgetisnull(result, 0, 3)) {
                stats["average_quiz_score"] = std::stod(PQgetvalue(result, 0, 3));
            }
            stats["passed_quizzes"] = std::stoi(PQgetvalue(result, 0, 4));
            stats["total_quiz_attempts"] = std::stoi(PQgetvalue(result, 0, 5));
            
            if (!PQgetisnull(result, 0, 6)) {
                stats["total_learning_time"] = std::stoi(PQgetvalue(result, 0, 6));
            }

            // Calculate derived stats
            int total_attempts = stats["total_quiz_attempts"];
            if (total_attempts > 0) {
                stats["quiz_success_rate"] = (static_cast<double>(stats["passed_quizzes"]) / total_attempts) * 100.0;
            } else {
                stats["quiz_success_rate"] = 0.0;
            }

            int total_enrollments = stats["total_enrollments"];
            if (total_enrollments > 0) {
                stats["completion_rate"] = (static_cast<double>(stats["completed_courses"]) / total_enrollments) * 100.0;
            } else {
                stats["completion_rate"] = 0.0;
            }
        }

        PQclear(result);

        // Get certifications count
        PGresult* cert_result = PQexecParams(
            conn,
            "SELECT COUNT(*) FROM training_certifications WHERE user_id = $1 AND is_valid = true",
            1, nullptr, params, nullptr, nullptr, 0
        );

        if (PQresultStatus(cert_result) == PGRES_TUPLES_OK && PQntuples(cert_result) > 0) {
            stats["active_certifications"] = std::stoi(PQgetvalue(cert_result, 0, 0));
        }
        PQclear(cert_result);

        json response = {
            {"stats", stats},
            {"user_id", user_id},
            {"generated_at", std::to_string(std::time(nullptr))}
        };

        return response.dump();

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_get_training_stats: " + std::string(e.what()));
        return R"({"error": "Internal server error"})";
    }
}

std::string TrainingAPIHandlers::handle_update_progress(const std::string& enrollment_id, const std::string& request_body) {
    try {
        json request = json::parse(request_body);

        if (!request.contains("progress") || !request.contains("current_module")) {
            return R"({"error": "Missing required fields: progress, current_module"})";
        }

        auto conn = db_conn_->get_connection();
        if (!conn) {
            return R"({"error": "Database connection failed"})";
        }

        double progress = request["progress"];
        int current_module = request["current_module"];

        std::string progress_str = std::to_string(progress);
        std::string current_module_str = std::to_string(current_module);

        const char* params[3] = {
            progress_str.c_str(),
            current_module_str.c_str(),
            enrollment_id.c_str()
        };

        PGresult* result = PQexecParams(
            conn,
            "UPDATE training_enrollments SET progress = $1, current_module = $2, "
            "last_accessed = CURRENT_TIMESTAMP WHERE enrollment_id = $3 "
            "RETURNING enrollment_id, progress, current_module, last_accessed",
            3, nullptr, params, nullptr, nullptr, 0
        );

        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            std::string error = PQerrorMessage(conn);
            PQclear(result);
            logger_->log(LogLevel::ERROR, "Failed to update progress: " + error);
            return R"({"error": "Failed to update progress"})";
        }

        if (PQntuples(result) == 0) {
            PQclear(result);
            return R"({"error": "Enrollment not found"})";
        }

        json response = {
            {"enrollment_id", PQgetvalue(result, 0, 0)},
            {"progress", std::stod(PQgetvalue(result, 0, 1))},
            {"current_module", std::stoi(PQgetvalue(result, 0, 2))},
            {"last_accessed", PQgetvalue(result, 0, 3)},
            {"message", "Progress updated successfully"}
        };

        PQclear(result);
        return response.dump();

    } catch (const json::exception& e) {
        logger_->log(LogLevel::ERROR, "JSON parsing error in handle_update_progress: " + std::string(e.what()));
        return R"({"error": "Invalid JSON format"})";
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_update_progress: " + std::string(e.what()));
        return R"({"error": "Internal server error"})";
    }
}

std::string TrainingAPIHandlers::handle_mark_complete(const std::string& course_id, const std::string& user_id) {
    try {
        auto conn = db_conn_->get_connection();
        if (!conn) {
            return R"({"error": "Database connection failed"})";
        }

        const char* params[2] = {user_id.c_str(), course_id.c_str()};
        PGresult* result = PQexecParams(
            conn,
            "UPDATE training_enrollments SET status = 'completed', completed_at = CURRENT_TIMESTAMP, "
            "progress = 100.0 WHERE user_id = $1 AND course_id = $2 "
            "RETURNING enrollment_id, completed_at",
            2, nullptr, params, nullptr, nullptr, 0
        );

        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            std::string error = PQerrorMessage(conn);
            PQclear(result);
            logger_->log(LogLevel::ERROR, "Failed to mark course complete: " + error);
            return R"({"error": "Failed to mark course complete"})";
        }

        if (PQntuples(result) == 0) {
            PQclear(result);
            return R"({"error": "Enrollment not found"})";
        }

        // Issue certificate
        std::string certificate_url = generate_certificate_url(user_id, course_id);
        std::string cert_hash = generate_certificate_hash(user_id, course_id, std::to_string(std::time(nullptr)));

        const char* cert_params[4] = {
            user_id.c_str(),
            course_id.c_str(),
            certificate_url.c_str(),
            cert_hash.c_str()
        };

        PGresult* cert_result = PQexecParams(
            conn,
            "INSERT INTO training_certifications (user_id, course_id, certification_name, certificate_url, certificate_hash, verification_code) "
            "VALUES ($1, $2, (SELECT title || ' Certification' FROM training_courses WHERE course_id = $2), $3, $4, $5) "
            "RETURNING certification_id",
            4, nullptr, cert_params, nullptr, nullptr, 0
        );

        std::string certification_id = "";
        if (PQresultStatus(cert_result) == PGRES_TUPLES_OK && PQntuples(cert_result) > 0) {
            certification_id = PQgetvalue(cert_result, 0, 0);
        }
        PQclear(cert_result);

        json response = {
            {"enrollment_id", PQgetvalue(result, 0, 0)},
            {"completed_at", PQgetvalue(result, 0, 1)},
            {"certification_id", certification_id},
            {"certificate_url", certificate_url},
            {"message", "Course marked as completed successfully"}
        };

        PQclear(result);
        return response.dump();

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_mark_complete: " + std::string(e.what()));
        return R"({"error": "Internal server error"})";
    }
}

std::string TrainingAPIHandlers::handle_get_quiz_results(const std::string& enrollment_id) {
    try {
        auto conn = db_conn_->get_connection();
        if (!conn) {
            return R"({"error": "Database connection failed"})";
        }

        const char* params[1] = {enrollment_id.c_str()};
        PGresult* result = PQexecParams(
            conn,
            "SELECT submission_id, quiz_id, user_answers, score, passed, submitted_at, "
            "time_taken_seconds, feedback FROM training_quiz_submissions "
            "WHERE enrollment_id = $1 ORDER BY submitted_at DESC",
            1, nullptr, params, nullptr, nullptr, 0
        );

        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            std::string error = PQerrorMessage(conn);
            PQclear(result);
            logger_->log(LogLevel::ERROR, "Failed to fetch quiz results: " + error);
            return R"({"error": "Failed to fetch quiz results"})";
        }

        json results = json::array();
        int num_rows = PQntuples(result);

        for (int i = 0; i < num_rows; i++) {
            results.push_back(serialize_quiz_submission(result, i));
        }

        PQclear(result);

        json response = {
            {"quiz_results", results},
            {"count", num_rows},
            {"enrollment_id", enrollment_id}
        };

        return response.dump();

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_get_quiz_results: " + std::string(e.what()));
        return R"({"error": "Internal server error"})";
    }
}

std::string TrainingAPIHandlers::handle_issue_certificate(const std::string& enrollment_id) {
    try {
        auto conn = db_conn_->get_connection();
        if (!conn) {
            return R"({"error": "Database connection failed"})";
        }

        // Get enrollment details
        const char* params[1] = {enrollment_id.c_str()};
        PGresult* enrollment_result = PQexecParams(
            conn,
            "SELECT user_id, course_id, quiz_score FROM training_enrollments "
            "WHERE enrollment_id = $1 AND status = 'completed'",
            1, nullptr, params, nullptr, nullptr, 0
        );

        if (PQresultStatus(enrollment_result) != PGRES_TUPLES_OK || PQntuples(enrollment_result) == 0) {
            PQclear(enrollment_result);
            return R"({"error": "Enrollment not found or not completed"})";
        }

        std::string user_id = PQgetvalue(enrollment_result, 0, 0);
        std::string course_id = PQgetvalue(enrollment_result, 0, 1);
        PQclear(enrollment_result);

        // Check if certificate already exists
        const char* check_params[2] = {user_id.c_str(), course_id.c_str()};
        PGresult* check_result = PQexecParams(
            conn,
            "SELECT certification_id FROM training_certifications "
            "WHERE user_id = $1 AND course_id = $2 AND is_valid = true",
            2, nullptr, check_params, nullptr, nullptr, 0
        );

        if (PQntuples(check_result) > 0) {
            std::string existing_cert_id = PQgetvalue(check_result, 0, 0);
            PQclear(check_result);
            return R"({"error": "Certificate already issued", "certification_id": ")" + existing_cert_id + R"("})";
        }
        PQclear(check_result);

        // Issue new certificate
        std::string certificate_url = generate_certificate_url(user_id, course_id);
        std::string cert_hash = generate_certificate_hash(user_id, course_id, std::to_string(std::time(nullptr)));

        const char* cert_params[4] = {
            user_id.c_str(),
            course_id.c_str(),
            certificate_url.c_str(),
            cert_hash.c_str()
        };

        PGresult* cert_result = PQexecParams(
            conn,
            "INSERT INTO training_certifications (user_id, course_id, certification_name, certificate_url, certificate_hash, verification_code) "
            "VALUES ($1, $2, (SELECT title || ' Certification' FROM training_courses WHERE course_id = $2), $3, $4, $5) "
            "RETURNING certification_id, verification_code",
            4, nullptr, cert_params, nullptr, nullptr, 0
        );

        if (PQresultStatus(cert_result) != PGRES_TUPLES_OK) {
            std::string error = PQerrorMessage(conn);
            PQclear(cert_result);
            logger_->log(LogLevel::ERROR, "Failed to issue certificate: " + error);
            return R"({"error": "Failed to issue certificate"})";
        }

        json response = {
            {"certification_id", PQgetvalue(cert_result, 0, 0)},
            {"verification_code", PQgetvalue(cert_result, 0, 1)},
            {"certificate_url", certificate_url},
            {"message", "Certificate issued successfully"}
        };

        PQclear(cert_result);
        return response.dump();

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_issue_certificate: " + std::string(e.what()));
        return R"({"error": "Internal server error"})";
    }
}

std::string TrainingAPIHandlers::handle_verify_certificate(const std::string& verification_code) {
    try {
        auto conn = db_conn_->get_connection();
        if (!conn) {
            return R"({"error": "Database connection failed"})";
        }

        const char* params[1] = {verification_code.c_str()};
        PGresult* result = PQexecParams(
            conn,
            "SELECT c.certification_id, c.user_id, c.course_id, c.certification_name, "
            "c.issued_date, c.expiry_date, c.certificate_url, c.is_valid, "
            "u.name as user_name, u.email as user_email, "
            "t.title as course_title, t.course_type "
            "FROM training_certifications c "
            "JOIN users u ON c.user_id = u.id "
            "JOIN training_courses t ON c.course_id = t.course_id "
            "WHERE c.verification_code = $1",
            1, nullptr, params, nullptr, nullptr, 0
        );

        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            std::string error = PQerrorMessage(conn);
            PQclear(result);
            logger_->log(LogLevel::ERROR, "Failed to verify certificate: " + error);
            return R"({"error": "Failed to verify certificate"})";
        }

        if (PQntuples(result) == 0) {
            PQclear(result);
            return R"({"error": "Certificate not found", "valid": false})";
        }

        json certificate = serialize_certification(result, 0);
        certificate["user_name"] = PQgetvalue(result, 0, 8);
        certificate["user_email"] = PQgetvalue(result, 0, 9);
        certificate["course_title"] = PQgetvalue(result, 0, 10);
        certificate["course_type"] = PQgetvalue(result, 0, 11);
        certificate["valid"] = true;
        certificate["verified_at"] = std::to_string(std::time(nullptr));

        PQclear(result);
        return certificate.dump();

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_verify_certificate: " + std::string(e.what()));
        return R"({"error": "Internal server error"})";
    }
}

std::string TrainingAPIHandlers::handle_update_course(const std::string& course_id, const std::string& request_body) {
    try {
        json request = json::parse(request_body);

        auto conn = db_conn_->get_connection();
        if (!conn) {
            return R"({"error": "Database connection failed"})";
        }

        std::vector<std::string> updates;
        std::vector<std::string> params;
        int param_count = 0;

        if (request.contains("title")) {
            updates.push_back("title = $" + std::to_string(++param_count));
            params.push_back(request["title"]);
        }
        if (request.contains("description")) {
            updates.push_back("description = $" + std::to_string(++param_count));
            params.push_back(request["description"]);
        }
        if (request.contains("course_type")) {
            updates.push_back("course_type = $" + std::to_string(++param_count));
            params.push_back(request["course_type"]);
        }
        if (request.contains("difficulty_level")) {
            updates.push_back("difficulty_level = $" + std::to_string(++param_count));
            params.push_back(request["difficulty_level"]);
        }
        if (request.contains("duration_minutes")) {
            updates.push_back("duration_minutes = $" + std::to_string(++param_count));
            params.push_back(std::to_string(request["duration_minutes"].get<int>()));
        }
        if (request.contains("pass_threshold")) {
            updates.push_back("pass_threshold = $" + std::to_string(++param_count));
            params.push_back(std::to_string(request["pass_threshold"].get<double>()));
        }
        if (request.contains("content")) {
            updates.push_back("course_content = $" + std::to_string(++param_count) + "::jsonb");
            params.push_back(request["content"].dump());
        }
        if (request.contains("prerequisites")) {
            updates.push_back("prerequisites = $" + std::to_string(++param_count) + "::jsonb");
            params.push_back(request["prerequisites"].dump());
        }
        if (request.contains("tags")) {
            updates.push_back("tags = $" + std::to_string(++param_count));
            params.push_back(request["tags"].dump());
        }

        if (updates.empty()) {
            return R"({"error": "No fields to update"})";
        }

        updates.push_back("updated_at = CURRENT_TIMESTAMP");

        std::string query = "UPDATE training_courses SET " + updates[0];
        for (size_t i = 1; i < updates.size(); i++) {
            query += ", " + updates[i];
        }
        query += " WHERE course_id = $" + std::to_string(++param_count);
        query += " RETURNING course_id, title, updated_at";

        params.push_back(course_id);

        std::vector<const char*> param_values;
        for (const auto& p : params) {
            param_values.push_back(p.c_str());
        }

        PGresult* result = PQexecParams(
            conn,
            query.c_str(),
            param_values.size(),
            nullptr,
            param_values.data(),
            nullptr,
            nullptr,
            0
        );

        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            std::string error = PQerrorMessage(conn);
            PQclear(result);
            logger_->log(LogLevel::ERROR, "Failed to update course: " + error);
            return R"({"error": "Failed to update course"})";
        }

        if (PQntuples(result) == 0) {
            PQclear(result);
            return R"({"error": "Course not found"})";
        }

        json response = {
            {"course_id", PQgetvalue(result, 0, 0)},
            {"title", PQgetvalue(result, 0, 1)},
            {"updated_at", PQgetvalue(result, 0, 2)},
            {"message", "Course updated successfully"}
        };

        PQclear(result);
        return response.dump();

    } catch (const json::exception& e) {
        logger_->log(LogLevel::ERROR, "JSON parsing error in handle_update_course: " + std::string(e.what()));
        return R"({"error": "Invalid JSON format"})";
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_update_course: " + std::string(e.what()));
        return R"({"error": "Internal server error"})";
    }
}

std::string TrainingAPIHandlers::extract_user_id_from_jwt([[maybe_unused]] const std::map<std::string, std::string>& headers) {
    // This would be implemented based on the JWT extraction pattern used in the existing codebase
    // For now, return empty string - this should be implemented according to the existing pattern
    return "";
}

bool TrainingAPIHandlers::validate_json_schema([[maybe_unused]] const nlohmann::json& data, [[maybe_unused]] const std::string& schema_type) {
    // This would implement JSON schema validation based on the schema type
    // For now, return true - this should be implemented with proper validation
    return true;
}

} // namespace training
} // namespace regulens