# Kubernetes Operator Framework CMakeLists.txt
# Enterprise-grade Kubernetes operator for managing Regulens custom resources

set(OPERATOR_SOURCES
    operator_framework.cpp
    agent_orchestrator_controller.cpp
    compliance_agent_controller.cpp
    regulatory_data_controller.cpp
)

set(OPERATOR_HEADERS
    operator_framework.hpp
    agent_orchestrator_controller.hpp
    compliance_agent_controller.hpp
    regulatory_data_controller.hpp
)

# Create operator framework library
add_library(k8s_operator ${OPERATOR_SOURCES} ${OPERATOR_HEADERS})

# Include directories
target_include_directories(k8s_operator
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/shared
        ${CMAKE_SOURCE_DIR}/infrastructure
)

# Link dependencies
target_link_libraries(k8s_operator
    PUBLIC
        config
        logging
        error_handler
        metrics
)

# In production, would also link to Kubernetes client-go or similar:
# find_package(kubernetes REQUIRED)
# target_link_libraries(k8s_operator PUBLIC kubernetes::client)

# Set library properties
set_target_properties(k8s_operator PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# Install headers
install(FILES ${OPERATOR_HEADERS}
    DESTINATION include/regulens/k8s/operator
)

# Install library
install(TARGETS k8s_operator
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
