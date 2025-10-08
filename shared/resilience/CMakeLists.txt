# Resilience Module CMakeLists.txt
# Circuit breaker and resilience patterns for production-grade fault tolerance

set(RESILIENCE_SOURCES
    circuit_breaker.cpp
)

set(RESILIENCE_HEADERS
    circuit_breaker.hpp
)

# Create resilience library
add_library(resilience ${RESILIENCE_SOURCES} ${RESILIENCE_HEADERS})

# Include directories
target_include_directories(resilience
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/shared
)

# Link dependencies
# Note: config, logging, error_handler are part of regulens_shared
# No external link dependencies needed for resilience

# Set library properties
set_target_properties(resilience PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# Install headers
install(FILES ${RESILIENCE_HEADERS}
    DESTINATION include/regulens/resilience
)

# Install library
install(TARGETS resilience
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
