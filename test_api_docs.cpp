/**
 * Test program for API documentation generation
 * This program tests the OpenAPI generator and saves the specification to a file
 */

#include <iostream>
#include <fstream>
#include "shared/api_docs/openapi_generator.hpp"

int main() {
    try {
        std::cout << "🧪 Testing OpenAPI Generator..." << std::endl;

        // Create OpenAPI generator
        regulens::OpenAPIGenerator generator(
            "Regulens API",
            "1.0.0",
            "Agentic AI Compliance System API"
        );

        // Register all endpoints
        regulens::register_regulens_api_endpoints(generator);

        // Generate JSON specification
        std::string json_spec = generator.generate_json();
        std::cout << "✅ Generated OpenAPI JSON specification (" << json_spec.length() << " characters)" << std::endl;

        // Save to file
        bool saved = generator.write_to_file("api_specification.json", "json");
        if (saved) {
            std::cout << "✅ Saved OpenAPI specification to api_specification.json" << std::endl;
        } else {
            std::cout << "❌ Failed to save OpenAPI specification" << std::endl;
            return 1;
        }

        // Generate YAML specification
        std::string yaml_spec = generator.generate_yaml();
        std::cout << "✅ Generated OpenAPI YAML specification (" << yaml_spec.length() << " characters)" << std::endl;

        // Save YAML to file
        bool yaml_saved = generator.write_to_file("api_specification.yaml", "yaml");
        if (yaml_saved) {
            std::cout << "✅ Saved OpenAPI YAML specification to api_specification.yaml" << std::endl;
        } else {
            std::cout << "❌ Failed to save OpenAPI YAML specification" << std::endl;
            return 1;
        }

        // Test Swagger UI HTML generation
        std::string swagger_html = regulens::OpenAPIGenerator::generate_swagger_ui_html("/api/docs");
        std::cout << "✅ Generated Swagger UI HTML (" << swagger_html.length() << " characters)" << std::endl;

        // Test ReDoc HTML generation
        std::string redoc_html = regulens::OpenAPIGenerator::generate_redoc_html("/api/docs");
        std::cout << "✅ Generated ReDoc HTML (" << redoc_html.length() << " characters)" << std::endl;

        std::cout << "\n🎉 API Documentation Generation Test Completed Successfully!" << std::endl;
        std::cout << "\n📋 Test Results:" << std::endl;
        std::cout << "   - JSON Specification: api_specification.json" << std::endl;
        std::cout << "   - YAML Specification: api_specification.yaml" << std::endl;
        std::cout << "   - Swagger UI: Available at /docs endpoint" << std::endl;
        std::cout << "   - ReDoc: Available at /redoc endpoint" << std::endl;
        std::cout << "   - OpenAPI JSON: Available at /api/docs endpoint" << std::endl;

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "❌ Error during API documentation generation test: " << e.what() << std::endl;
        return 1;
    }
}
