# Cache Module CMakeLists.txt
# Redis-based distributed caching for enterprise performance optimization

set(CACHE_SOURCES
    redis_client.cpp
)

set(CACHE_HEADERS
    redis_client.hpp
)

# Create cache library
add_library(cache ${CACHE_SOURCES} ${CACHE_HEADERS})

# Include directories
target_include_directories(cache
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/shared
)

# Link dependencies
# Note: config, logging, error_handler are part of regulens_shared
# No external link dependencies needed for cache

# In production, would also link to hiredis or similar Redis C client:
# find_package(hiredis REQUIRED)
# target_link_libraries(cache PUBLIC hiredis::hiredis)

# Set library properties
set_target_properties(cache PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# Install headers
install(FILES ${CACHE_HEADERS}
    DESTINATION include/regulens/cache
)

# Install library
install(TARGETS cache
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
