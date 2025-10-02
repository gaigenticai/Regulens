#include <iostream>
#include <cstdlib>
#include "shared/config/environment_validator.hpp"
#include "shared/logging/structured_logger.hpp"

int main() {
    // Set some test environment variables
    setenv("REGULENS_ENVIRONMENT", "production", 1);
    setenv("DB_HOST", "prod-db.example.com", 1);
    setenv("DB_USER", "regulens_user", 1);
    setenv("DB_PASSWORD", "StrongPass123!", 1);
    setenv("ENCRYPTION_MASTER_KEY", "0123456789012345678901234567890123456789012345678901234567890123", 1);
    setenv("JWT_SECRET_KEY", "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123", 1);
    
    // Create validator with singleton logger
    auto logger = std::shared_ptr<regulens::StructuredLogger>(&regulens::StructuredLogger::get_instance(), [](regulens::StructuredLogger*){});
    regulens::EnvironmentValidator validator(logger.get());
    
    // Run validation
    auto result = validator.validate_all();
    
    std::cout << "Environment validation result: " << (result.valid ? "PASS" : "FAIL") << std::endl;
    std::cout << "Errors: " << result.errors.size() << std::endl;
    std::cout << "Warnings: " << result.warnings.size() << std::endl;
    
    if (!result.errors.empty()) {
        std::cout << "First error: " << result.errors[0] << std::endl;
    }
    
    return result.valid ? 0 : 1;
}
