# Metrics Module CMakeLists.txt
# Prometheus metrics collection and monitoring for enterprise observability

set(METRICS_SOURCES
    prometheus_metrics.cpp
    prometheus_client.cpp
)

set(METRICS_HEADERS
    prometheus_metrics.hpp
    prometheus_client.hpp
)

# Create metrics library
add_library(metrics ${METRICS_SOURCES} ${METRICS_HEADERS})

# Include directories
target_include_directories(metrics
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/shared
)

# Link dependencies
target_link_libraries(metrics
    PUBLIC
        CURL::libcurl
)

# Set library properties
set_target_properties(metrics PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# Install headers
install(FILES ${METRICS_HEADERS}
    DESTINATION include/regulens/metrics
)

# Install library
install(TARGETS metrics
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
